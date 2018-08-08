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


MainWindow::MainWindow(QWidget *parent, QApplication* application, QStringList filesToLoad, QString jackClientName) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialise variables

    app = application;
    currentProject = -1;
    panicState = false;
    masterPatch = NULL;
    previewPatch = NULL;
    previewMode = false;
    patchNote_ignoreChange = false;
    jackPage_audio = true;

    midiFilter_lastChan = 0;
    midiFilter_lastData1 = 0;
    midiFilter_lastData2 = 0;

    lastBankSelectMSB = -1;
    lastBankSelectLSB = -1;

    // Initialise console dialog
    this->consoleDiag = new ConsoleDialog(this);

    userMessage(QString(APP_NAME) + " " + APP_VERSION);

    // Initialise About Dialog
    initAboutDialog();

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
        // If settings file does not exist, it's probably the first run. Show about dialog.
        showAboutDialog();
    }

    // ----------------------------------------------------
    // Initialise jack client
    // ----------------------------------------------------

    jack = new KonfytJackEngine();

    connect(jack, &KonfytJackEngine::userMessage, this, &MainWindow::userMessage);

    connect(jack, &KonfytJackEngine::jackPortRegisterOrConnectCallback,
            this, &MainWindow::jackPortRegisterOrConnectCallback);

    qRegisterMetaType<KonfytMidiEvent>("KonfytMidiEvent"); // To be able to use konfytMidiEvent in the Qt signal/slot system
    connect(jack, &KonfytJackEngine::midiEventSignal, this, &MainWindow::midiEventSlot);

    connect(jack, &KonfytJackEngine::xrunSignal, this, &MainWindow::jackXrun);

    if (jackClientName.isEmpty()) {
        jackClientName = KONFYT_JACK_DEFAULT_CLIENT_NAME;
    }
    if ( jack->InitJackClient(jackClientName) ) {
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

    connect(pengine, &konfytPatchEngine::userMessage, this, &MainWindow::userMessage);

    pengine->initPatchEngine(this->jack);
    this->masterGain = 0.8;
    pengine->setMasterGain(masterGain);
    this->previewGain = 0.8;


    // ----------------------------------------------------
    // Set up gui stuff that needs to happen before loading project or commandline arguments
    // ----------------------------------------------------

    // Triggers Page (must happen before setting project)
    initTriggers();

    // Library filesystem view
    this->fsview_currentPath = QDir::homePath();
    refreshFilesystemView();
    ui->tabWidget_library->setCurrentIndex(LIBRARY_TAB_LIBRARY);

    // ----------------------------------------------------
    // Initialise soundfont database
    // ----------------------------------------------------

    connect(&db, &konfytDatabase::userMessage, this, &MainWindow::userMessage);

    connect(&db, &konfytDatabase::scanDirs_finished,
            this, &MainWindow::database_scanDirsFinished);

    connect(&db, &konfytDatabase::scanDirs_status,
            this, &MainWindow::database_scanDirsStatus);

    connect(&db, &konfytDatabase::returnSfont_finished,
            this, &MainWindow::database_returnSfont);

    // Check if database file exists. Otherwise, scan directories.
    if (db.loadDatabaseFromFile(settingsDir + "/" + DATABASE_FILE)) {
        userMessage("Database loaded from file. Rescan to refresh.");
        userMessage("Database contains:");
        userMessage("   " + n2s(db.getNumSfonts()) + " soundfonts.");
        userMessage("   " + n2s(db.getNumPatches()) + " patches.");
        userMessage("   " + n2s(db.getNumSfz()) + " sfz/gig samples.");
    } else {
        // Scan for soundfonts
        userMessage("No database file found.");
        userMessage("You can scan directories to create a database from Settings.");
    }

    fillTreeWithAll(); // Fill the tree widget with all the database entries


    // ----------------------------------------------------
    // Projects / commandline arguments
    // ----------------------------------------------------

    // Tab widget has some tabs for design purposes. Remove all.
    ui->tabWidget_Projects->blockSignals(true);
    ui->tabWidget_Projects->clear();
    ui->tabWidget_Projects->blockSignals(true);
    // Scan projectsDir for projects. If none found, create a new project and empty patch.
    if ( !scanDirForProjects(projectsDir) ) {
        userMessage("No project directory " + projectsDir);
    }
    // Load project if one was passed as an argument
    for (int i=0; i<filesToLoad.count(); i++) {
        QString file = filesToLoad[i];
        if ( fileIsPatch(file) || fileIsSfzOrGig(file) || fileIsSoundfont(file) ) {
            // If no project loaded, create a new project
            if (projectList.count() == 0) {
                userMessage("Creating default new project to load " + file);
                newProject();           // Create new project and add to list and GUI
                setCurrentProject(0);   // Set current project to newly created project.
            }
            KonfytProject *prj = getCurrentProject();
            if (fileIsPatch(file)) {
                // Load patch into current project and switch to patch

                konfytPatch* pt = new konfytPatch();
                if (pt->loadPatchFromFile(file)) {
                    addPatchToProject(pt);
                    setCurrentPatch(prj->getNumPatches()-1);
                } else {
                    userMessage("Failed loading patch " + file);
                    delete pt;
                }
                // Locate in filesystem view
                ui->tabWidget_library->setCurrentIndex(LIBRARY_TAB_FILESYSTEM);
                cdFilesystemView(file);
                selectItemInFilesystemView(file);

            } else if (fileIsSfzOrGig(file)) {
                // Create new patch and load sfz into patch
                newPatchToProject();    // Create a new patch and add to current project.
                setCurrentPatch(prj->getNumPatches()-1);

                addSfzToCurrentPatch(file);
                // Rename patch
                ui->lineEdit_PatchName->setText( getBaseNameWithoutExtension(file) );
                on_lineEdit_PatchName_editingFinished();

                // Locate in filesystem view
                ui->tabWidget_library->setCurrentIndex(LIBRARY_TAB_FILESYSTEM);
                cdFilesystemView(file);
                selectItemInFilesystemView(file);

            } else if (fileIsSoundfont(file)) {
                // Create new blank patch
                newPatchToProject();    // Create a new patch and add to current project.
                setCurrentPatch(prj->getNumPatches()-1);
                // Locate soundfont in filebrowser, select it and show its programs

                // Locate in filesystem view
                ui->tabWidget_library->setCurrentIndex(LIBRARY_TAB_FILESYSTEM);
                cdFilesystemView(file);
                selectItemInFilesystemView(file);
                // Load from filesystem view
                on_treeWidget_filesystem_itemDoubleClicked( ui->treeWidget_filesystem->currentItem(), 0 );
                // Add first program to current patch
                if (ui->listWidget_LibraryBottom->count()) {
                    ui->listWidget_LibraryBottom->setCurrentRow(0);
                    addProgramToCurrentPatch( library_getSelectedProgram() );
                }

                // Rename patch
                ui->lineEdit_PatchName->setText( getBaseNameWithoutExtension(file) );
                on_lineEdit_PatchName_editingFinished();

            }
        } else {
            // Try to load project
            userMessage("Opening project " + file);
            if (openProject(file)) {
                userMessage("Project loaded from argument.");
                setCurrentProject( -1 );
                startupProject = false;
            } else {
                userMessage("Failed to load project from argument.");
            }
        }
    }
    // If no project is loaded yet, create default project
    if (projectList.count() == 0) {
        userMessage("Creating default new project.");
        newProject();           // Create new project and add to list and GUI
        setCurrentProject(0);   // Set current project to newly created project.
        newPatchToProject();    // Create a new patch and add to current project.
        setCurrentPatch(0);
        startupProject = true;
        KonfytProject *prj = getCurrentProject();
        prj->setModified(false);
    }


    // ----------------------------------------------------
    // Initialise and update GUI
    // ----------------------------------------------------

    // Global Transpose
    ui->spinBox_MasterIn_Transpose->setValue(0);

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
    connect(&projectsMenu, &QMenu::triggered,
            this, &MainWindow::onprojectMenu_ActionTrigger);
    projectButtonMenu->addMenu(&projectsMenu);
    projectButtonMenu->addAction(ui->actionProject_New);
    //projectButtonMenu->addAction(ui->actionProject_SaveAs); // TODO: SaveAs menu entry disabled for now until it is implemented.
    ui->toolButton_Project->setMenu(projectButtonMenu);


    // Add-midi-port-to-patch button
    connect(&patchMidiOutPortsMenu, &QMenu::triggered,
            this, &MainWindow::onPatchMidiOutPortsMenu_ActionTrigger);
    ui->toolButton_layer_AddMidiPort->setMenu(&patchMidiOutPortsMenu);

    // Button: add audio input port to patch
    connect(&patchAudioInPortsMenu, &QMenu::triggered,
            this, &MainWindow::onPatchAudioInPortsMenu_ActionTrigger);
    ui->toolButton_layer_AddAudioInput->setMenu(&patchAudioInPortsMenu);

    // Layer bus menu
    connect(&layerBusMenu, &QMenu::triggered,
            this, &MainWindow::onLayerBusMenu_ActionTrigger);

    // Layer MIDI output channel menu
    connect(&layerMidiOutChannelMenu, &QMenu::triggered,
            this, &MainWindow::onLayerMidiOutChannelMenu_ActionTrigger);

    // Layer MIDI input port menu
    connect(&layerMidiInPortsMenu, &QMenu::triggered,
            this, &MainWindow::onLayerMidiInMenu_ActionTrigger);

    // If one of the paths are not set, show settings. Else, switch to patch view.
    if ( (projectsDir == "") || (patchesDir == "") || (soundfontsDir == "") || (sfzDir == "") ) {
        showSettingsDialog();
    } else {
        ui->stackedWidget->setCurrentWidget(ui->PatchPage);
    }
    currentPatchIndex = -1;

    // Console
    console_showMidiMessages = false;

    // Connections page
    busParent = NULL; // Helps later on when deleting items.
    audioInParent = NULL;
    midiOutParent = NULL;

    // Set up portsBusses tree context menu
    ui->tree_portsBusses->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_portsBusses, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::tree_portsBusses_Menu);

    // Resize some layouts
    QList<int> sizes;
    sizes << 8 << 2;
    ui->splitter_library->setSizes( sizes );

    // Right Sidebar
    // Hide the bottom tabs used for experimentation
    ui->tabWidget_right->tabBar()->setVisible(false);
    ui->tabWidget_right->setCurrentIndex(0);

    // Set up keyboard shortcuts
    shortcut_save = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(shortcut_save, &QShortcut::activated,
            this, &MainWindow::shortcut_save_activated);

    shortcut_panic = new QShortcut(QKeySequence("Ctrl+P"), this);
    connect(shortcut_panic, &QShortcut::activated,
            this, &MainWindow::shortcut_panic_activated);
    ui->pushButton_Panic->setToolTip( ui->pushButton_Panic->toolTip() + " [" + shortcut_panic->key().toString() + "]");

    // Set up external apps combo box
    setupExtAppMenu();

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
    ui->actionPanicToggle->trigger();
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
        setCurrentProject( -1 );
    }
}


void MainWindow::jackXrun()
{
    static int count = 1;
    userMessage("XRUN " + n2s(count++));
}

