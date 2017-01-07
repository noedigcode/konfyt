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


SOURCES += main.cpp\
        mainwindow.cpp \
    toetsdialog.cpp \
    consoledialog.cpp \
    konfytPatchEngine.cpp \
    konfytPatch.cpp \
    konfytPatchLayer.cpp \
    konfytLayerWidget.cpp \
    konfytFluidsynthEngine.cpp \
    konfytJackEngine.cpp \
    konfytDatabase.cpp \
    konfytProject.cpp \
    konfytDbTreeItem.cpp \
    konfytDbTree.cpp \
    konfytMidiFilter.cpp \
    konfytProcess.cpp \
    konfytDefines.cpp \
    konfytMidi.cpp \
    konfytCarlaEngine.cpp \
    konfytsfloader.cpp

HEADERS  += mainwindow.h \
    toetsdialog.h \
    consoledialog.h \
    konfytPatchEngine.h \
    konfytPatch.h \
    konfytPatchLayer.h \
    konfytLayerWidget.h \
    konfytFluidsynthEngine.h \
    konfytStructs.h \
    konfytJackEngine.h \
    konfytDatabase.h \
    konfytProject.h \
    konfytDbTreeItem.h \
    konfytDbTree.h \
    konfytMidiFilter.h \
    konfytProcess.h \
    konfytDefines.h \
    konfytJackStructs.h \
    konfytMidi.h \
    konfytCarlaEngine.h \
    konfytsfloader.h

FORMS    += mainwindow.ui \
    toetsdialog.ui \
    consoledialog.ui \
    konfytLayerWidget.ui

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



