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
#include "ui_mainwindow.h"

#include <iostream>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);



    // Initialise variables

    panicState = false;
    masterPatch = NULL;
    previewPatch = NULL;
    previewMode = false;
    patchNote_ignoreChange = false;

    midiFilter_lastChan = 0;
    midiFilter_lastData1 = 0;
    midiFilter_lastData2 = 0;

    // Initialise console dialog
    this->consoleDiag = new ConsoleDialog(this);

    userMessage(QString(APP_NAME) + " " + n2s(APP_VERSION));

    // ----------------------------------------------------
    // Sort out settings
    // ----------------------------------------------------

    // Settings dir is in user home directory
    settingsDir = QDir::homePath() + "/" + SETTINGS_DIR;
    userMessage("Settings dir: " + settingsDir);
    ui->label_SettingsPath->setText( ui->label_SettingsPath->text() + settingsDir );
    if (loadSettingsFile()) {
        userMessage("Settings loaded.");
    } else {
        userMessage("Could not load settings.");
    }

    // ----------------------------------------------------
    // Initialise jack client
    // ----------------------------------------------------

    jack = new konfytJackEngine();
    connect(jack, SIGNAL(userMessage(QString)), this, SLOT(userMessage(QString)));
    connect(jack, SIGNAL(JackPortsChanged()), this, SLOT(jackPortsChanged()));
    qRegisterMetaType<konfytMidiEvent>("konfytMidiEvent"); // To be able to use konfytMidiEvent in the Qt signal/slot system
    connect(jack, SIGNAL(midiEventSignal(konfytMidiEvent)), this, SLOT(midiEventSlot(konfytMidiEvent)));
    connect(jack, SIGNAL(xrunSignal()), this, SLOT(jackXrun()));

    if ( jack->InitJackClient(KONFYT_JACK_DEFAULT_CLIENT_NAME) ) {
        // Jack client initialised.
        userMessage("Initialised JACK client with name " + jack->clientName());
    } else {
        // not.
        userMessage("Could not initialise JACK client.");

        // Remove all widgets in centralWidget, add the warning message, and put them back
        // (Workaround to insert warning message at the top :/ )
        QList<QLayoutItem*> l;
        while (ui->centralWidget->layout()->count()) {
            l.append(ui->centralWidget->layout()->takeAt(0));
        }
        ui->centralWidget->layout()->addWidget(ui->groupBox_JackError); // Add error message as first widget
        // And add the rest of the widgets back:
        for (int i=0; i<l.count(); i++) {
            ui->centralWidget->layout()->addItem( l[i] );
        }
    }

    ui->stackedWidget_Console->setCurrentIndex(0);


    // ----------------------------------------------------
    // Initialise patch engine
    // ----------------------------------------------------
    pengine = new konfytPatchEngine();
    connect(pengine, SIGNAL(userMessage(QString)), this, SLOT(userMessage(QString)));
    pengine->initPatchEngine(this->jack);
    this->masterGain = 0.8;
    pengine->setMasterGain(masterGain);
    this->previewGain = 0.8;


    // ----------------------------------------------------
    // Set up gui stuff that needs to happen before loading project
    // ----------------------------------------------------

    // Triggers Page (must happen before setting project)
    initTriggers();


    // ----------------------------------------------------
    // Projects
    // ----------------------------------------------------

    ui->tabWidget_Projects->clear();
    // Scan projectsDir for projects. If none found, create a new project and empty patch.
    if ( !scanDirForProjects(projectsDir) ) {
        userMessage("No project directory " + projectsDir);
    }
    // Load project if one was passed as an argument
    if (app->arguments().count() > 1) {
        if (openProject(app->arguments()[1])) {
            // Project loaded!
            userMessage("Project loaded from argument.");
            setCurrentProject(0);
        }
        startupProject = false;
    }
    // If not loaded, create default project
    if (projectList.count() == 0) {
        userMessage("Creating default new project.");
        newProject();           // Create new project and add to list and GUI
        setCurrentProject(0);   // Set current project to newly created project.
        newPatchToProject();    // Create a new patch and add to current project.
        setCurrentPatch(0);
        startupProject = true;
        konfytProject *prj = getCurrentProject();
        prj->setModified(false);
    }


    // ----------------------------------------------------
    // Initialise soundfont database
    // ----------------------------------------------------
    db = new konfytDatabase();
    connect(db, SIGNAL(userMessage(QString)), this, SLOT(userMessage(QString)));
    connect(db, SIGNAL(scanDirs_finished()), this, SLOT(database_scanDirsFinished()));
    connect(db, SIGNAL(scanDirs_status(QString)), this, SLOT(database_scanDirsStatus(QString)));
    connect(db, SIGNAL(returnSfont_finished(konfytSoundfont*)), this, SLOT(database_returnSfont(konfytSoundfont*)));
    // Check if database file exists. Otherwise, scan directories.
    if (db->loadDatabaseFromFile(settingsDir + "/" + DATABASE_FILE)) {
        userMessage("Database loaded from file. Rescan to refresh.");
        userMessage("Database contains:");
        userMessage("   " + n2s(db->getNumSfonts()) + " soundfonts.");
        userMessage("   " + n2s(db->getNumPatches()) + " patches.");
        userMessage("   " + n2s(db->getNumSfz()) + " sfz/gig samples.");
    } else {
        // Scan for soundfonts
        userMessage("No database file found.");
        userMessage("You can scan directories to create a database from Settings.");
    }

    fillTreeWithAll(); // Fill the tree widget with all the database entries


    // ----------------------------------------------------
    // Initialise and update GUI
    // ----------------------------------------------------

    // Global Transpose
    ui->spinBox_MasterIn_Transpose->setValue(0);

    // Library filesystem view
    this->fsview_currentPath = QDir::homePath();
    refreshFilesystemView();
    ui->tabWidget_library->setCurrentIndex(LIBRARY_TAB_LIBRARY);

    // Add-patch button menu
    QMenu* addPatchMenu = new QMenu();
    addPatchMenu->addAction(ui->actionNew_Patch);
    addPatchMenu->addAction(ui->actionAdd_Patch_From_Library);
    addPatchMenu->addAction(ui->actionAdd_Patch_From_File);
    ui->toolButton_AddPatch->setMenu(addPatchMenu);

    // Save-patch button menu
    QMenu* savePatchMenu = new QMenu();
    //savePatchMenu->addAction(ui->actionSave_Patch); // 2017-01-05 Not used at the moment; patches are saved automatically.
    savePatchMenu->addAction(ui->actionSave_Patch_As_Copy);
    savePatchMenu->addAction(ui->actionAdd_Patch_To_Library);
    savePatchMenu->addAction(ui->actionSave_Patch_To_File);
    ui->toolButton_SavePatch->setMenu(savePatchMenu);

    // Project button menu
    QMenu* projectButtonMenu = new QMenu();
    projectButtonMenu->addAction(ui->actionProject_save);
    updateProjectsMenu();
    connect(&projectsMenu, SIGNAL(triggered(QAction*)), this, SLOT(onprojectMenu_ActionTrigger(QAction*)));
    projectButtonMenu->addMenu(&projectsMenu);
    projectButtonMenu->addAction(ui->actionProject_New);
    //projectButtonMenu->addAction(ui->actionProject_SaveAs); // TODO: SaveAs menu entry disabled for now until it is implemented.
    ui->toolButton_Project->setMenu(projectButtonMenu);


    // Add-midi-port-to-patch button
    connect(&patchMidiOutPortsMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(onPatchMidiOutPortsMenu_ActionTrigger(QAction*)));
    ui->toolButton_layer_AddMidiPort->setMenu(&patchMidiOutPortsMenu);

    // Button: add audio input port to patch
    connect(&patchAudioInPortsMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(onPatchAudioInPortsMenu_ActionTrigger(QAction*)));
    ui->toolButton_layer_AddAudioInput->setMenu(&patchAudioInPortsMenu);

    // Layer bus menu
    connect(&layerBusMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(onLayerBusMenu_ActionTrigger(QAction*)));

    // Layer MIDI output channel menu
    connect(&layerMidiOutChannelMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(onLayerMidiOutChannelMenu_ActionTrigger(QAction*)));

    // Hide main toolbar
    ui->mainToolBar->hide();

    // If one of the paths are not set, show settings. Else, switch to patch view.
    if ( (projectsDir == "") || (patchesDir == "") || (soundfontsDir == "") || (sfzDir == "") ) {
        showSettingsDialog();
    } else {
        ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
    }
    currentPatchIndex = -1;

    // Console
    console_showMidiMessages = false;

    // Connections page
    busParent = NULL; // Helps later on when deleting items.
    audioInParent = NULL;
    midiOutParent = NULL;

    // Checkboxes in connections treeWidget are mapped to a signal mapper
    connect( &conSigMap, SIGNAL(mapped(QWidget*)), this, SLOT(checkboxes_signalmap_slot(QWidget*)) );

    // Set up portsBusses tree context menu
    ui->tree_portsBusses->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_portsBusses, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(tree_portsBusses_Menu(QPoint)));

    // Resize some layouts
    QList<int> sizes;
    sizes << 8 << 2;
    ui->splitter_library->setSizes( sizes );

    // Right Sidebar
    // Hide the bottom tabs used for experimentation
    QTabBar* rightTabBar = qFindChild<QTabBar*>( ui->tabWidget_right );
    rightTabBar->setVisible(false);
    ui->tabWidget_right->setCurrentIndex(0);


    // Filemanager processes (when a process is finished, it will be mapped to the
    // signal mapper, so that we can get the process object that emitted the signal
    // end destroy it.
    connect( &filemanProcessSignalMapper, SIGNAL(mapped(QObject*)), this, SLOT(filemanProcessSignalMapperSlot(QObject*)));

    // Set up keyboard shortcuts
    shortcut_save = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(shortcut_save, SIGNAL(activated()), this, SLOT(shortcut_save_activated()));

    shortcut_panic = new QShortcut(QKeySequence("Ctrl+P"), this);
    connect(shortcut_panic, SIGNAL(activated()), this, SLOT(shortcut_panic_activated()));
    ui->pushButton_Panic->setToolTip( ui->pushButton_Panic->toolTip() + " [" + shortcut_panic->key().toString() + "]");

    // Show library view (not live mode)
    ui->stackedWidget_left->setCurrentIndex(STACKED_WIDGET_LEFT_LIBRARY);

    // Show welcome message in statusbar
    QString app_name(APP_NAME);
    ui->statusBar->showMessage("Welkom by " + app_name + ".",5000);
}


MainWindow::~MainWindow()
{
    carla_engine_close();
    jack->stopJackClient();

    delete ui;
}

void MainWindow::shortcut_save_activated()
{
    ui->actionProject_save->trigger();
}

void MainWindow::shortcut_panic_activated()
{
    ui->actionPanic->trigger();
}

// Build project-open menu with an Open action and a list of projects in the projects dir.
void MainWindow::updateProjectsMenu()
{
    projectsMenu.clear();
    projectsMenuMap.clear();

    projectsMenu.setTitle("Open");

    projectsMenu.addAction(ui->actionProject_Open);
    projectsMenu.addSeparator();
    if (projectDirList.count() == 0) {
        projectsMenu.addAction("No projects found in project directory.");
    } else {
        for (int i=0; i<projectDirList.count(); i++) {
            QFileInfo fi = projectDirList[i];
            QAction* newAction = projectsMenu.addAction( fi.fileName().remove(PROJECT_FILENAME_EXTENSION) );
            newAction->setToolTip(fi.filePath());
            projectsMenuMap.insert(newAction, fi);
        }
    }
    projectsMenu.addSeparator();
    projectsMenu.addAction(ui->actionProject_OpenDirectory);
}

void MainWindow::onprojectMenu_ActionTrigger(QAction *action)
{
    if ( projectsMenuMap.contains(action) ) {
        QFileInfo fi = projectsMenuMap.value(action);
        openProject(fi.filePath());
        // Switch to newly opened project
        ui->tabWidget_Projects->setCurrentIndex(ui->tabWidget_Projects->count()-1);
    }
}


// Slot for signal when a jack port as been (un)registered.
void MainWindow::jackPortsChanged()
{

}

void MainWindow::jackXrun()
{
    static int count = 1;
    userMessage("XRUN " + n2s(count++));
}

// Scan given directory recursively and add project files to list.
bool MainWindow::scanDirForProjects(QString dirname)
{
    QDir dir(dirname);
    if (!dir.exists()) {
        emit userMessage("scanDirForProjects: Dir does not exist.");
        return false;
    }

    // Get list of all subfiles and directories. Then for each:
    // If a file, add it if it is a project.
    // If a directory, run this function on it.
    QFileInfoList fil = dir.entryInfoList();
    for (int i=0; i<fil.count(); i++) {
        QFileInfo fi = fil.at(i);
        if (fi.fileName() == ".") { continue; }
        if (fi.fileName() == "..") { continue; }

        if (fi.isFile()) {
            // Check extension and add if project.
            QString suffix = "." + fi.suffix();
            if (suffix == PROJECT_FILENAME_EXTENSION) {
                projectDirList.append(fi);
            }
        } else if (fi.isDir()) {
            // Scan the dir
            scanDirForProjects(fi.filePath());
        }
    }
    return true;
}

// Show the settings dialog.
void MainWindow::showSettingsDialog()
{
    ui->lineEdit_Settings_PatchesDir->setText(this->patchesDir);
    ui->lineEdit_Settings_ProjectsDir->setText(this->projectsDir);
    ui->lineEdit_Settings_SfzDir->setText(this->sfzDir);
    ui->lineEdit_Settings_SoundfontsDir->setText(this->soundfontsDir);
    ui->lineEdit_Settings_filemanager->setText(this->filemanager);
    // Switch to settings page
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_SETTINGS);
}

void MainWindow::updateMidiFilterEditorLastRx()
{
    ui->lineEdit_MidiFilter_Last->setText("Ch " + n2s(midiFilter_lastChan+1)
                                          + " - " + n2s(midiFilter_lastData1)
                                          + ", " + n2s(midiFilter_lastData2));
}

void MainWindow::showMidiFilterEditor()
{
    // Switch to midi filter view

    konfytPatchLayer g = midiFilterEditItem->getPatchLayerItem();

    konfytMidiFilter f = g.getMidiFilter();
    QList<konfytMidiFilterZone> l = f.getZoneList();
    if (l.count() != 0) {
        konfytMidiFilterZone z = f.getZoneList().at(0);
        ui->spinBox_midiFilter_LowNote->setValue(z.lowNote);
        ui->spinBox_midiFilter_HighNote->setValue(z.highNote);
        ui->spinBox_midiFilter_Multiply->setValue(z.multiply);
        ui->spinBox_midiFilter_Add->setValue(z.add);
        ui->spinBox_midiFilter_LowVel->setValue(z.lowVel);
        ui->spinBox_midiFilter_HighVel->setValue(z.highVel);
    } else {
        // Fill with default values
        ui->spinBox_midiFilter_LowNote->setValue(0);
        ui->spinBox_midiFilter_HighNote->setValue(127);
        ui->spinBox_midiFilter_Multiply->setValue(1);
        ui->spinBox_midiFilter_Add->setValue(0);
        ui->spinBox_midiFilter_LowVel->setValue(0);
        ui->spinBox_midiFilter_HighVel->setValue(127);
    }
    // Midi in channel combo box
    if (f.inChan<0) {
        // <0 means all channels
        ui->comboBox_midiFilter_inChannel->setCurrentIndex(0);
    } else {
        ui->comboBox_midiFilter_inChannel->setCurrentIndex(f.inChan+1);
    }
    ui->checkBox_midiFilter_AllCCs->setChecked(f.passAllCC);
    ui->checkBox_midiFilter_Prog->setChecked(f.passProg);
    ui->checkBox_midiFilter_pitchbend->setChecked(f.passPitchbend);
    ui->listWidget_midiFilter_CC->clear();
    for (int i=0; i<f.passCC.count(); i++) {
        ui->listWidget_midiFilter_CC->addItem( n2s( f.passCC.at(i) ) );
    }

    updateMidiFilterEditorLastRx();

    // Switch to midi filter page
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_FILTER);
}

// This slot is called when the settings dialog sends a signal to
// update the settings.
void MainWindow::applySettings()
{
    // Get settings from dialog.
    projectsDir = ui->lineEdit_Settings_ProjectsDir->text();
    patchesDir = ui->lineEdit_Settings_PatchesDir->text();
    soundfontsDir = ui->lineEdit_Settings_SoundfontsDir->text();
    sfzDir = ui->lineEdit_Settings_SfzDir->text();
    filemanager = ui->lineEdit_Settings_filemanager->text();

    userMessage("Settings applied.");

    // Save the settings.
    if (saveSettingsFile()) {
        userMessage("Settings saved.");
    } else {
        userMessage("Failed to save settings to file.");
    }
}

bool MainWindow::loadSettingsFile()
{
    QString filename = settingsDir + "/" + SETTINGS_FILE;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        userMessage("Failed to open settings file: " + filename);
        return false;
    }

    QXmlStreamReader r(&file);
    r.setNamespaceProcessing(false);

    while (r.readNextStartElement()) { // wsap_settings

        if (r.name() == "wsap_settings") {

            while (r.readNextStartElement()) { // Settings

                if (r.name() == "projectsDir") {
                    projectsDir = r.readElementText();
                } else if (r.name() == "soundfontsDir") {
                    soundfontsDir = r.readElementText();
                } else if (r.name() == "patchesDir") {
                    patchesDir = r.readElementText();
                } else if (r.name() == "sfzDir") {
                    sfzDir = r.readElementText();
                } else if (r.name() == "filemanager") {
                    filemanager = r.readElementText();
                }

            }

        } else { // name not wsap_settings
            r.skipCurrentElement();
        }
    }



    file.close();
    return true;
}

bool MainWindow::saveSettingsFile()
{
    // First, create settings directory if it doesn't exist.
    QDir dir(settingsDir);
    if (!dir.exists()) {
        if (dir.mkpath(settingsDir)) {
            userMessage("Created settings directory: " + settingsDir);
        } else {
            userMessage("Failed to create settings directory: " + settingsDir);
            return false;
        }
    }

    // Open settings file for writing.
    QString filename = settingsDir + "/" + SETTINGS_FILE;
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        userMessage("Failed to open settings file for writing: " + filename);
        return false;
    }

    // Create xml writer and write settings to file.
    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    stream.writeComment("This is a wsap settings file.");

    stream.writeStartElement("wsap_settings");

    // Settings
    stream.writeTextElement("projectsDir", projectsDir);
    stream.writeTextElement("soundfontsDir", soundfontsDir);
    stream.writeTextElement("patchesDir", patchesDir);
    stream.writeTextElement("sfzDir", sfzDir);
    stream.writeTextElement("filemanager", filemanager);

    stream.writeEndElement(); // wsap_settings

    stream.writeEndDocument();

    file.close();
    return true;
}

