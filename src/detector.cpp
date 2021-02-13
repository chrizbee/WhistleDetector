#include "detector.h"
#include "config.h"
#include <QIODevice>
#include <QAudioInput>
#include <xtensor/xview.hpp>
#include <xtensor/xsort.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor-fftw/basic.hpp>
#include <xtensor-fftw/helper.hpp>
#include <QDebug>

typedef xt::xarray<char> arrc;
typedef xt::xarray<double> arrd;

static struct {
	arrd freqs;
	uint lowerIndex;
	uint upperIndex;
	uint periodSize;
} f;

Detector::Detector(QObject *parent) :
	QObject(parent),
	enabled_(true),
	i_(0)
{
	// Create format
	QAudioFormat format;
	format.setCodec("audio/pcm");
	format.setSampleRate(config::sampleRate);
	format.setSampleSize(config::sampleSize);
	format.setChannelCount(config::channelCount);
	format.setSampleType(QAudioFormat::SampleType::SignedInt);

	// Check format
	QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
	qInfo() << "Using Device" << info.deviceName() << ".";
	if (!info.isFormatSupported(format)) {
		qCritical() << "Format not supported - using nearest format";
		format = info.nearestFormat(format);
	}

	// Create audio input with format and start recording
	input_ = new QAudioInput(format, this);
	device_ = input_->start();

	// Fourier precalculations
	f.periodSize = input_->periodSize();
	f.freqs = xt::fftw::rfftfreq(f.periodSize, 1.0 / config::sampleRate);
	f.lowerIndex = xt::argmin(xt::abs(f.freqs - config::cutoffLower))();
	f.upperIndex = xt::argmin(xt::abs(f.freqs - config::cutoffUpper))();
	f.freqs = xt::view(f.freqs, xt::range(f.lowerIndex, f.upperIndex + 1));

	// Configure timer
	timer_.setSingleShot(true);
	timer_.setInterval(config::pause + config::deltaT);

	// Connect signals
	connect(&timer_, &QTimer::timeout, [this]() { i_ = 0; });
	connect(device_, &QIODevice::readyRead, this, &Detector::onBlockReady);
}

void Detector::setEnabled(bool enabled)
{
	// Set enabled
	enabled_ = enabled;
}

void Detector::detect(double freq)
{
	// Check for index out of range
	uint cnt = config::freqs.count();
	if (i_ >= cnt)
		return;

	// Check for correct frequency and pause
	if ((abs(freq - config::freqs[i_]) <= config::deltaF) &&
		(i_ == 0 || timer_.remainingTime() <= config::deltaT * 2)) {

		// Check if end of pattern is reached
		if (++i_ >= cnt) {
			i_ = 0;
			timer_.stop();
			emit patternDetected();
		} else timer_.start();
	}
}

void Detector::onBlockReady()
{
	// Check if can read
	if (input_->bytesReady() == f.periodSize) {

		// Read a block of samples
		arrc cdata = arrc::from_shape({f.periodSize});
		device_->read(static_cast<char*>(cdata.data()), f.periodSize);
		arrd data = xt::cast<double>(cdata);

		// Do FFT only if enabled
		if (enabled_) {
			arrd mags = xt::abs(xt::fftw::rfft(data));
			mags = xt::view(mags, xt::range(f.lowerIndex, f.upperIndex + 1));

			// Get maximum
			uint maxIdx = xt::argmax(mags)();
			double mag = mags(maxIdx);

			// Check for pattern only if magnitude is high enough
			if (mag > config::cutoffMag)
				detect(f.freqs(maxIdx));
		}
	}
}
