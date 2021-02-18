#include "settings.h"
#include <QSettings>
#include <QCoreApplication>

Settings &Settings::instance()
{
	// Only one config instance
	static Settings inst;
	return inst;
}

Settings::Settings()
{
	// Load settings only one time (protected constructor)
	QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
	for (const QString &key : settings.allKeys())
		config.insert(key, settings.value(key));
}
