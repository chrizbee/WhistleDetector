#ifndef CONFIG_H
#define CONFIG_H

#include <QStringList>
#include <QHostAddress>

namespace config {

	// Audio input settings
	static const uint sampleSize = 8;		// 8 bit per sample
	static const uint sampleRate = 44100;	// 44100 samples per second
	static const uint channelCount = 1;		// Single channel

	// Measure limits
	static const double cutoffMag = 300;    // Cutoff magnitude
	static const double cutoffLower = 200;	// Lower cutoff frequency
	static const double cutoffUpper = 3000; // Upper cutoff frequency

	// Frequencies and tolerances
	static const double f1 = 1800;			// First freqency
	static const double f2 = 1400;			// Second freqency
	static const double pause = 300;		// Pause between two tones in ms
	static const double deltaF = 150;		// Frequencies +- this is ok
	static const double deltaT = 100;		// Pause +- this is ok
	static const QList<double> freqs = {
		1800, 1400, 1800, 1400, 1800, 1400
	};

	// MQTT client settings
	static const uint port = 1883;
	static const QHostAddress broker("192.168.10.100");
	static const QString clientName = "WhistleDetector";
	static const QString subTopic = "cmnd/whistledetector/POWER";
	static const QString pubTopic = "stat/whistledetector/POWER";
	static const QStringList toggleTopics = {
		"cmnd/ledchr1/POWER",
		"cmnd/ledchr2/POWER"
	};
}

#endif // CONFIG_H
