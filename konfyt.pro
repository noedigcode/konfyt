#-------------------------------------------------
#
# Project created by QtCreator 2014-06-23T21:37:39
#
#-------------------------------------------------

CONFIG += qt
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += C++11

TARGET = konfyt
TEMPLATE = app

INCLUDEPATH += src

SOURCES += src/main.cpp\
        src/mainwindow.cpp \
    src/consoledialog.cpp \
    src/konfytPatchEngine.cpp \
    src/konfytPatch.cpp \
    src/konfytPatchLayer.cpp \
    src/konfytLayerWidget.cpp \
    src/konfytFluidsynthEngine.cpp \
    src/konfytJackEngine.cpp \
    src/konfytDatabase.cpp \
    src/konfytProject.cpp \
    src/konfytDbTreeItem.cpp \
    src/konfytDbTree.cpp \
    src/konfytMidiFilter.cpp \
    src/konfytProcess.cpp \
    src/konfytDefines.cpp \
    src/konfytMidi.cpp \
    src/konfytArrayList.cpp \
    src/aboutdialog.cpp \
    src/konfytBridgeEngine.cpp \
    src/konfytBaseSoundEngine.cpp \
    src/konfytLscpEngine.cpp \
    src/gidls.cpp

HEADERS  += src/mainwindow.h \
    src/consoledialog.h \
    src/konfytPatchEngine.h \
    src/konfytPatch.h \
    src/konfytPatchLayer.h \
    src/konfytLayerWidget.h \
    src/konfytFluidsynthEngine.h \
    src/konfytStructs.h \
    src/konfytJackEngine.h \
    src/konfytDatabase.h \
    src/konfytProject.h \
    src/konfytDbTreeItem.h \
    src/konfytDbTree.h \
    src/konfytMidiFilter.h \
    src/konfytProcess.h \
    src/konfytDefines.h \
    src/konfytJackStructs.h \
    src/konfytMidi.h \
    src/konfytArrayList.h \
    src/aboutdialog.h \
    src/konfytBridgeEngine.h \
    src/konfytBaseSoundEngine.h \
    src/konfytLscpEngine.h \
    src/gidls.h \
    src/ringbufferqmutex.h

FORMS    += src/mainwindow.ui \
    src/consoledialog.ui \
    src/konfytLayerWidget.ui \
    src/aboutdialog.ui

# For pkgconfig
unix: CONFIG += link_pkgconfig

# libLSCP stuff
unix: PKGCONFIG += lscp

# Carla stuff
# To build without Carla support, run qmake with the option
# "CONFIG+=KONFYT_NO_CARLA"
# or add it below.

!KONFYT_NO_CARLA {
    SOURCES += src/konfytCarlaEngine.cpp
    HEADERS += src/konfytCarlaEngine.h
    DEFINES += KONFYT_USE_CARLA
    LIBS += -Wl,-rpath=/usr/lib/carla -L/usr/lib/carla -lcarla_standalone2
    QMAKE_CXXFLAGS += -DREAL_BUILD -I/usr/include/carla -I/usr/include/carla/includes
}

# Fluidsynth
unix: PKGCONFIG += fluidsynth

# JACK
unix: PKGCONFIG += jack

# -fpermissive flags (making on Ubuntu Studio complains without this)
QMAKE_CFLAGS += -fpermissive
QMAKE_CXXFLAGS += -fpermissive
QMAKE_LFLAGS += -fpermissive

RESOURCES += \
    images.qrc



