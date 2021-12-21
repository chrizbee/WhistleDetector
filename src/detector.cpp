#include "detector.h"
#include "settings.h"
#include <QIODevice>
#include <QAudioInput>
#include <QCoreApplication>
#include <xtensor/xview.hpp>
#include <xtensor/xsort.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor-fftw/basic.hpp>
#include <xtensor-fftw/helper.hpp>

#define DEBUG // Debug frequency and magnitude
#ifdef DEBUG
#include <iostream>
#include <iomanip>
#include <string>
#endif

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
    enabled_(true),
    number_(0)
{
    // Create format
    QAudioFormat format;
    format.setChannelCount(1);
    format.setCodec("audio/pcm");
    format.setSampleRate(ConfM.value<int>("sampleRate"));
    format.setSampleSize(ConfM.value<int>("sampleSize"));
    format.setSampleType(QAudioFormat::SampleType::UnSignedInt);

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

    // Print configuration
    const double cutoffMag = ConfM.value<double>("cutoffMag");
    const int cutoffLower = ConfM.value<double>("cutoffLower");
    const int cutoffUpper = ConfM.value<double>("cutoffUpper");
    const int pause = ConfM.value<int>("pause");
    const int maxDeltaF = ConfM.value<int>("deltaF");
    const int maxDeltaT = ConfM.value<int>("deltaT");
    const QList<double> freqs = toDouble(ConfM.value<QStringList>("freqs"));
    qInfo() << "Using configuration:";
    qInfo() << "cutoffMag" << cutoffMag;
    qInfo() << "cutoffLower" << cutoffLower;
    qInfo() << "cutoffUpper" << cutoffUpper;
    qInfo() << "pause" << pause;
    qInfo() << "maxDeltaF" << maxDeltaF;
    qInfo() << "maxDeltaT" << maxDeltaT;
    qInfo() << "freqs" << freqs;

    // Create audio input with format and start recording
    input_ = new QAudioInput(format, this);
    device_ = input_->start();

    // Fourier precalculations
    f.periodSize = input_->periodSize();
    f.bytesPerSample = format.sampleSize() / 8;
    f.sampleCount = f.periodSize / f.bytesPerSample;
    f.freqs = xt::fftw::rfftfreq(f.sampleCount, 1.0 / format.sampleRate());
    f.lowerIndex = (uint)(xt::argmin(xt::abs(f.freqs - cutoffLower))());
    f.upperIndex = (uint)(xt::argmin(xt::abs(f.freqs - cutoffUpper))());
    f.freqs = xt::view(f.freqs, xt::range(f.lowerIndex, f.upperIndex + 1));
    f.window = arrd::from_shape({f.sampleCount});
    for (uint i = 0; i < f.sampleCount; ++i)
        f.window(i) = 0.5 * (1 - cos(2 * xt::numeric_constants<double>::PI * i / (f.sampleCount - 1))); // Hanning

    // Configure timer
    timer_.setSingleShot(true);
    timer_.setInterval(pause + maxDeltaT);

    // Connect signals
    connect(&timer_, &QTimer::timeout, [this]() { number_ = 0; });
    connect(device_, &QIODevice::readyRead, this, &Detector::onBlockReady);
}

void Detector::setEnabled(bool enabled)
{
    // Set enabled
    enabled_ = enabled;
}

void Detector::onBlockReady()
{
    // Static config
    static const double cutoffMag = ConfM.value<double>("cutoffMag");

    // Check if can read
    while ((uint)(input_->bytesReady()) >= f.periodSize) {

        // Read a block of samples
        arrd data = arrd::from_shape({f.sampleCount});
        QByteArray byteData = device_->read(f.periodSize);
        const char *d = byteData.constData();

        // Continue if enabled
        if (enabled_) {

            // Convert bytes to double and apply window function
            // TODO: Endianness!
            for (uint i = 0; i < f.sampleCount; ++i) {
                int64_t val = static_cast<int64_t>(*d);
                for (uint j = 1; j < f.bytesPerSample; ++j)
                    val |= *(d + j) << 8;
                data(i) = static_cast<double>(val) * f.window(i);
                d += f.bytesPerSample;
            }

            // Calculate FFT and use absolute magnitudes only
            arrd mags = xt::abs(xt::fftw::rfft(data));
            mags = xt::view(mags, xt::range(f.lowerIndex, f.upperIndex + 1));

            // Get maximum
            size_t maxIdx = xt::argmax(mags)();
            double mag = mags(maxIdx);

#ifdef DEBUG
            // Debug frequency and magnitude
            debug(f.freqs(maxIdx), mag, cutoffMag);
#else
            // Check for pattern only if magnitude is high enough
            if (mag > cutoffMag)
                detect(f.freqs(maxIdx));
#endif
        }
    }
}

void Detector::detect(double freq)
{
    // Static config
    static const int maxDeltaF = ConfM.value<int>("deltaF");
    static const int maxDeltaT = ConfM.value<int>("deltaT");
    static const QList<double> freqs = toDouble(ConfM.value<QStringList>("freqs"));
    static double lastFreq = 0.0;

    // Check for index out of range
    uint cnt = freqs.count();
    if (number_ >= cnt)
        return;

    // Get delta values
    if (number_ == 0)
        lastFreq = 0.0;
    double expected = lastFreq + freqs[number_];
    double deltaF = abs(freq - expected);
    int deltaT = abs(timer_.remainingTime() - maxDeltaT);

    // First frequency will be more forgiving
    if ((number_ == 0 && deltaF <= maxDeltaF * 1.5) ||
        (deltaF <= maxDeltaF && deltaT <= maxDeltaT)) {

        // Set last frequency to this one
        // First real frequency will be the base for all upcoming
        lastFreq = number_ == 0 ? freq : expected;

        // Check if end of pattern is reached
        if (++number_ >= cnt) {
            number_ = 0;
            lastFreq = 0.0;
            timer_.stop();
            emit patternDetected();
        } else timer_.start();
    }
}

void Detector::debug(double freq, double mag, double cutoff)
{
    uint part1 = 0, part2 = 0, rest1 = 50, rest2 = 50;
    uint ticks = (uint)(50 * mag / cutoff);
    if (ticks <= 50) {
        part1 = ticks;
        rest1 = 50 - ticks;
    } else if (ticks <= 100) {
        part1 = 50;
        rest1 = 0;
        part2 = ticks - 50;
        rest2 = 100 - ticks;
    } else {
        part1 = 50;
        rest1 = 0;
        part2 = 50;
        rest2 = 0;
    }
    std::cout << std::string(ticks, '█');
}

QList<double> toDouble(const QStringList &stringList)
{
    // Convert QStringList to QList<double>
    QList<double> out;
    for (const QString &str : stringList)
        out.append(str.toDouble());
    return out;
}
