/******************************************************************************
 *
 * Copyright 2017 Gideon van der Kolf
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

#include "mainwindow.h"
#include <QApplication>
#include <iostream>

void printUsage()
{
    std::cout << std::endl << APP_NAME << std::endl;
    std::cout << "Version " << APP_VERSION << std::endl;
    std::cout << "Gideon van der Kolf 2014-2017" << std::endl;
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
    std::cout << std::endl;
}

enum KonfytArgument {KonfytArg_Help, KonfytArg_Version, KonfytArg_Jackname};

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
    }
}

int main(int argc, char *argv[])
{
    // MainWindow can exit with QCoreApplication::exit(APP_RESTART_CODE) to restart.
    int return_code = 0;

    QApplication a(argc, argv);

    do {

        // Handle arguments

        bool nextIsValue = false;
        QString prevArg;
        QStringList filesToLoad;
        QString jackClientName;

        for (int i=1; i<a.arguments().count(); i++) {
            QString arg = a.arguments()[i];
            if (nextIsValue == false) {
                if ( matchArgument(arg, KonfytArg_Help) ) {

                    printUsage();
                    return 0;

                } else if ( matchArgument(arg, KonfytArg_Version) ) {

                    printUsage();
                    return 0;

                } else if ( matchArgument(arg, KonfytArg_Jackname) ) {

                    nextIsValue = true;
                    prevArg = arg;

                } else {
                    if (arg[0] == '-') {
                        std::cout << "Invalid argument \"" << arg.toLocal8Bit().constData() << "\". Ignoring it." << std::endl;
                    } else {
                        filesToLoad.append(arg);
                    }
                }
            } else {
                // nextIsValue is true; So this argument is a value related to prevArg.
                if ( matchArgument(prevArg, KonfytArg_Jackname) ) {
                    jackClientName = arg;
                }
            }
        }

        // Create MainWindow

        MainWindow w(0, &a, filesToLoad, jackClientName);
        w.show();
        return_code = a.exec();

    } while (return_code == APP_RESTART_CODE);
    
    return return_code;
}
