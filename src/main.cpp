/******************************************************************************
 *
 * Copyright 2022 Gideon van der Kolf
 *
 * This file is part of Konfyt.
 *
 *     Konfyt is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Konfyt is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with Konfyt.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <QApplication>

#include "konfytDefines.h"
#include "konfytStructs.h"
#include "mainwindow.h"

#include <iostream>

void printAppNameAndVersion()
{
    std::cout << std::endl << APP_NAME << std::endl;
    std::cout << "Version " << APP_VERSION << std::endl;
    std::cout << "Gideon van der Kolf 2014-" << APP_YEAR << std::endl;
}

void printAllVersionInfo()
{
    printAppNameAndVersion();
    std::cout << std::endl;
    std::cout << getCompileVersionText().toStdString() << std::endl;
}

void printUsage()
{
    QStringList lines;
    //        |------------------------------------------------------------------------------|
    lines << ""
          << "Usage: konfyt [OPTION]... [PROJECT_FILE] [SOUNDFILE]"
          << ""
          << "  [PROJECT_FILE]         Path of project file to load"
          << "  [SOUNDFILE]            Path of patch, sf2 or sfz file to load. If a project is"
          << "                           specified, the item will be loaded as a new patch in"
          << "                           the project. If no project is specified, the item is"
          << "                           loaded in a new project."
          << "  -v, --version          Print version information"
          << "  -h, --help             Print usage information"
          << "  -j, --jackname <name>  Set name of JACK client"
          << "  -q, --headless         Hide GUI"
          << "  -b, --bridge           Load sfz's in separate processes (experimental feature,"
          << "                           uses Carla)"
          << "  -c, --carla            Use Carla to load sfz's and not Linuxsampler"
#ifndef KONFYT_USE_CARLA
          << "                           Note: This version of Konfyt was compiled without"
          << "                           Carla support."
#endif
          << "  -x, --noxcbev          Do not set the QT_XCB_GL_INTEGRATION=none environment"
          << "                           variable. This environment variable is used to"
          << "                           prevent some functionality from stopping when the"
          << "                           screen is locked on some systems."
          << "                           See the Konfyt documentation for more details."
          << "";
    //        |------------------------------------------------------------------------------|

    printAppNameAndVersion();
    foreach (QString s, lines) {
        std::cout << s.toStdString() << std::endl;
    }
}

enum KonfytArgument {KonfytArg_Help, KonfytArg_Version, KonfytArg_Jackname,
                    KonfytArg_Bridge, KonfytArg_Headless, KonfytArg_Carla,
                    KonfytArg_NoXcbEv};

bool matchArgument(QString arg, KonfytArgument expected)
{
    switch (expected) {
    case KonfytArg_Help:
        return ( (arg=="-h") || (arg=="--help") );
        break;
    case KonfytArg_Version:
        return ( (arg=="-v") || (arg=="--version") );
        break;
    case KonfytArg_Jackname:
        return ( (arg=="-j") || (arg=="--jackname") );
        break;
    case KonfytArg_Bridge:
        return ( (arg=="-b") || (arg=="--bridge") );
        break;
    case KonfytArg_Headless:
        return ( (arg=="-q") || (arg=="--headless") );
        break;
    case KonfytArg_Carla:
        return ( (arg=="-c") || (arg=="--carla") );
        break;
    case KonfytArg_NoXcbEv:
        return ( (arg=="-x") || (arg=="--noxcbev") );
        break;
    }
    return false;
}

int main(int argc, char *argv[])
{
    // MainWindow can exit with QCoreApplication::exit(APP_RESTART_CODE) to restart.
    int return_code = 0;

    QApplication a(argc, argv);
    a.setApplicationName(APP_NAME);

    do {

        // Handle arguments

        bool nextIsValue = false;
        QString prevArg;
        KonfytAppInfo appInfo;
        bool setXcbEv = true;
        appInfo.a = &a;
        appInfo.exePath = a.arguments()[0];

        for (int i=1; i<a.arguments().count(); i++) {
            QString arg = a.arguments()[i];
            if (nextIsValue == false) {
                if ( matchArgument(arg, KonfytArg_Help) ) {

                    printUsage();
                    return 0;

                } else if ( matchArgument(arg, KonfytArg_Version) ) {

                    printAllVersionInfo();
                    return 0;

                } else if ( matchArgument(arg, KonfytArg_Jackname) ) {

                    nextIsValue = true;
                    prevArg = arg;

                } else if ( matchArgument(arg, KonfytArg_Bridge) ) {

                    appInfo.bridge = true;
                    appInfo.carla = true;

                } else if ( matchArgument(arg, KonfytArg_Headless) ) {

                    appInfo.headless = true;

                } else if ( matchArgument(arg, KonfytArg_Carla) ) {

                    appInfo.carla = true;

                } else if ( matchArgument(arg, KonfytArg_NoXcbEv) ) {

                    setXcbEv = false;

                } else {
                    if (arg[0] == '-') {
                        std::cout << "Invalid argument \"" << arg.toLocal8Bit().constData() << "\". Ignoring it." << std::endl;
                    } else {
                        appInfo.filesToLoad.append(arg);
                    }
                }
            } else {
                // nextIsValue is true; So this argument is a value related to prevArg.
                if ( matchArgument(prevArg, KonfytArg_Jackname) ) {
                    appInfo.jackClientName = arg;
                }
                nextIsValue = false;
            }
        }

        // If not disabled by a command-line argument, set an environment variable
        // that prevents the event loop from stopping when the screen is locked
        // on some systems.
        if (setXcbEv) {
            qputenv("QT_XCB_GL_INTEGRATION", "none");
        }

        // Create MainWindow

        MainWindow w(0, appInfo);
        if (!appInfo.headless) { w.show(); }
        return_code = a.exec();

    } while (return_code == APP_RESTART_CODE);
    
    return return_code;
}
