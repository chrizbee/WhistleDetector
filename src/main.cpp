#include "application.h"

int main(int argc, char *argv[])
{
    // Initialize app
    Application app(argc, argv);
    app.setOrganizationName("chrizbee");
    app.setOrganizationDomain("chrizbee.github.io");
    app.setApplicationName(APP_NAME);
    app.setApplicationVersion(APP_VERSION);
    return app.exec();
}