// Remove project from projectList and GUI
void MainWindow::removeProject(int i)
{
    if ( (i >=0) && (i < projectList.count()) ) {
        projectList.removeAt(i);
        // Remove from tabs
        ui->tabWidget_Projects->removeTab(i);
    }
}

// Create a new project and add new project to projectList and GUI.
void MainWindow::newProject()
{
    konfytProject* prj = new konfytProject();
    QString name = "New Project";
    // Check if a project with similar name doesn't already exist in list.
    bool duplicate = true;
    QString extra = "";
    int count = 1;
    while (duplicate) {
        duplicate = false;
        for (int i=0; i<projectList.count(); i++) {
            if (name + extra == projectList.at(i)->getProjectName()) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            count++;
            extra = " " + n2s(count);
        }
    }
    // Finally we found a unique name.
    name = name + extra;
    prj->setProjectName(name);
    addProject(prj);
}

// Open a project from the specified filename and add to project list and GUI.
bool MainWindow::openProject(QString filename)
{
    konfytProject* prj = new konfytProject();

    if (prj->loadProject(filename)) {
        // Add project to list and gui
        addProject(prj); // This will add to list, connect signal and add to gui.
        userMessage("Project loaded.");
        return true;
    } else {
        userMessage("Failed to load project.");
        messageBox("Error loading project " + filename);
        delete prj;
        return false;
    }
}

// Add project to projectList and GUI.
void MainWindow::addProject(konfytProject* prj)
{
    // If startupProject is true, a default created project exists.
    // If this project has not been modified, remove it.
    if (startupProject) {
        konfytProject* prj = getCurrentProject();
        if (prj == NULL) {
            // So the project has already been removed.
        } else {
            if (prj->isModified() == false) {
                // Remove it!
                removeProject(0);
            }
        }
        startupProject = false;
    }

    connect(prj, SIGNAL(userMessage(QString)), this, SLOT(userMessage(QString)));
    projectList.append(prj);
    // Add to tabs
    QLabel* lbl = new QLabel();
    ui->tabWidget_Projects->addTab(lbl,prj->getProjectName());
}

/* Update the patch list according to the current project. */
void MainWindow::gui_updatePatchList()
{
    ui->listWidget_Patches->clear();

    konfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("gui_updatePatchList(): Current project is NULL");
        return;
    }

    // Populate patch list for current project
    QList<konfytPatch*> pl = prj->getPatchList();
    bool allPatchesLoaded = true;
    for (int j=0; j<pl.count(); j++) {
        konfytPatch* pat = pl.at(j);
        QListWidgetItem* item = new QListWidgetItem(n2s(j+1) + " " + pat->getName());
        // If patch has been loaded, mark it white. Else, gray.
        if (prj->isPatchLoaded(pat->id_in_project)) {
            item->setTextColor(Qt::white);
        } else {
            item->setTextColor(Qt::gray);
            allPatchesLoaded = false;
        }

        ui->listWidget_Patches->addItem(item);
    }

    setCurrentPatchIcon();


}


void MainWindow::showConnectionsPage()
{
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_CONNECTIONS);

    // Adjust column widths
    int RLwidth = 30;
    ui->tree_Connections->setColumnWidth(TREECON_COL_PORT,ui->tree_Connections->width() - RLwidth*2);
    ui->tree_Connections->setColumnWidth(TREECON_COL_L,RLwidth);
    ui->tree_Connections->setColumnWidth(TREECON_COL_R,RLwidth);

    gui_updatePortsBussesTree();
    gui_updateConnectionsTree();
}

void MainWindow::gui_updatePortsBussesTree()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    /* Busses
     *  |__ bus 1
     *  |__  :
     *  |__ bus n
     *
     * Audio In Ports
     *  |__ port 1
     *  |__  :
     *  |__ port n
     *
     * Midi Out Ports
     *  |__ port 1
     *  |__  :
     *  |__ port n
     *etc...
     */

    // Clear tree before deleting items so that the onItemChanged signal is not emitted while
    // deleting the items.
    ui->tree_portsBusses->clear();

    // Delete all tree items
    if (busParent != NULL) {

        tree_busMap.clear();
        tree_audioInMap.clear();
        tree_midiOutMap.clear();
        tree_midiInMap.clear();
    }

    busParent = new QTreeWidgetItem();
    busParent->setText(0, "Busses");
    QList<int> busIds = prj->audioBus_getAllBusIds();
    for (int i=0; i<busIds.count(); i++) {
        int id = busIds[i];
        prjAudioBus b = prj->audioBus_getBus(id);
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setText(0, b.busName);
        tree_busMap.insert(item, id);
        busParent->addChild(item);
    }

    audioInParent = new QTreeWidgetItem();
    audioInParent->setText(0, "Audio Input Ports");
    QList<int> audioInIds = prj->audioInPort_getAllPortIds();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        prjAudioInPort p = prj->audioInPort_getPort(id);
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setText(0, p.portName);
        tree_audioInMap.insert(item, id);
        audioInParent->addChild(item);
    }

    midiOutParent = new QTreeWidgetItem();
    midiOutParent->setText(0, "MIDI Output Ports");
    QList<int> midiOutIds = prj->midiOutPort_getAllPortIds();
    for (int i=0; i<midiOutIds.count(); i++) {
        int id = midiOutIds[i];
        prjMidiOutPort p = prj->midiOutPort_getPort(id);
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setText(0, p.portName);
        tree_midiOutMap.insert(item, id);
        midiOutParent->addChild(item);
    }

    midiInParent = new QTreeWidgetItem();
    midiInParent->setText(0, "MIDI Input");
    {
        // currently we only have one midi input port
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, "Midi Input Port");
        tree_midiInMap.insert(item, 0);
        midiInParent->addChild(item);
    }


    ui->tree_portsBusses->addTopLevelItem(busParent);
    ui->tree_portsBusses->addTopLevelItem(audioInParent);
    ui->tree_portsBusses->addTopLevelItem(midiOutParent);
    ui->tree_portsBusses->addTopLevelItem(midiInParent);

    ui->tree_portsBusses->expandAll();

}

void MainWindow::gui_updateConnectionsTree()
{
    // First, clear everything

    QList<QCheckBox*> ll = conChecksMap1.keys();
    for (int i=0; i<ll.count(); i++) {
        delete ll[i];
    }
    conChecksMap1.clear();
    ll = conChecksMap2.keys();
    for (int i=0; i<ll.count(); i++) {
        delete ll[i];
    }
    conChecksMap2.clear();

    ui->tree_Connections->clear();
    conClientsMap.clear();
    conPortsMap.clear();


    // Get current project and exit function if null
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    // Exit function if current ports/busses item is null
    if (ui->tree_portsBusses->currentItem() == NULL) { return; }

    // Get list of JACK ports, depending on the selected tree item.
    QStringList l; // List of Jack client:ports
    int j; // Index/id of bus/port
    if ( ui->tree_portsBusses->currentItem()->parent() == busParent ) {
        // A bus is selected
        l = jack->getAudioInputPortsList();
        j = tree_busMap.value( ui->tree_portsBusses->currentItem() );
    } else if ( ui->tree_portsBusses->currentItem()->parent() == audioInParent ) {
        // An audio input port is selected
        l = jack->getAudioOutputPortsList();
        j = tree_audioInMap.value( ui->tree_portsBusses->currentItem() );
    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiOutParent ) {
        // A midi output port is selected
        l = jack->getMidiInputPortsList();
        j = tree_midiOutMap.value( ui->tree_portsBusses->currentItem() );
    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiInParent ) {
        // Midi input port is selected
        l = jack->getMidiOutputPortsList();
    } else {
        // One of the parents are selected.
        return;
    }

    // TODO: do not show ports of our (Konfyt's) client.

    // We have a list of JACK client:port. Fill the connection tree.
    for (int i=0; i<l.count(); i++) {
        addClientPortToTree(l[i], true);
    }

    // Tick the appropriate checkboxes according to the selected item in the
    // ports/busses tree widget

    QStringList leftCons;
    QStringList rightCons;
    if ( ui->tree_portsBusses->currentItem()->parent() == busParent ) {
        // Get the selected bus
        prjAudioBus bus = prj->audioBus_getBus(j);
        leftCons = bus.leftOutClients;
        rightCons = bus.rightOutClients;

    } else if ( ui->tree_portsBusses->currentItem()->parent() == audioInParent ) {
        // An audio input port is selected
        prjAudioInPort p = prj->audioInPort_getPort(j);
        leftCons = p.leftInClients;
        rightCons = p.rightInClients;

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiOutParent ) {
        // A midi output port is selected
        leftCons = prj->midiOutPort_getClients(j);

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiInParent ) {
        // Midi input is selected
        leftCons = prj->midiInPort_getClients();
    }

    for (int i=0; i<leftCons.count(); i++) {
        if (conPortsMap.values().contains( leftCons[i] ) == false) {
            // The port is not currently in Jack's list (client probably not running)
            // Add to tree and mark with background colour
            addClientPortToTree(leftCons[i], false);
        }
        QTreeWidgetItem* item = conPortsMap.key( leftCons[i] );
        QCheckBox* cb = conChecksMap1.key(item);
        cb->setChecked(true);
    }
    for (int i=0; i<rightCons.count(); i++) {
        if (conPortsMap.values().contains( rightCons[i] ) == false) {
            // The port is not currently in Jack's list (client probably not running)
            // Add to tree and mark with background colour
            addClientPortToTree(rightCons[i], false);
        }
        QTreeWidgetItem* item = conPortsMap.key( rightCons[i] );
        QCheckBox* cb = conChecksMap2.key(item);
        cb->setChecked(true);
    }


    ui->tree_Connections->expandAll();
}

/* Helper function to add a Jack client:port string to the connections tree and
 * mark it if active is false. */
void MainWindow::addClientPortToTree(QString jackport, bool active)
{
    QColor activeColor = QColor(Qt::transparent);
    QColor inactiveColor = QColor(Qt::red);

    // Extract client name
    QString client = jackport.split(":").at(0);
    QTreeWidgetItem* clientItem;
    // If client is already in map, get the tree item. Otherwise, add to map.
    if (conClientsMap.contains(client)) {
        clientItem = conClientsMap[client];
    } else {
        clientItem = new QTreeWidgetItem();
        clientItem->setText(TREECON_COL_PORT, client);
        conClientsMap.insert(client, clientItem);
        // If active is false, mark the client item
        if (!active) {
            clientItem->setBackgroundColor(0, inactiveColor );
        }
    }
    // Add client tree item to treeWidget
    ui->tree_Connections->addTopLevelItem(clientItem);
    // Create a tree item for the port, and add as a child to the client tree item.
    QTreeWidgetItem* portItem = new QTreeWidgetItem();
    QString portname = jackport;
    portname.replace(client + ":", "");
    portItem->setText(TREECON_COL_PORT, portname); // Extract port name
    clientItem->addChild(portItem);
    conPortsMap.insert(portItem, jackport);
    // Add checkboxes to client
    QCheckBox* cbl = new QCheckBox();
    QCheckBox* cbr = new QCheckBox();
    ui->tree_Connections->setItemWidget(portItem, TREECON_COL_L, cbl);
    conSigMap.setMapping(cbl, cbl); // Map checkbox signal
    connect( cbl, SIGNAL(clicked()), &conSigMap, SLOT(map()) );
    conChecksMap1.insert(cbl, portItem); // Map the checkbox to the tree item
    if ( ui->tree_portsBusses->currentItem()->parent() != midiOutParent ) { // Midi ports only have one checkbox
        if (ui->tree_portsBusses->currentItem()->parent() != midiInParent) {
            ui->tree_Connections->setItemWidget(portItem, TREECON_COL_R, cbr);
            conSigMap.setMapping(cbr, cbr);
            connect( cbr, SIGNAL(clicked()), &conSigMap, SLOT(map()) );
            conChecksMap2.insert(cbr, portItem); // Map the checkbox to the tree item
        }
    }
    // If active is false, mark the port item. Else, ensure client item is unmarked.
    if (active) {
        clientItem->setBackgroundColor(0, activeColor);
    } else {
        portItem->setBackgroundColor(0, inactiveColor);
    }
}

/* The clicked signals of the connections treeWidget checkboxes are mapped to a signal mapper,
 * which signal is connected to this slot so that we know which checkbox emitted the signal.
 * (In hindsight, QObject::sender() could probably just have been used.) */
void MainWindow::checkboxes_signalmap_slot(QWidget *widget)
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    QCheckBox* c = (QCheckBox*)widget;

    QTreeWidgetItem* t;
    portLeftRight leftRight;
    if (conChecksMap1.contains(c)) {
        // Checkbox is in the left column
        t = conChecksMap1[c];
        leftRight = leftPort;
    } else {
        // Checkbox is in the right column
        t = conChecksMap2[c];
        leftRight = rightPort;
    }

    // Now we get the JACK port string:
    QString portString = conPortsMap[t];

    if ( ui->tree_portsBusses->currentItem()->parent() == busParent ) {
        // Get the selected bus
        int busId = tree_busMap.value( ui->tree_portsBusses->currentItem() );
        prjAudioBus bus = prj->audioBus_getBus(busId);

        konfytJackPort* jackPort;
        if (leftRight == leftPort) { jackPort = bus.leftJackPort; }
        else { jackPort = bus.rightJackPort; }

        if (c->isChecked()) {
            // Connect
            jack->addPortClient(KonfytJackPortType_AudioOut, jackPort, portString);
            // Also add in project
            prj->audioBus_addClient(busId, leftRight, portString);
        } else {
            // Disconnect
            jack->removePortClient_andDisconnect(KonfytJackPortType_AudioOut, jackPort, portString);
            // Also remove in project
            prj->audioBus_removeClient(busId, leftRight, portString);
        }


    } else if ( ui->tree_portsBusses->currentItem()->parent() == audioInParent ) {
        // An audio input port is selected
        int portId = tree_audioInMap.value( ui->tree_portsBusses->currentItem() );
        prjAudioInPort p = prj->audioInPort_getPort(portId);

        konfytJackPort* jackPort;
        if (leftRight == leftPort) { jackPort = p.leftJackPort; }
        else { jackPort = p.rightJackPort; }

        if (c->isChecked()) {
            // Connect
            jack->addPortClient(KonfytJackPortType_AudioIn, jackPort, portString);
            // Also add in project
            prj->audioInPort_addClient(portId, leftRight, portString);
        } else {
            // Disconnect
            jack->removePortClient_andDisconnect(KonfytJackPortType_AudioIn, jackPort, portString);
            // Also remove in project
            prj->audioInPort_removeClient(portId, leftRight, portString);
        }

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiOutParent ) {
        // A midi output port is selected
        int portId = tree_midiOutMap.value( ui->tree_portsBusses->currentItem() );
        prjMidiOutPort p = prj->midiOutPort_getPort(portId);

        konfytJackPort* jackPort = p.jackPort;
        if (c->isChecked()) {
            // Connect
            jack->addPortClient(KonfytJackPortType_MidiOut, jackPort, portString);
            // Also add in project
            prj->midiOutPort_addClient(portId, portString);
        } else {
            // Disconnect
            jack->removePortClient_andDisconnect(KonfytJackPortType_MidiOut, jackPort, portString);
            // Also remove in project
            prj->midiOutPort_removeClient(portId, portString);
        }

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiInParent ) {
        // Midi input is selected
        if (c->isChecked()) {
            // Connect
            jack->addPortToAutoConnectList(portString);
            // Also add in project
            prj->midiInPort_addClient(portString);
        } else {
            // Disconnect
            jack->removePortFromAutoConnectList_andDisconnect(portString);
            // Also remove in project
            prj->midiInPort_removeClient(portString);
        }
    }
}



/* Slot that gets called when the custom context menu of tree_portsBusses is requested.
 * This adds the appropriate actions to the menu based on the item selected, and shows
 * the menu. When an action is clicked, the slot of the corresponding action is called. */
void MainWindow::tree_portsBusses_Menu(const QPoint &pos)
{
    portsBussesTreeMenu.clear();
    QTreeWidgetItem* item = ui->tree_portsBusses->itemAt(pos);
    portsBussesTreeMenuItem = item;
    QList<QAction*> l;
    if (item != NULL) {
        if ( (item->parent() == busParent) ||
                    (item->parent() == audioInParent) ||
                    (item->parent() == midiOutParent) ){
            l.append(ui->actionRename_BusPort);
            l.append(ui->actionRemove_BusPort);
        }
    }
    l.append(ui->actionAdd_Bus);
    l.append(ui->actionAdd_Audio_In_Port);
    l.append(ui->actionAdd_MIDI_Out_Port);
    portsBussesTreeMenu.addActions(l);
    portsBussesTreeMenu.popup(QCursor::pos());
}


void MainWindow::initTriggers()
{
    // Create a list of actions we will be adding to the triggers list
    QList<QAction*> l;
    l << ui->actionPanic
      << ui->actionNext_Patch
      << ui->actionPrevious_Patch
      << ui->actionMaster_Volume_Slider
      << ui->actionMaster_Volume_Up
      << ui->actionMaster_Volume_Down
      << ui->actionProject_save
      << ui->actionPatch_1
      << ui->actionPatch_2
      << ui->actionPatch_3
      << ui->actionPatch_4
      << ui->actionPatch_5
      << ui->actionPatch_6
      << ui->actionPatch_7
      << ui->actionPatch_8
      << ui->actionLayer_1_Gain << ui->actionLayer_1_Mute << ui->actionLayer_1_Solo
      << ui->actionLayer_2_Gain << ui->actionLayer_2_Mute << ui->actionLayer_2_Solo
      << ui->actionLayer_3_Gain << ui->actionLayer_3_Mute << ui->actionLayer_3_Solo
      << ui->actionLayer_4_Gain << ui->actionLayer_4_Mute << ui->actionLayer_4_Solo
      << ui->actionLayer_5_Gain << ui->actionLayer_5_Mute << ui->actionLayer_5_Solo
      << ui->actionLayer_6_Gain << ui->actionLayer_6_Mute << ui->actionLayer_6_Solo
      << ui->actionLayer_7_Gain << ui->actionLayer_7_Mute << ui->actionLayer_7_Solo
      << ui->actionLayer_8_Gain << ui->actionLayer_8_Mute << ui->actionLayer_8_Solo
      << ui->actionGlobal_Transpose_12_Down
      << ui->actionGlobal_Transpose_12_Up
      << ui->actionGlobal_Transpose_1_Down
      << ui->actionGlobal_Transpose_1_Up
      << ui->actionGlobal_Transpose_Zero;

    channelGainActions << ui->actionLayer_1_Gain << ui->actionLayer_2_Gain
                       << ui->actionLayer_3_Gain << ui->actionLayer_4_Gain
                       << ui->actionLayer_5_Gain << ui->actionLayer_6_Gain
                       << ui->actionLayer_7_Gain << ui->actionLayer_8_Gain;
    channelSoloActions << ui->actionLayer_1_Solo << ui->actionLayer_2_Solo
                       << ui->actionLayer_3_Solo << ui->actionLayer_4_Solo
                       << ui->actionLayer_5_Solo << ui->actionLayer_6_Solo
                       << ui->actionLayer_7_Solo << ui->actionLayer_8_Solo;
    channelMuteActions << ui->actionLayer_1_Mute << ui->actionLayer_2_Mute
                       << ui->actionLayer_3_Mute << ui->actionLayer_4_Mute
                       << ui->actionLayer_5_Mute << ui->actionLayer_6_Mute
                       << ui->actionLayer_7_Mute << ui->actionLayer_8_Mute;
    patchActions << ui->actionPatch_1
                 << ui->actionPatch_2
                 << ui->actionPatch_3
                 << ui->actionPatch_4
                 << ui->actionPatch_5
                 << ui->actionPatch_6
                 << ui->actionPatch_7
                 << ui->actionPatch_8;


    triggersItemActionHash.clear();
    ui->tree_Triggers->clear();

    for (int i=0; i<l.count(); i++) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, l[i]->text());
        ui->tree_Triggers->addTopLevelItem(item);
        triggersItemActionHash.insert(item, l[i]);
    }
}

