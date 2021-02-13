QT += core multimedia network qmqtt
QT -= gui
CONFIG += c++14 console
CONFIG -= app_bundle
TARGET = WhistleDetector
VERSION = 0.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DESTDIR = build

LIBS += -L"$$(CONDA_PREFIX)/libs/" -lfftw3
INCLUDEPATH += $$(CONDA_PREFIX)/include/

CONFIG(release, debug|release) {
    OBJECTS_DIR = release/obj
    MOC_DIR = release/moc
    RCC_DIR = release/rcc
    UI_DIR = release/ui
}

CONFIG(debug, debug|release) {
    OBJECTS_DIR = debug/obj
    MOC_DIR = debug/moc
    RCC_DIR = debug/rcc
    UI_DIR = debug/ui
}

HEADERS += \
    src/application.h \
    src/config.h \
    src/detector.h

SOURCES += \
    src/application.cpp \
    src/detector.cpp \
    src/main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
