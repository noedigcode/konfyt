#-------------------------------------------------
#
# Project created by QtCreator 2014-06-23T21:37:39
#
#-------------------------------------------------

CONFIG += qt
QT       += core gui network qml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += C++11

TARGET = konfyt
TEMPLATE = app

INCLUDEPATH += src

SOURCES += src/main.cpp\
    src/consolewindow.cpp \
    src/indicatorHandlers.cpp \
    src/konfytAudio.cpp \
    src/konfytJs.cpp \
    src/konfytLscp.cpp \
        src/mainwindow.cpp \
    src/konfytPatchEngine.cpp \
    src/konfytPatch.cpp \
    src/konfytPatchLayer.cpp \
    src/konfytLayerWidget.cpp \
    src/konfytFluidsynthEngine.cpp \
    src/konfytJackEngine.cpp \
    src/konfytDatabase.cpp \
    src/konfytProject.cpp \
    src/konfytDbTree.cpp \
    src/konfytMidiFilter.cpp \
    src/konfytProcess.cpp \
    src/konfytDefines.cpp \
    src/konfytMidi.cpp \
    src/konfytArrayList.cpp \
    src/konfytBridgeEngine.cpp \
    src/konfytBaseSoundEngine.cpp \
    src/konfytLscpEngine.cpp \
    src/menuEntryWidget.cpp \
    src/midiEventListWidgetAdapter.cpp \
    src/midiMapGraphWidget.cpp \
    src/patchListWidgetAdapter.cpp \
    src/remotescanner.cpp \
    src/scriptEditWidget.cpp

HEADERS  += src/mainwindow.h \
    src/consolewindow.h \
    src/indicatorHandlers.h \
    src/konfytAudio.h \
    src/konfytDebug.h \
    src/konfytJs.h \
    src/konfytLscp.h \
    src/konfytPatchEngine.h \
    src/konfytPatch.h \
    src/konfytPatchLayer.h \
    src/konfytLayerWidget.h \
    src/konfytFluidsynthEngine.h \
    src/konfytStructs.h \
    src/konfytJackEngine.h \
    src/konfytDatabase.h \
    src/konfytProject.h \
    src/konfytDbTree.h \
    src/konfytMidiFilter.h \
    src/konfytProcess.h \
    src/konfytDefines.h \
    src/konfytJackStructs.h \
    src/konfytMidi.h \
    src/konfytArrayList.h \
    src/konfytBridgeEngine.h \
    src/konfytBaseSoundEngine.h \
    src/konfytLscpEngine.h \
    src/menuEntryWidget.h \
    src/midiEventListWidgetAdapter.h \
    src/midiMapGraphWidget.h \
    src/patchListWidgetAdapter.h \
    src/remotescanner.h \
    src/ringbufferqmutex.h \
    src/scriptEditWidget.h \
    src/sleepyRingBuffer.h

FORMS    += src/mainwindow.ui \
    src/consolewindow.ui \
    src/konfytLayerWidget.ui

# For pkgconfig
CONFIG += link_pkgconfig

# libLSCP stuff
PKGCONFIG += lscp

# Carla stuff
# To build without Carla support, run qmake with the option
# "CONFIG+=KONFYT_NO_CARLA"
# or add it below.

!KONFYT_NO_CARLA {
    SOURCES += src/konfytCarlaEngine.cpp
    HEADERS += src/konfytCarlaEngine.h
    DEFINES += KONFYT_USE_CARLA
    PKGCONFIG += carla-standalone
}

# Fluidsynth
PKGCONFIG += fluidsynth

# JACK
PKGCONFIG += jack

# -fpermissive flags (making on Ubuntu Studio complains without this)
QMAKE_CFLAGS += -fpermissive
QMAKE_CXXFLAGS += -fpermissive
QMAKE_LFLAGS += -fpermissive

RESOURCES += \
    images.qrc \
    scripting/scripting.qrc