void MainWindow::showTriggersPage()
{
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_TRIGGERS);

    ui->tree_Triggers->setColumnWidth(0, ui->tree_Triggers->width()/2);
    ui->tree_Triggers->setColumnWidth(1, ui->tree_Triggers->width()/2 - 16); // -16 is quick and dirty fix to accomodate scroll bar

    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    // Clear trigger text for whole gui list
    QList<QTreeWidgetItem*> items = triggersItemActionHash.keys();
    for (int i=0; i<items.count(); i++) {
        items[i]->setText(1,"");
    }

    // Get triggers from project and show in gui list
    QList<konfytTrigger> l = prj->getTriggerList();
    for (int i=0; i<l.count(); i++) {
        for (int j=0; j<items.count(); j++) {
            if (triggersItemActionHash[items[j]]->text() == l[i].actionText) {
                QString text = l[i].toString();
                items[j]->setText(1, text);
            }
        }
    }

}


void MainWindow::setCurrentProject(int i)
{
    // First, disconnect signals from current project.
    konfytProject* oldprj = getCurrentProject();
    if (oldprj != NULL) {
        // Process started signal
        disconnect(oldprj, SIGNAL(processStartedSignal(int,konfytProcess*)), this, SLOT(processStartedSlot(int,konfytProcess*)));
        // Process stopped signal
        disconnect(oldprj, SIGNAL(processFinishedSignal(int,konfytProcess*)), this, SLOT(processFinishedSlot(int,konfytProcess*)));
        // Project modified signal
        disconnect(oldprj, SIGNAL(projectModifiedChanged(bool)), this, SLOT(projectModifiedStateChanged(bool)));
    }
    // Set up the current project
    if ( (i<0) || (i>=projectList.count()) ) { return; }
    currentProject = i;
    konfytProject* prj = projectList.at(i);

    pengine->setProject(prj); // patch engine needs a pointer to the current project for some stuff.

    ui->lineEdit_ProjectName->setText(prj->getProjectName());
    // Populate patch list for current project
    gui_updatePatchList();

    // And also update the midi input autoconnect list in the jack client
    if (jack->clientIsActive()) {
        jack->clearAutoConnectList_andDisconnect();
        jack->addAutoConnectList(prj->midiInPort_getClients());
    }

    jack->pauseJackProcessing(true);

    // Process Midi out ports

    jack->removeAllMidiOutPorts();

    QList<int> midiOutIds = prj->midiOutPort_getAllPortIds();
    for (int j=0; j<midiOutIds.count(); j++) {
        int id = midiOutIds[j];
        // Add to Jack, and update jack port reference in project
        prjMidiOutPort projectPort = prj->midiOutPort_getPort(id);
        konfytJackPort* jackPort;
        addMidiOutPortToJack(id, &jackPort);
        projectPort.jackPort = jackPort;
        prj->midiOutPort_replace_noModify(id, projectPort); // Replace in project since the port has been updated with the jackPort

        // Also add port clients to Jack
        QStringList c = projectPort.clients;
        for (int k=0; k<c.count(); k++) {
            jack->addPortClient(KonfytJackPortType_MidiOut, jackPort, c.at(k));
        }

    }

    // Update the port menus in patch view
    gui_updatePatchMidiOutPortsMenu();
    gui_updatePatchAudioInPortsMenu();


    // In Jack, remove all audio input ports and busses (audio output ports)
    jack->removeAllAudioInAndOutPorts();

    // Audio Busses

    QList<int> busIds = prj->audioBus_getAllBusIds();
    for (int j=0; j<busIds.count(); j++) {
        int id = busIds[j];
        prjAudioBus b =  prj->audioBus_getBus(id);

        // Add audio bus ports to jack client
        konfytJackPort *left, *right;
        addAudioBusToJack( id, &left, &right);
        if ( (left != NULL) && (right != NULL) ) {
            // Update left and right port references of bus in project
            b.leftJackPort = left;
            b.rightJackPort = right;
            prj->audioBus_replace_noModify( id, b ); // use noModify function as to not change the project's modified state.
            // Add port clients to jack client
            jack->setPortClients(KonfytJackPortType_AudioOut, b.leftJackPort, b.leftOutClients);
            jack->setPortClients( KonfytJackPortType_AudioOut, b.rightJackPort, b.rightOutClients );
        } else {
            userMessage("ERROR: setCurrentProject: Failed to create audio bus Jack port(s).");
        }

    }

    // Audio input ports
    QList<int> audioInIds = prj->audioInPort_getAllPortIds();
    for (int j=0; j<audioInIds.count(); j++) {
        int id = audioInIds[j];
        prjAudioInPort p = prj->audioInPort_getPort(id);

        // Add audio ports to jack client
        konfytJackPort *left, *right;
        addAudioInPortsToJack( id, &left, &right );
        if ((left != NULL) && (right != NULL)) {
            // Update left and right port numbers in project
            p.leftJackPort = left;
            p.rightJackPort = right;
            prj->audioInPort_replace_noModify( id, p );
            // Add port clients to jack client
            jack->setPortClients( KonfytJackPortType_AudioIn, p.leftJackPort, p.leftInClients );
            jack->setPortClients( KonfytJackPortType_AudioIn, p.rightJackPort, p.rightInClients );
        } else {
            userMessage("ERROR: setCurrentProject: Failed to create audio input Jack port(s).");
        }

    }

    // Update external applications list
    ui->listWidget_ExtApps->clear();
    QList<konfytProcess*> prl = prj->getProcessList();
    for (int j=0; j<prl.count(); j++) {
        konfytProcess* gp = prl.at(j);
        QString temp = gp->toString_appAndArgs();
        if (gp->isRunning()) {
            temp = "[running] " + temp;
        }
        ui->listWidget_ExtApps->addItem(temp);
    }
    // Connect signals
    connect(prj, SIGNAL(processStartedSignal(int,konfytProcess*)), this, SLOT(processStartedSlot(int,konfytProcess*)));
    connect(prj, SIGNAL(processFinishedSignal(int,konfytProcess*)), this, SLOT(processFinishedSlot(int,konfytProcess*)));
    connect(prj, SIGNAL(projectModifiedChanged(bool)), this, SLOT(projectModifiedStateChanged(bool)));

    // Get triggers from project and add to quick lookup hash
    QList<konfytTrigger> trigs = prj->getTriggerList();
    QList<QAction*> actions = triggersItemActionHash.values();
    for (int i=0; i<trigs.count(); i++) {
        // Find action matching text
        for (int j=0; j<actions.count(); j++) {
            if (trigs[i].actionText == actions[j]->text()) {
                triggersMidiActionHash.insert(trigs[i].toInt(), actions[j]);
            }
        }
    }

    // Update project modified indication in GUI
    projectModifiedStateChanged(prj->isModified());

    masterPatch = NULL;
    gui_updatePatchView();

    if (ui->stackedWidget->currentIndex() == STACKED_WIDGET_PAGE_CONNECTIONS) {
        showConnectionsPage();
    }
    if (ui->stackedWidget->currentIndex() == STACKED_WIDGET_PAGE_TRIGGERS) {
        showTriggersPage();
    }

    // Indicate warnings to user
    updateGUIWarnings();

    jack->pauseJackProcessing(false);

}

/* Update the midi output ports menu */
void MainWindow::gui_updatePatchMidiOutPortsMenu()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    patchMidiOutPortsMenu.clear();
    patchMidiOutPortsMenu_map.clear();

    QList<int> midiOutIds = prj->midiOutPort_getAllPortIds();
    for (int i=0; i<midiOutIds.count(); i++) {
        int id = midiOutIds[i];
        prjMidiOutPort projectPort = prj->midiOutPort_getPort(id);
        QAction* action = patchMidiOutPortsMenu.addAction( n2s(id) + " " + projectPort.portName) ;
        patchMidiOutPortsMenu_map.insert(action, id);
    }
    patchMidiOutPortsMenu.addSeparator();
    patchMidiOutPortsMenu_NewPortAction = patchMidiOutPortsMenu.addAction("New Port");
}

/* Update the audio input ports menu */
void MainWindow::gui_updatePatchAudioInPortsMenu()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    patchAudioInPortsMenu.clear();
    patchAudioInPortsMenu_map.clear();

    QList<int> audioInIds = prj->audioInPort_getAllPortIds();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        prjAudioInPort projectPort = prj->audioInPort_getPort(id);
        QAction* action = patchAudioInPortsMenu.addAction( n2s(id) + " " + projectPort.portName );
        patchAudioInPortsMenu_map.insert(action, id);
    }
    patchAudioInPortsMenu.addSeparator();
    patchAudioInPortsMenu_NewPortAction = patchAudioInPortsMenu.addAction( "New Port" );
}

// Create a new patch, and add it to the current project. (and update the gui).
void MainWindow::newPatchToProject()
{
    konfytPatch* pt = new konfytPatch();
    pt->setName("New Patch");

    addPatchToProject(pt);
}

// Remove the patch with specified index from the project.
void MainWindow::removePatchFromProject(int i)
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    if ( (i>=0) && (i<prj->getNumPatches()) ) {

        // Remove from project
        konfytPatch* patch = prj->removePatch(i);

        // Remove from patch engine
        pengine->unloadPatch(patch);

        // Remove from GUI
        delete ui->listWidget_Patches->item(i);
        gui_updatePatchList();
        if (masterPatch == patch) {
            masterPatch = NULL;
            gui_updatePatchView();
        }
        userMessage("Patch Removed.");

        // Delete the patch
        delete patch;
    }
}


// Add a patch to the current project, and update the gui.
void MainWindow::addPatchToProject(konfytPatch* newPatch)
{
    if (getCurrentProject() == NULL) {
        userMessage("Select a project.");
        return;
    }

    getCurrentProject()->addPatch(newPatch);
    // Add to list in gui:
    gui_updatePatchList();
}


konfytProject* MainWindow::getCurrentProject()
{
    if (projectList.count()) {
        if ( (currentProject<0) || (currentProject >= projectList.count())) {
            userMessage("Invalid currentProject index.");
            return NULL;
        } else {
            return projectList.at(currentProject);
        }
    } else {
        return NULL;
    }
}



// Returns true if a program is selected in the library.
bool MainWindow::library_isProgramSelected()
{
    // This is possible only if the list widget contains programs
    if (programList.count()) {
        // If the currentrow of the program list widget is positive,
        // it contains programs and one is currently selected.
        int currentRow = ui->listWidget_2->currentRow();
        if (currentRow>=0) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

// Returns the currently selected program, or a blank one
// if nothing is selected.
konfytSoundfontProgram MainWindow::library_getSelectedProgram()
{
    if (library_isProgramSelected()) {
        konfytSoundfontProgram p = programList.at(ui->listWidget_2->currentRow());
        return p;
    } else {
        // Return blank one
        konfytSoundfontProgram p;
        return p;
    }
}


/* Returns the type of item in the library tree. */
libraryTreeItemType MainWindow::library_getTreeItemType(QTreeWidgetItem *item)
{
    if (item == library_patchRoot) { return libTreePatchesRoot; }
    else if (library_patchMap.contains(item)) { return libTreePatch; }
    else if (item == library_sfzRoot) { return libTreeSFZRoot; }
    else if (library_sfzFolders.contains(item)) { return libTreeSFZFolder; }
    else if (library_sfzMap.contains(item)) { return libTreeSFZ; }
    else if (item == library_sfRoot) { return libTreeSoundfontRoot; }
    else if (library_sfFolders.contains(item)) { return libTreeSoundfontFolder; }
    else if (library_sfMap.contains(item)) { return libTreeSoundfont; }
    else { return libTreeInvalid; }
}

libraryTreeItemType MainWindow::library_getSelectedTreeItemType()
{
    return library_getTreeItemType( ui->treeWidget->currentItem() );
}


// Returns the currently selected patch, or a blank one
// if nothing is selected.
konfytPatch MainWindow::library_getSelectedPatch()
{
    if ( library_getSelectedTreeItemType() == libTreePatch ) {
        konfytPatch p = library_patchMap.value(ui->treeWidget->currentItem());
        return p;
    } else {
        // Return blank one
        konfytPatch p;
        return p;
    }
}


// Returns the currently selected soundfont, or NULL if
// nothing is selected.
konfytSoundfont* MainWindow::library_getSelectedSfont()
{
    if ( library_getSelectedTreeItemType() == libTreeSoundfont ) {
        QTreeWidgetItem* current = ui->treeWidget->currentItem();
        return library_sfMap.value(current);
    } else {
        return NULL;
    }
}

// Set master gain if in normal mode, or preview gain if in preview mode,
// and set the master gain in the patch engine.
void MainWindow::setMasterGain(float gain)
{
    if (previewMode) {
        previewGain = gain;
    } else {
        masterGain = gain;
    }
    pengine->setMasterGain(gain);
}

// Load the appropriate patch based on the mode (preview mode or normal) and
// updates the GUI accordingly.
void MainWindow::loadPatchForModeAndUpdateGUI()
{

    // Make sure the appropriate "preview mode" gui buttons are checked
    ui->pushButton_LibraryPreview->setChecked(previewMode);


    if (previewMode) {

        // Load the selected item in the library to preview

        if (previewPatch == NULL) {
            previewPatch = new konfytPatch();
        }

        pengine->loadPatch(previewPatch);
        // Remove all layers
        QList<konfytPatchLayer> l = previewPatch->getLayerItems();
        for (int i=0; i<l.count(); i++) {
            konfytPatchLayer layer = l[i];
            pengine->removeLayer(&layer);
        }

        libraryTreeItemType type = library_getSelectedTreeItemType();

        if (library_isProgramSelected()) {
            // Program selected. Load program into preview patch
            konfytSoundfontProgram program = library_getSelectedProgram();
            pengine->addProgramLayer(program);

        } else if ( type == libTreePatch ) {

            // Patch is selected.
            // We don't do preview for patches yet.

        } else if ( type == libTreeSFZ ) {

            // Sfz is selected. Add sfz layer to preview patch
            pengine->addSfzLayer(library_selectedSfz);
        }

        // Deactivate all midi output jack ports
        jack->setAllPortsActive(KonfytJackPortType_MidiOut, false);

        setMasterGain(previewGain);


    } else {

        if (masterPatch != NULL) {
            pengine->loadPatch(this->masterPatch);

            setMasterGain(masterGain);

            // Mark patch as loaded in project, so we can indicate this to the user later.
            konfytProject* p = this->getCurrentProject();
            p->markPatchLoaded(this->masterPatch->id_in_project);
        }

    }

    gui_updatePatchView();

    // Update master slider
    ui->horizontalSlider_MasterGain->setValue(pengine->getMasterGain()*ui->horizontalSlider_MasterGain->maximum());

    // Indicate to the user that the patch is not modified.
    setPatchModified(false);

}

void MainWindow::gui_updatePatchView()
{
    clearLayerItems_GUIonly();

    // Only for master patch, not preview mode patch
    konfytPatch* p = masterPatch;
    if (p == NULL) {
        // No patch active
        ui->lineEdit_PatchName->setText("");
        ui->lineEdit_PatchName->setEnabled(false);
        patchNote_ignoreChange = true;
        ui->textBrowser_patchNote->clear();
        ui->stackedWidget_patchLayers->setCurrentIndex(STACKED_WIDGET_PATCHLAYERS_NOPATCH);
        return;
    } else {
        ui->stackedWidget_patchLayers->setCurrentIndex(STACKED_WIDGET_PATCHLAYERS_PATCH);
        ui->lineEdit_PatchName->setEnabled(true);
    }

    // Get list of layer items
    QList<konfytPatchLayer> l = p->getLayerItems();
    for (int i=0; i<l.count(); i++) {

        // Add to gui layer list
        addLayerItemToGUI(l.at(i));

    }

    // Patch title
    ui->lineEdit_PatchName->setText(p->getName());
    // Patch note
    patchNote_ignoreChange = true;
    ui->textBrowser_patchNote->setText(p->getNote());
}

void MainWindow::gui_updateWindowTitle()
{
    if (previewMode) {
        this->setWindowTitle("Preview - " + QString(APP_NAME));
    } else {
        if (currentPatchIndex < 0) {
            // No patch is selected
            this->setWindowTitle(APP_NAME);

        } else if (currentPatchIndex >= ui->listWidget_Patches->count()) {
            // Invalid
            userMessage("gui_updateWindowTitle error: out of bounds");
            this->setWindowTitle(APP_NAME);

        } else {
            this->setWindowTitle( ui->listWidget_Patches->item(currentPatchIndex)->text()
                                 + " - " + APP_NAME );
        }
    }
}




// Fill the tree widget with all the entries in the soundfont database.
void MainWindow::fillTreeWithAll()
{
    searchMode = false; // Controls the behaviour when the user selects a tree item
    ui->treeWidget->clear();
    programList.clear(); // Internal list of programs displayed


    // Create parent soundfonts tree item, with soundfont children
    library_sfRoot = new QTreeWidgetItem();
    library_sfRoot->setText(0,TREE_ITEM_SOUNDFONTS);
    library_sfFolders.clear();
    library_sfMap.clear();
    db->buildSfontTree();
    buildSfTree(library_sfRoot, db->sfontTree->root);
    library_sfRoot->setIcon(0, QIcon(":/icons/folder.png"));

    // Create parent patches tree item, with patch children
    library_patchRoot = new QTreeWidgetItem();
    library_patchRoot->setText(0,TREE_ITEM_PATCHES);
    library_patchRoot->setIcon(0, QIcon(":/icons/folder.png"));
    QList<konfytPatch> pl = db->getPatchList();
    for (int i=0; i<pl.count(); i++) {
        konfytPatch pt = pl.at(i);

        QTreeWidgetItem* twiChild = new QTreeWidgetItem();
        twiChild->setIcon(0, QIcon(":/icons/picture.png"));
        twiChild->setText(0, pt.getName());
        library_patchMap.insert(twiChild, pt);

        library_patchRoot->addChild(twiChild);

    }

    // Create parent sfz item, with one child indicating the number of items
    library_sfzRoot = new QTreeWidgetItem();
    library_sfzRoot->setText(0,TREE_ITEM_SFZ);
    library_sfzFolders.clear();
    library_sfzMap.clear();
    db->buildSfzTree();
    buildSfzTree(library_sfzRoot, db->sfzTree->root);
    library_sfzRoot->setIcon(0, QIcon(":/icons/folder.png"));

    // Add items to tree

    ui->treeWidget->insertTopLevelItem(0,library_sfRoot);
    //ui->treeWidget->expandItem(soundfontsParent);
    ui->treeWidget->insertTopLevelItem(0,library_sfzRoot);
    //ui->treeWidget->expandItem(sfzParent);
    ui->treeWidget->insertTopLevelItem(0,library_patchRoot);
}

/* Build TreeWidget tree from the database's tree. */
void MainWindow::buildSfzTree(QTreeWidgetItem* twi, konfytDbTreeItem* item)
{
    if ( !item->hasChildren() ) {
        library_sfzMap.insert(twi, item->path); // Add to sfz map
        twi->setToolTip(0,twi->text(0));
        twi->setIcon(0, QIcon(":/icons/picture.png"));
    } else {
        twi->setIcon(0, QIcon(":/icons/folder.png"));
        // If this is not the root item, add to sfz folders map
        if (twi != library_sfzRoot) {
            library_sfzFolders.insert(twi, item->path);
        }
    }

    // If database tree item has only one child that is not a leaf, skip it.
    if (item->hasChildren()) {
        if ( (item->children.count() == 1) && (item->children[0]->hasChildren()) ) {
            buildSfzTree( twi, item->children[0] );
        } else {
            for (int i=0; i<item->children.count(); i++) {
                QTreeWidgetItem* child = new QTreeWidgetItem();
                child->setText( 0, item->children.at(i)->name );
                buildSfzTree(child, item->children.at(i));
                twi->addChild(child);
            }
        }
    }
}

void MainWindow::buildSfTree(QTreeWidgetItem *twi, konfytDbTreeItem *item)
{
    if ( !item->hasChildren() ) {
        library_sfMap.insert(twi, (konfytSoundfont*)(item->data)); // Add to soundfonts map
        twi->setIcon(0, QIcon(":/icons/picture.png"));
    } else {
        twi->setIcon(0, QIcon(":/icons/folder.png"));
        // If item is not the root, add to soundfont folders map
        if (twi != library_sfRoot) {
            library_sfFolders.insert(twi, item->path);
        }
    }

    // If database tree item has only one child that is not a leaf, skip it.
    if (item->hasChildren()) {
        if ( (item->children.count() == 1) && (item->children[0]->hasChildren()) ) {
            buildSfTree( twi, item->children[0] );
        } else {
            for (int i=0; i<item->children.count(); i++) {
                QTreeWidgetItem* child = new QTreeWidgetItem();
                child->setText( 0, item->children.at(i)->name );
                buildSfTree(child, item->children.at(i));
                twi->addChild(child);
            }
        }
    }
}


void MainWindow::fillTreeWithSearch(QString search)
{
    searchMode = true; // Controls the behaviour when the user selects a tree item
    db->searchProgram(search);

    ui->treeWidget->clear();

    QTreeWidgetItem* twiResults = new QTreeWidgetItem();
    twiResults->setText(0, TREE_ITEM_SEARCH_RESULTS);

    // Soundfonts

    library_sfRoot = new QTreeWidgetItem();
    library_sfRoot->setText(0,TREE_ITEM_SOUNDFONTS);

    library_sfFolders.clear();
    library_sfMap.clear();
    db->buildSfontTree_results();
    buildSfTree(library_sfRoot, db->sfontTree_results->root);
    library_sfRoot->setIcon(0, QIcon(":/icons/folder.png"));

    // Patches

    library_patchRoot = new QTreeWidgetItem();
    library_patchRoot->setText(0,TREE_ITEM_PATCHES);
    library_patchRoot->setIcon(0, QIcon(":/icons/folder.png"));

    QList<konfytPatch> pl = db->getResults_patches();

    for (int i=0; i<pl.count(); i++) {
        konfytPatch pt = pl.at(i);

        QTreeWidgetItem* twiChild = new QTreeWidgetItem();
        twiChild->setText(0,pt.getName());
        library_patchMap.insert(twiChild, pt);

        library_patchRoot->addChild(twiChild);
    }

    // SFZ

    library_sfzRoot = new QTreeWidgetItem();
    library_sfzRoot->setText(0,TREE_ITEM_SFZ);
    library_sfzFolders.clear();
    library_sfzMap.clear();
    db->buildSfzTree_results();
    buildSfzTree(library_sfzRoot, db->sfzTree_results->root);
    library_sfzRoot->setIcon(0, QIcon(":/icons/folder.png"));

    // Add items to tree

    twiResults->addChild(library_patchRoot);
    twiResults->addChild(library_sfzRoot);
    twiResults->addChild(library_sfRoot);


    ui->treeWidget->insertTopLevelItem(0,twiResults);
    ui->treeWidget->expandItem(twiResults);
    ui->treeWidget->expandItem(library_sfzRoot);
    ui->treeWidget->expandItem(library_sfRoot);
}



// Displays a user message on the GUI.
void MainWindow::userMessage(QString message)
{
    static bool start = true;

    ui->textBrowser->append(message);

    /* Ensure textBrowser scrolls to maximum when it is filled with text. Usually
     * this is only done when the user explicitely scrolls to the end. We want it to
     * happen from the start. Once it's there, we set 'start=false' as the
     * user / textBrowser can then handle it themselves. */
    QScrollBar* v = ui->textBrowser->verticalScrollBar();
    if (start) {
        if (v->value() != v->maximum()) {
            v->setValue(v->maximum());
            start = false;
        }
    }

    // Seperate console dialog
    this->consoleDiag->userMessage(message);

}

void MainWindow::error_abort(QString msg)
{
    std::cout << "\n\n" << "Konfyt ERROR, ABORTING: MainWindow:"
              << msg.toLocal8Bit().constData() << "\n\n";
    abort();
}

void MainWindow::messageBox(QString msg)
{
    QMessageBox msgbox;
    msgbox.setText(msg);
    msgbox.exec();
}


void MainWindow::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    // Expand / unexpand item due to click (makes things a lot easier)
    item->setExpanded(!item->isExpanded());
}


// Set the current patch, and update the gui accordingly.
void MainWindow::setCurrentPatch(konfytPatch* newPatch)
{
    this->masterPatch = newPatch;
    loadPatchForModeAndUpdateGUI();

}

void MainWindow::setCurrentPatch(int index)
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    // Make index zero if out of bounds
    if ( (index < 0) || (index >= prj->getNumPatches()) ) {
        index = 0;
    }

    // Remove icon of current item in list
    unsetCurrentPatchIcon();

    // Set current patch. This will update gui also.
    if ( (index >= 0) && (index < prj->getNumPatches()) ) {
        currentPatchIndex = index;
        setCurrentPatch( prj->getPatch(index) );

        // Indicate in the gui patch list that the patch has been loaded.
        setCurrentPatchIcon();
        QListWidgetItem* item = ui->listWidget_Patches->item(currentPatchIndex);
        item->setTextColor(Qt::white);

        gui_updateWindowTitle();
    }

}