void MainWindow::jackPortRegisterOrConnectCallback()
{
    // Refresh ports/connections tree
    //gui_updateConnectionsTree(); // Disabled for now to prevent connections list from scrolling to top
                                   // when user ticks checkbox; TODO Fix

    // Update warnings section
    updateGUIWarnings();
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

    int i = ui->comboBox_Settings_filemanager->findText( this->filemanager );
    if (i>=0) {
        ui->comboBox_Settings_filemanager->setCurrentIndex(i);
    } else {
        ui->comboBox_Settings_filemanager->addItem(this->filemanager);
        ui->comboBox_Settings_filemanager->setCurrentIndex( ui->comboBox_Settings_filemanager->count()-1 );
    }

    // Switch to settings page
    ui->stackedWidget->setCurrentWidget(ui->SettingsPage);
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

    KonfytMidiFilter f;
    if (midiFilterEditType == MidiFilterEditPort) {
        KonfytProject* prj = getCurrentProject();
        if (prj == NULL) { return; }
        f = prj->midiInPort_getPort(midiFilterEditPort).filter;
    } else {
        KonfytPatchLayer g = midiFilterEditItem->getPatchLayerItem();
        f = g.getMidiFilter();
    }

    KonfytMidiFilterZone z = f.zone;
    ui->spinBox_midiFilter_LowNote->setValue(z.lowNote);
    ui->spinBox_midiFilter_HighNote->setValue(z.highNote);
    ui->spinBox_midiFilter_Add->setValue(z.add);
    ui->spinBox_midiFilter_LowVel->setValue(z.lowVel);
    ui->spinBox_midiFilter_HighVel->setValue(z.highVel);
    ui->spinBox_midiFilter_VelLimitMin->setValue(z.velLimitMin);
    ui->spinBox_midiFilter_VelLimitMax->setValue(z.velLimitMax);
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
    ui->stackedWidget->setCurrentWidget(ui->FilterPage);
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
    filemanager = ui->comboBox_Settings_filemanager->currentText();

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

    while (r.readNextStartElement()) { // Settings

        if (r.name() == XML_SETTINGS) {

            while (r.readNextStartElement()) {

                if (r.name() == XML_SETTINGS_PRJDIR) {
                    projectsDir = r.readElementText();
                } else if (r.name() == XML_SETTINGS_SFDIR) {
                    soundfontsDir = r.readElementText();
                } else if (r.name() == XML_SETTINGS_PATCHESDIR) {
                    patchesDir = r.readElementText();
                } else if (r.name() == XML_SETTINGS_SFZDIR) {
                    sfzDir = r.readElementText();
                } else if (r.name() == XML_SETTINGS_FILEMAN) {
                    filemanager = r.readElementText();
                }

            }

        } else {
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

    stream.writeComment("This is a Konfyt settings file.");

    stream.writeStartElement(XML_SETTINGS);

    // Settings
    stream.writeTextElement(XML_SETTINGS_PRJDIR, projectsDir);
    stream.writeTextElement(XML_SETTINGS_SFDIR, soundfontsDir);
    stream.writeTextElement(XML_SETTINGS_PATCHESDIR, patchesDir);
    stream.writeTextElement(XML_SETTINGS_SFZDIR, sfzDir);
    stream.writeTextElement(XML_SETTINGS_FILEMAN, filemanager);

    stream.writeEndElement(); // Settings

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
    KonfytProject* prj = new KonfytProject();
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
    KonfytProject* prj = new KonfytProject();
    connect(prj, &KonfytProject::userMessage, this, &MainWindow::userMessage);

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
void MainWindow::addProject(KonfytProject* prj)
{
    // If startupProject is true, a default created project exists.
    // If this project has not been modified, remove it.
    if (startupProject) {
        KonfytProject* prj = getCurrentProject();
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

    connect(prj, &KonfytProject::userMessage, this, &MainWindow::userMessage);
    projectList.append(prj);
    // Add to tabs
    QLabel* lbl = new QLabel();
    ui->tabWidget_Projects->blockSignals(true);
    ui->tabWidget_Projects->addTab(lbl,prj->getProjectName());
    ui->tabWidget_Projects->blockSignals(false);
}

/* Update the patch list according to the current project. */
void MainWindow::gui_updatePatchList()
{
    ui->listWidget_Patches->clear();

    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("gui_updatePatchList(): Current project is NULL");
        return;
    }

    // Populate patch list for current project
    QList<konfytPatch*> pl = prj->getPatchList();
    QString notenames = "CDEFGAB";
    for (int j=0; j<pl.count(); j++) {
        konfytPatch* pat = pl.at(j);

        // Add number and/or note name to patch name based on project settings.
        QString txt;
        if (prj->getShowPatchListNumbers()) { txt.append(n2s(j+1) + " "); }
        if (prj->getShowPatchListNotes()) { txt.append(QString(notenames.at(j%7)) + " "); }
        txt.append(pat->getName());
        QListWidgetItem* item = new QListWidgetItem(txt);
        // If patch has been loaded, mark it white. Else, gray.
        if (prj->isPatchLoaded(pat->id_in_project)) {
            item->setTextColor(Qt::white);
        } else {
            item->setTextColor(Qt::gray);
        }

        ui->listWidget_Patches->addItem(item);
    }

    setCurrentPatchIcon();


}


void MainWindow::showConnectionsPage()
{
    ui->stackedWidget->setCurrentWidget(ui->connectionsPage);
    ui->pushButton_connectionsPage_MidiFilter->setVisible(false);

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
    KonfytProject* prj = getCurrentProject();
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
     * Midi In Ports
     *  |__ port 1
     *  |__  :
     *  |__ port n
     *
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
        PrjAudioBus b = prj->audioBus_getBus(id);
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
        PrjAudioInPort p = prj->audioInPort_getPort(id);
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
        PrjMidiPort p = prj->midiOutPort_getPort(id);
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setText(0, p.portName);
        tree_midiOutMap.insert(item, id);
        midiOutParent->addChild(item);
    }

    midiInParent = new QTreeWidgetItem();
    midiInParent->setText(0, "MIDI Input Ports");
    QList<int> midiInIds = prj->midiInPort_getAllPortIds();
    for (int i=0; i < midiInIds.count(); i++) {
        int id = midiInIds[i];
        PrjMidiPort p = prj->midiInPort_getPort(id);
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setText(0, p.portName);
        tree_midiInMap.insert(item, id);
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
    KonfytProject* prj = getCurrentProject();
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
        j = tree_midiInMap.value( ui->tree_portsBusses->currentItem() );
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
        PrjAudioBus bus = prj->audioBus_getBus(j);
        leftCons = bus.leftOutClients;
        rightCons = bus.rightOutClients;

    } else if ( ui->tree_portsBusses->currentItem()->parent() == audioInParent ) {
        // An audio input port is selected
        PrjAudioInPort p = prj->audioInPort_getPort(j);
        leftCons = p.leftInClients;
        rightCons = p.rightInClients;

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiOutParent ) {
        // A midi output port is selected
        leftCons = prj->midiOutPort_getClients(j);

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiInParent ) {
        // Midi input is selected
        leftCons = prj->midiInPort_getClients(j);
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
    connect(cbl, &QCheckBox::clicked, [this, cbl](){ checkboxes_clicked_slot(cbl); });
    conChecksMap1.insert(cbl, portItem); // Map the checkbox to the tree item
    if ( ui->tree_portsBusses->currentItem()->parent() != midiOutParent ) { // Midi ports only have one checkbox
        if (ui->tree_portsBusses->currentItem()->parent() != midiInParent) {
            ui->tree_Connections->setItemWidget(portItem, TREECON_COL_R, cbr);
            connect(cbr, &QCheckBox::clicked, [this, cbr](){ checkboxes_clicked_slot(cbr); });
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

/* One of the connections page ports checkboxes have been clicked. */
void MainWindow::checkboxes_clicked_slot(QCheckBox *c)
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

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
        PrjAudioBus bus = prj->audioBus_getBus(busId);

        int jackPort;
        if (leftRight == leftPort) { jackPort = bus.leftJackPortId; }
        else { jackPort = bus.rightJackPortId; }

        if (c->isChecked()) {
            // Connect
            jack->addPortClient(jackPort, portString);
            // Also add in project
            prj->audioBus_addClient(busId, leftRight, portString);
        } else {
            // Disconnect
            jack->removePortClient_andDisconnect(jackPort, portString);
            // Also remove in project
            prj->audioBus_removeClient(busId, leftRight, portString);
        }


    } else if ( ui->tree_portsBusses->currentItem()->parent() == audioInParent ) {
        // An audio input port is selected
        int portId = tree_audioInMap.value( ui->tree_portsBusses->currentItem() );
        PrjAudioInPort p = prj->audioInPort_getPort(portId);

        int jackPort;
        if (leftRight == leftPort) { jackPort = p.leftJackPortId; }
        else { jackPort = p.rightJackPortId; }

        if (c->isChecked()) {
            // Connect
            jack->addPortClient(jackPort, portString);
            // Also add in project
            prj->audioInPort_addClient(portId, leftRight, portString);
        } else {
            // Disconnect
            jack->removePortClient_andDisconnect(jackPort, portString);
            // Also remove in project
            prj->audioInPort_removeClient(portId, leftRight, portString);
        }

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiOutParent ) {
        // A midi output port is selected
        int portId = tree_midiOutMap.value( ui->tree_portsBusses->currentItem() );
        PrjMidiPort p = prj->midiOutPort_getPort(portId);

        int jackPort = p.jackPortId;
        if (c->isChecked()) {
            // Connect
            jack->addPortClient(jackPort, portString);
            // Also add in project
            prj->midiOutPort_addClient(portId, portString);
        } else {
            // Disconnect
            jack->removePortClient_andDisconnect(jackPort, portString);
            // Also remove in project
            prj->midiOutPort_removeClient(portId, portString);
        }

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiInParent ) {
        // Midi input is selected
        int portId = tree_midiInMap.value( ui->tree_portsBusses->currentItem() );
        PrjMidiPort p = prj->midiInPort_getPort(portId);

        int jackPort = p.jackPortId;
        if (c->isChecked()) {
            // Connect
            jack->addPortClient(jackPort, portString);
            // Also add in project
            prj->midiInPort_addClient(portId, portString);
        } else {
            // Disconnect
            jack->removePortClient_andDisconnect(jackPort, portString);
            // Also remove in project
            prj->midiInPort_removeClient(portId, portString);
        }
    }

    updateGUIWarnings();
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
        if (item->parent() != NULL) {
            l.append(ui->actionRename_BusPort);
            l.append(ui->actionRemove_BusPort);
        }
    }
    l.append(ui->actionAdd_Bus);
    l.append(ui->actionAdd_Audio_In_Port);
    l.append(ui->actionAdd_MIDI_Out_Port);
    l.append(ui->actionAdd_MIDI_In_Port);
    portsBussesTreeMenu.addActions(l);
    portsBussesTreeMenu.popup(QCursor::pos());
}


void MainWindow::initTriggers()
{
    // Create a list of actions we will be adding to the triggers list
    QList<QAction*> l;
    l << ui->actionPanic
      << ui->actionPanicToggle
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
    ui->stackedWidget->setCurrentWidget(ui->triggersPage);

    ui->tree_Triggers->setColumnWidth(0, ui->tree_Triggers->width()/2);
    ui->tree_Triggers->setColumnWidth(1, ui->tree_Triggers->width()/2 - 16); // -16 is quick and dirty fix to accomodate scroll bar

    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    // Clear trigger text for whole gui list
    QList<QTreeWidgetItem*> items = triggersItemActionHash.keys();
    for (int i=0; i<items.count(); i++) {
        items[i]->setText(1,"");
    }

    // Get triggers from project and show in gui list
    QList<KonfytTrigger> l = prj->getTriggerList();
    for (int i=0; i<l.count(); i++) {
        for (int j=0; j<items.count(); j++) {
            if (triggersItemActionHash[items[j]]->text() == l[i].actionText) {
                QString text = l[i].toString();
                items[j]->setText(1, text);
            }
        }
    }

    // Program change text box
    ui->checkBox_Triggers_ProgSwitchPatches->setChecked( prj->isProgramChangeSwitchPatches() );

}


/* Set current project corresponding to specified index.
 * If index is -1, the last project in the list is loaded. */
void MainWindow::setCurrentProject(int i)
{
    if (i==-1) { i = projectList.count()-1; }
    if ( (i<0) || (i>=projectList.count()) ) {
        userMessage("DEBUG: SET_CURRENT_PROJECT: INVALID INDEX " + n2s(i));
        return;
    }

    // First, disconnect signals from current project.
    KonfytProject* oldprj = getCurrentProject();
    if (oldprj != NULL) {
        oldprj->disconnect();
    }
    // Set up the current project

    currentProject = i;
    KonfytProject* prj = projectList.at(i);
    pengine->setProject(prj); // patch engine needs a pointer to the current project for some stuff.

    ui->lineEdit_ProjectName->setText(prj->getProjectName());
    // Populate patch list for current project
    gui_updatePatchList();

    jack->pauseJackProcessing(true);

    // Process Midi in ports

    jack->removeAllMidiInPorts();

    QList<int> midiInIds = prj->midiInPort_getAllPortIds();
    for (int j=0; j < midiInIds.count(); j++) {
        int prjPortId = midiInIds[j];
        // Add to Jack, and update Jack port reference in project
        PrjMidiPort projectPort = prj->midiInPort_getPort(prjPortId);
        int jackPortId = addMidiInPortToJack(prjPortId);
        projectPort.jackPortId = jackPortId;
        prj->midiInPort_replace_noModify(prjPortId, projectPort); // Replace in project since the port has been updated with the jackPort

        // Also add port clients to Jack
        QStringList c = projectPort.clients;
        for (int k=0; k < c.count(); k++) {
            jack->addPortClient(jackPortId, c.at(k));
        }

        // Set port midi filter in Jack
        jack->setPortFilter(jackPortId, projectPort.filter);
    }


    // Process Midi out ports

    jack->removeAllMidiOutPorts();

    QList<int> midiOutIds = prj->midiOutPort_getAllPortIds();
    for (int j=0; j<midiOutIds.count(); j++) {
        int prjPortId = midiOutIds[j];
        // Add to Jack, and update Jack port reference in project
        PrjMidiPort projectPort = prj->midiOutPort_getPort(prjPortId);
        int jackPortId = addMidiOutPortToJack(prjPortId);
        projectPort.jackPortId = jackPortId;
        prj->midiOutPort_replace_noModify(prjPortId, projectPort); // Replace in project since the port has been updated with the jackPort

        // Also add port clients to Jack
        QStringList c = projectPort.clients;
        for (int k=0; k<c.count(); k++) {
            jack->addPortClient(jackPortId, c.at(k));
        }

    }

    // Update the port menus in patch view
    gui_updateLayerMidiInPortsMenu();
    gui_updatePatchMidiOutPortsMenu();
    gui_updatePatchAudioInPortsMenu();


    // In Jack, remove all audio input ports and busses (audio output ports)
    jack->removeAllAudioInAndOutPorts();

    // Audio Busses

    QList<int> busIds = prj->audioBus_getAllBusIds();
    for (int j=0; j<busIds.count(); j++) {
        int id = busIds[j];
        PrjAudioBus b =  prj->audioBus_getBus(id);

        // Add audio bus ports to jack client
        int left, right;
        addAudioBusToJack( id, &left, &right);
        if ( (left != KONFYT_JACK_PORT_ERROR) && (right != KONFYT_JACK_PORT_ERROR) ) {
            // Update left and right port references of bus in project
            b.leftJackPortId = left;
            b.rightJackPortId = right;
            prj->audioBus_replace_noModify( id, b ); // use noModify function as to not change the project's modified state.
            // Add port clients to jack client
            jack->setPortClients( b.leftJackPortId, b.leftOutClients);
            jack->setPortClients( b.rightJackPortId, b.rightOutClients );
        } else {
            userMessage("ERROR: setCurrentProject: Failed to create audio bus Jack port(s).");
        }

    }

    // Audio input ports
    QList<int> audioInIds = prj->audioInPort_getAllPortIds();
    for (int j=0; j<audioInIds.count(); j++) {
        int id = audioInIds[j];
        PrjAudioInPort p = prj->audioInPort_getPort(id);

        // Add audio ports to jack client
        int left, right;
        addAudioInPortsToJack( id, &left, &right );
        if ((left != KONFYT_JACK_PORT_ERROR) && (right != KONFYT_JACK_PORT_ERROR)) {
            // Update left and right port numbers in project
            p.leftJackPortId = left;
            p.rightJackPortId = right;
            prj->audioInPort_replace_noModify( id, p );
            // Add port clients to jack client
            jack->setPortClients( p.leftJackPortId, p.leftInClients );
            jack->setPortClients( p.rightJackPortId, p.rightInClients );
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
    connect(prj, &KonfytProject::processStartedSignal, this, &MainWindow::processStartedSlot);
    connect(prj, &KonfytProject::processFinishedSignal, this, &MainWindow::processFinishedSlot);
    connect(prj, &KonfytProject::projectModifiedChanged, this, &MainWindow::projectModifiedStateChanged);

    // Get triggers from project and add to quick lookup hash
    QList<KonfytTrigger> trigs = prj->getTriggerList();
    QList<QAction*> actions = triggersItemActionHash.values();
    for (int i=0; i<trigs.count(); i++) {
        // Find action matching text
        for (int j=0; j<actions.count(); j++) {
            if (trigs[i].actionText == actions[j]->text()) {
                triggersMidiActionHash.insert(trigs[i].toInt(), actions[j]);
            }
        }
    }

    // Update other JACK connections in Jack
    jack->clearOtherJackConPair();
    // MIDI
    QList<KonfytJackConPair> jackCons = prj->getJackMidiConList();
    for (int i=0; i < jackCons.count(); i++) {
        jack->addOtherJackConPair( jackCons[i] );
    }
    // Audio
    jackCons = prj->getJackAudioConList();
    for (int i=0; i < jackCons.count(); i++) {
        jack->addOtherJackConPair( jackCons[i] );
    }

    // Update project modified indication in GUI
    projectModifiedStateChanged(prj->isModified());

    masterPatch = NULL;
    gui_updatePatchView();


    if (ui->stackedWidget->currentWidget() == ui->connectionsPage) {
        showConnectionsPage();
    }
    if (ui->stackedWidget->currentWidget() == ui->triggersPage) {
        showTriggersPage();
    }
    if (ui->stackedWidget->currentWidget() == ui->otherJackConsPage) {
        showJackPage();
    }

    // Indicate warnings to user
    updateGUIWarnings();

    // Change project tab in GUI
    ui->tabWidget_Projects->blockSignals(true);
    ui->tabWidget_Projects->setCurrentIndex(currentProject);
    ui->tabWidget_Projects->blockSignals(false);

    jack->pauseJackProcessing(false);

}

/* Update the midi output ports menu */
void MainWindow::gui_updatePatchMidiOutPortsMenu()
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    patchMidiOutPortsMenu.clear();
    patchMidiOutPortsMenu_map.clear();

    QList<int> midiOutIds = prj->midiOutPort_getAllPortIds();
    for (int i=0; i<midiOutIds.count(); i++) {
        int id = midiOutIds[i];
        PrjMidiPort projectPort = prj->midiOutPort_getPort(id);
        QAction* action = patchMidiOutPortsMenu.addAction( n2s(id) + " " + projectPort.portName) ;
        patchMidiOutPortsMenu_map.insert(action, id);
    }
    patchMidiOutPortsMenu.addSeparator();
    patchMidiOutPortsMenu_NewPortAction = patchMidiOutPortsMenu.addAction("New Port");
}

/* Update the audio input ports menu */
void MainWindow::gui_updatePatchAudioInPortsMenu()
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    patchAudioInPortsMenu.clear();
    patchAudioInPortsMenu_map.clear();

    QList<int> audioInIds = prj->audioInPort_getAllPortIds();
    for (int i=0; i<audioInIds.count(); i++) {
        int id = audioInIds[i];
        PrjAudioInPort projectPort = prj->audioInPort_getPort(id);
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
    KonfytProject* prj = getCurrentProject();
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
    KonfytProject *prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("Select a project.");
        return;
    }

    prj->addPatch(newPatch);
    // Add to list in gui:
    gui_updatePatchList();
}


KonfytProject* MainWindow::getCurrentProject()
{
    if (projectList.count()) {
        if ( (currentProject<0) || (currentProject >= projectList.count())) {
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
        int currentRow = ui->listWidget_LibraryBottom->currentRow();
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
        konfytSoundfontProgram p = programList.at(ui->listWidget_LibraryBottom->currentRow());
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
    return library_getTreeItemType( ui->treeWidget_Library->currentItem() );
}


// Returns the currently selected patch, or a blank one
// if nothing is selected.
konfytPatch MainWindow::library_getSelectedPatch()
{
    if ( library_getSelectedTreeItemType() == libTreePatch ) {
        konfytPatch p = library_patchMap.value(ui->treeWidget_Library->currentItem());
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
        QTreeWidgetItem* current = ui->treeWidget_Library->currentItem();
        return library_sfMap.value(current);
    } else {
        return NULL;
    }
}



/* Determine if file suffix is as specified. The specified suffix should not
 * contain a leading dot. The suffix is checked by adding a dot.
 * E.g. pass wav as suffix, then example.wav will match but not example.awav. */
bool MainWindow::fileSuffixIs(QString file, QString suffix)
{
    suffix.prepend('.');
    suffix = suffix.toLower();
    QString right = file.right(suffix.length()).toLower();
    return  right == suffix;
}



/* Determine if specified file is a Konfyt patch file. */
bool MainWindow::fileIsPatch(QString file)
{
    return fileSuffixIs(file, KONFYT_PATCH_SUFFIX);
}



bool MainWindow::fileIsSfzOrGig(QString file)
{
    return fileSuffixIs(file, "sfz") || fileSuffixIs(file, "gig");
}



bool MainWindow::fileIsSoundfont(QString file)
{
    return fileSuffixIs(file, "sf2");
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
        QList<KonfytPatchLayer> l = previewPatch->getLayerItems();
        for (int i=0; i<l.count(); i++) {
            KonfytPatchLayer layer = l[i];
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
            KonfytProject* p = this->getCurrentProject();
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
    QList<KonfytPatchLayer> l = p->getLayerItems();
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
    ui->treeWidget_Library->clear();
    programList.clear(); // Internal list of programs displayed


    // Create parent soundfonts tree item, with soundfont children
    library_sfRoot = new QTreeWidgetItem();
    library_sfRoot->setText(0,QString(TREE_ITEM_SOUNDFONTS) + " [" + n2s(db.getNumSfonts()) + "]");
    library_sfFolders.clear();
    library_sfMap.clear();
    db.buildSfontTree();
    buildSfTree(library_sfRoot, db.sfontTree->root);
    library_sfRoot->setIcon(0, QIcon(":/icons/folder.png"));

    // Create parent patches tree item, with patch children
    library_patchRoot = new QTreeWidgetItem();
    library_patchRoot->setText(0,QString(TREE_ITEM_PATCHES) + " [" + n2s(db.getNumPatches()) + "]");
    library_patchRoot->setIcon(0, QIcon(":/icons/folder.png"));
    QList<konfytPatch> pl = db.getPatchList();
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
    library_sfzRoot->setText(0,QString(TREE_ITEM_SFZ) + " [" + n2s(db.getNumSfz()) + "]");
    library_sfzFolders.clear();
    library_sfzMap.clear();
    db.buildSfzTree();
    buildSfzTree(library_sfzRoot, db.sfzTree->root);
    library_sfzRoot->setIcon(0, QIcon(":/icons/folder.png"));

    // Add items to tree
    ui->treeWidget_Library->insertTopLevelItem(0,library_sfRoot);
    //ui->treeWidget_Library->expandItem(soundfontsParent);
    ui->treeWidget_Library->insertTopLevelItem(0,library_sfzRoot);
    //ui->treeWidget_Library->expandItem(sfzParent);
    ui->treeWidget_Library->insertTopLevelItem(0,library_patchRoot);
}



/* Build TreeWidget tree from the database's tree. */
void MainWindow::buildSfzTree(QTreeWidgetItem* twi, konfytDbTreeItem* item)
{
    if ( !item->hasChildren() ) {
        // Remove soundfont directory from item name if present
        QString rem = sfzDir + "/";
        rem.remove(0,1); // sfzDir probably starts with "/", tree item does not. And yes, this is less than ideal.
        QString pathRemoved = twi->text(0).remove(rem);
        twi->setText(0, pathRemoved);

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
        // Remove soundfont directory from item name if present
        QString rem = soundfontsDir + "/";
        rem.remove(0,1); // soundfontsDir probably starts with "/", tree item does not. And yes, this is less than ideal.
        QString pathRemoved = twi->text(0).remove(rem);
        twi->setText(0, pathRemoved);

        library_sfMap.insert(twi, (konfytSoundfont*)(item->data)); // Add to soundfonts map
        twi->setToolTip(0,twi->text(0));
        twi->setIcon(0, QIcon(":/icons/picture.png"));
    } else {
        twi->setIcon(0, QIcon(":/icons/folder.png"));
        // If item is not the root, add to soundfont folders map
        if (twi != library_sfRoot) {
            library_sfFolders.insert(twi, item->path);
        }
    }

    if (item->hasChildren()) {
        if ( (item->children.count() == 1) && (item->children[0]->hasChildren()) ) {
            // If database tree item has only one child that is not a leaf, skip it.
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
    db.searchProgram(search);

    ui->treeWidget_Library->clear();

    QTreeWidgetItem* twiResults = new QTreeWidgetItem();
    twiResults->setText(0, TREE_ITEM_SEARCH_RESULTS);

    // Soundfonts

    library_sfRoot = new QTreeWidgetItem();
    library_sfRoot->setText(0,QString(TREE_ITEM_SOUNDFONTS) + " [" + n2s(db.getNumSfontsResults()) + " / " + n2s(db.getNumSfontProgramResults()) + "]");

    library_sfFolders.clear();
    library_sfMap.clear();
    db.buildSfontTree_results();
    buildSfTree(library_sfRoot, db.sfontTree_results->root);
    library_sfRoot->setIcon(0, QIcon(":/icons/folder.png"));

    // Patches

    library_patchRoot = new QTreeWidgetItem();
    library_patchRoot->setText(0,QString(TREE_ITEM_PATCHES) + " [" + n2s(db.getNumPatchesResults()) + "]");
    library_patchRoot->setIcon(0, QIcon(":/icons/folder.png"));

    QList<konfytPatch> pl = db.getResults_patches();

    for (int i=0; i<pl.count(); i++) {
        konfytPatch pt = pl.at(i);

        QTreeWidgetItem* twiChild = new QTreeWidgetItem();
        twiChild->setText(0,pt.getName());
        library_patchMap.insert(twiChild, pt);

        library_patchRoot->addChild(twiChild);
    }

    // SFZ

    library_sfzRoot = new QTreeWidgetItem();
    library_sfzRoot->setText(0,QString(TREE_ITEM_SFZ) + " [" + n2s(db.getNumSfzResults()) + "]");
    library_sfzFolders.clear();
    library_sfzMap.clear();
    db.buildSfzTree_results();

    buildSfzTree(library_sfzRoot, db.sfzTree_results->root);
    library_sfzRoot->setIcon(0, QIcon(":/icons/folder.png"));

    // Add items to tree

    twiResults->addChild(library_patchRoot);
    twiResults->addChild(library_sfzRoot);
    twiResults->addChild(library_sfRoot);


    ui->treeWidget_Library->insertTopLevelItem(0,twiResults);
    ui->treeWidget_Library->expandItem(twiResults);
    ui->treeWidget_Library->expandItem(library_patchRoot);
    ui->treeWidget_Library->expandItem(library_sfzRoot);
    ui->treeWidget_Library->expandItem(library_sfRoot);
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

QString MainWindow::getBaseNameWithoutExtension(QString filepath)
{
    QFileInfo fi(filepath);
    return fi.baseName();
}


void MainWindow::on_treeWidget_Library_itemClicked(QTreeWidgetItem *item, int column)
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
    KonfytProject* prj = getCurrentProject();
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
    KonfytProject* prj = getCurrentProject();
    if ( (currentPatchIndex >= 0) && (currentPatchIndex < prj->getNumPatches()) ) {
        QListWidgetItem* item = ui->listWidget_Patches->item(currentPatchIndex);
        item->setIcon(QIcon(":/icons/play.png"));
    }
}

void MainWindow::unsetCurrentPatchIcon()
{
    KonfytProject* prj = getCurrentProject();
    if ( (currentPatchIndex >= 0) && (currentPatchIndex < prj->getNumPatches()) ) {
        QListWidgetItem* item = ui->listWidget_Patches->item(currentPatchIndex);
        item->setIcon(QIcon());
    }
}

void MainWindow::on_treeWidget_Library_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    ui->listWidget_LibraryBottom->clear();  // Program list view
    programList.clear();        // Our internal program list, corresponding to the list view
    ui->textBrowser_LibraryBottom->clear();

    if (current == NULL) {
        return;
    }

    if ( library_getSelectedTreeItemType() == libTreeSFZ ) {

        library_selectedSfz = library_sfzMap.value(current);
        if (previewMode) {
            loadPatchForModeAndUpdateGUI();
        }

        // Display contents in text view below library
        showSfzContentsBelowLibrary(library_selectedSfz);

    } else if ( library_getSelectedTreeItemType() == libTreeSoundfont ) {

        // Soundfont is selected.
        if (searchMode) {
            // Fill programList variable with program results of selected soundfont
            konfytSoundfont* sf = library_getSelectedSfont();
            programList = db.getResults_sfontPrograms(sf);
        } else {
            // Fill programList variable with all programs of selected soundfont
            konfytSoundfont* sf = library_getSelectedSfont();
            programList = sf->programlist;
        }
        // Refresh the GUI program list with programs (if any).
        ui->stackedWidget_libraryBottom->setCurrentWidget(ui->page_libraryBottom_ProgramList);
        library_refreshGUIProgramList();
        // Automatically select the first program
        if (ui->listWidget_LibraryBottom->count()) {
            ui->listWidget_LibraryBottom->setCurrentRow(0);
        }

    } else if ( library_getSelectedTreeItemType() == libTreePatch ) {

        if (previewMode) {
            // Patch is selected in preview mode. Load patch.
            loadPatchForModeAndUpdateGUI();
        }
        // Do nothing

    }
}

/* Refresh the program list view in the library, according to programList. */
void MainWindow::library_refreshGUIProgramList()
{
    ui->listWidget_LibraryBottom->clear();
    ui->stackedWidget_libraryBottom->setCurrentWidget(ui->page_libraryBottom_ProgramList);
    for (int i=0; i<programList.count(); i++) {
        ui->listWidget_LibraryBottom->addItem(n2s(programList.at(i).bank)
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
void MainWindow::on_listWidget_LibraryBottom_currentRowChanged(int currentRow)
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
    KonfytPatchLayer g = pengine->addSfzLayer(sfzPath);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
}

void MainWindow::addLV2ToCurrentPatch(QString lv2Path)
{
    newPatchIfMasterNull();

    // Add layer to engine
    KonfytPatchLayer g = pengine->addLV2Layer(lv2Path);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
}

// Adds an internal Carla plugin to the current patch in engine and GUI.
void MainWindow::addCarlaInternalToCurrentPatch(QString URI)
{
    newPatchIfMasterNull();

    // Add layer to engine
    KonfytPatchLayer g = pengine->addCarlaInternalLayer(URI);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
}

// Adds soundfont program to current patch in engine and in GUI.
void MainWindow::addProgramToCurrentPatch(konfytSoundfontProgram p)
{
    newPatchIfMasterNull();

    // Add program to engine
    KonfytPatchLayer g = pengine->addProgramLayer(p);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
}

// If masterPatch is NULL, adds a new patch to the project and switches to it
void MainWindow::newPatchIfMasterNull()
{
    KonfytProject* prj = getCurrentProject();
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
    KonfytPatchLayer g = pengine->addMidiOutPortToPatch(port);

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
    KonfytPatchLayer g = pengine->addAudioInPortToPatch(port);

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

/* Patch midi input port menu item has been clicked. */
void MainWindow::onPatchMidiInPortsMenu_ActionTrigger(QAction *action)
{
    if (action == patchMidiOutPortsMenu_NewPortAction) {
        // Add new port
        int portId = addMidiInPort();
        if (portId >= 0) {

        }
    }
}



void MainWindow::on_lineEdit_ProjectName_editingFinished()
{
    KonfytProject* prj = getCurrentProject();
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

    QString patchName = getUniqueFilename(dir.path(), patch->getName(), "." + QString(KONFYT_PATCH_SUFFIX) );
    if (patchName == "") {
        userMessage("Could not find a suitable filename.");
        return false;
    }

    if (patchName != patch->getName() + "." + KONFYT_PATCH_SUFFIX) {
        userMessage("Duplicate name exists. Saving patch as:");
        userMessage(patchName);
    }

    // Add directory, and save.
    patchName = patchesDir + "/" + patchName;
    if (patch->savePatchToFile(patchName)) {
        userMessage("Patch saved as " + patchName);
        db.addPatch(patchName);
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
    KonfytProject* p = getCurrentProject();
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
    KonfytProject* p = getCurrentProject();
    if (p == NULL) {
        return;
    }
    // Abort if process is already running
    if (p->isProcessRunning(index)) {
        userMessage("Process is already running. Stop it before running it again.");
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
    KonfytProject* p = getCurrentProject();
    if (p == NULL) {
        return;
    }
    // Stop process
    p->stopProcess(index);
}

void MainWindow::removeProcess(int index)
{
    KonfytProject* p = getCurrentProject();
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

void MainWindow::extAppsMenuTriggered(QAction *action)
{
    ui->lineEdit_ExtApp->setText( extAppsMenuActions.value(action, "") );
}


void MainWindow::on_toolButton_ExtAppsMenu_clicked()
{
    extAppsMenu.popup(QCursor::pos());
}


void MainWindow::showWaitingPage(QString title)
{
    ui->label_WaitingTitle->setText(title);
    ui->stackedWidget->setCurrentWidget(ui->page_Waiting);
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

void MainWindow::initAboutDialog()
{
    // Add additional version text to about dialog
    QStringList txt;
    txt.append( "Compiled with Fluidsynth " + QString(fluid_version_str()) );
    txt.append( "Compiled with Carla " + QString(CARLA_VERSION_STRING) );
    aboutDialog.setExtraVersionText(txt);

    aboutDialog.setParent(this);
    aboutDialog.hide();
    resizeAboutDialog();
}

void MainWindow::showAboutDialog()
{
    aboutDialog.show();
}

void MainWindow::resizeAboutDialog()
{
    aboutDialog.move(0,0);
    aboutDialog.resize(this->width(),this->height());
}

void MainWindow::resizeEvent(QResizeEvent *ev)
{
    resizeAboutDialog();
}

/* Helper function for scanning things into database. */
void MainWindow::scanForDatabase()
{
    startWaiter("Scanning database directories...");
    // Display waiting screen.
    userMessage("Starting database scan.");
    showWaitingPage("Scanning database directories...");
    // Start scanning for directories.
    db.scanDirs(soundfontsDir, sfzDir, patchesDir);
    // When the finished signal is received, remove waiting screen.
    // See database_scanDirsFinished()
    // Periodic status info might be received in database_scanDirsStatus()
}

void MainWindow::database_scanDirsFinished()
{
    userMessage("Database scanning complete.");
    userMessage("   Found " + n2s(db.getNumSfonts()) + " soundfonts.");
    userMessage("   Found " + n2s(db.getNumSfz()) + " sfz/gig samples.");
    userMessage("   Found " + n2s(db.getNumPatches()) + " patches.");

    // Save to database file
    saveDatabase();

    fillTreeWithAll();
    stopWaiter();
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);
}

bool MainWindow::saveDatabase()
{
    // Save to database file
    if (db.saveDatabaseToFile(settingsDir + "/" + DATABASE_FILE)) {
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
    ui->stackedWidget->setCurrentWidget(ui->page_Waiting);
    applySettings();

    db.clearDatabase();
    scanForDatabase();
}

// Quick scan database button clicked
void MainWindow::on_pushButtonSettings_QuickRescanLibrary_clicked()
{
    startWaiter("Scanning database...");
    applySettings();

    db.clearDatabase_exceptSoundfonts();
    scanForDatabase();
    // This will be done in the background and database_scanDirsFinished()
    // will be called when done.
}

void MainWindow::scanThreadFihishedSlot()
{
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
    userMessage("moved."); // TODO
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
    KonfytProject* prj = getCurrentProject();
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
    KonfytProject* p = getCurrentProject();

    if (p == NULL) {
        userMessage("Select a project.");
        return false;
    }

    return saveProject(p);
}

bool MainWindow::saveProject(KonfytProject *p)
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
            QString dir = getUniqueFilename(this->projectsDir,sanitiseFilename( p->getProjectName() ),"");
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

    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    QStringList moports = jack->getMidiOutputPortsList();
    QStringList miports = jack->getMidiInputPortsList();
    QStringList aoports = jack->getAudioOutputPortsList();
    QStringList aiports = jack->getAudioInputPortsList();


    // Check warnings

    // MIDI input ports
    QList<int> midiInIds = prj->midiInPort_getAllPortIds();
    for (int i=0; i < midiInIds.count(); i++) {
        PrjMidiPort prjPort = prj->midiInPort_getPort(midiInIds[i]);
        // Check for no connections
        if (prjPort.clients.count() == 0) {
            addWarning("MIDI-in \"" + prjPort.portName + "\" not connected");
        } else {
            // Check if clients are available
            bool notRunning = false;
            for (int j=0; j < prjPort.clients.count(); j++) {
                if ( !moports.contains(prjPort.clients[j]) ) {
                    notRunning = true;
                    break;
                }
            }
            if (notRunning) {
                addWarning("MIDI-in \"" + prjPort.portName + "\" client(s) inactive");
            }
        }
    }

    // Busses
    QList<int> busIds = prj->audioBus_getAllBusIds();
    for (int i=0; i<busIds.count(); i++) {
        PrjAudioBus bus = prj->audioBus_getBus(busIds[i]);
        // Check for no connections
        bool left = (bus.leftOutClients.count() == 0);
        bool right = (bus.rightOutClients.count() == 0);
        if (left && right) {
            addWarning("Bus \"" + bus.busName + "\" not connected");
        } else if (left) {
            addWarning("Bus \"" + bus.busName + "\" left not connected");
        } else if (right) {
            addWarning("Bus \"" + bus.busName + "\" right not connected");
        }
        // Check if clients are available
        bool notRunning = false;
        for (int j=0; j < bus.leftOutClients.count(); j++) {
            if ( !aiports.contains(bus.leftOutClients[j]) ) {
                notRunning = true;
                break;
            }
        }
        if (!notRunning) {
            for (int j=0; j < bus.rightOutClients.count(); j++) {
                if ( !aiports.contains(bus.rightOutClients[j]) ) {
                    notRunning = true;
                    break;
                }
            }
        }
        if (notRunning) {
            addWarning("Bus \"" + bus.busName + "\" client(s) inactive");
        }
    }

    // MIDI out ports
    QList<int> midiOutIds = prj->midiOutPort_getAllPortIds();
    for (int i=0; i<midiOutIds.count(); i++) {
        PrjMidiPort prjPort = prj->midiOutPort_getPort(midiOutIds[i]);
        // Check for no connections
        if (prjPort.clients.count() == 0) {
            addWarning("MIDI-out \"" + prjPort.portName + "\" not connected");
        } else {
            // Check if clients are available
            bool notRunning = false;
            for (int j=0; j < prjPort.clients.count(); j++) {
                if ( !miports.contains(prjPort.clients[j]) ) {
                    notRunning = true;
                    break;
                }
            }
            if (notRunning) {
                addWarning("MIDI-out \"" + prjPort.portName + "\" client(s) inactive");
            }
        }
    }

    // Audio in ports
    QList<int> aiIds = prj->audioInPort_getAllPortIds();
    for (int i=0; i<aiIds.count(); i++) {
        PrjAudioInPort prjPort = prj->audioInPort_getPort(aiIds[i]);
        // Check for no connections
        bool left = (prjPort.leftInClients.count() == 0);
        bool right = (prjPort.rightInClients.count() == 0);
        if (left && right) {
            addWarning("Audio-in \"" + prjPort.portName + "\" not connected");
        } else if (left) {
            addWarning("Audio-in \"" + prjPort.portName + "\" left not connected");
        } else if (right) {
            addWarning("Audio-in \"" + prjPort.portName + "\" right not connected");
        }
        // Check for clients not available
        bool notRunning = false;
        for (int j=0; j < prjPort.leftInClients.count(); j++) {
            if ( !aoports.contains(prjPort.leftInClients[j]) ) {
                notRunning = true;
                break;
            }
        }
        if (!notRunning) {
            for (int j=0; j < prjPort.rightInClients.count(); j++) {
                if ( !aoports.contains(prjPort.rightInClients[j]) ) {
                    notRunning = true;
                    break;
                }
            }
        }
        if (notRunning) {
            addWarning("Audio-in \"" + prjPort.portName + "\" client(s) inactive");
        }

    }

    // Other JACK MIDI connections
    bool first = true;
    QList<KonfytJackConPair> midiCons = prj->getJackMidiConList();
    for (int i=0; i<midiCons.count(); i++) {
        if ( !moports.contains(midiCons[i].srcPort) ) {
            if (first) {
                addWarning("Missing Other JACK MIDI Ports:");
                first = false;
            }
            addWarning(" -TX:  " + midiCons[i].srcPort);
        }
        if ( !miports.contains(midiCons[i].destPort) ) {
            if (first) {
                addWarning("Missing Other JACK MIDI Ports:");
                first = false;
            }
            addWarning(" -RX: " + midiCons[i].destPort);
        }
    }

    // Other JACK Audio connections
    first = true;
    QList<KonfytJackConPair> audioCons = prj->getJackAudioConList();
    for (int i=0; i<audioCons.count(); i++) {
        if ( !aoports.contains(audioCons[i].srcPort) ) {
            if (first) {
                addWarning("Missing Other JACK Audio Ports:");
                first = false;
            }
            addWarning(" -TX:  " + audioCons[i].srcPort);
        }
        if ( !aiports.contains(audioCons[i].destPort) ) {
            if (first) {
                addWarning("Missing Other JACK Audio Ports:");
                first = false;
            }
            addWarning(" -RX: " + audioCons[i].destPort);
        }
    }

}

void MainWindow::addWarning(QString warning)
{
    ui->listWidget_Warnings->addItem(warning);
}

void MainWindow::triggerPanic(bool panic)
{
    panicState = panic;

    pengine->panic(panicState);

    // Update in GUI
    ui->pushButton_Panic->setChecked(panicState);
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
void MainWindow::midiEventSlot(KonfytMidiEvent ev)
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

    if (ui->stackedWidget->currentWidget() == ui->triggersPage) {

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

    // If program change without bank select, switch patch if checkbox is checked.
    if (ev.type == MIDI_EVENT_TYPE_PROGRAM) {
        if ( (lastBankSelectLSB == -1) && (lastBankSelectMSB == -1) ) {
            KonfytProject* prj = getCurrentProject();
            if (prj != NULL) {
                if (prj->isProgramChangeSwitchPatches()) {
                    setCurrentPatch(ev.data1);
                }
            }
        }
    }

    // Hash midi event to a key
    int key;
    if (ev.type == MIDI_EVENT_TYPE_PROGRAM) {
        key = hashMidiEventToInt(ev.type, ev.channel, ev.data1, lastBankSelectMSB, lastBankSelectLSB);
    } else {
        key = hashMidiEventToInt(ev.type, ev.channel, ev.data1, -1, -1);
    }
    // Determine if event passes as button press
    bool buttonPass = 0;
    if (ev.type == MIDI_EVENT_TYPE_PROGRAM) {
        buttonPass = true;
    } else {
        buttonPass = ev.data2 > 0;
    }

    // Get the appropriate action based on the key
    QAction* action = triggersMidiActionHash[key];

    // Perform the action
    if (action == ui->actionPanic) {

        if (buttonPass) { ui->actionPanic->trigger(); }

    } else if (action == ui->actionPanicToggle) {

        if (buttonPass) { ui->actionPanicToggle->trigger(); }

    } else if (action == ui->actionNext_Patch) {

        if (buttonPass) { setCurrentPatch( currentPatchIndex+1 ); }

    } else if (action == ui->actionPrevious_Patch) {

        if (buttonPass) { setCurrentPatch( currentPatchIndex-1 ); }

    } else if (action == ui->actionMaster_Volume_Slider) {

        ui->horizontalSlider_MasterGain->setValue(((float)ev.data2)/127.0*ui->horizontalSlider_MasterGain->maximum());
        ui->horizontalSlider_MasterGain->triggerAction(QSlider::SliderMove);
        on_horizontalSlider_MasterGain_sliderMoved(ui->horizontalSlider_MasterGain->value());

    } else if (action == ui->actionMaster_Volume_Up) {

        if (buttonPass) { ui->actionMaster_Volume_Up->trigger(); }

    } else if (action == ui->actionMaster_Volume_Down) {

        if (buttonPass) { ui->actionMaster_Volume_Down->trigger(); }

    } else if (action == ui->actionSave_Patch) {

        if (buttonPass) { ui->actionSave_Patch->trigger(); }

    } else if (action == ui->actionProject_save) {

        if (buttonPass) { ui->actionProject_save->trigger(); }

    } else if (channelGainActions.contains(action)) {

        midi_setLayerGain( channelGainActions.indexOf(action), ev.data2 );

    } else if (channelSoloActions.contains(action)) {

        midi_setLayerSolo( channelSoloActions.indexOf(action), ev.data2 );

    } else if (channelMuteActions.contains(action)) {

        midi_setLayerMute( channelMuteActions.indexOf(action), ev.data2 );

    } else if (patchActions.contains(action)) {

        setCurrentPatch( patchActions.indexOf(action) );

    } else if (action == ui->actionGlobal_Transpose_12_Down) {

        if (buttonPass) { setMasterInTranspose(-12,true); }

    } else if (action == ui->actionGlobal_Transpose_12_Up) {

        if (buttonPass) { setMasterInTranspose(12,true); }

    } else if (action == ui->actionGlobal_Transpose_1_Down) {

        if (buttonPass) { setMasterInTranspose(-1,true); }

    } else if (action == ui->actionGlobal_Transpose_1_Up) {

        if (buttonPass) { setMasterInTranspose(1,true); }

    } else if (action == ui->actionGlobal_Transpose_Zero) {

        if (buttonPass) { setMasterInTranspose(0,false); }

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

    KonfytPatchLayer layer = layerBusMenu_sourceItem->getPatchLayerItem();
    KonfytMidiFilter filter = layer.getMidiFilter();
    filter.outChan = channel;
    layer.setMidiFilter(filter);

    // Update layer widget
    layerBusMenu_sourceItem->setLayerItem(layer);
    // Update in pengine
    pengine->setLayerFilter(&layer, filter);

    setPatchModified(true);
}

/* Menu item has been clicked in the layer MIDI-In port menu. */
void MainWindow::onLayerMidiInMenu_ActionTrigger(QAction *action)
{
    int portId;
    if (action == layerMidiInPortsMenu_newPortAction) {
        // Add new MIDI in port
        portId = addMidiInPort();
        if (portId < 0) { return; }
        // Open the connections page and show port.
        showConnectionsPage();
        ui->tree_portsBusses->setCurrentItem( tree_midiInMap.key(portId) );
    } else {
        // User chose a MIDI-in port
        portId = layerMidiInPortsMenu_map.value(action);
    }

    // Set the MIDI Input port in the GUI layer item
    KonfytPatchLayer layer = layerMidiInMenu_sourceItem->getPatchLayerItem();
    layer.midiInPortIdInProject = portId;

    // Update the layer widget
    layerMidiInMenu_sourceItem->setLayerItem( layer );
    // Update in pengine
    pengine->setLayerMidiInPort( &layer, portId );

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
        // Open the connections page and show bus.
        showConnectionsPage();
        ui->tree_portsBusses->setCurrentItem( tree_busMap.key(busId) );
    } else {
        // User chose a bus
        busId = layerBusMenu_actionsBusIdsMap.value(action);
    }

    // Set the destination bus in gui layer item
    KonfytPatchLayer g = layerBusMenu_sourceItem->getPatchLayerItem();
    g.busIdInProject = busId;

    // Update the layer widget
    layerBusMenu_sourceItem->setLayerItem( g );
    // Update in pengine
    pengine->setLayerBus( &g, busId );

    setPatchModified(true);
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
    midiFilterEditType = MidiFilterEditLayer;
    midiFilterEditItem = layerItem;
    showMidiFilterEditor();
}

/* Slot: on layer item slider move. */
void MainWindow::onLayer_slider_moved(konfytLayerWidget *layerItem, float gain)
{
    KonfytPatchLayer g = layerItem->getPatchLayerItem();
    pengine->setLayerGain(&g, gain);
}

/* Slot: on layer item solo button clicked. */
void MainWindow::onLayer_solo_clicked(konfytLayerWidget *layerItem, bool solo)
{
    KonfytPatchLayer g = layerItem->getPatchLayerItem();
    pengine->setLayerSolo(&g, solo);

}

/* Slot: on layer item mute button clicked. */
void MainWindow::onLayer_mute_clicked(konfytLayerWidget *layerItem, bool mute)
{
    KonfytPatchLayer g = layerItem->getPatchLayerItem();
    pengine->setLayerMute(&g, mute);
}

/* Slot: on layer item bus button clicked. */
void MainWindow::onLayer_bus_clicked(konfytLayerWidget *layerItem)
{
    // Save the layer item for future use
    layerBusMenu_sourceItem = layerItem;

    KonfytLayerType type = layerItem->getPatchLayerItem().getLayerType();
    if (type == KonfytLayerType_MidiOut) {
        // Show MIDI channel menu
        gui_updateLayerMidiOutChannelMenu();
        layerMidiOutChannelMenu.popup(QCursor::pos());
    } else {
        // Show Busses menu
        gui_updateLayerBusMenu();
        layerBusMenu.popup(QCursor::pos());

    }
    // The rest will be done in onlayerBusMenu_ActionTrigger() when the user clicked a menu item.
}

/* Slot: on layer item Midi-In button clicked. */
void MainWindow::onLayer_midiIn_clicked(konfytLayerWidget *layerItem)
{
    // Save the layer item for future use
    layerMidiInMenu_sourceItem = layerItem;

    // Show MIDI input port menu
    gui_updateLayerMidiInPortsMenu();
    layerMidiInPortsMenu.popup(QCursor::pos());

    // The rest will be done in onLayerMidiInMenu_ActionTrigger() when the user clicked a menu item.
}

void MainWindow::onLayer_reload_clicked(konfytLayerWidget *layerItem)
{
    KonfytPatchLayer l = layerItem->getPatchLayerItem();
    KonfytPatchLayer lnew = pengine->reloadLayer( &l );
    layerItem->setLayerItem( lnew );

}

void MainWindow::onLayer_openInFileManager_clicked(konfytLayerWidget *layerItem, QString filepath)
{
    // If path is a file, change path to the folder name of the file
    QFileInfo info(filepath);
    if (!info.isDir()) { filepath = info.path(); }
    openFileManager(filepath);
}

void MainWindow::gui_updateLayerBusMenu()
{
    layerBusMenu.clear();
    layerBusMenu_actionsBusIdsMap.clear();
    KonfytProject* prj = getCurrentProject();
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
void MainWindow::addLayerItemToGUI(KonfytPatchLayer layerItem)
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
    connect(gui, &konfytLayerWidget::slider_moved_signal,
            this, &MainWindow::onLayer_slider_moved);

    connect(gui, &konfytLayerWidget::remove_clicked_signal,
            this, &MainWindow::onLayer_remove_clicked);

    connect(gui, &konfytLayerWidget::filter_clicked_signal,
            this, &MainWindow::onLayer_filter_clicked);

    connect(gui, &konfytLayerWidget::solo_clicked_signal,
            this, &MainWindow::onLayer_solo_clicked);

    connect(gui, &konfytLayerWidget::mute_clicked_signal,
            this, &MainWindow::onLayer_mute_clicked);

    connect(gui, &konfytLayerWidget::midiIn_clicked_signal,
            this, &MainWindow::onLayer_midiIn_clicked);

    connect(gui, &konfytLayerWidget::bus_clicked_signal,
            this, &MainWindow::onLayer_bus_clicked);

    connect(gui, &konfytLayerWidget::reload_clicked_signal,
            this, &MainWindow::onLayer_reload_clicked);

    connect(gui, &konfytLayerWidget::openInFileManager_clicked_signal,
            this, &MainWindow::onLayer_openInFileManager_clicked);

}

/* Remove a layer item from the layer list.
 * This includes from the engine, GUI and the internal list. */
void MainWindow::removeLayerItem(konfytLayerWidget *layerItem)
{
    // First, remove from engine
    KonfytPatchLayer g = layerItem->getPatchLayerItem();
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

void MainWindow::gui_updateLayerMidiInPortsMenu()
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    layerMidiInPortsMenu.clear();
    layerMidiInPortsMenu_map.clear();

    QList<int> midiInIds = prj->midiInPort_getAllPortIds();
    for (int i=0; i < midiInIds.count(); i++) {
        int id = midiInIds[i];
        PrjMidiPort projectPort = prj->midiInPort_getPort(id);
        QAction* action = layerMidiInPortsMenu.addAction( n2s(id) + " " + projectPort.portName);
        layerMidiInPortsMenu_map.insert(action, id);
    }
    layerMidiInPortsMenu.addSeparator();
    layerMidiInPortsMenu_newPortAction = layerMidiInPortsMenu.addAction("New Port");

}



void MainWindow::on_pushButton_Settings_Cancel_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);
}

void MainWindow::on_pushButton_Settings_Apply_clicked()
{
    applySettings();
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);
}

void MainWindow::on_pushButton_settings_Projects_clicked()
{
    // Show dialog to select projects directory
    QFileDialog d;
    QString path = d.getExistingDirectory( this, "Select projects directory", ui->lineEdit_Settings_ProjectsDir->text() );
    if ( !path.isEmpty() ) {
        ui->lineEdit_Settings_ProjectsDir->setText( path );
    }
}

void MainWindow::on_pushButton_Settings_Soundfonts_clicked()
{
    // Show dialog to select soundfonts directory
    QFileDialog d;
    QString path = d.getExistingDirectory( this, "Select soundfonts directory", ui->lineEdit_Settings_SoundfontsDir->text() );
    if ( !path.isEmpty() ) {
        ui->lineEdit_Settings_SoundfontsDir->setText( path );
    }
}

void MainWindow::on_pushButton_Settings_Patches_clicked()
{
    // Show dialog to select patches directory
    QFileDialog d;
    QString path = d.getExistingDirectory( this, "Select patches directory", ui->lineEdit_Settings_PatchesDir->text() );
    if ( !path.isEmpty() ) {
        ui->lineEdit_Settings_PatchesDir->setText( path );
    }
}

void MainWindow::on_pushButton_Settings_Sfz_clicked()
{
    // Show dialog to select sfz directory
    QFileDialog d;
    QString path = d.getExistingDirectory( this, "Select sfz directory", ui->lineEdit_Settings_SfzDir->text() );
    if ( !path.isEmpty() ) {
        ui->lineEdit_Settings_SfzDir->setText( path );
    }
}


/* Action to save current patch (in pengine; may have been modified)
 * to currently selected patch in the project. */
void MainWindow::on_actionSave_Patch_triggered()
{

    // 2017-01-05 This is not really used anymore as patches are automatically saved at the moment.

    KonfytProject* prj = getCurrentProject();

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
    QString filename = d.getSaveFileName(this,"Save patch as file", patchesDir, "*." + QString(KONFYT_PATCH_SUFFIX));
    if (filename=="") {return;} // Dialog was cancelled.

    // Add suffix if not already added (this is not foolproof, but what the hell.)
    if (!filename.contains("." + QString(KONFYT_PATCH_SUFFIX))) { filename = filename + "." + QString(KONFYT_PATCH_SUFFIX); }

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

    KonfytProject *prj = getCurrentProject();
    if (prj == NULL) { return; }

    konfytPatch* pt = new konfytPatch();
    QFileDialog d;
    QString filename = d.getOpenFileName(this, "Open patch from file", patchesDir, "*." + QString(KONFYT_PATCH_SUFFIX));
    if (filename=="") { return; }
    if (pt->loadPatchFromFile(filename)) {
        addPatchToProject(pt);
    } else {
        userMessage("Failed loading patch from file.");
        delete pt;
    }
}

/* Add button clicked (not its menu). */
void MainWindow::on_toolButton_AddPatch_clicked()
{
    // Add a new patch to the project.
    on_actionNew_Patch_triggered();
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
                ui->actionPanicToggle->trigger();
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
    // Switch back to previous view
    if (midiFilterEditType == MidiFilterEditPort) {
        ui->stackedWidget->setCurrentWidget(ui->connectionsPage);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->PatchPage);
    }
}

// The user has been editing the midi filter and has now clicked apply.
void MainWindow::on_pushButton_midiFilter_Apply_clicked()
{
    KonfytMidiFilter f;
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("ERROR: No current project.");
        return;
    }

    if (midiFilterEditType == MidiFilterEditPort) {
        if (prj->midiInPort_exists(midiFilterEditPort)) {
            f = prj->midiInPort_getPort(midiFilterEditPort).filter;
        } else {
            userMessage("ERROR: Port does not exist in project.");
            return;
        }
    } else {
        KonfytPatchLayer g = midiFilterEditItem->getPatchLayerItem();
        f = g.getMidiFilter();
    }

    // Update the filter from the GUI
    f.setZone( ui->spinBox_midiFilter_LowNote->value(),
               ui->spinBox_midiFilter_HighNote->value(),
               ui->spinBox_midiFilter_Add->value(),
               ui->spinBox_midiFilter_LowVel->value(),
               ui->spinBox_midiFilter_HighVel->value(),
               ui->spinBox_midiFilter_VelLimitMin->value(),
               ui->spinBox_midiFilter_VelLimitMax->value());
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

    if (midiFilterEditType == MidiFilterEditPort) {
        // Update filter in project
        prj->midiInPort_setPortFilter(midiFilterEditPort, f);
        // And also in Jack.
        jack->setPortFilter(prj->midiInPort_getPort(midiFilterEditPort).jackPortId,
                            f);
    } else {
        // Update filter in gui layer item
        KonfytPatchLayer g = midiFilterEditItem->getPatchLayerItem();
        g.setMidiFilter(f);
        midiFilterEditItem->setLayerItem(g);
        // and also in engine.
        pengine->setLayerFilter(&g, f);
    }

    // Indicate project needs to be saved
    setProjectModified();

    // Switch back to previous view
    if (midiFilterEditType == MidiFilterEditPort) {
        ui->stackedWidget->setCurrentWidget(ui->connectionsPage);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->PatchPage);
    }
}



void MainWindow::on_toolButton_MidiFilter_lowNote_clicked()
{
    ui->spinBox_midiFilter_LowNote->setValue(midiFilter_lastData1);
}

void MainWindow::on_toolButton_MidiFilter_HighNote_clicked()
{
    ui->spinBox_midiFilter_HighNote->setValue(midiFilter_lastData1);
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
    KonfytProject* prj = getCurrentProject();
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
void MainWindow::on_listWidget_LibraryBottom_itemDoubleClicked(QListWidgetItem *item)
{
    // Add soundfont program to current patch.

    if (previewMode) { setPreviewMode(false); }

    if (library_isProgramSelected()) {

        addProgramToCurrentPatch( library_getSelectedProgram() );
    }
}


/* Library tree: item double clicked. */
void MainWindow::on_treeWidget_Library_itemDoubleClicked(QTreeWidgetItem *item, int column)
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

    KonfytProject* prj = getCurrentProject();
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
            bool show = false;
            if (ui->checkBox_filesystem_ShowOnlySounds->isChecked() == false) {
                show = true;
            } else {
                show = fileIsSfzOrGig(info.filePath())       // sfz or gig
                       || fileIsSoundfont(info.filePath())   // sf2
                       || fileIsPatch(info.filePath());      // patch
            }
            if ( show ) {

                QTreeWidgetItem* item = new QTreeWidgetItem();
                item->setIcon(0, QIcon(":/icons/picture.png"));
                item->setText(0, info.fileName());
                ui->treeWidget_filesystem->addTopLevelItem(item);
                fsMap.insert(item, info);
            }
        }

    }

}

/* Change filesystem view directory, storing current path for the 'back' functionality. */
void MainWindow::cdFilesystemView(QString newpath)
{
    QFileInfo info(newpath);
    QString path;
    if (info.isDir()) {
        path = info.filePath();
    } else {
        path = info.dir().path();
    }

    fsview_back.append(fsview_currentPath);
    fsview_currentPath = path;
    refreshFilesystemView();
}

void MainWindow::selectItemInFilesystemView(QString path)
{
    QFileInfo info(path);
    if (fsview_currentPath == info.path()) {
        QList<QTreeWidgetItem*> l = fsMap.keys();
        for (int i=0; i<l.count(); i++) {
            if (fsMap[l[i]].fileName() == info.fileName()) {
                ui->treeWidget_filesystem->setCurrentItem(l[i]);
                break;
            }
        }
    }
}


void MainWindow::on_treeWidget_filesystem_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    ui->textBrowser_LibraryBottom->clear();
    ui->stackedWidget_libraryBottom->setCurrentWidget(ui->page_libraryBottom_Text);

    QFileInfo info = fsMap.value(current);
    if (info.isDir()) {
        // Do nothing
    } else if ( fileIsSoundfont(info.filePath()) ) {

        ui->textBrowser_LibraryBottom->append("SF2 Soundfont");
        ui->textBrowser_LibraryBottom->append("File size: " + n2s(info.size()/1024/1024) + " MB");
        ui->textBrowser_LibraryBottom->append("\nDouble-click to load program list.");

    } else if ( fileIsSfzOrGig(info.filePath()) ) {

        showSfzContentsBelowLibrary(info.filePath());

    } else if ( fileIsPatch(info.filePath())) {

        ui->textBrowser_LibraryBottom->append("Double-click to load patch.");

    }
}


/* Filesystem view: double clicked file or folder in file list. */
void MainWindow::on_treeWidget_filesystem_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    ui->textBrowser_LibraryBottom->clear();
    ui->stackedWidget_libraryBottom->setCurrentWidget(ui->page_libraryBottom_Text);
    ui->listWidget_LibraryBottom->clear();
    programList.clear();

    QFileInfo info = fsMap.value(item);
    if (info.isDir()) {
        // If directory, cd to directory.
        cdFilesystemView(info.filePath());
    } else if ( fileIsSoundfont(info.filePath()) ) {
        // If soundfont, read soundfont and fill program list.

        // Initiate mainwindow waiter (this disables the GUI and indicates to the user
        // that we are busy getting the soundfont in the background)
        startWaiter("Loading soundfont...");
        ui->textBrowser_LibraryBottom->append("Loading soundfont...");
        // Request soundfont from database
        returnSfontRequester = returnSfontRequester_on_treeWidget_filesystem_itemDoubleClicked;
        this->db.returnSfont(info.filePath());
        // This might take a while. The result will be sent by signal to the
        // database_returnSfont() slot where we will continue.
        return;

    } else if ( fileIsSfzOrGig(info.filePath()) ) {
        // If sfz or gig, load file.

        addSfzToCurrentPatch( info.filePath() );
        showSfzContentsBelowLibrary(info.filePath());

    } else if ( fileIsPatch(info.filePath()) ) {
        // File is a patch

        konfytPatch* pt = new konfytPatch();
        if (pt->loadPatchFromFile(info.filePath())) {
            addPatchToProject(pt);
        } else {
            userMessage("Failed to load patch " + info.filePath());
            delete pt;
        }

    }

    // Refresh program list in the GUI based on contents of programList variable.
    library_refreshGUIProgramList();
}

/* Filesystem view: one up button clicked. */
void MainWindow::on_toolButton_filesystem_up_clicked()
{
    QString itemToSelect = fsview_currentPath;
    QFileInfo info(fsview_currentPath);
    cdFilesystemView( info.path() ); // info.path() gives parent directory because info is a dir.
    selectItemInFilesystemView(itemToSelect);
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
    cdFilesystemView( ui->lineEdit_filesystem_path->text() );
}


// Add left and right audio output ports to Jack client for a bus, named
// according to the given bus number. The left and right port references in Jack
// are assigned to the leftPort and rightPort parameters.
void MainWindow::addAudioBusToJack(int busNo, int *leftPortId, int *rightPortId)
{
    *leftPortId = jack->addPort( KonfytJackPortType_AudioOut, "bus_" + n2s( busNo ) + "_L" );
    *rightPortId = jack->addPort( KonfytJackPortType_AudioOut, "bus_" + n2s( busNo ) + "_R" );
}

/* Adds an audio bus to the current project and Jack. Returns bus index.
   Returns -1 on error. */
int MainWindow::addBus()
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("Select a project.");
        return -1;
    }

    // Add to project
    QString busName = "AudioBus_" + n2s( prj->audioBus_count() );
    int busId = prj->audioBus_add(busName);

    int left, right;
    addAudioBusToJack( busId, &left, &right );
    if ( (left != KONFYT_JACK_PORT_ERROR) && (right != KONFYT_JACK_PORT_ERROR) ) {
        PrjAudioBus bus = prj->audioBus_getBus(busId);
        bus.leftJackPortId = left;
        bus.rightJackPortId = right;
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
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("Select a project.");
        return -1;
    }


    int portId = prj->audioInPort_add( "New Audio In Port" );
    int left, right;
    addAudioInPortsToJack( portId, &left, &right );
    if ( (left != KONFYT_JACK_PORT_ERROR) && (right != KONFYT_JACK_PORT_ERROR) ) {
        // Update in project
        PrjAudioInPort p = prj->audioInPort_getPort(portId);
        p.leftJackPortId = left;
        p.rightJackPortId = right;
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

/* Adds new Midi input port to project and Jack. Returns the port index.
 * Returns -1 on error. */
int MainWindow::addMidiInPort()
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("Select a project.");
        return -1;
    }

    int prjPortId = prj->midiInPort_addPort("New MIDI In Port");
    int jackPortId = addMidiInPortToJack(prjPortId);
    if (jackPortId != KONFYT_JACK_PORT_ERROR) {

        PrjMidiPort p = prj->midiInPort_getPort(prjPortId);
        p.jackPortId = jackPortId;
        prj->midiInPort_replace(prjPortId, p);

        // Update filter in Jack
        jack->setPortFilter(jackPortId, p.filter);

        // Add to GUI
        gui_updateLayerMidiInPortsMenu();

        return prjPortId;

    } else {
        // Could not create Jack port. Remove port from project again.
        userMessage("ERROR: Could not add MIDI input port. Failed to create JACK port.");
        prj->midiInPort_removePort(prjPortId);
        return -1;
    }
}

void MainWindow::on_actionAdd_Audio_In_Port_triggered()
{
    addAudioInPort();
    showConnectionsPage();
}

void MainWindow::on_actionAdd_MIDI_In_Port_triggered()
{
    addMidiInPort();
    showConnectionsPage();
}

/* Adds new midi output port to project and Jack. Returns the port index.
 *  Returns -1 on error. */
int MainWindow::addMidiOutPort()
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) {
        userMessage("Select a project.");
        return -1;
    }


    int prjPortId = prj->midiOutPort_addPort("New MIDI Out Port"); // Add to current project
    int jackPortId = addMidiOutPortToJack(prjPortId);
    if (jackPortId != KONFYT_JACK_PORT_ERROR) {

        PrjMidiPort p = prj->midiOutPort_getPort(prjPortId);
        p.jackPortId = jackPortId;
        prj->midiOutPort_replace(prjPortId, p);

        // Add to GUI
        gui_updatePatchMidiOutPortsMenu();

        return prjPortId;
    } else {
        // Could not create Jack port. Remove port from project again.
        userMessage("ERROR: Could not add MIDI output port. Failed to create JACK port.");
        prj->midiOutPort_removePort(prjPortId);
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
 * to the given port number. The resulting Jack port IDs are assigned to the
 * leftPortId and rightPortId function parameters. */
void MainWindow::addAudioInPortsToJack(int portNo, int *leftPortId, int *rightPortId)
{
    *leftPortId = jack->addPort(KonfytJackPortType_AudioIn, "audio_in_" + n2s(portNo) + "_L");
    *rightPortId = jack->addPort(KonfytJackPortType_AudioIn, "audio_in_" + n2s(portNo) + "_R");
}

/* Adds a new MIDI output port to JACK, named with the specified port ID. The
 * JACK engine port ID is returned, and -1 if an error occured. */
int MainWindow::addMidiOutPortToJack(int portId)
{
    return jack->addPort(KonfytJackPortType_MidiOut, "midi_out_" + n2s(portId));
}

/* Adds a new MIDI port to JACK, named with the specified port ID. The JACK
 * engine port ID is returned, and -1 if an error occured. */
int MainWindow::addMidiInPortToJack(int portId)
{
    return jack->addPort(KonfytJackPortType_MidiIn, "midi_in_" + n2s(portId));
}

void MainWindow::setupExtAppMenu()
{
    extAppsMenuActions.insert( extAppsMenu.addAction("a2jmidid -ue (export hardware, without ALSA IDs)"),
                               "a2jmidid -ue" );
    extAppsMenuActions.insert( extAppsMenu.addAction("zynaddsubfx -l (Load .xmz state file)"),
                               "zynaddsubfx -l " );
    extAppsMenuActions.insert( extAppsMenu.addAction("zynaddsubfx -L (Load .xiz instrument file)"),
                               "zynaddsubfx -L " );
    extAppsMenuActions.insert( extAppsMenu.addAction("jack-keyboard"),
                               "jack-keyboard" );
    extAppsMenuActions.insert( extAppsMenu.addAction("VMPK (Virtual Keyboard)"),
                               "vmpk" );
    extAppsMenuActions.insert( extAppsMenu.addAction("Ardour"),
                               "ardour " );
    extAppsMenuActions.insert( extAppsMenu.addAction("Carla"),
                               "carla " );

    connect(&extAppsMenu, &QMenu::triggered, this, &MainWindow::extAppsMenuTriggered);
}


void MainWindow::on_pushButton_connectionsPage_OK_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);
}

