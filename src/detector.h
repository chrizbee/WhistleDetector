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

private slots:
    void onBlockReady();

protected:
    void detect(double freq);
    void debug(double freq, double mag, double cutoff);

signals:
    void patternDetected();

private:
    bool enabled_;
    QAudioInput *input_;
    QIODevice *device_;
    double lastFreq_;
    QTimer timer_;
    uint number_;
};

class Lowpass
{
public:
    Lowpass(int size);
    double add(double value);

private:
    const int size_;
    QVector<double> samples_;
    int index_;
};

QList<double> toDouble(const QStringList &stringList);

#endif // DETECTOR_H
