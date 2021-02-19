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
#include <cmath>
#include <QDebug>

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
	i_(0)
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
		qWarning() << "Format not supported - what is supported:";
		qInfo() << "Byte Order     :" << info.supportedByteOrders();
		qInfo() << "Channel Counts :" << info.supportedChannelCounts();
		qInfo() << "Codecs         :" << info.supportedCodecs();
		qInfo() << "Sample Rates   :" << info.supportedSampleRates();
		qInfo() << "Sample Sizes   :" << info.supportedSampleSizes();
		qInfo() << "Sample Types   :" << info.supportedSampleTypes();
	}

	// Create audio input with format and start recording
	input_ = new QAudioInput(format, this);
	device_ = input_->start();

	// Fourier precalculations
	f.periodSize = input_->periodSize();
	f.bytesPerSample = format.sampleSize() / 8;
	f.sampleCount = f.periodSize / f.bytesPerSample;
	f.freqs = xt::fftw::rfftfreq(f.sampleCount, 1.0 / format.sampleRate());
	f.lowerIndex = xt::argmin(xt::abs(f.freqs - ConfM.value<int>("cutoffLower")))();
	f.upperIndex = xt::argmin(xt::abs(f.freqs - ConfM.value<int>("cutoffUpper")))();
	f.freqs = xt::view(f.freqs, xt::range(f.lowerIndex, f.upperIndex + 1));
	f.window = arrd::from_shape({f.sampleCount});
	for (uint i = 0; i < f.sampleCount; ++i)
		f.window(i) = 0.5 * (1 - cos(2 * M_PI * i / (f.sampleCount - 1))); // Hanning

	qDebug() << "Bytes/Sample:" << f.bytesPerSample << "\nSample Size:" << format.sampleSize();

	// Configure timer
	timer_.setSingleShot(true);
	timer_.setInterval(ConfM.value<int>("pause") + ConfM.value<int>("deltaT"));

	// Connect signals
	connect(&timer_, &QTimer::timeout, [this]() { i_ = 0; });
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
	while (input_->bytesReady() >= f.periodSize) {

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
			uint maxIdx = xt::argmax(mags)();
			double mag = mags(maxIdx);

			// Check for pattern only if magnitude is high enough
			if (mag > cutoffMag) {
				qDebug() << mag << f.freqs(maxIdx);
				detect(f.freqs(maxIdx));
			}
		}
	}
}

void Detector::detect(double freq)
{
	// Static config
	static const int deltaF = ConfM.value<int>("deltaF");
	static const int deltaT = ConfM.value<int>("deltaT");
	static const QList<double> freqs = toDouble(ConfM.value<QStringList>("freqs"));

	// Check for index out of range
	uint cnt = freqs.count();
	if (i_ >= cnt)
		return;

	// Check for correct frequency and pause
	if ((abs(freq - freqs[i_]) <= deltaF) &&
		(i_ == 0 || timer_.remainingTime() <= deltaT * 2)) {

		// Check if end of pattern is reached
		if (++i_ >= cnt) {
			i_ = 0;
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
