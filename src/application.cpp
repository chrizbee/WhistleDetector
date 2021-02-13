#include "application.h"
#include "config.h"
#include "detector.h"
#include <qmqtt.h>
#include <QDebug>

Application::Application(int &argc, char **argv) :
	QCoreApplication(argc, argv),
	detector_(new Detector(this)),
	client_(new QMQTT::Client(config::broker, config::port, this))
{
	// Set id and connect signals
	connect(detector_, &Detector::patternDetected, this, &Application::onPatternDetected);
	connect(client_, &QMQTT::Client::received, this, &Application::onClientReceived);
	connect(client_, &QMQTT::Client::connected, [this]() {
		qInfo() << "MQTT Client connected - subscribing to" << config::subTopic;
		client_->subscribe(config::subTopic);
	});

	// Setup client
	client_->setClientId(config::clientName);
	client_->setAutoReconnect(true);
	client_->connectToHost();
}

Application::~Application()
{
}

void Application::onPatternDetected()
{
	// Toggle light
	qInfo() << "Pattern detected";
	int cnt = config::toggleTopics.length();
	for (int i = 0; i < cnt; ++i)
		client_->publish(QMQTT::Message(i, config::toggleTopics[i], "TOGGLE"));
}

void Application::onClientReceived(const QMQTT::Message &message)
{
	// Turn WhistleDetector on / off
	QString payload = message.payload().toUpper();
	bool on = payload.contains("ON") ? true : false;
	QString keyword = on ? "ON" : "OFF";
	qInfo() << "Turning WhistleDetector" << keyword;
	QMQTT::Message sendMessage(0, config::pubTopic, keyword.toUtf8());
	client_->publish(sendMessage);
	detector_->setEnabled(on);
}