void MainWindow::on_pushButton_ShowConnections_clicked()
{
    if (ui->stackedWidget->currentWidget() == ui->connectionsPage) {
        ui->stackedWidget->setCurrentWidget(ui->PatchPage);
    } else {
        showConnectionsPage();
    }
}

void MainWindow::on_tree_portsBusses_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if (current == NULL){ return; }

    // Enable MIDI Filter button if MIDI in port selected
    if ( current->parent() == midiInParent ) {
        // Midi input port is selected
        ui->pushButton_connectionsPage_MidiFilter->setVisible(true);
    } else {
        ui->pushButton_connectionsPage_MidiFilter->setVisible(false);
    }

    gui_updateConnectionsTree();
}

/* Remove the bus/port selected in the connections ports/busses tree widget. */
void MainWindow::on_actionRemove_BusPort_triggered()
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    QTreeWidgetItem* item = portsBussesTreeMenuItem;

    bool busSelected = item->parent() == busParent;
    bool audioInSelected = item->parent() == audioInParent;
    bool midiOutSelected = item->parent() == midiOutParent;
    bool midiInSelected = item->parent() == midiInParent;

    int id = 0;
    QString name;
    PrjAudioBus bus;
    PrjAudioInPort audioInPort;
    PrjMidiPort midiOutPort;
    PrjMidiPort midiInPort;

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
    } else if (midiInSelected) {
        if (prj->midiInPort_count() == 1) { return; } // Do not remove last MIDI in port
        id = tree_midiInMap.value(item);
        midiInPort = prj->midiInPort_getPort(id);
        name = midiInPort.portName;
    }

    // Check if any patch layers are using this bus/port
    QList<int> usingPatches;
    QList<int> usingLayers;
    QList<konfytPatch*> patchList = prj->getPatchList();
    for (int i=0; i<patchList.count(); i++) {
        konfytPatch* patch = patchList[i];
        QList<KonfytPatchLayer> layerList = patch->getLayerItems();
        for (int j=0; j<layerList.count(); j++) {
            KonfytPatchLayer layer = layerList[j];
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
            if (midiInSelected) {
                if ( (layer.getLayerType() == KonfytLayerType_CarlaPlugin)
                     || ( layer.getLayerType() == KonfytLayerType_MidiOut)
                     || ( layer.getLayerType() == KonfytLayerType_SoundfontProgram) ) {
                    append = layer.midiInPortIdInProject == id;
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
                // Change the bus for all layers still using this one
                for (int i=0; i<usingPatches.count(); i++) {
                    konfytPatch* patch = prj->getPatch(usingPatches[i]);
                    KonfytPatchLayer layer = patch->getLayerItems()[usingLayers[i]];
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
                    KonfytPatchLayer layer = patch->getLayerItems()[usingLayers[i]];
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
                    KonfytPatchLayer layer = patch->getLayerItems()[usingLayers[i]];
                    pengine->removeLayer( patch, &layer );
                }
                // Removal will be done below

            } else { return; } // Do not remove port
        // ------------------------------------------------------------------------------
        } else if (midiInSelected) {

            int portToChangeTo = prj->midiInPort_getFirstPortId(id);
            msgbox.setText("The selected MIDI input port " + selectedText + " is used by some patches."
                           + " Are you sure you want to delete the port?"
                           + " All layers using this port will be assigned to port "
                           + n2s(portToChangeTo) + " - "
                           + prj->midiInPort_getPort(portToChangeTo).portName + ".");
            int ret = msgbox.exec();
            if (ret == QMessageBox::Yes) {
                // User chose to remove port
                // Change the port for all layers still using this one
                for (int i=0; i<usingPatches.count(); i++) {
                    konfytPatch* patch = prj->getPatch(usingPatches[i]);
                    KonfytPatchLayer layer = patch->getLayerItems()[usingLayers[i]];
                    pengine->setLayerMidiInPort( patch, &layer, portToChangeTo );
                }
                // Removal will be done below.

            } else { return; } // Do not remove port

        }
    } // end usingPatches.count()

    // Remove the bus/port
    if (busSelected) {
        // Remove the bus
        jack->removePort(bus.leftJackPortId);
        jack->removePort(bus.rightJackPortId);
        prj->audioBus_remove(id);
        tree_busMap.remove(item);
    }
    else if (audioInSelected) {
        // Remove the port
        jack->removePort(audioInPort.leftJackPortId);
        jack->removePort(audioInPort.rightJackPortId);
        prj->audioInPort_remove(id);
        tree_audioInMap.remove(item);
        gui_updatePatchAudioInPortsMenu(); // TODO: why not just do this when opening the menu
    }
    else if (midiOutSelected) {
        // Remove the port
        jack->removePort(midiOutPort.jackPortId);
        prj->midiOutPort_removePort(id);
        tree_midiOutMap.remove(item);
        gui_updatePatchMidiOutPortsMenu(); // TODO: why not just do this when opening the menu
    } else if (midiInSelected) {
        // Remove the port
        jack->removePort(midiInPort.jackPortId);
        prj->midiInPort_removePort(id);
        tree_midiInMap.remove(item);
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
    KonfytProject* prj = getCurrentProject();
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
    KonfytProject* prj = getCurrentProject();
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Go through list of projects and ask to save for modified projects.

    for (int i=0; i<this->projectList.count(); i++) {
        KonfytProject* prj = projectList[i];
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

    event->accept();
}

/* Context menu requested for a library tree item. */
void MainWindow::on_treeWidget_Library_customContextMenuRequested(const QPoint &pos)
{
    libraryMenuItem = ui->treeWidget_Library->itemAt(pos);
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

        connect(process, static_cast<void (QProcess::*)(int)>(&QProcess::finished),
                [this, process](int exitCode){
            process->deleteLater();
        });

        process->start( this->filemanager, QStringList() << path );

    } else {
        QDesktopServices::openUrl( path );
    }
}

void MainWindow::showSfzContentsBelowLibrary(QString filename)
{
    ui->stackedWidget_libraryBottom->setCurrentWidget(ui->page_libraryBottom_Text);
    ui->textBrowser_LibraryBottom->clear();
    ui->textBrowser_LibraryBottom->append(loadSfzFileText(filename));
    QScrollBar* v = ui->textBrowser_LibraryBottom->verticalScrollBar();
    v->setValue(0);
    QScrollBar* h = ui->textBrowser_LibraryBottom->horizontalScrollBar();
    h->setValue(0);
}

QString MainWindow::loadSfzFileText(QString filename)
{
    QString text;

    QFileInfo fi(filename);
    QFile file(filename);

    if ( fi.size() > 1024*500 ) {

        userMessage("File exceeds max allowed size to show contents: " + filename);
        text = "File exceeds max allowed size to show contents.";

    } else if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {

        userMessage("Failed to open file: " + filename);
        text = "Failed to open file.";

    } else {

        text = QString(file.readAll());
        file.close();

    }

    return text;
}

void MainWindow::on_actionRename_BusPort_triggered()
{
    KonfytProject* prj = getCurrentProject();
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

    } else if (item->parent() == midiInParent) {
        // MIDI Input port selected
        ui->tree_portsBusses->editItem(item, 0);
    }
}

