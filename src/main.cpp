/******************************************************************************
 *
 * Copyright 2021 Gideon van der Kolf
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
    std::cout << "Gideon van der Kolf 2014-2021" << std::endl;
}

void printAllVersionInfo()
{
    printAppNameAndVersion();
    std::cout << std::endl;
    std::cout << getCompileVersionText().toStdString() << std::endl;
}

void printUsage()
{
    printAppNameAndVersion();
    std::cout << std::endl;
    std::cout << "   Usage: konfyt [OPTION]... [PROJECT_FILE] [SOUNDFILE]" << std::endl;
    std::cout << std::endl;
    std::cout << "      [PROJECT_FILE]         Path of project file to load" << std::endl;
    std::cout << "      [SOUNDFILE]            Path of patch, sf2 or sfz file to load. If a project is specified," << std::endl;
    std::cout << "                               the item will be loaded as a new patch in the project. If no" << std::endl;
    std::cout << "                               project is specified, the item is loaded in a new project." << std::endl;
    std::cout << "      -v, --version          Print version information" << std::endl;
    std::cout << "      -h, --help             Print usage information" << std::endl;
    std::cout << "      -j, --jackname <name>  Set name of JACK client" << std::endl;
    std::cout << "      -q, --headless         Hide GUI" << std::endl;
    std::cout << "      -b, --bridge           Load sfz's in separate processes (experimental feature, uses Carla)" << std::endl;
    std::cout << "      -c, --carla            Use Carla to load sfz's and not Linuxsampler" << std::endl;
#ifndef KONFYT_USE_CARLA
    std::cout << "                             Note: This version of Konfyt was compiled without Carla support." << std::endl;
#endif
    std::cout << std::endl;
}

enum KonfytArgument {KonfytArg_Help, KonfytArg_Version, KonfytArg_Jackname,
                    KonfytArg_Bridge, KonfytArg_Headless, KonfytArg_Carla};

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

        // Create MainWindow

        MainWindow w(0, appInfo);
        if (!appInfo.headless) { w.show(); }
        return_code = a.exec();

    } while (return_code == APP_RESTART_CODE);
    
    return return_code;
}
