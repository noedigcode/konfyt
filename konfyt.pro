#-------------------------------------------------
#
# Project created by QtCreator 2014-06-23T21:37:39
#
#-------------------------------------------------

CONFIG += qt
QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = konfyt
TEMPLATE = app

INCLUDEPATH += src

SOURCES += src/main.cpp\
        src/mainwindow.cpp \
    src/toetsdialog.cpp \
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
    src/konfytCarlaEngine.cpp \
    src/konfytsfloader.cpp

HEADERS  += src/mainwindow.h \
    src/toetsdialog.h \
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
    src/konfytCarlaEngine.h \
    src/konfytsfloader.h

FORMS    += src/mainwindow.ui \
    src/toetsdialog.ui \
    src/consoledialog.ui \
    src/konfytLayerWidget.ui

unix: CONFIG += link_pkgconfig
# Carla stuff
LIBS += -Wl,-rpath=/usr/lib/carla -L/usr/lib/carla -lcarla_standalone2
QMAKE_CXXFLAGS += -DREAL_BUILD -I/usr/include/carla -I/usr/include/carla/includes
# Fluidsynth
unix: PKGCONFIG += fluidsynth
# Jack
unix: PKGCONFIG += jack

# -fpermissive flags (making on Ubuntu Studio complains without this)
QMAKE_CFLAGS += -fpermissive
QMAKE_CXXFLAGS += -fpermissive
QMAKE_LFLAGS += -fpermissive

RESOURCES += \
    images.qrc