/* User has renamed a port or bus. */
void MainWindow::on_tree_portsBusses_itemChanged(QTreeWidgetItem *item, int column)
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    if (item->parent() == busParent) {
        // Bus is selected
        int id = tree_busMap.value( item );
        PrjAudioBus bus = prj->audioBus_getBus(id);

        bus.busName = item->text(0);
        prj->audioBus_replace(id, bus);

    } else if (item->parent() == audioInParent) {
        // Audio input port selected
        int id = tree_audioInMap.value(item);
        PrjAudioInPort p = prj->audioInPort_getPort(id);

        p.portName = item->text(0);
        prj->audioInPort_replace(id, p);

    } else if (item->parent() == midiOutParent) {
        // MIDI Output port selected
        int id = tree_midiOutMap.value(item);
        PrjMidiPort p = prj->midiOutPort_getPort(id);

        p.portName = item->text(0);
        prj->midiOutPort_replace(id, p);

    } else if (item->parent() == midiInParent) {
        // MIDI Input port selected
        int id = tree_midiInMap.value(item);
        PrjMidiPort p = prj->midiInPort_getPort(id);

        p.portName = item->text(0);
        prj->midiInPort_replace(id, p);
    }

    gui_updatePatchAudioInPortsMenu(); // TODO why not just do this when menu is opened?
    gui_updatePatchMidiOutPortsMenu(); // TODO why not just do this when menu is opened?

}



