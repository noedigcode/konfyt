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

#include "konfytUtils.h"
#include "konfytStructs.h"
#include "mainwindow.h"
#include "remotescanner.h"

#include <iostream>

void print(QString msg)
{
    std::cout << msg.toStdString() << std::endl;
}

void printAppNameAndVersion()
{
    print("");
    print(APP_NAME);
    print(QString("Version %1").arg(APP_VERSION));
    print(QString("Gideon van der Kolf 2014-%1").arg(APP_YEAR));
}

void printAllVersionInfo()
{
    printAppNameAndVersion();
    print("");
    print(getCompileVersionText());
}

void printUsage()
{
    printAppNameAndVersion();

    //     |------------------------------------------------------------------------------|
    print("");
    print("Usage: konfyt [OPTION]... [PROJECT_FILE] [SOUNDFILE]");
    print("");
    print("  [PROJECT_FILE]         Path of project file to load");
    print("  [SOUNDFILE]            Path of patch, sf2 or sfz file to load. If a project is");
    print("                           specified, the item will be loaded as a new patch in");
    print("                           the project. If no project is specified, the item is");
    print("                           loaded in a new project.");
    print("  -v, --version          Print version information");
    print("  -h, --help             Print usage information");
    print("  -j, --jackname <name>  Set name of JACK client");
    print("  -q, --headless         Hide GUI");
    print("  --minimized            Start up with main window minimized");
    print("  -b, --bridge           Load sfz's in separate processes (experimental feature,");
    print("                           uses Carla)");
    print("  -c, --carla            Use Carla to load sfz's and not Linuxsampler");
#ifndef KONFYT_USE_CARLA
    print("                           Note: This version of Konfyt was compiled without");
    print("                           Carla support.");
#endif
    print("  -x, --noxcbev          Do not set the QT_XCB_GL_INTEGRATION=none environment");
    print("                           variable. This environment variable is used to");
    print("                           prevent some functionality from stopping when the");
    print("                           screen is locked on some systems.");
    print("                           See the Konfyt documentation for more details.");
    print("");
    //     |------------------------------------------------------------------------------|
}

int main(int argc, char *argv[])
{
    QStringList argsHelp({"-h", "--help", "help"});
    QStringList argsVersion({"-v", "--version"});
    QStringList argsJackname({"-j", "--jackname"});
    QStringList argsBridge({"-b", "--bridge"});
    QStringList argsHeadless({"-q", "--headless"});
    QStringList argsCarla({"-c", "--carla"});
    QStringList argsNoXcbEv({"-x", "--noxcbev"});
    QStringList argsScan({"--scan"});
    QStringList argsMinimized({"--minimized"});

    // Handle arguments

    bool nextIsValue = false;
    QString prevArg;
    KonfytAppInfo appInfo;
    bool setXcbEv = true;
    bool scanMode = false;
    appInfo.exePath = QString(argv[0]);

    for (int i=1; i < argc; i++) {
        QString arg = QString(argv[i]);
        if (nextIsValue == false) {
            if (argsHelp.contains(arg)) {

                printUsage();
                return 0;

            } else if (argsVersion.contains(arg)) {

                printAllVersionInfo();
                return 0;

            } else if (argsJackname.contains(arg)) {

                nextIsValue = true;
                prevArg = arg;

            } else if (argsBridge.contains(arg)) {

                appInfo.bridge = true;
                appInfo.carla = true;
                print("Carla bridge mode.");

            } else if (argsHeadless.contains(arg)) {

                appInfo.headless = true;

            } else if (argsCarla.contains(arg)) {

                appInfo.carla = true;
                print("Carla mode.");

            } else if (argsNoXcbEv.contains(arg)) {

                setXcbEv = false;
                print("QT_XCB_GL_INTEGRATION will not be set.");

            } else if (argsScan.contains(arg)) {

                scanMode = true;

            } else if (argsMinimized.contains(arg)) {

                appInfo.startMinimized = true;

            } else {
                if (arg[0] == '-') {
                    print(QString("Invalid argument %1. Ignoring it.").arg(arg));
                } else {
                    appInfo.filesToLoad.append(arg);
                }
            }
        } else {
            // nextIsValue is true; So this argument is a value related to prevArg.
            if (argsJackname.contains(prevArg)) {
                appInfo.jackClientName = arg;
                print("JACK name specified: " + appInfo.jackClientName);
            }
            nextIsValue = false;
        }
    }



    if (scanMode) {

        // Scan mode: the program is started in scan mode by another instance
        // in order to scan soundfonts in a separate process to protect against
        // crashes. Run without GUI, connect to a local socket server and wait
        // for filenames which will be scanned in this process.

        QApplication a(argc, argv);

        RemoteScannerClient c;
        c.connect(&c, &RemoteScannerClient::timeout, &c, [=]()
        {
            qApp->exit();
        });
        c.connectToServer();

        return a.exec();

    } else {

        // Normal mode: run GUI.

        QApplication a(argc, argv);
        a.setApplicationName(APP_NAME);

        // If not disabled by a command-line argument, set an environment variable
        // that prevents the event loop from stopping when the screen is locked
        // on some systems.
        if (setXcbEv) {
            print("Setting QT_XCB_GL_INTEGRATION to none.");
            qputenv("QT_XCB_GL_INTEGRATION", "none");
        }

        // MainWindow can exit with QCoreApplication::exit(APP_RESTART_CODE) to restart.
        int return_code = 0;
        do {
            // Create MainWindow

            MainWindow w(0, appInfo);
            if (!appInfo.headless) {
                if (appInfo.startMinimized) {
                    w.showMinimized();
                } else {
                    w.show();
                }
            }
            return_code = a.exec();

        } while (return_code == APP_RESTART_CODE);

        return return_code;
    }
}
