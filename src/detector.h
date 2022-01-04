#ifndef DETECTOR_H
#define DETECTOR_H

#include <QObject>
#include <QTimer>
#include <QVector>

class QAudioInput;
class QIODevice;
class Visualizer;

class Detector : public QObject
{
    Q_OBJECT

public:
    Detector(QObject *parent = nullptr);
    ~Detector();
    void start();
    void stop();

private slots:
    void onBlockReady();

protected:
    void detect(double freq);

signals:
    void patternDetected();

private:
    QAudioInput *input_;
    QIODevice *device_;
    Visualizer *vis_;
    double lastFreq_;
    QTimer timer_;
    uint number_;
};

QList<double> toDouble(const QStringList &stringList);

#endif // DETECTOR_H
