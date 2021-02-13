#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCoreApplication>

class Detector;
namespace QMQTT {
	class Client;
	class Message;
}

class Application : public QCoreApplication
{
	Q_OBJECT

public:
	Application(int &argc, char **argv);
	~Application();

private slots:
	void onPatternDetected();
	void onClientReceived(const QMQTT::Message &message);

private:
	Detector *detector_;
	QMQTT::Client *client_;
};

#endif // APPLICATION_H
