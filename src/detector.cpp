#include "detector.h"
#include "settings.h"
#include "visualizer.h"
#include <QIODevice>
#include <QAudioInput>
#include <QCoreApplication>
#include <xtensor/xio.hpp>
#include <xtensor/xview.hpp>
#include <xtensor/xsort.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor-fftw/basic.hpp>
#include <xtensor-fftw/helper.hpp>

typedef xt::xarray<double> arrd;

static struct {
    arrd freqs;
    arrd window;
    uint lowerIndex;
    uint upperIndex;
    uint periodSize;
    uint bytesPerSample;
    uint sampleCount;
} f;

Detector::Detector(QObject *parent) :
    QObject(parent),
    device_(nullptr),
    vis_(nullptr),
    lastFreq_(0.0),
    number_(0)
{
    // Create format
    QAudioFormat format;
    format.setChannelCount(1);
    format.setCodec("audio/pcm");
    format.setSampleRate(ConfM.value<int>("sampleRate"));
    format.setSampleSize(ConfM.value<int>("sampleSize"));
    format.setSampleType(QAudioFormat::SampleType::UnSignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);

    // Check if format is supported
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    qInfo() << "Using Device" << info.deviceName();
    if (!info.isFormatSupported(format)) {
        format = info.nearestFormat(format);
        qWarning() << "Format not supported - what is supported:";
        qInfo() << "Byte Order     :" << info.supportedByteOrders();
        qInfo() << "Channel Counts :" << info.supportedChannelCounts();
        qInfo() << "Codecs         :" << info.supportedCodecs();
        qInfo() << "Sample Rates   :" << info.supportedSampleRates();
        qInfo() << "Sample Sizes   :" << info.supportedSampleSizes();
        qInfo() << "Sample Types   :" << info.supportedSampleTypes();
        qInfo() << "Nearest Format :" << format;
    }

    // Create audio input with format
    input_ = new QAudioInput(format, this);

    // Configure timer
    const int pause = ConfM.value<int>("pause");
    const int maxDeltaT = ConfM.value<int>("deltaT");
    timer_.setSingleShot(true);
    timer_.setInterval(pause + maxDeltaT);
    connect(&timer_, &QTimer::timeout, [this]() { lastFreq_ = 0; number_ = 0; });

    // Create and show visualizer
#ifdef VISUALIZE
    vis_ = new Visualizer;
    vis_->show();
#endif
}

Detector::~Detector()
{
    // Noop if not created
    delete vis_;
}

void Detector::start()
{
    // Static settings
    static const int cutoffLower = ConfM.value<double>("cutoffLower");
    static const int cutoffUpper = ConfM.value<double>("cutoffUpper");
    static const double cutoffMag = ConfM.value<double>("cutoffMag");
    static const double twoPi = 2 * xt::numeric_constants<double>::PI;

    // Start recording
    device_ = input_->start();

    // Fourier precalculations
    // QAudioInput::periodSize() can only be accessed after start().
    QAudioFormat format = input_->format();
    f.periodSize = input_->periodSize();
    f.bytesPerSample = format.sampleSize() / 8;
    f.sampleCount = f.periodSize / f.bytesPerSample;
    f.freqs = xt::fftw::rfftfreq(f.sampleCount, 1.0 / format.sampleRate());
    f.lowerIndex = (uint)(xt::argmin(xt::abs(f.freqs - cutoffLower))());
    f.upperIndex = (uint)(xt::argmin(xt::abs(f.freqs - cutoffUpper))());
    f.freqs = xt::view(f.freqs, xt::range(f.lowerIndex, f.upperIndex + 1));
    f.window = arrd::from_shape({f.sampleCount});
    for (uint i = 0; i < f.sampleCount; ++i)
        f.window(i) = 0.5 * (1 - cos(twoPi * i / (f.sampleCount - 1))); // Hanning

    // Set x axis of visualizer
#ifdef VISUALIZE
    vis_->setCutoff(cutoffMag);
    vis_->setX(std::vector<double>(f.freqs.begin(), f.freqs.end()));
#endif

    // Connect readyRead
    connect(device_, &QIODevice::readyRead, this, &Detector::onBlockReady);
}

void Detector::stop()
{
    // Stop recording
    input_->stop();
    device_ = nullptr;
}

void Detector::onBlockReady()
{
    // Static config
    static const double cutoffMag = ConfM.value<double>("cutoffMag");
    static const double maxToMean = ConfM.value<double>("maxToMean");

    // Check if device exists
    if (device_ == nullptr)
        return;

    // Read one period of data at a time
    // TODO: SampleCount should be a power of 2 for faster FFTs
    while ((uint)(input_->bytesReady()) >= f.periodSize) {

        // Read a block of samples
        arrd data = arrd::from_shape({f.sampleCount});
        QByteArray byteData = device_->read(f.periodSize);
        const char *d = byteData.constData();

        // Convert bytes to double and apply window function
        for (uint i = 0; i < f.sampleCount; ++i) {
            int64_t val = static_cast<int64_t>(*d);
            for (uint8_t j = 1; j < f.bytesPerSample; ++j)
                val |= *(d + j) << 8; // Little endian!
            data(i) = static_cast<double>(val) * f.window(i);
            d += f.bytesPerSample;
        }

        // Calculate FFT and use absolute magnitudes only
        // RFFT -> Negative frequency terms are not calculated
        // Also extract only frequencies within the limits
        arrd mags = xt::abs(xt::fftw::rfft(data));
        mags = xt::view(mags, xt::range(f.lowerIndex, f.upperIndex + 1));

        // Get maximum and mean
        size_t maxIdx = xt::argmax(mags)();
        double mean = xt::mean(mags)();
        double mag = mags(maxIdx);

        // Try to detect pattern only if
        // - the magnitude of the main frequency is high enough
        // - the max to mean ratio is big enough
        // -> This will prevent random detections of loud noise
        if (mag > cutoffMag && mag / mean > maxToMean)
            detect(f.freqs(maxIdx));

        // Visualize frequencies
#ifdef VISUALIZE
        vis_->setValues(std::vector<double>(mags.begin(), mags.end()));
#endif
    }
}

void Detector::detect(double freq)
{
    // Static config
    static const int maxDeltaF = ConfM.value<int>("deltaF");
    static const int maxDeltaT = ConfM.value<int>("deltaT");
    static const QList<double> freqs = toDouble(ConfM.value<QStringList>("freqs"));

    // Check for index out of range
    uint cnt = freqs.count();
    if (number_ >= cnt)
        return;

    // Get delta values
    double expected = lastFreq_ + freqs[number_];
    double deltaF = abs(freq - expected);
    int deltaT = abs(timer_.remainingTime() - maxDeltaT);

    // First frequency will be more forgiving
    if ((number_ == 0 && deltaF <= maxDeltaF * 1.5) ||
        (deltaF <= maxDeltaF && deltaT <= maxDeltaT)) {

        // Set last frequency to this one
        // First real frequency will be the base for all upcoming
        lastFreq_ = number_ == 0 ? freq : expected;

        // Check if end of pattern is reached
        if (++number_ >= cnt) {
            number_ = 0;
            lastFreq_ = 0.0;
            timer_.stop();
            emit patternDetected();
        } else timer_.start();
    }
}

QList<double> toDouble(const QStringList &stringList)
{
    // Convert QStringList to QList<double>
    QList<double> out;
    for (const QString &str : stringList)
        out.append(str.toDouble());
    return out;
}