void MainWindow::setCurrentPatchIcon()
{
    konfytProject* prj = getCurrentProject();
    if ( (currentPatchIndex >= 0) && (currentPatchIndex < prj->getNumPatches()) ) {
        QListWidgetItem* item = ui->listWidget_Patches->item(currentPatchIndex);
        item->setIcon(QIcon(":/icons/play.png"));
    }
}

void MainWindow::unsetCurrentPatchIcon()
{
    konfytProject* prj = getCurrentProject();
    if ( (currentPatchIndex >= 0) && (currentPatchIndex < prj->getNumPatches()) ) {
        QListWidgetItem* item = ui->listWidget_Patches->item(currentPatchIndex);
        item->setIcon(QIcon());
    }
}

void MainWindow::on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    ui->listWidget_2->clear();  // Program list view
    programList.clear();        // Our internal program list, corresponding to the list view

    if (current == NULL) {
        return;
    }

    if ( library_getSelectedTreeItemType() == libTreeSFZ ) {

        library_selectedSfz = library_sfzMap.value(current);
        if (previewMode) {
            loadPatchForModeAndUpdateGUI();
        }
        return;
    }

    if ( library_getSelectedTreeItemType() == libTreeSoundfont ) {
        // Soundfont is selected.
        if (searchMode) {
            // Fill programList variable with program results of selected soundfont
            konfytSoundfont* sf = library_getSelectedSfont();
            programList = db->getResults_sfontPrograms(sf);
            // TODO: Track sf, might be a memory leak.
        } else {
            // Fill programList variable with all programs of selected soundfont
            konfytSoundfont* sf = library_getSelectedSfont();
            programList = sf->programlist;
            // TODO: Track sf, might be a memory leak.
        }
        // Refresh the GUI program list with programs (if any).
        library_refreshGUIProgramList();
        // Automatically select the first program
        if (ui->listWidget_2->count()) {
            ui->listWidget_2->setCurrentRow(0);
        }
    }

    if ( library_getSelectedTreeItemType() == libTreePatch ) {
        if (previewMode) {
            // Patch is selected in preview mode. Load patch.
            loadPatchForModeAndUpdateGUI();
        }
        // Do nothing.
    }
}

/* Refresh the program list view in the library, according to programList. */
void MainWindow::library_refreshGUIProgramList()
{
    ui->listWidget_2->clear();
    for (int i=0; i<programList.count(); i++) {
        ui->listWidget_2->addItem(n2s(programList.at(i).bank)
                                  + "-"
                                  + n2s(programList.at(i).program)
                                  + " " + programList.at(i).name);
    }
}

/* Enter pressed in the search box. */
void MainWindow::on_lineEdit_Search_returnPressed()
{
    fillTreeWithSearch(ui->lineEdit_Search->text());
    return;

}

/* Clear search button clicked. */
void MainWindow::on_pushButton_ClearSearch_clicked()
{
    ui->lineEdit_Search->clear();

    fillTreeWithAll();

    ui->lineEdit_Search->setFocus();

}

void MainWindow::on_pushButton_LibraryPreview_clicked()
{
    setPreviewMode( ui->pushButton_LibraryPreview->isChecked() );
}


/* Library program list: Soundfont program selected. */
void MainWindow::on_listWidget_2_currentRowChanged(int currentRow)
{
    if (currentRow < 0) {
        return;
    }

    if (programList.count()) {
        // List contains soundfont programs.

        // Load program, if in previewMode
        if (previewMode) {
            loadPatchForModeAndUpdateGUI();
        }
    }
}


// Adds SFZ to current patch in engine and in GUI.
void MainWindow::addSfzToCurrentPatch(QString sfzPath)
{
    newPatchIfMasterNull();

    // Add layer to engine
    konfytPatchLayer g = pengine->addSfzLayer(sfzPath);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
}

void MainWindow::addLV2ToCurrentPatch(QString lv2Path)
{
    newPatchIfMasterNull();

    // Add layer to engine
    konfytPatchLayer g = pengine->addLV2Layer(lv2Path);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
}

// Adds an internal Carla plugin to the current patch in engine and GUI.
void MainWindow::addCarlaInternalToCurrentPatch(QString URI)
{
    newPatchIfMasterNull();

    // Add layer to engine
    konfytPatchLayer g = pengine->addCarlaInternalLayer(URI);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
}

// Adds soundfont program to current patch in engine and in GUI.
void MainWindow::addProgramToCurrentPatch(konfytSoundfontProgram p)
{
    newPatchIfMasterNull();

    // Add program to engine
    konfytPatchLayer g = pengine->addProgramLayer(p);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
}

// If masterPatch is NULL, adds a new patch to the project and switches to it
void MainWindow::newPatchIfMasterNull()
{
    konfytProject* prj = getCurrentProject();
    Q_ASSERT( prj != NULL );

    if (masterPatch == NULL) {
        newPatchToProject();
        // Switch to latest patch
        setCurrentPatch( prj->getNumPatches()-1 );
    }
}


// Adds a midi port to the current patch in engine and GUI.
void MainWindow::addMidiPortToCurrentPatch(int port)
{
    newPatchIfMasterNull();

    // Check if the port isn't already in the patch
    QList<int> l = pengine->getPatch()->getMidiOutputPortList_ids();
    if (l.contains(port)) { return; }

    // Add port to current patch in engine
    konfytPatchLayer g = pengine->addMidiOutPortToPatch(port);

    // Add to GUI list
    addLayerItemToGUI(g);

    setPatchModified(true);
}

// Adds an audio bus to the current patch in engine and GUI
void MainWindow::addAudioInPortToCurrentPatch(int port)
{
    newPatchIfMasterNull();

    // Check if the port isn't already in the patch
    QList<int> l = pengine->getPatch()->getAudioInPortList_ids();
    if (l.contains(port)) { return; }

    // Add port to current patch in engine
    konfytPatchLayer g = pengine->addAudioInPortToPatch(port);

    // Add to GUI list
    addLayerItemToGUI(g);

    setPatchModified(true);
}

// Sets previewMode based on choice, and updates the gui.
void MainWindow::setPreviewMode(bool choice)
{
    previewMode = choice;

    ui->stackedWidget->setEnabled(!previewMode);

    // Update the GUI
    loadPatchForModeAndUpdateGUI();
}

// Master gain slider moved.
void MainWindow::on_horizontalSlider_MasterGain_sliderMoved(int position)
{
    setMasterGain( (float)ui->horizontalSlider_MasterGain->value() /
                   (float)ui->horizontalSlider_MasterGain->maximum() );
}



void MainWindow::on_lineEdit_PatchName_returnPressed()
{
    // Rename patch
    pengine->setPatchName(ui->lineEdit_PatchName->text());

    // Remove focus to something else to give the impression that the name has been changed.
    ui->label_PatchList->setFocus();

    // Update name in patch list
    gui_updatePatchList();

    // Indicate to the user that the patch has been modified.
    setPatchModified(true);
}


void MainWindow::on_lineEdit_PatchName_editingFinished()
{
    // Rename patch
    pengine->setPatchName(ui->lineEdit_PatchName->text());

    // Update name in patch list
    gui_updatePatchList();

    // Indicate to the user that the patch has been modified.
    setPatchModified(true);
}



void MainWindow::on_lineEdit_ProjectName_editingFinished()
{
    konfytProject* prj = getCurrentProject();
    if (prj != NULL) {
        prj->setProjectName(ui->lineEdit_ProjectName->text());
        ui->tabWidget_Projects->setTabText(ui->tabWidget_Projects->currentIndex(),ui->lineEdit_ProjectName->text());
    }
}




// Save patch to library (in other words, to patchesDir directory.)
bool MainWindow::savePatchToLibrary(konfytPatch *patch)
{
    QDir dir(patchesDir);
    if (!dir.exists()) {
        emit userMessage("Patches directory does not exist.");
        return false;
    }

    QString patchName = getUniqueFilename(dir.path(), patch->getName(), "." + QString(KonfytPatchSuffix) );
    if (patchName == "") {
        userMessage("Could not find a suitable filename.");
        return false;
    }

    if (patchName != patch->getName() + "." + KonfytPatchSuffix) {
        userMessage("Duplicate name exists. Saving patch as:");
        userMessage(patchName);
    }

    // Add directory, and save.
    patchName = patchesDir + "/" + patchName;
    if (patch->savePatchToFile(patchName)) {
        userMessage("Patch saved as " + patchName);
        db->addPatch(patchName);
        // Refresh tree view if not in searchmode
        if (!searchMode) {
            fillTreeWithAll();
        }
        // Now save database
        saveDatabase();

        return true;
    } else {
        userMessage("Failed saving patch to file " + patchName);
        return false;
    }
}

/* Scans a directory and determine if filename with extension (dot included) exists.
 * Adds a number to the filename until it is unique.
 * Returns filename only, without path.
 * If the directory does not exist, empty string is returned. */
QString MainWindow::getUniqueFilename(QString dirname, QString name, QString extension)
{
    QDir dir(dirname);
    if (!dir.exists()) {
        emit userMessage("getUniqueFilename: Directory does not exist.");
        return "";
    }

    // Scan the directory and get a unique name.
    QString extra = "";
    int count = 1;
    bool duplicate = true;

    while (duplicate) {

        duplicate = false;

        QFileInfoList fil = dir.entryInfoList();
        for (int i=0; i<fil.count(); i++) {
            QFileInfo fi = fil.at(i);
            if (fi.fileName() == ".") { continue; }
            if (fi.fileName() == "..") { continue; }

            // Check if name is in use.
            if (fi.fileName() == name + extra + extension) {
                duplicate = true;
                break;
            }

        }
        if (duplicate) {
            count++;
            extra = " " + n2s(count);
        }
    }
    // We finally found a unique filename.
    return name + extra + extension;
}


// Add process (External application) to GUI and current project.
void MainWindow::addProcess(konfytProcess* process)
{
    konfytProject* p = getCurrentProject();
    if (p == NULL) {
        userMessage("Select a project.");
        return;
    }
    // Add to project list
    p->addProcess(process);
    // Add to GUI list
    ui->listWidget_ExtApps->addItem(process->toString_appAndArgs());
}

void MainWindow::runProcess(int index)
{
    konfytProject* p = getCurrentProject();
    if (p == NULL) {
        return;
    }
    // Start process
    p->runProcess(index);
    // Indicate in list widget
    if ( (index >=0) && (index < ui->listWidget_ExtApps->count()) ) {
        // Indicate in list widget
        QListWidgetItem* item = ui->listWidget_ExtApps->item(index);
        item->setText("[starting] " + item->text());
    } else {
        userMessage("ERROR: PROCESS INDEX NOT IN GUI LIST: " + n2s(index));
    }
}

void MainWindow::stopProcess(int index)
{
    konfytProject* p = getCurrentProject();
    if (p == NULL) {
        return;
    }
    // Stop process
    p->stopProcess(index);
}

void MainWindow::removeProcess(int index)
{
    konfytProject* p = getCurrentProject();
    if (p == NULL) {
        return;
    }
    // Remove process from project
    p->removeProcess(index);
    // Remove from GUI list
    QListWidgetItem* item = ui->listWidget_ExtApps->item(index);
    delete item;
}

void MainWindow::processStartedSlot(int index, konfytProcess *process)
{
    if ( (index >=0) && (index < ui->listWidget_ExtApps->count()) ) {
        // Indicate in list widget
        QListWidgetItem* item = ui->listWidget_ExtApps->item(index);
        item->setText("[running] " + process->toString_appAndArgs());
    } else {
        userMessage("ERROR: PROCESS INDEX NOT IN GUI LIST: " + n2s(index));
    }
}