void MainWindow::on_pushButton_ShowTriggersPage_clicked()
{
    if (ui->stackedWidget->currentWidget() == ui->triggersPage) {
        ui->stackedWidget->setCurrentWidget(ui->PatchPage);
    } else {
        showTriggersPage();
    }
}

void MainWindow::on_pushButton_triggersPage_OK_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);
}

void MainWindow::on_pushButton_triggersPage_assign_clicked()
{
    QTreeWidgetItem* item = ui->tree_Triggers->currentItem();
    if (item == NULL) { return; }

    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    int eventRow = ui->listWidget_triggers_eventList->currentRow();
    if (eventRow < 0) { return; }

    KonfytMidiEvent selectedEvent = triggersLastEvents[eventRow];
    QAction* action = triggersItemActionHash[item];
    KonfytTrigger trig = KonfytTrigger();

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

    KonfytProject* prj = getCurrentProject();
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

void MainWindow::on_checkBox_Triggers_ProgSwitchPatches_clicked()
{
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    prj->setProgramChangeSwitchPatches( ui->checkBox_Triggers_ProgSwitchPatches->isChecked() );
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
    setCurrentProject(-1);
}

void MainWindow::on_actionProject_Open_triggered()
{
    // Show open dialog box
    QFileDialog* d = new QFileDialog();
    QString filename = d->getOpenFileName(this,
                                          "Select project to open",
                                          projectsDir,
                                          "*" + QString(PROJECT_FILENAME_EXTENSION) );
    if (filename == "") {
        userMessage("Cancelled.");
        return;
    }

    openProject(filename);
    // Switch to newly opened project
    setCurrentProject(-1);
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
    ui->actionPanicToggle->trigger();
}

void MainWindow::on_actionPanic_triggered()
{
    // Momentary panic
    // Enable panic state and disable after short time delay

    triggerPanic(true);

    QTimer* t = new QTimer(this);
    connect(t, &QTimer::timeout, [this, t](){
        triggerPanic(false);
        t->deleteLater();
    });
    t->start(100);
}

void MainWindow::on_actionPanicToggle_triggered()
{
    // Toggle panic state
    triggerPanic( !panicState );
}

void MainWindow::on_pushButton_LoadAll_clicked()
{
    KonfytProject* prj = getCurrentProject();
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

    KonfytProject* prj = getCurrentProject();
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

void MainWindow::on_pushButton_ShowJackPage_clicked()
{
    if (ui->stackedWidget->currentWidget() == ui->otherJackConsPage) {
        ui->stackedWidget->setCurrentWidget(ui->PatchPage);
    } else {
        showJackPage();
    }
}

void MainWindow::showJackPage()
{
    ui->stackedWidget->setCurrentWidget(ui->otherJackConsPage);
    ui->pushButton_JackAudioPorts->setChecked(jackPage_audio);
    ui->pushButton_JackMidiPorts->setChecked(!jackPage_audio);

    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    QStringList outPorts;
    QStringList inPorts;
    QList<KonfytJackConPair> conList;
    if (jackPage_audio) {
        // Audio Jack ports
        outPorts = jack->getAudioOutputPortsList();
        inPorts = jack->getAudioInputPortsList();
        conList = prj->getJackAudioConList();
    } else {
        // MIDI Jack ports
        outPorts = jack->getMidiOutputPortsList();
        inPorts = jack->getMidiInputPortsList();
        conList = prj->getJackMidiConList();
    }

    // Update JACK output ports
    ui->treeWidget_jackPortsOut->clear();
    for (int i=0; i < outPorts.count(); i++) {
        QString client_port = outPorts[i];
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0,client_port);
        ui->treeWidget_jackPortsOut->addTopLevelItem(item);
    }

    // Update JACK input ports
    ui->treeWidget_jackportsIn->clear();
    for (int i=0; i < inPorts.count(); i++) {
        QString client_port = inPorts[i];
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0,client_port);
        ui->treeWidget_jackportsIn->addTopLevelItem(item);
    }

    // Fill connections list with connections from project
    ui->listWidget_jackConnections->clear();
    for (int i=0; i < conList.count(); i++) {
        KonfytJackConPair portPair = conList[i];
        QListWidgetItem* item = new QListWidgetItem( portPair.toString() );
        // Colour red if one of the ports aren't present in JACK.
        if ( !outPorts.contains(portPair.srcPort) || !inPorts.contains(portPair.destPort) ) {
            item->setBackgroundColor(Qt::red);
        }
        ui->listWidget_jackConnections->addItem( item );
    }

    updateGUIWarnings();

}

