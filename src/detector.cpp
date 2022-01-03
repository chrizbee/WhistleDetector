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

    // Get and print configuration
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
    connect(device_, &QIODevice::readyRead, this, &Detector::onBlockReady);
    connect(&timer_, &QTimer::timeout, [this]() {
        lastFreq_ = 0;
        number_ = 0;
#ifdef DEBUG
        qDebug() << "\n--------- next try ---------";
#endif
    });
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
    static Lowpass lowpass(ConfM.value<int>("magnitudeLowpass"));

    // Check if can read
    while ((uint)(input_->bytesReady()) >= f.periodSize) {

        // Read a block of samples
        arrd data = arrd::from_shape({f.sampleCount});
        QByteArray byteData = device_->read(f.periodSize);
        const char *d = byteData.constData();

        // Continue if enabled
        if (enabled_) {

            // Convert bytes to double and apply window function
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
            double mag = lowpass.add(mags(maxIdx));

#ifdef DEBUG
            debug(f.freqs(maxIdx), mag, cutoffMag);
#endif
            // Check for pattern only if magnitude is high enough
            if (mag > cutoffMag)
                detect(f.freqs(maxIdx));
        }
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
#ifdef DEBUG
        qDebug() << "\nTone" << expected << ":" << freq;
#endif

        // Check if end of pattern is reached
        if (++number_ >= cnt) {
            number_ = 0;
            lastFreq_ = 0.0;
            timer_.stop();
            emit patternDetected();
        } else timer_.start();
    }
}

void Detector::debug(double freq, double mag, double cutoff)
{
    // Cutoff is at half the line
    static double maxLevel = mag;
    static const uint maxTicks = 100, cutoffTicks = 50;

    // Check for max level
    if (mag > maxLevel) maxLevel = mag;

    // Calculate number of ticks and generate string
    uint ticks = (uint)(50 * mag / cutoff);
    if (ticks > maxTicks) ticks = maxTicks;
    QString line = QString(ticks, '=') + QString(maxTicks - ticks, ' ');
    line[0] = '['; line[cutoffTicks] = '|'; line[maxTicks] = ']';

    // Print string and CR
    std::cout << line.toStdString()
              << " Max Level: " << maxLevel
              << " Frequency: " << freq
              << '\r' << std::flush;
}

QList<double> toDouble(const QStringList &stringList)
{
    // Convert QStringList to QList<double>
    QList<double> out;
    for (const QString &str : stringList)
        out.append(str.toDouble());
    return out;
}

Lowpass::Lowpass(int size) :
    size_(size > 0 ? size : 1),
    samples_(size_, 0),
    index_(0)
{
}

double Lowpass::add(double value)
{
    // Replace oldest value with new one
    samples_[index_] = value;

    // Loop index at end
    if (++index_ >= size_)
        index_ = 0;

    // Calculate and return the average
    double sum = 0.0;
    for (int i = 0; i < size_; ++i)
        sum += samples_[i];
    return sum / size_;
}