void MainWindow::processFinishedSlot(int index, konfytProcess *process)
{
    if ( (index >=0) && (index < ui->listWidget_ExtApps->count()) ) {
        // Indicate in list widget
        QListWidgetItem* item = ui->listWidget_ExtApps->item(index);
        item->setText("[stopped] " + process->toString_appAndArgs());
    } else {
        userMessage("ERROR: PROCESS INDEX NOT IN GUI LIST: " + n2s(index));
    }
}





void MainWindow::showWaitingPage(QString title)
{
    ui->label_WaitingTitle->setText(title);
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_WAITING);
}

void MainWindow::startWaiter(QString msg)
{
    // Disable all input (install event filter)
    eventFilterMode = EVENT_FILTER_MODE_WAITER;
    app->installEventFilter(this);
    // Start waiterTimer
    waiterMessage = msg;
    waiterState = 0;
    waiterTimer.start(100, this);
}

void MainWindow::stopWaiter()
{
    // Stop waiterTimer
    waiterTimer.stop();

    // Show done message on status bar for a few seconds
    ui->statusBar->showMessage("Done.", 3000);

    // Re-enable all input (remove event filter)
    app->removeEventFilter(this);
}

void MainWindow::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == waiterTimer.timerId()) {
        // We are busy waiting for something (startWaiter() has been called).
        // Rotate the cool fan in the statusbar.
        QString anim = "|/-\\";
        ui->statusBar->showMessage(QString(anim.at(waiterState)) + " " + waiterMessage);
        waiterState++;
        if (waiterState >= anim.count()) {
            waiterState = 0;
        }
    } else if (ev->timerId() == midiIndicatorTimer.timerId()) {
        ui->MIDI_indicator->setChecked(false);
        midiIndicatorTimer.stop();
    }
}

/* Helper function for scanning things into database. */
void MainWindow::scanForDatabase()
{
    startWaiter("Scanning database directories...");
    // Display waiting screen.
    userMessage("Starting database scan.");
    showWaitingPage("Scanning database directories...");
    // Start scanning for directories.
    db->scanDirs(soundfontsDir, sfzDir, patchesDir);
    // When the finished signal is received, remove waiting screen.
    // See database_scanDirsFinished()
    // Periodic status info might be received in database_scanDirsStatus()
}

void MainWindow::database_scanDirsFinished()
{
    userMessage("Database scanning complete.");
    userMessage("   Found " + n2s(db->getNumSfonts()) + " soundfonts.");
    userMessage("   Found " + n2s(db->getNumSfz()) + " sfz/gig samples.");
    userMessage("   Found " + n2s(db->getNumPatches()) + " patches.");

    // Save to database file
    saveDatabase();

    fillTreeWithAll();
    stopWaiter();
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
}

bool MainWindow::saveDatabase()
{
    // Save to database file
    if (db->saveDatabaseToFile(settingsDir + "/" + DATABASE_FILE)) {
        userMessage("Saved database to file " + settingsDir + "/" + DATABASE_FILE);
        return true;
    } else {
        userMessage("Failed to save database.");
        return false;
    }
}

void MainWindow::database_scanDirsStatus(QString msg)
{
    ui->label_WaitingStatus->setText(msg);
}

void MainWindow::database_returnSfont(konfytSoundfont *sf)
{
    if (returnSfontRequester == returnSfontRequester_on_treeWidget_filesystem_itemDoubleClicked) {
        // Soundfont received from database after request was made from
        // on_treeWidget_filesystem_itemDoubleClicked()
        programList = sf->programlist;
        library_refreshGUIProgramList();
    }

    // Enable GUI again
    stopWaiter();
}

// Rescan database button pressed.
void MainWindow::on_pushButtonSettings_RescanLibrary_clicked()
{
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_WAITING);
    applySettings();

    db->clearDatabase();
    scanForDatabase();
}

// Quick scan database button clicked
void MainWindow::on_pushButtonSettings_QuickRescanLibrary_clicked()
{
    startWaiter("Scanning database...");
    applySettings();

    db->clearDatabase_exceptSoundfonts();
    scanForDatabase();
    // This will be done in the background and database_scanDirsFinished()
    // will be called when done.
}

void MainWindow::scanThreadFihishedSlot()
{
    // Return to patch view
    //ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);

    userMessage("ScanThread finished!");
}



void MainWindow::on_tabWidget_Projects_currentChanged(int index)
{
    if (index >=0) {
        setCurrentProject(index);
    }
}

void MainWindow::on_pushButton_RemovePatch_clicked()
{
    // Remove patch
    removePatchFromProject(ui->listWidget_Patches->currentRow());
}

void MainWindow::on_pushButton_PatchUp_clicked()
{
    int row = ui->listWidget_Patches->currentRow();
    if ( row >= 1 ) {
        // Move actual patch in the project.
        getCurrentProject()->movePatchUp(row);
        // Update list in gui
        gui_updatePatchList();
        // Select item.
        ui->listWidget_Patches->setCurrentRow(row-1);
    }
}

void MainWindow::on_listWidget_Patches_indexesMoved(const QModelIndexList &indexes)
{
    userMessage("moved.");
}

void MainWindow::on_pushButton_PatchDown_clicked()
{
    int row = ui->listWidget_Patches->currentRow();
    if ( (row >= 0) && (row < ui->listWidget_Patches->count()-1)  ) {
        // Move actual patch in the project.
        getCurrentProject()->movePatchDown(row);
        // Update list in gui
        gui_updatePatchList();
        // Select item.
        ui->listWidget_Patches->setCurrentRow(row+1);
    }
}

// Indicate to the user whether the patch has been modified and needs to be saved.
void MainWindow::setPatchModified(bool modified)
{
    QString stylesheet_orange = "background-color: qlineargradient(spread:pad, x1:0, y1:1, x2:1, y2:0, stop:0 rgba(95, 59, 28, 255), stop:1 rgba(199, 117, 18, 255));";

    if (modified) {
        //ui->pushButton_ReplacePatch->setStyleSheet(stylesheet_orange);
        setProjectModified();
    } else {
        //ui->pushButton_ReplacePatch->setStyleSheet("");
    }
}

void MainWindow::setProjectModified()
{
    konfytProject* prj = getCurrentProject();
    if (prj != NULL) {
        prj->setModified(true);
    }
}


void MainWindow::projectModifiedStateChanged(bool modified)
{
    QString stylesheet_base = "border-top-left-radius: 0;"
            "border-bottom-left-radius: 0;"
            "border-top-right-radius: 0;"
            "border-bottom-right-radius: 0;";

    QString stylesheet_normal = stylesheet_base + "border-top-right-radius: 0; border-bottom-right-radius: 0;";
    QString stylesheet_orange = stylesheet_base + "background-color: qlineargradient(spread:pad, x1:0, y1:1, x2:1, y2:0, stop:0 rgba(95, 59, 28, 255), stop:1 rgba(199, 117, 18, 255));border-top-right-radius: 0; border-bottom-right-radius: 0;";

    if (modified) {
        ui->toolButton_Project->setStyleSheet(stylesheet_orange);
        ui->tabWidget_Projects->setTabText( currentProject, getCurrentProject()->getProjectName() + "*" );
    } else {
        ui->toolButton_Project->setStyleSheet(stylesheet_normal);
        ui->tabWidget_Projects->setTabText( currentProject, getCurrentProject()->getProjectName() );
    }
}

// Save current project in its own folder, in the projects dir.
bool MainWindow::saveCurrentProject()
{
    konfytProject* p = getCurrentProject();

    if (p == NULL) {
        userMessage("Select a project.");
        return false;
    }

    return saveProject(p);
}

bool MainWindow::saveProject(konfytProject *p)
{
    static bool InformedUserAboutProjectsDir = false;

    if (p == NULL) {
        userMessage("Select a project.");
        return false;
    }

    // Try to save. If this fails, it means the project has not been saved
    // yet and we need to create a directory for it.
    if (p->saveProject()) {
        // Saved successfully.
        userMessage("Project saved.");
        return true;
    } else {
        // We need to find a directory for the project.

        QString saveDir;

        // See if a default projects directory is set
        if (this->projectsDir == "") {
            userMessage("Projects directory is not set.");
            // Inform the user about project directory that is not set
            if (InformedUserAboutProjectsDir == false) {
                messageBox("No default projects directory has been set. You can set this in Settings.");
                InformedUserAboutProjectsDir = true; // So we only show it once
            }
            // We need to bring up a save dialog.
        } else {
            // Find a unique directory name within our default projects dir
            QString dir = getUniqueFilename(this->projectsDir,p->getProjectName(),"");
            if (dir == "") {
                userMessage("Failed to obtain a unique directory name.");
            } else {
                dir = this->projectsDir + "/" + dir;
                // We now have a unique directory filename.
                QMessageBox msgbox;
                msgbox.setText("Do you want to save project \"" + p->getProjectName() + "\" to the following path? Selecting No will bring up a dialog box to select a location.");
                msgbox.setInformativeText(dir);
                msgbox.setIcon(QMessageBox::Question);
                msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
                int ret = msgbox.exec();
                if (ret == QMessageBox::Yes) {
                    QDir d;
                    if (d.mkdir(dir)) {
                        userMessage("Created project directory: " + dir);
                        saveDir = dir;
                    } else {
                        userMessage("Failed to create project directory: " + dir);
                        messageBox("Failed to create project directory " + dir);
                    }
                } else if (ret == QMessageBox::Cancel) {
                    return false;
                }
            }
        }

        if (saveDir == "") {
            // Show dialog so user can select location
            QFileDialog dialog;
            dialog.setFileMode(QFileDialog::Directory);
            if ( dialog.exec() ) {
                saveDir = dialog.selectedFiles()[0];
            } else { return false; }
        }

        // Save the project
        if (getCurrentProject()->saveProjectAs(saveDir)) {
            userMessage("Project Saved to " + saveDir);
            ui->statusBar->showMessage("Project saved.", 5000);
            return true;
        } else {
            userMessage("Failed to save project.");
            messageBox("Failed to save project to " + saveDir);
            return false;
        }


    }

} // end of saveProject()


void MainWindow::updateGUIWarnings()
{
    ui->listWidget_Warnings->clear();

    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }


    // Check warnings

    // MIDI input port connected
    if ( prj->midiInPort_getClients().count() == 0 ) {
        addWarning("MIDI input port not connected");
    }

    // Any other port not connected
    QList<int> busIds = prj->audioBus_getAllBusIds();
    for (int i=0; i<busIds.count(); i++) {
        prjAudioBus bus = prj->audioBus_getBus(busIds[i]);
        bool left = (bus.leftOutClients.count() == 0);
        bool right = (bus.rightOutClients.count() == 0);
        if (left && right) {
            addWarning("Bus " + bus.busName + " ports unconnected");
        } else if (left) {
            addWarning("Bus " + bus.busName + " left port unconnected");
        } else if (right) {
            addWarning("Bus " + bus.busName + " right port unconnected");
        }
    }

}

void MainWindow::addWarning(QString warning)
{
    ui->listWidget_Warnings->addItem(warning);
}




void MainWindow::midi_setLayerGain(int layer, int midiValue)
{
    // channel slider
    float temp = ((float)midiValue)/127.0;
    // Set channel gain in engine
    if ((layer>=0) && (layer < pengine->getNumLayers()) ) {
        pengine->setLayerGain(layer,temp);
        // Set channel gain in GUI slider
        this->guiLayerItemList.at(layer)->setSliderGain(temp);
    }
}

void MainWindow::midi_setLayerMute(int layer, int midiValue)
{
    if (midiValue > 0) {
        if ((layer>=0) && (layer < pengine->getNumLayers()) ) {
            bool newMute = !(pengine->getPatch()->getLayerItems().at(layer).isMute());
            pengine->setLayerMute(layer, newMute);
            // Set in GUI layer item
            this->guiLayerItemList.at(layer)->setMuteButton(newMute);
        }
    }
}

void MainWindow::midi_setLayerSolo(int layer, int midiValue)
{
    if (midiValue > 0) {
        if ((layer>=0) && (layer < pengine->getNumLayers()) ) {
            bool newSolo = !(pengine->getPatch()->getLayerItems().at(layer).isSolo());
            pengine->setLayerSolo(layer, newSolo);
            // Set in GUI layer item
            this->guiLayerItemList.at(layer)->setSoloButton(newSolo);
        }
    }
}

// Get midi event signal from patchengine.
void MainWindow::midiEventSlot(konfytMidiEvent ev)
{
    // Show in console if enabled.
    if (console_showMidiMessages) {

        userMessage("MIDI EVENT " + midiEventToString(ev.type, ev.channel, ev.data1, ev.data2, lastBankSelectMSB, lastBankSelectLSB));
    }

    if ( !midiIndicatorTimer.isActive() ) {
        ui->MIDI_indicator->setChecked(true);
        midiIndicatorTimer.start(500, this);
    }


    // Set the "last" lineEdits in the MidiFilter view
    midiFilter_lastChan = ev.channel;
    midiFilter_lastData1 = ev.data1;
    midiFilter_lastData2 = ev.data2;
    updateMidiFilterEditorLastRx();

    // Save bank selects
    if (ev.type == MIDI_EVENT_TYPE_CC) {
        if (ev.data1 == 0) {
            // Bank select MSB
            lastBankSelectMSB = ev.data2;
        } else if (ev.data1 == 32) {
            // Bank select LSB
            lastBankSelectLSB = ev.data2;
        } else {
            // Otherwise, reset bank select. Bank select
            // should only be taken into account if a program
            // is received directly after it. If not, it is cleared.
            lastBankSelectMSB = -1;
            lastBankSelectLSB = -1;
        }
    } else if (ev.type != MIDI_EVENT_TYPE_PROGRAM) {
        // Otherwise, reset bank select. Bank select
        // should only be taken into account if a program
        // is received directly after it. If not, it is cleared.
        lastBankSelectMSB = -1;
        lastBankSelectLSB = -1;
    }

    if (ui->stackedWidget->currentIndex() == STACKED_WIDGET_PAGE_TRIGGERS) {

        // Add event to last received events list
        ui->listWidget_triggers_eventList->addItem( ev.toString() );
        triggersLastEvents.append(ev);

        // If event is a program and the previous messages happened to be bank MSB and LSB,
        // then add an extra program event which includes the bank.
        if (ev.type == MIDI_EVENT_TYPE_PROGRAM) {
            if (lastBankSelectMSB >= 0) {
                if (lastBankSelectLSB >= 0) {
                    ev.bankLSB = lastBankSelectLSB;
                    ev.bankMSB = lastBankSelectMSB;
                    ui->listWidget_triggers_eventList->addItem( ev.toString() );
                    triggersLastEvents.append(ev);
                }
            }
        }

        // The list shouldn't get too crowded
        while (triggersLastEvents.count() > 15) {
            triggersLastEvents.removeFirst();
            delete ui->listWidget_triggers_eventList->item(0);
        }

        // Make sure the last received event is selected in the gui list
        ui->listWidget_triggers_eventList->setCurrentRow( ui->listWidget_triggers_eventList->count()-1 );

        return; // Skip normal processing

    }

    // Hash midi event to a key
    int key;
    if (ev.type == MIDI_EVENT_TYPE_PROGRAM) {
        key = hashMidiEventToInt(ev.type, ev.channel, ev.data1, lastBankSelectMSB, lastBankSelectLSB);
        userMessage("key = " + n2s(key));
    } else {
        key = hashMidiEventToInt(ev.type, ev.channel, ev.data1, -1, -1);
    }
    // Get the appropriate action based on the key
    QAction* action = triggersMidiActionHash[key];

    // Perform the action
    if (action == ui->actionPanic) {

        if (ev.data2 > 0) { ui->actionPanic->trigger(); }

    } else if (action == ui->actionNext_Patch) {

        if (ev.data2 > 0) { setCurrentPatch( currentPatchIndex+1 ); }

    } else if (action == ui->actionPrevious_Patch) {

        if (ev.data2 > 0) { setCurrentPatch( currentPatchIndex-1 ); }

    } else if (action == ui->actionMaster_Volume_Slider) {

        ui->horizontalSlider_MasterGain->setValue(((float)ev.data2)/127.0*ui->horizontalSlider_MasterGain->maximum());
        ui->horizontalSlider_MasterGain->triggerAction(QSlider::SliderMove);
        on_horizontalSlider_MasterGain_sliderMoved(ui->horizontalSlider_MasterGain->value());

    } else if (action == ui->actionMaster_Volume_Up) {

        if (ev.data2 > 0) { ui->actionMaster_Volume_Up->trigger(); }

    } else if (action == ui->actionMaster_Volume_Down) {

        if (ev.data2 > 0) { ui->actionMaster_Volume_Down->trigger(); }

    } else if (action == ui->actionSave_Patch) {

        if (ev.data2 > 0) { ui->actionSave_Patch->trigger(); }

    } else if (action == ui->actionProject_save) {

        if (ev.data2 > 0) { ui->actionProject_save->trigger(); }

    } else if (channelGainActions.contains(action)) {

        midi_setLayerGain( channelGainActions.indexOf(action), ev.data2 );

    } else if (channelSoloActions.contains(action)) {

        midi_setLayerSolo( channelSoloActions.indexOf(action), ev.data2 );

    } else if (channelMuteActions.contains(action)) {

        midi_setLayerMute( channelMuteActions.indexOf(action), ev.data2 );

    } else if (patchActions.contains(action)) {

        setCurrentPatch( patchActions.indexOf(action) );

    } else if (action == ui->actionGlobal_Transpose_12_Down) {

        if (ev.data2 > 0) { setMasterInTranspose(-12,true); }

    } else if (action == ui->actionGlobal_Transpose_12_Up) {

        if (ev.data2 > 0) { setMasterInTranspose(12,true); }

    } else if (action == ui->actionGlobal_Transpose_1_Down) {

        if (ev.data2 > 0) { setMasterInTranspose(-1,true); }

    } else if (action == ui->actionGlobal_Transpose_1_Up) {

        if (ev.data2 > 0) { setMasterInTranspose(1,true); }

    } else if (action == ui->actionGlobal_Transpose_Zero) {

        if (ev.data2 > 0) { setMasterInTranspose(0,true); }

    }

}




void MainWindow::on_pushButton_ClearConsole_clicked()
{
    ui->textBrowser->clear();
}




/* Patch midi output port menu item has been clicked. */
void MainWindow::onPatchMidiOutPortsMenu_ActionTrigger(QAction *action)
{
    if (action == patchMidiOutPortsMenu_NewPortAction) {
        // Add new port
        int portId = addMidiOutPort();
        if (portId >= 0) {
            addMidiPortToCurrentPatch(portId);
            // Show the newly created port in the connections tree
            showConnectionsPage();
            ui->tree_portsBusses->setCurrentItem( tree_midiOutMap.key(portId) );
        }
    } else {
        // Add chosen port to patch
        int portId = patchMidiOutPortsMenu_map.value(action);
        addMidiPortToCurrentPatch( portId );
    }
}