void MainWindow::on_pushButton_jackConRefresh_clicked()
{
    showJackPage();
}

void MainWindow::on_pushButton_jackConAdd_clicked()
{
    QTreeWidgetItem* itemOut = ui->treeWidget_jackPortsOut->currentItem();
    QTreeWidgetItem* itemIn = ui->treeWidget_jackportsIn->currentItem();
    if ( (itemOut==NULL) || (itemIn==NULL) ) { return; }

    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    KonfytJackConPair p;
    if (jackPage_audio) {
        // Add Audio Jack port to project
        p = prj->addJackAudioCon(itemOut->text(0), itemIn->text(0));
    } else {
        // Add MIDI Jack port to project
        p = prj->addJackMidiCon(itemOut->text(0), itemIn->text(0));
    }

    // Add to Jack engine
    jack->addOtherJackConPair(p);
    // Add to jack connections GUI list
    ui->listWidget_jackConnections->addItem( p.toString() );

    updateGUIWarnings();
}

void MainWindow::on_pushButton_jackConRemove_clicked()
{
    int row = ui->listWidget_jackConnections->currentRow();
    if (row<0) { return; }
    KonfytProject* prj = getCurrentProject();
    if (prj == NULL) { return; }

    // Remove from project
    KonfytJackConPair p;
    if (jackPage_audio) {
        // Audio
        p = prj->removeJackAudioCon(row);
    } else {
        // MIDI
        p = prj->removeJackMidiCon(row);
    }
    // Remove from JACK
    jack->removeOtherJackConPair(p);
    // Remove from GUI
    delete ui->listWidget_jackConnections->item(row);

    updateGUIWarnings();

}

