#ifndef DETECTOR_H
#define DETECTOR_H

#include <QObject>
#include <QTimer>

class QAudioInput;
class QIODevice;

class Detector : public QObject
{
	Q_OBJECT

public:
	Detector(QObject *parent = nullptr);
	void setEnabled(bool enabled);

protected:
	void precalculate(int periodSize);
	void detect(double freq);

private slots:
	void onBlockReady();

signals:
	void patternDetected();

private:
	QAudioInput *input_;
	QIODevice *device_;
	QTimer timer_;
	bool enabled_;
	uint i_;
};

#endif // DETECTOR_H
