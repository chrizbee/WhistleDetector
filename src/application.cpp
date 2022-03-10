#include "application.h"
#include "settings.h"
#include "detector.h"
#include <QNetworkReply>

Application::Application(int &argc, char **argv) :
    QApp(argc, argv),
    detector_(new Detector(this)),
    manager_(new QNetworkAccessManager(this))
{
    // Connect signals and start
    connect(detector_, &Detector::patternDetected, this, &Application::onPatternDetected);
    detector_->start();
}

Application::~Application()
{
}

void Application::onPatternDetected()
{
    // Toggle light
    static const QStringList addressList = ConfM.value<QString>("address").split(' ');
    qInfo() << "Pattern detected";
    for (const QString &address : addressList) {
        manager_->post(QNetworkRequest(QUrl(address)), QByteArray());
    }
}
