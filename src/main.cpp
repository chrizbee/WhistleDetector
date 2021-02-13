#include "application.h"

int main(int argc, char *argv[])
{
	// Initialize app
	Application app(argc, argv);
	app.setOrganizationName("chrizbee");
	app.setOrganizationDomain("chrizbee.github.io");
	app.setApplicationName("WhistleDetector");
	app.setApplicationVersion(APP_VERSION);
	return app.exec();
}
