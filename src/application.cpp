#include "application.h"
#include "settings.h"
#include "detector.h"
#include <qmqtt.h>

Application::Application(int &argc, char **argv) :
    QApp(argc, argv),
    detector_(new Detector(this)),
    client_(new QMQTT::Client(QHostAddress(ConfM.value<QString>("broker")), ConfM.value<int>("port"), this))
{
    // Set id and connect signals
    connect(detector_, &Detector::patternDetected, this, &Application::onPatternDetected);
    connect(client_, &QMQTT::Client::received, this, &Application::onClientReceived);
    connect(client_, &QMQTT::Client::connected, [this]() {
        static const QString subTopic = ConfM.value<QString>("subTopic");
        qInfo() << "MQTT Client connected - subscribing to" << subTopic;
        client_->subscribe(subTopic);
        qInfo() << "Start recording audio";
        detector_->start();
    });

    // Setup client
    QString clientName = ConfM.value<QString>("clientName");
    client_->setClientId(clientName);
    client_->setAutoReconnect(true);
    client_->connectToHost();
}

Application::~Application()
{
}

void Application::onPatternDetected()
{
    // Toggle light
    static const QStringList toggleTopics = ConfM.value<QStringList>("toggleTopics");
    qInfo() << "Pattern detected";
    int cnt = toggleTopics.length();
    for (int i = 0; i < cnt; ++i)
        client_->publish(QMQTT::Message(i, toggleTopics[i], "TOGGLE"));
}

void Application::onClientReceived(const QMQTT::Message &message)
{
    // Turn WhistleDetector on / off
    static const QString pubTopic = ConfM.value<QString>("pubTopic");
    QString payload = message.payload().toUpper();
    bool on = payload.contains("ON") ? true : false;
    QString keyword = on ? "ON" : "OFF";
    qInfo() << "Turning WhistleDetector" << keyword;
    QMQTT::Message sendMessage(0, pubTopic, keyword.toUtf8());
    client_->publish(sendMessage);
    on ? detector_->start() : detector_->stop();
}