/* Patch add audio bus menu item has been clicked. */
void MainWindow::onPatchAudioInPortsMenu_ActionTrigger(QAction *action)
{
    if (action == patchAudioInPortsMenu_NewPortAction) {
        // Add new port
        int portId = addAudioInPort();
        if (portId >= 0) {
            addAudioInPortToCurrentPatch( portId );
            // Show the newly created port in the connections tree
            showConnectionsPage();
            ui->tree_portsBusses->setCurrentItem( tree_audioInMap.key(portId) );
        }

    } else {
        // Add chosen port to patch
        int portId = patchAudioInPortsMenu_map.value(action);
        addAudioInPortToCurrentPatch( portId );
    }
}

/* Layer midi output channel menu item has been clicked. */
void MainWindow::onLayerMidiOutChannelMenu_ActionTrigger(QAction* action)
{
    int channel = layerMidiOutChannelMenu_map.value(action);

    konfytPatchLayer layer = layerBusMenu_sourceItem->getPatchLayerItem();
    konfytMidiFilter filter = layer.getMidiFilter();
    filter.outChan = channel;
    layer.setMidiFilter(filter);

    // Update layer widget
    layerBusMenu_sourceItem->setLayerItem(layer);
    // Update in pengine
    pengine->setLayerFilter(&layer, filter);

    setPatchModified(true);
}

/* Layer bus menu item has been clicked. */
void MainWindow::onLayerBusMenu_ActionTrigger(QAction *action)
{
    int busId;
    if (action == layerBusMenu_NewBusAction) {
        // Add new bus
        busId = addBus();
        if (busId < 0) { return; }
    } else {
        // User chose a bus
        busId = layerBusMenu_actionsBusIdsMap.value(action);
    }

    // Set the destination bus in gui layer item
    konfytPatchLayer g = layerBusMenu_sourceItem->getPatchLayerItem();
    g.busIdInProject = busId;

    // Update the layer widget
    layerBusMenu_sourceItem->setLayerItem( g );
    // Update in pengine
    pengine->setLayerBus( &g, busId );

    setPatchModified(true);

    // If the user added a new bus, open the connections page and show bus.
    if (action == layerBusMenu_NewBusAction) {
        showConnectionsPage();
        ui->tree_portsBusses->setCurrentItem( tree_busMap.key(busId) );
    }
}




void MainWindow::on_pushButton_ExtApp_add_clicked()
{
    // Create a new process and call addProcess function
    konfytProcess* p = new konfytProcess();

    p->appname = ui->lineEdit_ExtApp->text();

    addProcess(p);

}

void MainWindow::on_lineEdit_ExtApp_returnPressed()
{
    on_pushButton_ExtApp_add_clicked();
}

void MainWindow::on_pushButton_ExtApp_RunSelected_clicked()
{
    int row = ui->listWidget_ExtApps->currentRow();
    if (row < 0) {
        userMessage("Select an application.");
        return;
    }
    runProcess(row);
}




void MainWindow::on_pushButton_ExtApp_Stop_clicked()
{
    int row = ui->listWidget_ExtApps->currentRow();
    if (row < 0) {
        userMessage("Select an application.");
        return;
    }
    stopProcess(row);
}

void MainWindow::on_pushButton_ExtApp_RunAll_clicked()
{
    // Start all the processes in the list
    for (int i=0; i<ui->listWidget_ExtApps->count(); i++) {
        runProcess(i);
    }
}



void MainWindow::on_pushButton_ExtApp_StopAll_clicked()
{
    // Stop all the processes in the list
    for (int i=0; i<ui->listWidget_ExtApps->count(); i++) {
        stopProcess(i);
    }
}

void MainWindow::on_pushButton_ExtApp_remove_clicked()
{
    int row = ui->listWidget_ExtApps->currentRow();
    if (row < 0) {
        userMessage("Select an application.");
        return;
    }
    removeProcess(row);
}


/* Slot: on layer item remove button clicked. */
void MainWindow::onLayer_remove_clicked(konfytLayerWidget *layerItem)
{
    // Remove layer item from engine and GUI.
    removeLayerItem(layerItem);
}

/* Slot: on layer item filter button clicked. */
void MainWindow::onLayer_filter_clicked(konfytLayerWidget *layerItem)
{
    midiFilterEditItem = layerItem;
    showMidiFilterEditor();
}

/* Slot: on layer item slider move. */
void MainWindow::onLayer_slider_moved(konfytLayerWidget *layerItem, float gain)
{
    konfytPatchLayer g = layerItem->getPatchLayerItem();
    pengine->setLayerGain(&g, gain);
}

/* Slot: on layer item solo button clicked. */
void MainWindow::onLayer_solo_clicked(konfytLayerWidget *layerItem, bool solo)
{
    konfytPatchLayer g = layerItem->getPatchLayerItem();
    pengine->setLayerSolo(&g, solo);

}

/* Slot: on layer item mute button clicked. */
void MainWindow::onLayer_mute_clicked(konfytLayerWidget *layerItem, bool mute)
{
    konfytPatchLayer g = layerItem->getPatchLayerItem();
    pengine->setLayerMute(&g, mute);
}

/* Slot: on layer item bus button clicked. */
void MainWindow::onLayer_bus_clicked(konfytLayerWidget *layerItem)
{
    // Save the layer item for future use
    layerBusMenu_sourceItem = layerItem;

    konfytLayerType type = layerItem->getPatchLayerItem().getLayerType();
    if (type == KonfytLayerType_MidiOut) {
        // Show MIDI channel menu
        gui_updateLayerMidiOutChannelMenu();
        layerMidiOutChannelMenu.popup(QCursor::pos());
    } else {
        // Show Busses menu
        gui_updateLayerBusMenu();
        layerBusMenu.popup(QCursor::pos());

    }
    // The rest will be done in onlayerBusMenu_ActionTrigger when the user clicked.
}

void MainWindow::onLayer_reload_clicked(konfytLayerWidget *layerItem)
{
    konfytPatchLayer l = layerItem->getPatchLayerItem();
    konfytPatchLayer lnew = pengine->reloadLayer( &l );
    layerItem->setLayerItem( lnew );

}

void MainWindow::gui_updateLayerBusMenu()
{
    layerBusMenu.clear();
    layerBusMenu_actionsBusIdsMap.clear();
    konfytProject* prj = getCurrentProject();
    QList<int> busIds = prj->audioBus_getAllBusIds();
    for (int i=0; i<busIds.count(); i++) {
        int id = busIds[i];
        QAction* action = layerBusMenu.addAction( n2s(id) + " " + prj->audioBus_getBus(id).busName );
        layerBusMenu_actionsBusIdsMap.insert(action, id);
    }
    layerBusMenu.addSeparator();
    layerBusMenu_NewBusAction = layerBusMenu.addAction("New Bus");
}

void MainWindow::gui_updateLayerMidiOutChannelMenu()
{
    static bool done = false;

    if (!done) {
        done = true;
        layerMidiOutChannelMenu.clear();
        layerMidiOutChannelMenu_map.clear();
        QAction* action = layerMidiOutChannelMenu.addAction("Original Channel" );
        layerMidiOutChannelMenu_map.insert(action, -1);
        for (int i=0; i<=15; i++) {
            QAction* action = layerMidiOutChannelMenu.addAction("Channel " + n2s(i+1));
            layerMidiOutChannelMenu_map.insert(action, i);
        }
    }
}






/* Add a layer item to the GUI layer list. */
void MainWindow::addLayerItemToGUI(konfytPatchLayer layerItem)
{
    // Create new gui layer item
    konfytLayerWidget* gui = new konfytLayerWidget();
    gui->project = getCurrentProject();
    QListWidgetItem* item = new QListWidgetItem();
    gui->initLayer(layerItem, item);

    // Add to our internal list
    this->guiLayerItemList.append(gui);

    // Add to gui layer list
    ui->listWidget_Layers->addItem(item);
    // and set the item widget
    item->setSizeHint(gui->size());
    ui->listWidget_Layers->setItemWidget(item, gui);

    // Make all connections
    connect(gui, SIGNAL(slider_moved_signal(konfytLayerWidget*,float)), this, SLOT(onLayer_slider_moved(konfytLayerWidget*,float)));
    connect(gui, SIGNAL(remove_clicked_signal(konfytLayerWidget*)), this, SLOT(onLayer_remove_clicked(konfytLayerWidget*)));
    connect(gui, SIGNAL(filter_clicked_signal(konfytLayerWidget*)), this, SLOT(onLayer_filter_clicked(konfytLayerWidget*)));
    connect(gui, SIGNAL(solo_clicked_signal(konfytLayerWidget*,bool)), this, SLOT(onLayer_solo_clicked(konfytLayerWidget*,bool)));
    connect(gui, SIGNAL(mute_clicked_signal(konfytLayerWidget*,bool)), this, SLOT(onLayer_mute_clicked(konfytLayerWidget*,bool)));
    connect(gui, SIGNAL(bus_clicked_signal(konfytLayerWidget*)), this, SLOT(onLayer_bus_clicked(konfytLayerWidget*)));
    connect(gui, SIGNAL(reload_clicked_signal(konfytLayerWidget*)), this, SLOT(onLayer_reload_clicked(konfytLayerWidget*)));

}

/* Remove a layer item from the layer list.
 * This includes from the engine, GUI and the internal list. */
void MainWindow::removeLayerItem(konfytLayerWidget *layerItem)
{
    // First, remove from engine
    konfytPatchLayer g = layerItem->getPatchLayerItem();
    pengine->removeLayer(&g);

    removeLayerItem_GUIonly(layerItem);

    setPatchModified(true);
}

// Remove a layer item from the gui (and our internal list) only.
// This is used if the layers in the engine should not be modified.
void MainWindow::removeLayerItem_GUIonly(konfytLayerWidget *layerItem)
{
    // Remove from our internal list
    this->guiLayerItemList.removeAll(layerItem);

    // Remove from GUI list
    QListWidgetItem* item = layerItem->getListWidgetItem();
    delete item;
}

// Clear patch's layer items from GUI list.
void MainWindow::clearLayerItems_GUIonly()
{
    while (this->guiLayerItemList.count()) {

        this->removeLayerItem_GUIonly(this->guiLayerItemList.at(0));

    }
}



void MainWindow::on_pushButton_Settings_Cancel_clicked()
{
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
}

void MainWindow::on_pushButton_Settings_Apply_clicked()
{
    applySettings();
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
}

void MainWindow::on_pushButton_settings_Projects_clicked()
{
    // Show dialog to select projects directory
    QFileDialog* d = new QFileDialog();
    ui->lineEdit_Settings_ProjectsDir->setText( d->getExistingDirectory(this,"Select projects directory",
                                                                        ui->lineEdit_Settings_ProjectsDir->text()) );
}

void MainWindow::on_pushButton_Settings_Soundfonts_clicked()
{
    // Show dialog to select soundfonts directory
    QFileDialog* d = new QFileDialog();
    ui->lineEdit_Settings_SoundfontsDir->setText( d->getExistingDirectory(this,"Select soundfonts directory",
                                                                        ui->lineEdit_Settings_SoundfontsDir->text()) );
}

void MainWindow::on_pushButton_Settings_Patches_clicked()
{
    // Show dialog to select patches directory
    QFileDialog* d = new QFileDialog();
    ui->lineEdit_Settings_PatchesDir->setText( d->getExistingDirectory(this,"Select patches directory",
                                                                        ui->lineEdit_Settings_PatchesDir->text()) );
}

void MainWindow::on_pushButton_Settings_Sfz_clicked()
{
    // Show dialog to select sfz directory
    QFileDialog* d = new QFileDialog();
    ui->lineEdit_Settings_SfzDir->setText( d->getExistingDirectory(this,"Select sfz directory",
                                                                        ui->lineEdit_Settings_SfzDir->text()) );
}


/* Action to save current patch (in pengine; may have been modified)
 * to currently selected patch in the project. */
void MainWindow::on_actionSave_Patch_triggered()
{

    // 2017-01-05 This is not really used anymore as patches are automatically saved at the moment.

    konfytProject* prj = getCurrentProject();

    // Write the patch note from the GUI to the patch.
    pengine->setPatchNote(ui->textBrowser_patchNote->toPlainText());

    // If no patch is selected in the list, save as new patch. Otherwise, save to selected patch.
    int i = currentPatchIndex;
    if ( (i<0) || (i>=prj->getNumPatches())) {
        on_actionSave_Patch_As_Copy_triggered();
    } else {
        prj->markPatchLoaded(i);
        gui_updatePatchList();
        setCurrentPatch(i);
    }

    // Indicate to the user that the patch is not modified anymore.
    setPatchModified(false);
}

/* Action to save current patch as a copy in the current project. */
void MainWindow::on_actionSave_Patch_As_Copy_triggered()
{
    konfytPatch* p = pengine->getPatch();
    konfytPatch* newPatch = new konfytPatch();
    *newPatch = *p;
    addPatchToProject(newPatch);

    setCurrentPatch(ui->listWidget_Patches->count()-1);

    ui->lineEdit_PatchName->setFocus();
    ui->lineEdit_PatchName->selectAll();

    // Indicate to the user that the patch is not modified anymore.
    setPatchModified(false);
}

/* Action to add current patch to the library. */
void MainWindow::on_actionAdd_Patch_To_Library_triggered()
{
    konfytPatch* pt = pengine->getPatch(); // Get current patch

    if (savePatchToLibrary(pt)) {
        userMessage("Saved to library.");
    } else {
        userMessage("Could not save patch to library.");
    }
}

/* Action to save the current patch to file. */
void MainWindow::on_actionSave_Patch_To_File_triggered()
{
    // Save patch to user selected file

    konfytPatch* pt = pengine->getPatch(); // Get current patch
    QFileDialog d;
    QString filename = d.getSaveFileName(this,"Save patch as file", patchesDir, "*." + QString(KonfytPatchSuffix));
    if (filename=="") {return;} // Dialog was cancelled.

    // Add suffix if not already added (this is not foolproof, but what the hell.)
    if (!filename.contains("." + QString(KonfytPatchSuffix))) { filename = filename + "." + QString(KonfytPatchSuffix); }

    if (pt->savePatchToFile(filename)) {
        userMessage("Patch saved.");
    } else {
        userMessage("Failed saving patch to file.");
    }
}

/* Action to add new patch to project. */
void MainWindow::on_actionNew_Patch_triggered()
{
    newPatchToProject();
    setCurrentPatch( ui->listWidget_Patches->count()-1 );
    ui->lineEdit_PatchName->setFocus();
    ui->lineEdit_PatchName->selectAll();

}

/* Action to add patch from the library (currently selected) to the project. */
void MainWindow::on_actionAdd_Patch_From_Library_triggered()
{
    if ( library_getSelectedTreeItemType() == libTreePatch ) {
        konfytPatch* newPatch = new konfytPatch();
        *newPatch = library_getSelectedPatch();
        addPatchToProject(newPatch);
    } else {
        userMessage("Select a patch in the library.");
    }
}

/* Action to add a patch from a file to the project. */
void MainWindow::on_actionAdd_Patch_From_File_triggered()
{
    // Load patch from user selected file

    konfytPatch* pt = new konfytPatch();
    QFileDialog d;
    QString filename = d.getOpenFileName(this, "Open patch from file", patchesDir, "*." + QString(KonfytPatchSuffix));
    if (filename=="") { return; }
    if (pt->loadPatchFromFile(filename)) {
        getCurrentProject()->addPatch(pt);
        gui_updatePatchList();
    } else {
        userMessage("Failed loading patch from file.");
    }
}

/* Add button clicked (not its menu). */
void MainWindow::on_toolButton_AddPatch_clicked()
{
    // If a patch is selected in the library, add it to the project.
    // Otherwise, add a new patch to the project.

    if ( library_getSelectedTreeItemType() == libTreePatch ) {
        on_actionAdd_Patch_From_Library_triggered();
    } else {
        on_actionNew_Patch_triggered();
    }
}

/* Save button clicked (not its menu). */
void MainWindow::on_toolButton_SavePatch_clicked()
{
    //on_actionSave_Patch_triggered();

}



void MainWindow::on_pushButton_ShowConsole_clicked()
{
    this->consoleDiag->show();
}




bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    // To find all actions:
    // QList<QAction*> actionList = this->findChildren<QAction*>();

    if (eventFilterMode == EVENT_FILTER_MODE_WAITER) {
        // The GUI is disabled while waiting for a long task to finish.
        // Eat up all mouse and keyboard events
        if ( (event->type() == QEvent::MouseButtonDblClick)
             || (event->type() == QEvent::MouseButtonPress)
             || (event->type() == QEvent::MouseButtonRelease)
             || (event->type() == QEvent::KeyPress)
             || (event->type() == QEvent::KeyRelease) ) {
            return true;
        } else {
            return false;
        }

    } else if (eventFilterMode == EVENT_FILTER_MODE_LIVE) {

        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

            switch (keyEvent->key()) {
            case Qt::Key_Escape:
                ui->actionPanic->trigger();
                break;
            case Qt::Key_Plus:
                ui->actionMaster_Volume_Up->trigger();
                break;
            case Qt::Key_Equal:
                ui->actionMaster_Volume_Up->trigger();
                break;
            case Qt::Key_Minus:
                ui->actionMaster_Volume_Down->trigger();
                break;
            case Qt::Key_Space:
                // Next patch
                setCurrentPatch( currentPatchIndex+1 );
                break;
            case Qt::Key_Right:
            case Qt::Key_Down:
                // Next patch
                setCurrentPatch( currentPatchIndex+1 );
                break;
            case Qt::Key_Left:
            case Qt::Key_Up:
                // Previous patch
                setCurrentPatch( currentPatchIndex-1 );
                break;
            case Qt::Key_1:
                setCurrentPatch( 0 );
                break;
            case Qt::Key_2:
                setCurrentPatch( 1 );
                break;
            case Qt::Key_3:
                setCurrentPatch( 2 );
                break;
            case Qt::Key_4:
                setCurrentPatch( 3 );
                break;
            case Qt::Key_5:
                setCurrentPatch( 4 );
                break;
            case Qt::Key_6:
                setCurrentPatch( 5 );;
                break;
            case Qt::Key_7:
                setCurrentPatch( 6 );
                break;
            case Qt::Key_8:
                setCurrentPatch( 7 );
                break;
            case Qt::Key_Q:
                midi_setLayerMute(0, 127);
                break;
            case Qt::Key_W:
                midi_setLayerMute(1, 127);
                break;
            case Qt::Key_E:
                midi_setLayerMute(2, 127);
                break;
            case Qt::Key_R:
                midi_setLayerMute(3, 127);
                break;
            case Qt::Key_T:
                midi_setLayerMute(4, 127);
                break;
            case Qt::Key_Y:
                midi_setLayerMute(5, 127);
                break;
            case Qt::Key_U:
                midi_setLayerMute(6, 127);
                break;
            case Qt::Key_I:
                midi_setLayerMute(7, 127);
                break;
            case Qt::Key_O:
                midi_setLayerMute(8, 127);
                break;
            case Qt::Key_P:
                midi_setLayerMute(9, 127);
                break;
            default:
                break;
            }
            return true;

        } else if (event->type() == QEvent::MouseMove) {
            //userMessage("Mouse Move!");
        }

    } else {
        error_abort("MainWindow EventFilter: Invalid eventFilterMode " + n2s(eventFilterMode));
    }

    return false; // event not handled
}

