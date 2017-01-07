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
    std::cout << "Gideon van der Kolf 2014-2017" << std::endl << std::endl;

    std::cout << "   Usage: konfyt <projectFile>" << std::endl << std::endl;
}

int main(int argc, char *argv[])
{
    // MainWindow can exit with QCoreApplication::exit(APP_RESTART_CODE) to restart.
    int return_code = 0;

    do {
        QApplication a(argc, argv);

        if (a.arguments().count() > 1) {
            if ( (a.arguments()[1] == "-h") || (a.arguments()[1] == "--help") ) {
                printUsage();
                return 0;
            } else if ( (a.arguments()[1] == "-v") || (a.arguments()[1] == "--version") ) {
                printUsage();
                return 0;
            }
        }

        MainWindow w;
        w.app = &a;
        w.show();
        return_code = a.exec();
    } while (return_code == APP_RESTART_CODE);
    
    return return_code;
}
