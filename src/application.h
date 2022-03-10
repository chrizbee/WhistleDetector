#ifndef APPLICATION_H
#define APPLICATION_H

#ifdef VISUALIZE
#include <QApplication>
typedef QApplication QApp;
#else
#include <QCoreApplication>
typedef QCoreApplication QApp;
#endif
#include <QNetworkAccessManager>

class Detector;

class Application : public QApp
{
    Q_OBJECT

public:
    Application(int &argc, char **argv);
    ~Application();

private slots:
    void onPatternDetected();

private:
    Detector *detector_;
    QNetworkAccessManager *manager_;
};

#endif // APPLICATION_H