void MainWindow::on_pushButton_midiFilter_Cancel_clicked()
{
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
}

// The user has been editing the midi filter and has now clicked apply.
void MainWindow::on_pushButton_midiFilter_Apply_clicked()
{
    konfytPatchLayer g = midiFilterEditItem->getPatchLayerItem();
    konfytMidiFilter f = g.getMidiFilter();

    // Update the filter from the GUI
    f.addZone( ui->spinBox_midiFilter_LowNote->value(),
               ui->spinBox_midiFilter_HighNote->value(),
               ui->spinBox_midiFilter_Multiply->value(),
               ui->spinBox_midiFilter_Add->value(),
               ui->spinBox_midiFilter_LowVel->value(),
               ui->spinBox_midiFilter_HighVel->value());
    if (ui->comboBox_midiFilter_inChannel->currentIndex() == 0) {
        // Index zero is all channels
        f.inChan = -1;
    } else {
        f.inChan = ui->comboBox_midiFilter_inChannel->currentIndex()-1;
    }
    f.passAllCC = ui->checkBox_midiFilter_AllCCs->isChecked();
    f.passPitchbend = ui->checkBox_midiFilter_pitchbend->isChecked();
    f.passProg = ui->checkBox_midiFilter_Prog->isChecked();
    f.passCC.clear();
    for (int i=0; i<ui->listWidget_midiFilter_CC->count(); i++) {
        f.passCC.append( ui->listWidget_midiFilter_CC->item(i)->text().toInt() );
    }

    // Update filter in gui layer item
    g.setMidiFilter(f);
    midiFilterEditItem->setLayerItem(g);
    // and also in engine.
    pengine->setLayerFilter(&g, f);

    // Indicate project needs to be saved
    setProjectModified();

    // Switch back to patch view
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);

}



void MainWindow::on_toolButton_MidiFilter_lowNote_clicked()
{
    ui->spinBox_midiFilter_LowNote->setValue(midiFilter_lastData1);
}

void MainWindow::on_toolButton_MidiFilter_HighNote_clicked()
{
    ui->spinBox_midiFilter_HighNote->setValue(midiFilter_lastData1);
}

void MainWindow::on_toolButton_MidiFilter_Multiply_clicked()
{
    ui->spinBox_midiFilter_Multiply->setValue(midiFilter_lastData1);
}

void MainWindow::on_toolButton_MidiFilter_Add_clicked()
{
    ui->spinBox_midiFilter_Add->setValue(midiFilter_lastData1);
}

void MainWindow::on_toolButton_MidiFilter_Add_Plus12_clicked()
{
    ui->spinBox_midiFilter_Add->setValue(ui->spinBox_midiFilter_Add->value()-12);
}

void MainWindow::on_toolButton_MidiFilter_Add_Minus12_clicked()
{
    ui->spinBox_midiFilter_Add->setValue(ui->spinBox_midiFilter_Add->value()+12);
}

void MainWindow::on_toolButton_Settings_clicked()
{
    // Show the settings dialog.
    showSettingsDialog();
}

/* Live mode button clicked */
void MainWindow::on_pushButton_LiveMode_clicked()
{
    if (ui->pushButton_LiveMode->isChecked()) {
        // Switch to live mode
        ui->stackedWidget_left->setCurrentIndex(STACKED_WIDGET_LEFT_LIVE);
        // Install event filter to catch all global key presses
        eventFilterMode = EVENT_FILTER_MODE_LIVE;
        app->installEventFilter(this);
    } else {
        // Switch out of live mode to normal
        ui->stackedWidget_left->setCurrentIndex(STACKED_WIDGET_LEFT_LIBRARY);
        // Remove event filter
        app->removeEventFilter(this);

    }
}

void MainWindow::on_actionMaster_Volume_Up_triggered()
{
    ui->horizontalSlider_MasterGain->setValue(ui->horizontalSlider_MasterGain->value() + 1);
    on_horizontalSlider_MasterGain_sliderMoved(ui->horizontalSlider_MasterGain->value());
}

void MainWindow::on_actionMaster_Volume_Down_triggered()
{
    ui->horizontalSlider_MasterGain->setValue(ui->horizontalSlider_MasterGain->value() -1);
    on_horizontalSlider_MasterGain_sliderMoved(ui->horizontalSlider_MasterGain->value());
}

/* External apps list: item double clicked. */
void MainWindow::on_listWidget_ExtApps_doubleClicked(const QModelIndex &index)
{
    // Run the currently selected process.
    on_pushButton_ExtApp_RunSelected_clicked();
}

void MainWindow::on_listWidget_ExtApps_clicked(const QModelIndex &index)
{
    // Put the contents of the selected item in the External Apps text box
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    konfytProcess* p = prj->getProcessList()[index.row()];
    ui->lineEdit_ExtApp->setText(p->appname);
}

void MainWindow::on_toolButton_layer_AddMidiPort_clicked()
{
    // Button is configured to popup a menu upon click.
    // See patchMidiOutPortsMenu and on_patchMidiOutPortsMenu_ActionTrigger().
}



void MainWindow::on_listWidget_Patches_itemClicked(QListWidgetItem *item)
{
    setCurrentPatch( ui->listWidget_Patches->row(item) );
}


/* Library soundfont program list: item double clicked. */
void MainWindow::on_listWidget_2_itemDoubleClicked(QListWidgetItem *item)
{
    // Add soundfont program to current patch.

    if (previewMode) { setPreviewMode(false); }

    if (library_isProgramSelected()) {

        addProgramToCurrentPatch( library_getSelectedProgram() );
    }
}


/* Library tree: item double clicked. */
void MainWindow::on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (previewMode) { setPreviewMode(false); }

    if (library_isProgramSelected()) {

        addProgramToCurrentPatch( library_getSelectedProgram() );

    } else if ( library_getTreeItemType(item) == libTreeSFZ ) {

        addSfzToCurrentPatch( library_selectedSfz );

    } else if ( library_getTreeItemType(item) == libTreePatch ) {

        konfytPatch* p = new konfytPatch();
        *p = library_getSelectedPatch();
        addPatchToProject( p );
    }
}

void MainWindow::on_toolButton_MidiFilter_lowVel_clicked()
{
    ui->spinBox_midiFilter_LowVel->setValue( midiFilter_lastData2 );
}

void MainWindow::on_toolButton_MidiFilter_HighVel_clicked()
{
    ui->spinBox_midiFilter_HighVel->setValue( midiFilter_lastData2 );
}

void MainWindow::on_toolButton_MidiFilter_lastCC_clicked()
{
    ui->lineEdit_MidiFilter_CC->setText( n2s(midiFilter_lastData1) );
}

void MainWindow::on_toolButton_MidiFilter_Add_CC_clicked()
{
    // Midi Filter: Add CC in lineEdit to CC list
    int cc = ui->lineEdit_MidiFilter_CC->text().toInt();
    // First, check if it isn't already in list
    bool is_in_list = false;
    for ( int i=0; i<ui->listWidget_midiFilter_CC->count(); i++ ) {
        if ( ui->listWidget_midiFilter_CC->item(i)->text().toInt() == cc) {
            is_in_list = true;
            break;
        }
    }
    if (is_in_list == false) {
        // Not already in list. Add to list.
        ui->listWidget_midiFilter_CC->addItem( n2s(cc) );
    }
}

void MainWindow::on_toolButton_MidiFilter_removeCC_clicked()
{
    // Midi Filter: remove selected CC from list
    if (ui->listWidget_midiFilter_CC->currentRow() != -1) {
        delete ui->listWidget_midiFilter_CC->currentItem();
    }
}

// Library tab widget current tab changed.
void MainWindow::on_tabWidget_library_currentChanged(int index)
{
    if (index == LIBRARY_TAB_LIBRARY) {
        // Library tab selected

    } else if (index == LIBRARY_TAB_FILESYSTEM) {
        // Filesystem tab selected

        // Refresh
        refreshFilesystemView();
    }
}

// Refresh the library file system view
void MainWindow::refreshFilesystemView()
{

    ui->lineEdit_filesystem_path->setText(fsview_currentPath);

    konfytProject* prj = getCurrentProject();
    QString project_dir;
    if (prj != NULL) {
        project_dir = prj->getDirname();
    }

    QDir d(fsview_currentPath);
    QFileInfoList l = d.entryInfoList(QDir::NoFilter,QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);


    ui->treeWidget_filesystem->clear();
    fsMap.clear();

    for (int i=0; i<l.count(); i++) {
        QFileInfo info = l[i];
        if (info.fileName() == ".") { continue; }
        if (info.fileName() == "..") { continue; }

        if (info.isDir()) {
            QTreeWidgetItem* item = new QTreeWidgetItem();
            item->setIcon(0, QIcon(":/icons/folder.png"));
            item->setText(0, info.fileName());
            ui->treeWidget_filesystem->addTopLevelItem(item);
            fsMap.insert(item, info);
        } else {
            if ( ( (info.suffix().toLower() == "sfz")
                 || (info.suffix().toLower() == "sf2")
                 || (info.suffix().toLower() == "gig") )
                || ( info.path().contains(project_dir) ) ) { // Add everything if in project dir

                QTreeWidgetItem* item = new QTreeWidgetItem();
                item->setIcon(0, QIcon(":/icons/picture.png"));
                item->setText(0, info.fileName());
                ui->treeWidget_filesystem->addTopLevelItem(item);
                fsMap.insert(item, info);
            }
        }

    }

}

// Change filesystem view directory, storing current path for the 'back' functionality.
void MainWindow::cdFilesystemView(QString newpath)
{
    fsview_back.append(fsview_currentPath);
    fsview_currentPath = newpath;
    refreshFilesystemView();
}


/* Filesystem view: double clicked file or folder in file list. */
void MainWindow::on_treeWidget_filesystem_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    QFileInfo info = fsMap.value(item);

    programList.clear();

    if (info.isDir()) {
        // If directory, cd to directory.
        cdFilesystemView(info.filePath());
    } else if (info.suffix().toLower() == "sf2") {
        // If soundfont, read soundfont and fill program list.

        // Initiate mainwindow waiter (this disables the GUI and indicates to the user
        // that we are busy getting the soundfont in the background)
        startWaiter("Loading soundfont...");
        // Request soundfont from database
        returnSfontRequester = returnSfontRequester_on_treeWidget_filesystem_itemDoubleClicked;
        this->db->returnSfont(info.filePath());
        // This might take a while. The result will be sent by signal to the
        // database_returnSfont() slot where we will continue.
        return;

    } else if ( (info.suffix().toLower() == "sfz") || (info.suffix().toLower() == "gig") ) {
        // If sfz or gig, load file.

        addSfzToCurrentPatch( info.filePath() );

    }

    // Refresh program list in the GUI based on contents of programList variable.
    library_refreshGUIProgramList();
}

/* Filesystem view: one up button clicked. */
void MainWindow::on_toolButton_filesystem_up_clicked()
{
    QFileInfo info(this->fsview_currentPath);
    cdFilesystemView(info.path());
}

/* Filesystem view: refresh button clicked. */
void MainWindow::on_toolButton_filesystem_refresh_clicked()
{
    refreshFilesystemView();
}

/* Filesystem view: Home button clicked. */
void MainWindow::on_toolButton_filesystem_home_clicked()
{
    cdFilesystemView( QDir::homePath() );
}

/* Filesystem view: back button clicked. */
void MainWindow::on_toolButton_filesystem_back_clicked()
{
    if (fsview_back.count()) {
        fsview_currentPath = fsview_back.back();
        fsview_back.removeLast();
        refreshFilesystemView();
    }
}

/* Filesystem view: Enter pressed in file path text box. */
void MainWindow::on_lineEdit_filesystem_path_returnPressed()
{
    QFileInfo info(ui->lineEdit_filesystem_path->text());
    if (info.isDir()) {
        cdFilesystemView(info.filePath());
    } else {
        cdFilesystemView( info.dir().path() );
    }
}


// Add left and right audio output ports to Jack client for a bus, named
// according to the given bus number. The left and right port references in Jack
// are assigned to the leftPort and rightPort parameters.
void MainWindow::addAudioBusToJack(int busNo, konfytJackPort** leftPort, konfytJackPort** rightPort)
{
    *leftPort = jack->addPort( KonfytJackPortType_AudioOut, "bus_" + n2s( busNo ) + "_L" );
    *rightPort = jack->addPort( KonfytJackPortType_AudioOut, "bus_" + n2s( busNo ) + "_R" );
}

/* Adds an audio bus to the current project and Jack. Returns bus index.
   Returns -1 on error. */
int MainWindow::addBus()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("Select a project.");
        return -1;
    }

    // Add to project
    QString busName = "AudioBus_" + n2s( prj->audioBus_count() );
    int busId = prj->audioBus_add( busName, NULL, NULL );

    konfytJackPort *left, *right;
    addAudioBusToJack( busId, &left, &right );
    if ( (left != NULL) && (right != NULL) ) {
        prjAudioBus bus = prj->audioBus_getBus(busId);
        bus.leftJackPort = left;
        bus.rightJackPort = right;
        prj->audioBus_replace(busId, bus);
        return busId;
    } else {
        prj->audioBus_remove(busId);
        userMessage("ERROR: Failed to create audio bus. Failed to add Jack port(s).");
        return -1;
    }
}

void MainWindow::on_actionAdd_Bus_triggered()
{
    addBus();
    showConnectionsPage();
}



/* Adds an audio input port to current project and Jack and returns port's ID.
   Returns -1 on error. */
int MainWindow::addAudioInPort()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("Select a project.");
        return -1;
    }


    int portId = prj->audioInPort_add( "New Audio In Port" );
    konfytJackPort *left, *right;
    addAudioInPortsToJack( portId, &left, &right );
    if ( (left != NULL) && (right != NULL) ) {
        // Update in project
        prjAudioInPort p = prj->audioInPort_getPort(portId);
        p.leftJackPort = left;
        p.rightJackPort = right;
        prj->audioInPort_replace(portId, p);

        // Add to GUI
        gui_updatePatchAudioInPortsMenu();
        return portId;
    } else {
        userMessage("ERROR: Failed to create audio input port. Failed to add Jack port.");
        prj->audioInPort_remove(portId);
        return -1;
    }
}

void MainWindow::on_actionAdd_Audio_In_Port_triggered()
{
    addAudioInPort();
    showConnectionsPage();
}

// Adds new midi output port to project and jack. Returns the port index.
// Returns -1 on error.
int MainWindow::addMidiOutPort()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("Select a project.");
        return -1;
    }


    int portId = prj->midiOutPort_addPort("New MIDI Out Port"); // Add to current project
    konfytJackPort* jackPort;
    addMidiOutPortToJack(portId, &jackPort);
    if (jackPort != NULL) {

        prjMidiOutPort p = prj->midiOutPort_getPort(portId);
        p.jackPort = jackPort;
        prj->midiOutPort_replace(portId, p);

        // Add to GUI
        gui_updatePatchMidiOutPortsMenu();  // Update combo box in patch view

        return portId;
    } else {
        // Could not create jack port. Remove port from project again.
        userMessage("ERROR: Could not add midi output port. Failed to create Jack port.");
        prj->midiOutPort_removePort(portId);
        return -1;
    }
}

void MainWindow::on_actionAdd_MIDI_Out_Port_triggered()
{
    addMidiOutPort();
    showConnectionsPage();
}



/* An audio input port in the project consists of a left and right Jack port.
 * This function adds the left and right Jack audio input ports, named according
 * to the given port number. The resulting Jack port numbers are assigned to the
 * leftPort and rightPort function parameters. */
void MainWindow::addAudioInPortsToJack(int portNo, konfytJackPort **leftPort, konfytJackPort **rightPort)
{
    *leftPort = jack->addPort(KonfytJackPortType_AudioIn, "audio_in_" + n2s(portNo) + "_L");
    *rightPort = jack->addPort(KonfytJackPortType_AudioIn, "audio_in_" + n2s(portNo) + "_R");
}

void MainWindow::addMidiOutPortToJack(int portId, konfytJackPort **jackPort)
{
    *jackPort = jack->addPort(KonfytJackPortType_MidiOut, "midi_out_" + n2s(portId));
}


void MainWindow::on_pushButton_connectionsPage_OK_clicked()
{
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
}

void MainWindow::on_pushButton_ShowConnections_clicked()
{
    if (ui->stackedWidget->currentIndex() == STACKED_WIDGET_PAGE_CONNECTIONS) {
        ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
    } else {
        showConnectionsPage();
    }
}

void MainWindow::on_tree_portsBusses_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    gui_updateConnectionsTree();
}

