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

signals:
    void patternDetected();

private:
    QAudioInput *input_;
    QIODevice *device_;
    QTimer timer_;
    bool enabled_;
    uint number_;
};

QList<double> toDouble(const QStringList &stringList);

#endif // DETECTOR_H