void MainWindow::on_checkBox_filesystem_ShowOnlySounds_toggled(bool checked)
{
    refreshFilesystemView();
}

void MainWindow::on_pushButton_LavaMonster_clicked()
{
    showAboutDialog();
}

void MainWindow::on_toolButton_PatchListMenu_clicked()
{
    KonfytProject* prj = this->getCurrentProject();
    if (prj == NULL) { return; }

    // Build patch list menu (first time only)
    if (patchListMenu.isEmpty()) {
        patchListMenu_NumbersAction = patchListMenu.addAction("Show patch numbers");
        patchListMenu_NumbersAction->setCheckable(true);
        connect(patchListMenu_NumbersAction, &QAction::triggered,
                this, &MainWindow::toggleShowPatchListNumbers);
        patchListMenu_NotesAction = patchListMenu.addAction("Show notes next to patches");
        patchListMenu_NotesAction->setCheckable(true);
        connect(patchListMenu_NotesAction, &QAction::triggered,
                this, &MainWindow::toggleShowPatchListNotes);
    }
    // Refresh menu items
    patchListMenu_NumbersAction->setChecked( prj->getShowPatchListNumbers() );
    patchListMenu_NotesAction->setChecked( prj->getShowPatchListNotes() );

    // Show menu
    patchListMenu.popup(QCursor::pos());
}