/* Remove the bus/port selected in the connections ports/busses tree widget. */
void MainWindow::on_actionRemove_BusPort_triggered()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    QTreeWidgetItem* item = portsBussesTreeMenuItem;

    bool busSelected = item->parent() == busParent;
    bool audioInSelected = item->parent() == audioInParent;
    bool midiOutSelected = item->parent() == midiOutParent;

    int id = 0;
    QString name;
    prjAudioBus bus;
    prjAudioInPort audioInPort;
    prjMidiOutPort midiOutPort;

    if (busSelected) {
        if (prj->audioBus_count() == 1) { return; } // Do not remove last bus
        id = tree_busMap.value(item);
        bus = prj->audioBus_getBus(id);
        name = bus.busName;
    } else if (audioInSelected) {
        id = tree_audioInMap.value(item);
        audioInPort = prj->audioInPort_getPort(id);
        name = audioInPort.portName;
    } else if (midiOutSelected) {
        id = tree_midiOutMap.value(item);
        midiOutPort = prj->midiOutPort_getPort(id);
        name = midiOutPort.portName;
    }

    // Check if any patch layers are using this bus/port
    QList<int> usingPatches;
    QList<int> usingLayers;
    QList<konfytPatch*> patchList = prj->getPatchList();
    for (int i=0; i<patchList.count(); i++) {
        konfytPatch* patch = patchList[i];
        QList<konfytPatchLayer> layerList = patch->getLayerItems();
        for (int j=0; j<layerList.count(); j++) {
            konfytPatchLayer layer = layerList[j];
            bool append = false;
            if (busSelected) {
                if ( (layer.getLayerType() == KonfytLayerType_AudioIn)
                     || ( layer.getLayerType() == KonfytLayerType_CarlaPlugin)
                     || ( layer.getLayerType() == KonfytLayerType_SoundfontProgram) ) {
                    append = layer.busIdInProject == id;
                }
            }
            if (audioInSelected) {
                if ( layer.getLayerType() == KonfytLayerType_AudioIn ) {
                append = layer.audioInPortData.portIdInProject == id;
                }
            }
            if (midiOutSelected) {
                if ( layer.getLayerType() == KonfytLayerType_MidiOut ) {
                append = layer.midiOutputPortData.portIdInProject == id;
                }
            }
            if (append) {
                usingPatches.append(i);
                usingLayers.append(j);
            }
        }
    }

    if (usingPatches.count()) {
        // Some patches have layers that use the selected bus/port. Confirm with the user.
        QMessageBox msgbox;
        QString detailedText;
        for (int i=0; i<usingPatches.count(); i++) {
            detailedText.append("Patch " + n2s(usingPatches[i]+1) + " layer " + n2s(usingLayers[i]+1) + "\n");
        }
        QString selectedText = "(" + n2s(id) + " - " + name + ")";
        msgbox.setDetailedText( detailedText );
        msgbox.setIcon(QMessageBox::Question);
        msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        // ------------------------------------------------------------------------------
        if (busSelected) {
            int busToChangeTo = prj->audioBus_getFirstBusId(id);
            msgbox.setText("The selected bus " + selectedText + " is used by some patches."
                           + " Are you sure you want to delete the bus?"
                           + " All layers using this bus will be assigned to bus "
                           + n2s(busToChangeTo) + " - "
                           + prj->audioBus_getBus(busToChangeTo).busName + ".");
            int ret = msgbox.exec();
            if (ret == QMessageBox::Yes) {
                // User chose to remove bus
                // Set the bus for all layers still using this one to zero
                for (int i=0; i<usingPatches.count(); i++) {
                    konfytPatch* patch = prj->getPatch(usingPatches[i]);
                    konfytPatchLayer layer = patch->getLayerItems()[usingLayers[i]];
                    pengine->setLayerBus( patch, &layer, busToChangeTo );
                }
                // Removal will be done below.

            } else { return; } // Do not remove bus
        // ------------------------------------------------------------------------------
        } else if (audioInSelected) {
            msgbox.setText("The selected port " + selectedText + "is used by some patches."
                           + " Are you sure you want to delete the port?"
                           + " The port layer will be removed from the patches.");
            int ret = msgbox.exec();
            if (ret == QMessageBox::Yes) {
                // User chose to remove port.
                // Remove it from all patches where it was in use
                for (int i=0; i<usingPatches.count(); i++) {
                    konfytPatch* patch = prj->getPatch(usingPatches[i]);
                    konfytPatchLayer layer = patch->getLayerItems()[usingLayers[i]];
                    pengine->removeLayer( patch, &layer );
                }
                // Removal will be done below

            } else { return; } // Do not remove port
        // ------------------------------------------------------------------------------
        } else if (midiOutSelected) {
            msgbox.setText("The selected port " + selectedText + " is used by some patches."
                           + " Are you sure you want to delete the port?"
                           + " The port layer will be removed from the patches.");
            int ret = msgbox.exec();
            if (ret == QMessageBox::Yes) {
                // User chose to remove port.
                // Remove it from all patches where it was in use
                for (int i=0; i<usingPatches.count(); i++) {
                    konfytPatch* patch = prj->getPatch(usingPatches[i]);
                    konfytPatchLayer layer = patch->getLayerItems()[usingLayers[i]];
                    pengine->removeLayer( patch, &layer );
                }
                // Removal will be done below

            } else { return; } // Do not remove port
        }
    } // end usingPatches.count()

    // Remove the bus/port
    if (busSelected) {
        // Remove the bus
        jack->removePort(KonfytJackPortType_AudioOut, bus.leftJackPort);
        jack->removePort(KonfytJackPortType_AudioOut, bus.rightJackPort);
        prj->audioBus_remove(id);
        tree_busMap.remove(item);
    }
    else if (audioInSelected) {
        // Remove the port
        jack->removePort(KonfytJackPortType_AudioIn, audioInPort.leftJackPort);
        jack->removePort(KonfytJackPortType_AudioIn, audioInPort.rightJackPort);
        prj->audioInPort_remove(id);
        tree_audioInMap.remove(item);
        gui_updatePatchAudioInPortsMenu();
    }
    else if (midiOutSelected) {
        // Remove the port
        jack->removePort(KonfytJackPortType_MidiOut, midiOutPort.jackPort);
        prj->midiOutPort_removePort(id);
        tree_midiOutMap.remove(item);
        gui_updatePatchMidiOutPortsMenu();
    }

    delete item;
    gui_updatePatchView();
}


/* Prepare and show the filesystem tree view context menu. */
void MainWindow::on_treeWidget_filesystem_customContextMenuRequested(const QPoint &pos)
{
    fsViewMenu.clear();

    QList<QAction*> actions;
    actions.append( ui->actionAdd_Path_To_External_App_Box );
    actions.append( ui->actionOpen_In_File_Manager_fsview );
    fsViewMenu.addActions(actions);

    fsViewMenuItem = ui->treeWidget_filesystem->itemAt(pos);

    fsViewMenu.popup(QCursor::pos());
}

void MainWindow::on_actionAdd_Path_To_External_App_Box_triggered()
{
    QString path;

    if (fsViewMenuItem == NULL) {
        // No item is selected in the filesystem list. Use current path
        path = fsview_currentPath;
    } else {
        // Get item's path
        QFileInfo info = fsMap.value(fsViewMenuItem);
        path = info.filePath();
    }

    // Replace project path with variable, if present
    konfytProject* prj = getCurrentProject();
    if (prj != NULL) {
        QString projPath = prj->getDirname();
        if (projPath.length() > 0) {
            path.replace(projPath, STRING_PROJECT_DIR);
        }
    }

    // Add quotes
    path = "\"" + path + "\"";

    ui->lineEdit_ExtApp->setText( ui->lineEdit_ExtApp->text()
                                  + path);
    ui->lineEdit_ExtApp->setFocus();
}

void MainWindow::on_toolButton_filesystem_projectDir_clicked()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }
    if (prj->getDirname().length() == 0) { return; }

    cdFilesystemView( prj->getDirname() );
}

/* Action triggered from filesystem tree view to open item in file manager. */
void MainWindow::on_actionOpen_In_File_Manager_fsview_triggered()
{
    QString path;

    if (fsViewMenuItem == NULL) {
        // No item is selected. Use current path
        path = fsview_currentPath;
    } else {
        // Get item path
        QFileInfo info = fsMap.value(fsViewMenuItem);
        if (info.isDir()) {
            path = info.filePath();
        } else {
            path = info.path();
        }
    }

    openFileManager(path);
}

void MainWindow::filemanProcessSignalMapperSlot(QObject *object)
{
    // We should probabaly destroy the qprocess here.
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Go through list of projects and ask to save for modified projects.

    for (int i=0; i<this->projectList.count(); i++) {
        konfytProject* prj = projectList[i];
        if (prj->isModified()) {
            QMessageBox msgbox;
            msgbox.setText("Do you want to save the changes to project " + prj->getProjectName() + "?");
            msgbox.setIcon(QMessageBox::Question);
            msgbox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes | QMessageBox::No);
            msgbox.setDefaultButton(QMessageBox::Cancel);
            int ret = msgbox.exec();
            if (ret == QMessageBox::Yes) {
                if ( saveProject(prj) == false ) {
                    event->ignore();
                    return;
                }
            } else if (ret == QMessageBox::Cancel) {
                event->ignore();
                return;
            }
        }
    }

    konfytProject* prj = getCurrentProject();


    event->accept();
}

/* Context menu requested for a library tree item. */
void MainWindow::on_treeWidget_customContextMenuRequested(const QPoint &pos)
{
    libraryMenuItem = ui->treeWidget->itemAt(pos);
    libraryTreeItemType itemType = library_getTreeItemType( libraryMenuItem );

    libraryMenu.clear();

    QList<QAction*> actions;

    actions.append( ui->actionOpen_In_File_Manager_library );
    // Disable menu item if no applicable tree widget item is selected
    ui->actionOpen_In_File_Manager_library->setEnabled( itemType != libTreeInvalid );

    libraryMenu.addActions(actions);

    libraryMenu.popup(QCursor::pos());
}


/* Action triggered from library tree view to open item in file manager. */
void MainWindow::on_actionOpen_In_File_Manager_library_triggered()
{
    if (libraryMenuItem == NULL) { return; }

    QString path;

    libraryTreeItemType itemType = library_getTreeItemType( libraryMenuItem );

    if ( itemType == libTreeSoundfontRoot ) { path = this->soundfontsDir; }
    else if ( itemType == libTreePatchesRoot ) { path = this->patchesDir; }
    else if ( itemType == libTreeSFZRoot) { path = this->sfzDir; }
    else if ( itemType == libTreeSoundfontFolder ) {
        path = library_sfFolders.value( libraryMenuItem );
    }
    else if ( itemType == libTreeSoundfont ) {
        path = library_sfMap.value(libraryMenuItem)->filename;
    }
    else if ( itemType == libTreeSFZFolder ) {
        path = library_sfzFolders.value( libraryMenuItem );
    }
    else if ( itemType == libTreeSFZ ) {
        path = library_sfzMap.value(libraryMenuItem);
    }
    else if ( itemType == libTreePatch ) {
        path = this->patchesDir;
    } else {
        return;
    }

    // If a file is selected, change path to the folder name of the file
    QFileInfo info(path);
    if (!info.isDir()) { path = info.path(); }

    openFileManager(path);
}

void MainWindow::openFileManager(QString path)
{
    if (this->filemanager.length()) {
        QProcess* process = new QProcess();
        filemanProcessSignalMapper.setMapping(process, process);
        connect( process, SIGNAL(finished(int)), &filemanProcessSignalMapper, SLOT(map()) );
        process->start( this->filemanager, QStringList() << path );
    } else {
        QDesktopServices::openUrl( path );
    }
}

void MainWindow::on_actionRename_BusPort_triggered()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    QTreeWidgetItem* item = portsBussesTreeMenuItem;

    // See on_tree_portsBusses_itemChanged for the renaming.

    if (item->parent() == busParent) {
        // Bus selected

        ui->tree_portsBusses->editItem(item, 0);

    } else if (item->parent() == audioInParent) {
        // Audio input port selected

        ui->tree_portsBusses->editItem(item, 0);

    } else if (item->parent() == midiOutParent) {
        // MIDI Output port selected

        ui->tree_portsBusses->editItem(item, 0);

    }
}

/* User has renamed a port or bus. */
void MainWindow::on_tree_portsBusses_itemChanged(QTreeWidgetItem *item, int column)
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    if (item->parent() == busParent) {
        // Bus is selected
        int id = tree_busMap.value( item );
        prjAudioBus bus = prj->audioBus_getBus(id);

        bus.busName = item->text(0);
        prj->audioBus_replace(id, bus);

    } else if (item->parent() == audioInParent) {
        // Audio input port selected
        int id = tree_audioInMap.value(item);
        prjAudioInPort p = prj->audioInPort_getPort(id);

        p.portName = item->text(0);
        prj->audioInPort_replace(id, p);

    } else if (item->parent() == midiOutParent) {
        // MIDI Output port selected
        int id = tree_midiOutMap.value(item);
        prjMidiOutPort p = prj->midiOutPort_getPort(id);

        p.portName = item->text(0);
        prj->midiOutPort_replace(id, p);

    }

    gui_updatePatchAudioInPortsMenu();
    gui_updatePatchMidiOutPortsMenu();

}



void MainWindow::on_pushButton_ShowTriggersPage_clicked()
{
    if (ui->stackedWidget->currentIndex() == STACKED_WIDGET_PAGE_TRIGGERS) {
        ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
    } else {
        showTriggersPage();
    }
}

void MainWindow::on_pushButton_triggersPage_OK_clicked()
{
    ui->stackedWidget->setCurrentIndex(STACKED_WIDGET_PAGE_PATCHES);
}

void MainWindow::on_pushButton_triggersPage_assign_clicked()
{
    QTreeWidgetItem* item = ui->tree_Triggers->currentItem();
    if (item == NULL) { return; }

    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    int eventRow = ui->listWidget_triggers_eventList->currentRow();
    if (eventRow < 0) { return; }

    konfytMidiEvent selectedEvent = triggersLastEvents[eventRow];
    QAction* action = triggersItemActionHash[item];
    konfytTrigger trig = konfytTrigger();

    trig.actionText = action->text();
    trig.bankLSB = selectedEvent.bankLSB;
    trig.bankMSB = selectedEvent.bankMSB;
    trig.channel = selectedEvent.channel;
    trig.data1 = selectedEvent.data1;
    trig.type = selectedEvent.type;

    // Add to project
    prj->addAndReplaceTrigger(trig);
    // Add to quick-lookup-hash, which is used when events are received.
    // (First remove if it already contains the action)
    QList<QAction*> l = triggersMidiActionHash.values();
    if (l.contains(action)) {
        triggersMidiActionHash.remove( triggersMidiActionHash.key(action) );
    }
    triggersMidiActionHash.insert( trig.toInt(), action );
    // Refresh the page
    showTriggersPage();
}

void MainWindow::on_pushButton_triggersPage_clear_clicked()
{
    QTreeWidgetItem* item = ui->tree_Triggers->currentItem();
    if (item == NULL) { return; }

    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    QAction* action = triggersItemActionHash[item];

    // remove from project
    prj->removeTrigger(action->text());

    // Remove action from the quick lookup hash
    int key = triggersMidiActionHash.key(action);
    triggersMidiActionHash.remove(key);

    // Refresh the page
    showTriggersPage();
}

void MainWindow::on_tree_Triggers_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    on_pushButton_triggersPage_assign_clicked();
}





void MainWindow::on_checkBox_ConsoleShowMidiMessages_clicked()
{
    setConsoleShowMidiMessages( ui->checkBox_ConsoleShowMidiMessages->isChecked() );
}

void MainWindow::setConsoleShowMidiMessages(bool show)
{
    ui->checkBox_ConsoleShowMidiMessages->setChecked( show );
    consoleDiag->setShowMidiEvents( show );
    console_showMidiMessages = show;
}

void MainWindow::on_pushButton_RestartApp_clicked()
{
    // Restart the app (see code in main.cpp)
    QCoreApplication::exit(APP_RESTART_CODE);
}

void MainWindow::on_actionProject_save_triggered()
{
    // Save project
    saveCurrentProject();
}

void MainWindow::on_actionProject_New_triggered()
{
    newProject();
    // Switch to newly created project
    ui->tabWidget_Projects->setCurrentIndex(ui->tabWidget_Projects->count()-1);
}

void MainWindow::on_actionProject_Open_triggered()
{
    // Show open dialog box
    QFileDialog* d = new QFileDialog();
    QString filename = d->getOpenFileName(this,"Select project to open",projectsDir,"*.sfproject");
    if (filename == "") {
        userMessage("Cancelled.");
        return;
    }

    openProject(filename);
    // Switch to newly opened project
    ui->tabWidget_Projects->setCurrentIndex(ui->tabWidget_Projects->count()-1);
}

void MainWindow::on_actionProject_OpenDirectory_triggered()
{
    openFileManager( projectsDir );
}



void MainWindow::on_textBrowser_patchNote_textChanged()
{
    if (patchNote_ignoreChange) {
        // Change is due to contents being set by program
        patchNote_ignoreChange = false;
    } else {
        // Change is due to user typing in box
        pengine->setPatchNote(ui->textBrowser_patchNote->toPlainText());
        setPatchModified(true);
    }
}

void MainWindow::on_toolButton_Project_clicked()
{
    saveCurrentProject();
}

void MainWindow::on_actionProject_SaveAs_triggered()
{
    messageBox("Save As not implemented yet.");
}

void MainWindow::on_pushButton_Panic_clicked()
{
    ui->actionPanic->trigger();
}

void MainWindow::on_actionPanic_triggered()
{
    // Toggle panic state
    panicState = !panicState;

    pengine->panic( panicState );

    // Update GUI
    ui->pushButton_Panic->setChecked(panicState);
}

void MainWindow::on_pushButton_ThreadTest_clicked()
{
    static bool firstRun = true;
    if (firstRun) {
        connect(&sfLoader, SIGNAL(finished()), this, SLOT(scanThreadFihishedSlot()));
        connect(&sfLoader, SIGNAL(userMessage(QString)), this, SLOT(userMessage(QString)));
        firstRun = false;
    }

    fluid_settings_t* settings = new_fluid_settings();
    fluid_synth_t* synth = new_fluid_synth(settings);
    sfLoader.loadSoundfont(synth, "/home/gideon/Data/Sounds/Soundfonts/GM/Musyng Kite/Musyng Kite.sf2");
    //sfLoader.loadSoundfont(synth, "/home/gideon/VMShared/Musyng Kite.sf2");
    //sfLoader.loadSoundfont(synth, "/home/gideon/VMShared/splendid_72m.sf2");

    /*
    scanThread = new scanFoldersThread();
    scanThread->filename = "/home/gideon/VMShared/Musyng Kite.sf2";
    scanThread->synth = synth;
    connect(scanThread, SIGNAL(userMessage(QString)), this, SLOT(userMessage(QString)));
    connect(scanThread,SIGNAL(finished()), this, SLOT(scanThreadFihishedSlot()));
    userMessage("Starting ScanThread...");
    scanThread->start();
    */

}

void MainWindow::on_pushButton_LoadAll_clicked()
{
    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    int startPatch = currentPatchIndex;

    for (int i=0; i<prj->getNumPatches(); i++) {
        setCurrentPatch(i);
    }

    setCurrentPatch(startPatch);
}



void MainWindow::on_pushButton_ExtApp_Replace_clicked()
{
    int row = ui->listWidget_ExtApps->currentRow();
    if (row < 0) { return; }

    konfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    konfytProcess* process = prj->getProcessList()[row];
    process->appname = ui->lineEdit_ExtApp->text();

    QListWidgetItem* item = ui->listWidget_ExtApps->item(row);
    item->setText(ui->lineEdit_ExtApp->text());

    setProjectModified();
}

void MainWindow::on_MIDI_indicator_clicked()
{
    ui->MIDI_indicator->setChecked(false);
}

void MainWindow::on_toolButton_MidiFilter_inChan_last_clicked()
{
    ui->comboBox_midiFilter_inChannel->setCurrentIndex( midiFilter_lastChan+1 );
}

void MainWindow::setMasterInTranspose(int transpose, bool relative)
{
    if (relative) {
        transpose += ui->spinBox_MasterIn_Transpose->value();
    }
    ui->spinBox_MasterIn_Transpose->setValue( transpose );
}

void MainWindow::on_spinBox_MasterIn_Transpose_valueChanged(int arg1)
{
    this->jack->setGlobalTranspose(arg1);
}

void MainWindow::on_pushButton_MasterIn_TransposeSub12_clicked()
{
    setMasterInTranspose(-12,true);
}

void MainWindow::on_pushButton_MasterIn_TransposeAdd12_clicked()
{
    setMasterInTranspose(12,true);
}

void MainWindow::on_pushButton_MasterIn_TransposeZero_clicked()
{
    setMasterInTranspose(0,false);
}