void MainWindow::toggleShowPatchListNumbers()
{
    KonfytProject* prj = this->getCurrentProject();
    if (prj == NULL) { return; }
    prj->setShowPatchListNumbers( !prj->getShowPatchListNumbers() );
    gui_updatePatchList();
}

void MainWindow::toggleShowPatchListNotes()
{
    // Toggle option in project
    KonfytProject* prj = this->getCurrentProject();
    if (prj == NULL) { return; }
    prj->setShowPatchListNotes( !prj->getShowPatchListNotes() );
    gui_updatePatchList();
}


void MainWindow::on_pushButton_JackAudioPorts_clicked()
{
    jackPage_audio = true;
    showJackPage();
}

void MainWindow::on_pushButton_JackMidiPorts_clicked()
{
    jackPage_audio = false;
    showJackPage();
}

void MainWindow::on_pushButton_connectionsPage_MidiFilter_clicked()
{
    if ( ui->tree_portsBusses->currentItem()->parent() == midiInParent ) {
        // Midi input port is selected
        midiFilterEditPort = tree_midiInMap.value( ui->tree_portsBusses->currentItem() );
        midiFilterEditType = MidiFilterEditPort;
        showMidiFilterEditor();
    }
}

void MainWindow::on_toolButton_MidiFilter_VelLimitMin_last_clicked()
{
    ui->spinBox_midiFilter_VelLimitMin->setValue( midiFilter_lastData2 );
}

/* User right-clicked on panic button. */
void MainWindow::on_pushButton_Panic_customContextMenuRequested(const QPoint &pos)
{
    // Momentary panic
    on_actionPanic_triggered();
}

void MainWindow::on_toolButton_MidiFilter_VelLimitMax_last_clicked()
{
    ui->spinBox_midiFilter_VelLimitMax->setValue( midiFilter_lastData2 );
}

void MainWindow::on_pushButton_Test_clicked()
{
    userMessage("Test button: Launching another Konfyt instance.");

    // TODO 2018-08-08

    /*
    newPatchIfMasterNull();

    QString path = app->arguments().at(0);
    // Create process and launch

    // Add layer to engine
    KonfytPatchLayer g = pengine->addSfzLayer(sfzPath);

    // Add layer to GUI
    addLayerItemToGUI(g);

    setPatchModified(true);
    */
}
