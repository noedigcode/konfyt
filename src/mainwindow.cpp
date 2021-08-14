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

#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent, KonfytAppInfo appInfoArg) :
    QMainWindow(parent),
    appInfo(appInfoArg),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setupConsoleDialog();

    setupGuiStyle();
    printArgumentsInfo();
    initAboutDialog();
    setupSettings();
    setupJack();
    setupPatchEngine();

    // ----------------------------------------------------
    // The following need to happen before loading project or cmdline arguments
    initTriggers();
    setupFilesystemView();
    setupPatchListAdapter();
    setupDatabase();
    setupSavedMidiSendItems();
    // ----------------------------------------------------
    setupInitialProjectFromCmdLineArgs();

    scanDirForProjects(projectsDir);
    setupGuiMenuButtons();
    setupConnectionsPage();
    setupTriggersPage();
    setupMidiSendListEditor();
    setupKeyboardShortcuts();
    setupGuiDefaults();
    setupExternalAppsMenu();
    setMasterInTranspose(0, false);
    setMasterGain(1);

    // Show welcome message in statusbar
    QString app_name(APP_NAME);
    ui->statusBar->showMessage("Welkom by " + app_name + ".",5000);
}

MainWindow::~MainWindow()
{
    mBlockPrint = true;

    jack.stopJackClient();
    consoleDialog.close();

    delete ui;
}

void MainWindow::setupGuiStyle()
{
    QString stylename = "Fusion";
    QStyle* style = QStyleFactory::create(stylename);
    if (style) {
        appInfo.a->setStyle(style);
    } else {
        print("Unable to create style " + stylename);
    }
}

void MainWindow::setupGuiMenuButtons()
{
    // Add-patch button menu
    QMenu* addPatchMenu = new QMenu();
    addPatchMenu->addAction(ui->actionNew_Patch);
    addPatchMenu->addAction(ui->actionAdd_Patch_From_File);
    ui->toolButton_AddPatch->setMenu(addPatchMenu);

    // Save-patch button menu
    QMenu* savePatchMenu = new QMenu();
    savePatchMenu->addAction(ui->actionAlways_Active);
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
    projectButtonMenu->addAction(ui->actionProject_SaveAs);
    ui->toolButton_Project->setMenu(projectButtonMenu);


    // Add-midi-port-to-patch button
    connect(&patchMidiOutPortsMenu, &QMenu::aboutToShow,
            this, &MainWindow::onPatchMidiOutPortsMenu_aboutToShow);
    connect(&patchMidiOutPortsMenu, &QMenu::triggered,
            this, &MainWindow::onPatchMidiOutPortsMenu_ActionTrigger);
    ui->toolButton_layer_AddMidiPort->setMenu(&patchMidiOutPortsMenu);

    // Button: add audio input port to patch
    connect(&patchAudioInPortsMenu, &QMenu::aboutToShow,
            this, &MainWindow::onPatchAudioInPortsMenu_aboutToShow);
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
    layerMidiInPortsMenu.setTitle("MIDI In Port");
    connect(&layerMidiInPortsMenu, &QMenu::triggered,
            this, &MainWindow::onLayerMidiInPortsMenu_ActionTrigger);

    // Layer MIDI input channel menu
    layerMidiInChannelMenu.setTitle("MIDI In Channel");
    connect(&layerMidiInChannelMenu, &QMenu::triggered,
            this, &MainWindow::onLayerMidiInChannelMenu_ActionTrigger);

    // Preview button menu
    connect(&previewButtonMenu, &QMenu::aboutToShow,
            this, &MainWindow::preparePreviewMenu);
    ui->toolButton_LibraryPreview->setMenu(&previewButtonMenu);

    ui->tabWidget_library->setCornerWidget(ui->frame_preview);
    //ui->tabWidget_library->tabBar()->setFixedHeight(ui->frame_preview->height() + 2);
}

void MainWindow::setupGuiDefaults()
{
    // Bridge testing area visibility
    ui->groupBox_Testing->setVisible(appInfo.bridge);

    // Resize some layouts
    QList<int> sizes;
    sizes << 8 << 2;
    ui->splitter_library->setSizes( sizes );

    // Right Sidebar
    // Hide the bottom tabs used for experimentation
    ui->tabWidget_right->tabBar()->setVisible(false);
    ui->tabWidget_right->setCurrentIndex(0);

    // Console stacked widget page 1 houses the JACK engine warning and should never be seen
    ui->stackedWidget_Console->setCurrentIndex(0);

    // Show library view (not live mode)
    ui->stackedWidget_left->setCurrentWidget(ui->pageLibrary);

    // Show default view
    if (mSettingsFirstRun) {
        // Show settings on first run
        showSettingsDialog();
    } else {
        // Show normal patch view
        ui->stackedWidget->setCurrentWidget(ui->PatchPage);
    }
}

void MainWindow::printArgumentsInfo()
{
    print(QString(APP_NAME) + " " + APP_VERSION);
    print("Arguments:");
    if (appInfo.carla) { print(" - Carla mode"); }
    if (appInfo.bridge) { print(" - Bridging is enabled."); }


    print(QString(" - Files to load: %1")
          .arg(appInfo.filesToLoad.count() ? "" : "None specified"));
    for (int i=0; i < appInfo.filesToLoad.count(); i++) {
        print("   - " + appInfo.filesToLoad[i]);
    }

    print(QString(" - JackClientName: %1")
          .arg(appInfo.jackClientName.isEmpty() ? "Not specified" : appInfo.jackClientName));

}

void MainWindow::shortcut_save_activated()
{
    ui->actionProject_save->trigger();
}

void MainWindow::shortcut_panic_activated()
{
    ui->actionPanicToggle->trigger();
}

/* Build project-open menu with an Open action and a list of projects in the
 * projects dir. */
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
    if (!requestCurrentProjectClose()) { return; }

    if ( projectsMenuMap.contains(action) ) {
        QFileInfo fi = projectsMenuMap.value(action);
        loadProjectFromFile(fi.filePath()); // Open project from file and load it
    }
}

void MainWindow::onJackXrunOccurred()
{
    mJackXrunCount += 1;
    QString text = QString("XRUN %1").arg(mJackXrunCount);
    if (mJackXrunCount > 1) {
        text += QString(" (%1)").arg(xrunTimeString(mXrunTimer.elapsed()));
    }
    mXrunTimer.start();
    print(text);
}

void MainWindow::onJackPortRegisteredOrConnected()
{
    // Refresh ports/connections tree
    updateConnectionsTree();

    // Refresh the other JACK connections page
    updateJackPage();

    // Update warnings section
    updateGUIWarnings();
}

/* Scan given directory recursively and add project files to list. */
void MainWindow::scanDirForProjects(QString dirname)
{
    if (!dirExists(dirname)) {
        print("Projects directory does not exist: " + dirname);
    } else {
        projectDirList = scanDirForFiles(dirname, PROJECT_FILENAME_EXTENSION);
    }
}

void MainWindow::showSettingsDialog()
{
    ui->comboBox_settings_patchDirs->setCurrentText(this->mPatchesDir);
    ui->comboBox_settings_projectsDir->setCurrentText(this->projectsDir);
    ui->comboBox_settings_sfzDirs->setCurrentText(this->mSfzDir);
    ui->comboBox_settings_soundfontDirs->setCurrentText(this->mSoundfontsDir);

    int i = ui->comboBox_Settings_filemanager->findText( this->mFilemanager );
    if (i>=0) {
        ui->comboBox_Settings_filemanager->setCurrentIndex(i);
    } else {
        ui->comboBox_Settings_filemanager->addItem(this->mFilemanager);
        ui->comboBox_Settings_filemanager->setCurrentIndex( ui->comboBox_Settings_filemanager->count()-1 );
    }

    // Switch to settings page
    ui->stackedWidget->setCurrentWidget(ui->SettingsPage);
}

void MainWindow::updateMidiFilterEditorLastRx(KonfytMidiEvent ev)
{
    midiFilterLastEvent = ev;
    ui->lineEdit_MidiFilter_Last->setText(ev.toString());
}

void MainWindow::showMidiFilterEditor()
{
    // Switch to midi filter view

    KonfytMidiFilter f;
    if (midiFilterEditType == MidiFilterEditPort) {
        if (!mCurrentProject) { return; }
        f = mCurrentProject->midiInPort_getPort(midiFilterEditPort).filter;
    } else {
        f = midiFilterEditItem->getPatchLayer().toStrongRef()->midiFilter();
    }

    KonfytMidiFilterZone z = f.zone;
    ui->spinBox_midiFilter_LowNote->setValue(z.lowNote);
    ui->spinBox_midiFilter_HighNote->setValue(z.highNote);
    ui->spinBox_midiFilter_Add->setValue(z.add);
    ui->spinBox_midiFilter_LowVel->setValue(z.lowVel);
    ui->spinBox_midiFilter_HighVel->setValue(z.highVel);
    ui->spinBox_midiFilter_VelLimitMin->setValue(z.velLimitMin);
    ui->spinBox_midiFilter_VelLimitMax->setValue(z.velLimitMax);

    ui->checkBox_midiFilter_ignoreGlobalTranspose->setChecked(f.ignoreGlobalTranspose);

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

    // Switch to midi filter page
    ui->stackedWidget->setCurrentWidget(ui->FilterPage);
}

void MainWindow::applySettings()
{
    // Get settings from dialog.
    projectsDir = ui->comboBox_settings_projectsDir->currentText();
    setPatchesDir(ui->comboBox_settings_patchDirs->currentText());
    setSoundfontsDir(ui->comboBox_settings_soundfontDirs->currentText());
    setSfzDir(ui->comboBox_settings_sfzDirs->currentText());
    mFilemanager = ui->comboBox_Settings_filemanager->currentText();

    print("Settings applied.");

    // Save the settings.
    if (saveSettingsFile()) {
        print("Settings saved.");
    } else {
        print("Failed to save settings to file.");
    }

    // Create directories if they don't exist
    QDir d;
    d.mkpath(projectsDir);
    d.mkpath(mPatchesDir);
    d.mkpath(mSoundfontsDir);
    d.mkpath(mSfzDir);
}

bool MainWindow::loadSettingsFile(QString dir)
{
    QString filename = dir + "/" + SETTINGS_FILE;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        print("Failed to open settings file: " + filename);
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
                    setSoundfontsDir(r.readElementText());
                } else if (r.name() == XML_SETTINGS_PATCHESDIR) {
                    setPatchesDir(r.readElementText());
                } else if (r.name() == XML_SETTINGS_SFZDIR) {
                    setSfzDir(r.readElementText());
                } else if (r.name() == XML_SETTINGS_FILEMAN) {
                    mFilemanager = r.readElementText();
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
    createSettingsDir();

    // Open settings file for writing.
    QString filename = settingsDir + "/" + SETTINGS_FILE;
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        print("Failed to open settings file for writing: " + filename);
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
    stream.writeTextElement(XML_SETTINGS_SFDIR, mSoundfontsDir);
    stream.writeTextElement(XML_SETTINGS_PATCHESDIR, mPatchesDir);
    stream.writeTextElement(XML_SETTINGS_SFZDIR, mSfzDir);
    stream.writeTextElement(XML_SETTINGS_FILEMAN, mFilemanager);

    stream.writeEndElement(); // Settings

    stream.writeEndDocument();

    file.close();
    return true;
}

/* Create a new project and load it. */
void MainWindow::loadNewProject()
{
    ProjectPtr prj(new KonfytProject());
    prj->setProjectName("New Project");
    loadProject(prj);
}

/* Open a project from the specified filename and load it. */
bool MainWindow::loadProjectFromFile(QString filename)
{
    ProjectPtr prj(new KonfytProject());

    if (prj->loadProject(filename)) {
        print("Project loaded from file.");
        // Load project in engines and GUI
        loadProject(prj);
        return true;
    } else {
        print("Failed to load project from file: " + filename);
        messageBox("Error loading project " + filename);
        return false;
    }
}

void MainWindow::setupConnectionsPage()
{
    // Connections tree widget column widths
    ui->tree_Connections->header()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tree_Connections->header()->setStretchLastSection(false);
    ui->tree_Connections->header()->setSectionResizeMode(TREECON_COL_L, QHeaderView::Fixed);
    ui->tree_Connections->header()->resizeSection(TREECON_COL_L, 30);
    ui->tree_Connections->header()->setSectionResizeMode(TREECON_COL_R, QHeaderView::Fixed);
    ui->tree_Connections->header()->resizeSection(TREECON_COL_R, 30);

    // Set up portsBuses tree context menu
    ui->tree_portsBusses->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_portsBusses, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::tree_portsBusses_Menu);
}

void MainWindow::showConnectionsPage()
{
    ui->stackedWidget->setCurrentWidget(ui->connectionsPage);
    ui->frame_connectionsPage_MidiFilter->setVisible(false);
    ui->checkBox_connectionsPage_ignoreGlobalVolume->setVisible(false);

    updatePortsBussesTree();
    updateConnectionsTree();
}

void MainWindow::connectionsTreeSelectBus(int busId)
{
    ui->tree_portsBusses->setCurrentItem( tree_busMap.key(busId) );
}

void MainWindow::connectionsTreeSelectAudioInPort(int portId)
{
    ui->tree_portsBusses->setCurrentItem( tree_audioInMap.key(portId) );
}

void MainWindow::connectionsTreeSelectMidiInPort(int portId)
{
    ui->tree_portsBusses->setCurrentItem( tree_midiInMap.key(portId) );
}

void MainWindow::connectionsTreeSelectMidiOutPort(int portId)
{
    ui->tree_portsBusses->setCurrentItem( tree_midiOutMap.key(portId) );
}

void MainWindow::updatePortsBussesTree()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    /* Buses
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

    clearPortsBussesConnectionsData();

    busParent = new QTreeWidgetItem();
    busParent->setText(0, "Buses");
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

void MainWindow::updateConnectionsTree()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) {
        clearConnectionsTree();
        return;
    }

    if (ui->tree_portsBusses->currentItem() == nullptr) {
        clearConnectionsTree();
        return;
    }

    // Get list of JACK ports, depending on the selected tree item.
    QStringList l; // List of Jack client:ports
    int j; // Index/id of bus/port
    if ( ui->tree_portsBusses->currentItem()->parent() == busParent ) {
        // A bus is selected
        l = jack.getAudioInputPortsList();
        j = tree_busMap.value( ui->tree_portsBusses->currentItem() );
    } else if ( ui->tree_portsBusses->currentItem()->parent() == audioInParent ) {
        // An audio input port is selected
        l = jack.getAudioOutputPortsList();
        j = tree_audioInMap.value( ui->tree_portsBusses->currentItem() );
    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiOutParent ) {
        // A midi output port is selected
        l = jack.getMidiInputPortsList();
        j = tree_midiOutMap.value( ui->tree_portsBusses->currentItem() );
    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiInParent ) {
        // Midi input port is selected
        l = jack.getMidiOutputPortsList();
        j = tree_midiInMap.value( ui->tree_portsBusses->currentItem() );
    } else {
        // One of the parents are selected.
        clearConnectionsTree();
        return;
    }

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

    QList<QString> portsInTree = conPortsMap.values();

    // Determine which JACK ports to add to tree
    QStringList toAdd;
    for (int i=0; i < l.count(); i++) {
        if (!portsInTree.contains(l[i])) {
            toAdd.append(l[i]);
        }
    }

    // Also add ports marked as connected in project, but which are not in tree
    for (int i=0; i < leftCons.count(); i++) {
        QString p = leftCons[i];
        if (!portsInTree.contains(p) && !toAdd.contains(p)) {
            toAdd.append(p);
        }
    }
    for (int i=0; i < rightCons.count(); i++) {
        QString p = rightCons[i];
        if (!portsInTree.contains(p) && !toAdd.contains(p)) {
            toAdd.append(p);
        }
    }

    // Determine which JACK ports to remove from tree
    QStringList toRemTemp;
    for (int i=0; i < portsInTree.count(); i++) {
        if (!l.contains(portsInTree[i])) {
            toRemTemp.append(portsInTree[i]);
        }
    }

    // Don't remove ports which have connections in the project
    QStringList toRem;
    for (int i=0; i < toRemTemp.count(); i++) {
        QString port = toRemTemp[i];
        if ( (!leftCons.contains(port)) && (!rightCons.contains(port)) ) {
            toRem.append(toRemTemp[i]);
        }
    }


    // Now, remove all ports that are in toRem
    for (int i=0; i < toRem.count(); i++) {
        QString rem = toRem[i];
        QTreeWidgetItem* item = conPortsMap.key(rem);
        conPortsMap.remove(item);
        QCheckBox* c = conChecksMap1.key(item);
        conChecksMap1.remove(c);
        c = conChecksMap2.key(item);
        conChecksMap2.remove(c);
        delete item;
    }

    QStringList ourJackClients;
    ourJackClients.append(jack.clientName());
    ourJackClients.append(pengine.ourJackClientNames());

    // Add all ports that are in toAdd
    for (int i=0; i < toAdd.count(); i++) {
        QString add = toAdd[i];
        // Skip our client ports
        if (!jackPortBelongstoUs(add)) {
            addClientPortToTree(add);
        }
    }

    // Remove empty clients
    QList<QTreeWidgetItem*> clients = conClientsMap.values();
    for (int i=0; i < clients.count(); i++) {
        QTreeWidgetItem* client = clients[i];
        if (client->childCount() == 0) {
            QString c = conClientsMap.key(client);
            conClientsMap.remove(c);
            delete client;
        }
    }

    QColor activeColor = QColor(Qt::transparent);
    QColor inactiveColor = QColor(Qt::red);

    // Mark items red if not active and set checkboxes
    QList<QTreeWidgetItem*> items = conPortsMap.keys();
    for (int i=0; i < items.count(); i++) {
        QTreeWidgetItem* item = items[i];
        QString port = conPortsMap[item];
        if ( !l.contains(port) ) {
            // Mark red
            item->setBackground(0, QBrush(inactiveColor));
        } else {
            // Do not mark red
            item->setBackground(0, QBrush(activeColor));
        }
        QCheckBox* cb = conChecksMap1.key(item, nullptr);
        if (cb) { cb->setChecked(leftCons.contains(port)); }
        cb = conChecksMap2.key(item, nullptr);
        if (cb) { cb->setChecked(rightCons.contains(port)); }
    }

    ui->tree_Connections->sortItems(0, Qt::AscendingOrder);
    ui->tree_Connections->expandAll();

}

void MainWindow::clearPortsBussesConnectionsData()
{
    // Clear tree before deleting items so that the onItemChanged signal is not emitted while
    // deleting the items.
    ui->tree_portsBusses->clear();

    // Delete all tree items
    if (busParent) {
        tree_busMap.clear();
        tree_audioInMap.clear();
        tree_midiOutMap.clear();
        tree_midiInMap.clear();
    }

    busParent = nullptr;
    audioInParent = nullptr;
    midiOutParent = nullptr;
    midiInParent = nullptr;

    clearConnectionsTree();
}

void MainWindow::clearConnectionsTree()
{
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
}

/* Helper function to add a Jack client:port string to the connections tree. */
void MainWindow::addClientPortToTree(QString jackport)
{
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
}

/* One of the connections page ports checkboxes have been clicked. */
void MainWindow::checkboxes_clicked_slot(QCheckBox *c)
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

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

        KfJackAudioPort* jackPort;
        if (leftRight == leftPort) { jackPort = bus.leftJackPort; }
        else { jackPort = bus.rightJackPort; }

        if (c->isChecked()) {
            // Connect
            jack.addPortClient(jackPort, portString);
            // Also add in project
            prj->audioBus_addClient(busId, leftRight, portString);
        } else {
            // Disconnect
            jack.removeAndDisconnectPortClient(jackPort, portString);
            // Also remove in project
            prj->audioBus_removeClient(busId, leftRight, portString);
        }


    } else if ( ui->tree_portsBusses->currentItem()->parent() == audioInParent ) {
        // An audio input port is selected
        int portId = tree_audioInMap.value( ui->tree_portsBusses->currentItem() );
        PrjAudioInPort p = prj->audioInPort_getPort(portId);

        KfJackAudioPort* jackPort;
        if (leftRight == leftPort) { jackPort = p.leftJackPort; }
        else { jackPort = p.rightJackPort; }

        if (c->isChecked()) {
            // Connect
            jack.addPortClient(jackPort, portString);
            // Also add in project
            prj->audioInPort_addClient(portId, leftRight, portString);
        } else {
            // Disconnect
            jack.removeAndDisconnectPortClient(jackPort, portString);
            // Also remove in project
            prj->audioInPort_removeClient(portId, leftRight, portString);
        }

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiOutParent ) {
        // A midi output port is selected
        int portId = tree_midiOutMap.value( ui->tree_portsBusses->currentItem() );
        PrjMidiPort p = prj->midiOutPort_getPort(portId);

        if (c->isChecked()) {
            // Connect
            jack.addPortClient(p.jackPort, portString);
            // Also add in project
            prj->midiOutPort_addClient(portId, portString);
        } else {
            // Disconnect
            jack.removeAndDisconnectPortClient(p.jackPort, portString);
            // Also remove in project
            prj->midiOutPort_removeClient(portId, portString);
        }

    } else if ( ui->tree_portsBusses->currentItem()->parent() == midiInParent ) {
        // Midi input is selected
        int portId = tree_midiInMap.value( ui->tree_portsBusses->currentItem() );
        PrjMidiPort p = prj->midiInPort_getPort(portId);

        if (c->isChecked()) {
            // Connect
            jack.addPortClient(p.jackPort, portString);
            // Also add in project
            prj->midiInPort_addClient(portId, portString);
        } else {
            // Disconnect
            jack.removeAndDisconnectPortClient(p.jackPort, portString);
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
    QMenu* m = &portsBussesTreeMenu;

    m->clear();

    QTreeWidgetItem* item = ui->tree_portsBusses->itemAt(pos);
    portsBussesTreeMenuItem = item;

    if (item) {
        if (item->parent()) {
            m->addAction(ui->actionRename_BusPort);
            m->addAction(ui->actionRemove_BusPort);
            m->addSeparator();
        }
    }
    m->addAction(ui->actionAdd_Bus);
    m->addAction(ui->actionAdd_Audio_In_Port);
    m->addAction(ui->actionAdd_MIDI_Out_Port);
    m->addAction(ui->actionAdd_MIDI_In_Port);

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

void MainWindow::setupTriggersPage()
{
    // Tree widget column widths
    ui->tree_Triggers->header()->setSectionResizeMode(QHeaderView::Stretch);

    // Received MIDI events list
    connect(&triggersPageMidiEventList, &MidiEventListWidgetAdapter::itemDoubleClicked,
            this, &MainWindow::onTriggersMidiEventListDoubleClicked);
    triggersPageMidiEventList.init(ui->listWidget_triggers_eventList);
}

void MainWindow::showTriggersPage()
{
    ui->stackedWidget->setCurrentWidget(ui->triggersPage);

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

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

/* Load the specified project in the required engines and GUI. Unload the
 * previous active project. */
void MainWindow::loadProject(ProjectPtr prj)
{
    if (!prj) {
        print("ERROR: loadProject: null pointer");
        return;
    }

    unloadCurrentProject();

    mCurrentProject = prj;
    pengine.setProject(mCurrentProject);

    updateProjectNameInGui();
    // Populate patch list for current project
    patchListAdapter.clear();
    patchListAdapter.addPatches(mCurrentProject->getPatchList());


    // Process MIDI in ports

    QList<int> midiInIds = prj->midiInPort_getAllPortIds();
    for (int j=0; j < midiInIds.count(); j++) {
        int prjPortId = midiInIds[j];
        // Add to Jack, and update Jack port reference in project
        // TODO: PROJECT IS NOT TECHNICALLY THE BEST PLACE TO KEEP THE PORT
        // REFERENCE. KEEP IN MAINWINDOW OR PATCHENGINE.
        prj->midiInPort_setJackPort(prjPortId, addMidiInPortToJack(prjPortId));

        // Also add port clients to Jack
        PrjMidiPort projectPort = prj->midiInPort_getPort(prjPortId);
        foreach (QString client, projectPort.clients) {
            jack.addPortClient(projectPort.jackPort, client);
        }

        // Set port midi filter in Jack
        jack.setPortFilter(projectPort.jackPort, projectPort.filter);
    }


    // Process MIDI out ports

    QList<int> midiOutIds = prj->midiOutPort_getAllPortIds();
    for (int j=0; j<midiOutIds.count(); j++) {
        int prjPortId = midiOutIds[j];
        // Add to Jack, and update Jack port reference in project
        prj->midiOutPort_setJackPort(prjPortId, addMidiOutPortToJack(prjPortId));

        // Also add port clients to Jack
        PrjMidiPort projectPort = prj->midiOutPort_getPort(prjPortId);
        foreach (QString client, projectPort.clients) {
            jack.addPortClient(projectPort.jackPort, client);
        }
    }

    // Audio Buses (output ports)

    QList<int> busIds = prj->audioBus_getAllBusIds();
    for (int j=0; j<busIds.count(); j++) {
        int id = busIds[j];
        PrjAudioBus b =  prj->audioBus_getBus(id);

        // Add audio bus ports to jack client
        KfJackAudioPort* left;
        KfJackAudioPort* right;
        addAudioBusToJack( id, &left, &right);
        if ( (left != nullptr) && (right != nullptr) ) {
            // Update left and right port references of bus in project
            b.leftJackPort = left;
            b.rightJackPort = right;
            prj->audioBus_replace_noModify( id, b ); // use noModify function as to not change the project's modified state.
            // Add port clients to jack client
            foreach (QString client, b.leftOutClients) {
                jack.addPortClient(b.leftJackPort, client);
            }
            foreach (QString client, b.rightOutClients) {
                jack.addPortClient(b.rightJackPort, client);
            }
        } else {
            print("ERROR: setCurrentProject: Failed to create audio bus Jack port(s).");
        }
        updateBusGainInJackEngine(id);

    }

    // Audio input ports

    QList<int> audioInIds = prj->audioInPort_getAllPortIds();
    for (int j=0; j<audioInIds.count(); j++) {
        int id = audioInIds[j];

        // Add audio ports to jack client
        KfJackAudioPort* left;
        KfJackAudioPort* right;
        addAudioInPortsToJack( id, &left, &right );
        if ((left != nullptr) && (right != nullptr)) {
            // Update left and right port numbers in project
            prj->audioInPort_setJackPorts(id, left, right);
            // Add port clients to jack client
            PrjAudioInPort p = prj->audioInPort_getPort(id);
            foreach (QString client, p.leftInClients) {
                jack.addPortClient(p.leftJackPort, client);
            }
            foreach (QString client, p.rightInClients) {
                jack.addPortClient(p.rightJackPort, client);
            }
        } else {
            print("ERROR: setCurrentProject: Failed to create audio input Jack port(s).");
        }

    }

    // Update external applications list
    ui->listWidget_ExtApps->clear();
    QList<KonfytProcess*> prl = prj->getProcessList();
    for (int j=0; j<prl.count(); j++) {
        KonfytProcess* gp = prl.at(j);
        QString temp = gp->toString();
        if (gp->isRunning()) {
            temp = "[running] " + temp;
        }
        ui->listWidget_ExtApps->addItem(temp);
    }
    // Connect signals
    connect(prj.data(), &KonfytProject::print, this, &MainWindow::print);
    connect(prj.data(), &KonfytProject::processStartedSignal,
            this, &MainWindow::processStartedSlot);
    connect(prj.data(), &KonfytProject::processFinishedSignal,
            this, &MainWindow::processFinishedSlot);
    connect(prj.data(), &KonfytProject::projectModifiedChanged,
            this, &MainWindow::onProjectModifiedStateChanged);
    connect(prj.data(), &KonfytProject::midiPickupRangeChanged,
            this, &MainWindow::onProjectMidiPickupRangeChanged);

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

    // Update other JACK connections in JACK

    // MIDI
    QList<KonfytJackConPair> jackCons = prj->getJackMidiConList();
    for (int i=0; i < jackCons.count(); i++) {
        jack.addOtherJackConPair( jackCons[i] );
    }
    // Audio
    jackCons = prj->getJackAudioConList();
    for (int i=0; i < jackCons.count(); i++) {
        jack.addOtherJackConPair( jackCons[i] );
    }

    // Update project modified indication in GUI
    onProjectModifiedStateChanged(prj->isModified());

    onProjectMidiPickupRangeChanged(prj->getMidiPickupRange());

    mCurrentPatch = nullptr;
    updatePatchView();


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

    // Default to patch view (ensure no edit screens are open for previous projects)
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);


    print("Project loaded.");
}

void MainWindow::unloadCurrentProject()
{
    if (!mCurrentProject) { return; }

    mCurrentProject->disconnect();

    clearPortsBussesConnectionsData();

    foreach (KonfytPatch* patch, mCurrentProject->getPatchList()) {
        pengine.unloadPatch(patch);
    }

    // Remove all JACK audio and MIDI in/out ports
    jack.removeAllAudioInAndOutPorts();
    jack.removeAllMidiInAndOutPorts();
    jack.clearOtherJackConPair();
}

/* Clears and fills the specified menu with items corresponding to project MIDI
 * output ports and a new port entry. A property is set for each action
 * corresponding to the port id and -1 for the new port entry. */
void MainWindow::updateMidiOutPortsMenu(QMenu *menu)
{
    menu->clear();

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    QList<int> midiOutIds = prj->midiOutPort_getAllPortIds();
    foreach (int id, midiOutIds) {
        PrjMidiPort projectPort = prj->midiOutPort_getPort(id);
        QAction* action = patchMidiOutPortsMenu.addAction( n2s(id) + " " + projectPort.portName);
        action->setProperty(PTY_MIDI_OUT_PORT, id);
    }

    menu->addSeparator();
    QAction* action = menu->addAction("New Port");
    action->setProperty(PTY_MIDI_OUT_PORT, -1);
}

/* Clears and fills specified menu with items corresponding to project audio
 * input ports and a new port entry. A property is set for each action
 * corresponding to the port id and -1 for the new port entry. */
void MainWindow::updateAudioInPortsMenu(QMenu *menu)
{
    menu->clear();

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    QList<int> audioInIds = prj->audioInPort_getAllPortIds();
    foreach (int id, audioInIds) {
        PrjAudioInPort projectPort = prj->audioInPort_getPort(id);
        QAction* action = menu->addAction( n2s(id) + " " + projectPort.portName );
        action->setProperty(PTY_AUDIO_IN_PORT, id);
    }

    menu->addSeparator();
    QAction* action = menu->addAction("New Port");
    action->setProperty(PTY_AUDIO_IN_PORT, -1);
}

/* Create a new patch, and add it to the current project. (And update the GUI.) */
KonfytPatch *MainWindow::newPatchToProject()
{
    KonfytPatch* patch = new KonfytPatch();
    patch->setName("New Patch");

    addPatchToProject(patch);

    return patch;
}

/* Remove the patch with specified index from the project. */
void MainWindow::removePatchFromProject(int i)
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    if ( (i>=0) && (i<prj->getNumPatches()) ) {

        // Remove from project
        KonfytPatch* patch = prj->removePatch(i);

        // Remove from patch engine
        pengine.unloadPatch(patch);

        // Remove from GUI
        patchListAdapter.removePatch(patch);

        if (mCurrentPatch == patch) {
            mCurrentPatch = nullptr;
            updatePatchView();
        }
        print("Patch Removed.");

        // Delete the patch
        delete patch;
    }
}

/* Add a patch to the current project, and update the GUI. */
void MainWindow::addPatchToProject(KonfytPatch* patch)
{
    if (!mCurrentProject) {
        print("ERROR: addPatchToProject: No active project.");
        return;
    }

    mCurrentProject->addPatch(patch);
    // Add to list in gui
    patchListAdapter.addPatch(patch);
}

KonfytPatch *MainWindow::addPatchToProjectFromFile(QString filename)
{
    if (!mCurrentProject) {
        print("ERROR: addPatchToProjectFromFile: No active project.");
        return nullptr;
    }

    KonfytPatch* pt = new KonfytPatch();
    QString errors;
    if (pt->loadPatchFromFile(filename, &errors)) {
        addPatchToProject(pt);
    } else {
        print("Failed loading patch from file: " + filename);
        delete pt;
        pt = nullptr;
    }
    if (!errors.isEmpty()) {
        print("Load errors for patch " + filename + ":\n" + errors);
    }

    return pt;
}

/* Returns true if a soundfont program is selected in the library/filesystem. */
bool MainWindow::isSoundfontProgramSelectedInLibOrFs()
{
    return (selectedSfont && (ui->listWidget_LibraryBottom->currentRow() >= 0));
}

/* Returns the currently selected soundfont program in the library/filesystem,
 * or a blank one if nothing is selected. */
KonfytSoundPreset MainWindow::selectedSoundfontProgramInLibOrFs()
{
    KonfytSoundPreset p;
    if (isSoundfontProgramSelectedInLibOrFs()) {
        p = selectedSfont->presets.value(ui->listWidget_LibraryBottom->currentRow());
    }
    return p;
}

/* Returns the type of item in the library tree. */
LibraryTreeItemType MainWindow::libraryTreeItemType(QTreeWidgetItem *item)
{
    if (item == libraryPatchTree.rootTreeItem) { return libTreePatchesRoot; }
    else if (libraryPatchTree.soundsMap.contains(item)) { return libTreePatch; }
    else if (item == librarySfTree.rootTreeItem) { return libTreeSFZRoot; }
    else if (librarySfzTree.foldersMap.contains(item)) { return libTreeSFZFolder; }
    else if (librarySfzTree.soundsMap.contains(item)) { return libTreeSFZ; }
    else if (item == librarySfTree.rootTreeItem) { return libTreeSoundfontRoot; }
    else if (librarySfTree.foldersMap.contains(item)) { return libTreeSoundfontFolder; }
    else if (librarySfTree.soundsMap.contains(item)) { return libTreeSoundfont; }
    else { return libTreeInvalid; }
}

LibraryTreeItemType MainWindow::librarySelectedTreeItemType()
{
    return libraryTreeItemType( ui->treeWidget_Library->currentItem() );
}

/* Returns the currently selected patch in the library, or a null one if nothing
 * is selected. */
KfSoundPtr MainWindow::librarySelectedPatch()
{
    KfSoundPtr ret;
    if ( librarySelectedTreeItemType() == libTreePatch ) {
        ret = libraryPatchTree.soundsMap.value(ui->treeWidget_Library->currentItem());
    }
    return ret;
}

/* Returns the currently selected soundfont in the library, or null one if
 * nothing is selected. */
KfSoundPtr MainWindow::librarySelectedSfont()
{
    KfSoundPtr ret;
    if ( librarySelectedTreeItemType() == libTreeSoundfont ) {
        ret = librarySfTree.soundsMap.value(ui->treeWidget_Library->currentItem());
    }
    return ret;
}

KfSoundPtr MainWindow::librarySelectedSfz()
{
    KfSoundPtr ret;
    if ( librarySelectedTreeItemType() == libTreeSFZ ) {
        ret = librarySfzTree.soundsMap.value(ui->treeWidget_Library->currentItem());
    }
    return ret;
}

bool MainWindow::isSfzSelectedInFilesystem()
{
    QFileInfo info = fsMap.value(ui->treeWidget_filesystem->currentItem());
    return fileIsSfzOrGig(info.filePath());
}

bool MainWindow::isPatchSelectedInFilesystem()
{
    QFileInfo info = fsMap.value(ui->treeWidget_filesystem->currentItem());
    return fileIsPatch(info.filePath());
}

bool MainWindow::isSoundfontSelectedInFilesystem()
{
    QFileInfo info = fsMap.value(ui->treeWidget_filesystem->currentItem());
    return fileIsSoundfont(info.filePath());
}

KfSoundPtr MainWindow::selectedSoundInLibOrFs()
{
    KfSoundPtr ret;

    if (ui->tabWidget_library->currentWidget() == ui->tab_library) {
        // Library is currently visible

        if ( librarySelectedTreeItemType() == libTreePatch ) {
            ret = libraryPatchTree.soundsMap.value(ui->treeWidget_Library->currentItem());

        } else if ( librarySelectedTreeItemType() == libTreeSoundfont ) {

            // Create new SoundPtr from selected soundfont, containing only the
            // selected program as preset (if any)
            ret.reset(new KonfytSound(KfSoundTypeSoundfont));
            if (selectedSfont) {
                ret->filename = selectedSfont->filename;
                ret->name = selectedSfont->name;
                if (isSoundfontProgramSelectedInLibOrFs()) {
                    ret->presets.append(selectedSoundfontProgramInLibOrFs());
                }
            } else {
                KONFYT_ASSERT_FAIL("Selected KfSoundPtr null");
            }

        } else if ( librarySelectedTreeItemType() == libTreeSFZ ) {
            ret = librarySfzTree.soundsMap.value(ui->treeWidget_Library->currentItem());
        }

    } else {
        // Filesystem is currently visible

        QString path = fsMap.value(ui->treeWidget_filesystem->currentItem()).filePath();
        if (isPatchSelectedInFilesystem()) {

            ret.reset(new KonfytSound(KfSoundTypePatch));
            ret->filename = path;
            ret->name = path;

        } else if (isSoundfontSelectedInFilesystem()) {

            ret.reset(new KonfytSound(KfSoundTypeSoundfont));
            ret->filename = path;
            ret->name = path;
            // If a program is selected, add it as the only preset
            if (isSoundfontProgramSelectedInLibOrFs()) {
                ret->presets.append(selectedSoundfontProgramInLibOrFs());
            }

        } else if (isSfzSelectedInFilesystem()) {

            ret.reset(new KonfytSound(KfSoundTypeSfz));
            ret->filename = path;
            ret->name = path;

        }

    }

    return ret;
}

void MainWindow::clearLibFsInfoArea()
{
    ui->listWidget_LibraryBottom->clear();
    ui->textBrowser_LibraryBottom->clear();
}

void MainWindow::showPatchInLibFsInfoArea()
{
    ui->stackedWidget_libraryBottom->setCurrentWidget(ui->page_libraryBottom_Text);
    QString text = "Double-click to load patch.";
    if (mPreviewMode) {
        text.prepend("Patches cannot be previewed.\n");
    }
    ui->textBrowser_LibraryBottom->setPlainText(text);
}

void MainWindow::showSfontInfoInLibFsInfoArea(QString filename)
{
    ui->stackedWidget_libraryBottom->setCurrentWidget(ui->page_libraryBottom_Text);
    ui->textBrowser_LibraryBottom->clear();

    QFileInfo info(filename);
    ui->textBrowser_LibraryBottom->append("SF2 Soundfont");
    ui->textBrowser_LibraryBottom->append("File size: " + n2s(info.size()/1024/1024) + " MB");
    ui->textBrowser_LibraryBottom->append("\nDouble-click to load program list.");
}

void MainWindow::resetLibraryTree(MainWindow::LibraryTree &libTree, QString name)
{
    libTree.rootTreeItem = new QTreeWidgetItem();
    libTree.rootTreeItem->setText(0, name);
    libTree.rootTreeItem->setIcon(0, mFolderIcon);
    libTree.foldersMap.clear();
    libTree.soundsMap.clear();
}

void MainWindow::buildLibraryTree(QTreeWidgetItem *twi, KfDbTreeItemPtr dbTreeItem,
                                  MainWindow::LibraryTree *libTree)
{
    if (!dbTreeItem->hasChildren() && (twi != libTree->rootTreeItem)) {
        // Sound item: has no children and is not the root item
        libTree->soundsMap.insert(twi, dbTreeItem->data);
        twi->setIcon(0, mFileIcon);
    } else {
        // Root or intermediate folder item
        twi->setIcon(0, mFolderIcon);
        if (twi != libTree->rootTreeItem) {
            // Not the root, add to folders map
            libTree->foldersMap.insert(twi, dbTreeItem->path);
        }
    }
    twi->setToolTip(0, twi->text(0));

    foreach (KfDbTreeItemPtr dbChild, dbTreeItem->children) {
        QTreeWidgetItem* child = new QTreeWidgetItem();
        child->setText(0, dbChild->name);
        buildLibraryTree(child, dbChild, libTree);
        twi->addChild(child);
    }
}

void MainWindow::buildSfzTree(KfDbTreeItemPtr dbTreeItem)
{
    buildLibraryTree(librarySfzTree.rootTreeItem, dbTreeItem, &librarySfzTree);
}

void MainWindow::buildSfTree(KfDbTreeItemPtr dbTreeItem)
{
    buildLibraryTree(librarySfTree.rootTreeItem, dbTreeItem, &librarySfTree);
}

void MainWindow::buildPatchTree(KfDbTreeItemPtr dbTreeItem)
{
    buildLibraryTree(libraryPatchTree.rootTreeItem, dbTreeItem, &libraryPatchTree);
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
    return ( fileSuffixIs(file, "sf2") || fileSuffixIs(file, "sf3") );
}

/* Set master gain if in normal mode, or preview gain if in preview mode.
 * Also updates the GUI slider and the appropriate sound engine. */
void MainWindow::setMasterGain(float gain)
{
    if (gain > 1.0) { gain = 1.0; }
    if (gain < 0) { gain = 0; }

    masterGainMidiCtrlr.setValue(gain * 127.0);
    if (mPreviewMode) {
        previewGain = gain;
    } else {
        masterGain = gain;
    }

    ui->horizontalSlider_MasterGain->setValue(
                gain * ui->horizontalSlider_MasterGain->maximum());

    // Update all bus gains
    if (mCurrentProject) {
        foreach (int busId, mCurrentProject->audioBus_getAllBusIds()) {
            updateBusGainInJackEngine(busId);
        }
    }
}

void MainWindow::updateBusGainInJackEngine(int busId)
{
    KONFYT_ASSERT_RETURN(!mCurrentProject.isNull());
    KONFYT_ASSERT_RETURN(mCurrentProject->audioBus_exists(busId));

    PrjAudioBus bus = mCurrentProject->audioBus_getBus(busId);
    float gain = 1;
    if (!bus.ignoreMasterGain) {
        if (mPreviewMode) {
            gain *= previewGain;
        } else {
            gain *= masterGain;
        }
    }
    gain = konfytConvertGain(gain);

    jack.setPortGain(bus.leftJackPort, gain);
    jack.setPortGain(bus.rightJackPort, gain);
}

/* Returns index of current patch, or -1 if none. */
int MainWindow::currentPatchIndex()
{
    int ret = -1;

    ProjectPtr prj = mCurrentProject;
    if (prj) {
        ret = prj->getPatchIndex(mCurrentPatch);
    }

    return ret;
}

/* Load mCurrentPatch and update the GUI accordingly. */
void MainWindow::loadPatchAndUpdateGui()
{
    pengine.loadPatch(mCurrentPatch);
    patchListAdapter.setCurrentPatch(mCurrentPatch);
    patchListAdapter.setPatchLoaded(mCurrentPatch, true);

    updatePatchView();
    updateWindowTitle();
}

/* Loads the currently selected sound in the library/filesystem into the
 * preview patch, and loads the preview patch into the patch engine. */
void MainWindow::loadPreviewPatchAndUpdateGui()
{
    // Unload preview patch if it is loaded
    pengine.unloadPatch(&mPreviewPatch);
    // Clear preview patch
    mPreviewPatch.clearLayers();
    // Load empty preview patch
    pengine.loadPatch(&mPreviewPatch);

    // Add selected library/filesystem item as a layer to the patch

    KfSoundPtr s = selectedSoundInLibOrFs();
    if (s) {
        if (s->type == KfSoundTypeSoundfont) {

            if (s->presets.count()) {
                pengine.addSfProgramLayer(s->filename, s->presets.value(0));
            }

        } else if (s->type == KfSoundTypePatch) {

            // We don't do preview for patches.

        } else if (s->type == KfSoundTypeSfz) {

            pengine.addSfzLayer(s->filename);

        }
    }

    updatePreviewPatchLayer();

    updateWindowTitle();
}

void MainWindow::updatePatchView()
{
    clearPatchLayersFromGuiOnly();

    // Only for master patch, not preview mode patch
    KonfytPatch* patch = mCurrentPatch;
    if (patch == nullptr) {
        // No patch active
        ui->lineEdit_PatchName->setText("");
        ui->lineEdit_PatchName->setEnabled(false);
        patchNote_ignoreChange = true;
        ui->textBrowser_patchNote->clear();
        ui->stackedWidget_patchLayers->setCurrentWidget(ui->page_notPatchLayers);
        return;
    }

    ui->stackedWidget_patchLayers->setCurrentWidget(ui->page_patchLayers);

    // Get list of layer items
    foreach (KfPatchLayerWeakPtr layer, patch->layers()) {
        // Add to gui layer list
        addPatchLayerToGUI(layer);
    }

    // Patch title
    ui->lineEdit_PatchName->setText(patch->name());
    ui->lineEdit_PatchName->setEnabled(true);
    // Patch note
    patchNote_ignoreChange = true;
    ui->textBrowser_patchNote->setText(patch->note());

    // Always-active text
    ui->label_patch_alwaysActive->setVisible(patch->alwaysActive);
    // Always-active menu item
    ui->actionAlways_Active->setChecked(patch->alwaysActive);
}

void MainWindow::updateWindowTitle()
{
    QString title;

    if (mPreviewMode) {
        title += "Preview";
    } else {
        KonfytPatch* currentPatch = pengine.currentPatch();
        if (currentPatch) {
            title += currentPatch->name();
        }
    }

    if (mCurrentProject) {
        if (!title.isEmpty()) { title += " - "; }
        title += mCurrentProject->getProjectName();
    }

    if (!title.isEmpty()) { title += " "; }
    title += "[" + jack.clientName() + "]";

    setWindowTitle(title);
}

void MainWindow::setupPatchListAdapter()
{
    patchListAdapter.init(ui->listWidget_Patches);
    connect(&patchListAdapter, &PatchListWidgetAdapter::patchSelected,
            this, &MainWindow::onPatchSelected);
    connect(&patchListAdapter, &PatchListWidgetAdapter::patchMoved,
            [=](int indexFrom, int indexTo) {
        ProjectPtr prj = mCurrentProject;
        if (!prj) { return; }
        prj->movePatch(indexFrom, indexTo);
    });
}

void MainWindow::onPatchSelected(KonfytPatch *patch)
{
    setCurrentPatch(patch);
}

void MainWindow::onPatchLayerLoaded(KfPatchLayerWeakPtr patchLayer)
{
    foreach (KonfytLayerWidget* w, layerWidgetList) {
        if (w->getPatchLayer() == patchLayer) {
            w->refresh();
        }
    }
}

/* Fill the library tree widget with all the entries in the database. */
void MainWindow::fillLibraryTreeWithAll()
{
    mLibrarySearchModeActive = false; // Controls the behaviour when the user selects a tree item
    ui->treeWidget_Library->clear();

    // Create parent soundfonts tree item, with soundfont children
    resetLibraryTree(librarySfTree, QString("%1 [%2]")
                     .arg(TREE_ITEM_SOUNDFONTS).arg(db.soundfontCount()));
    buildSfTree(db.sfontTree.root);

    // Create parent patches tree item, with patch children
    resetLibraryTree(libraryPatchTree, QString("%1 [%2]")
                     .arg(TREE_ITEM_PATCHES).arg(db.patchCount()));
    buildPatchTree(db.patchTree.root);

    // Create parent sfz item, with one child indicating the number of items
    resetLibraryTree(librarySfzTree, QString("%1 [%2]")
                     .arg(TREE_ITEM_SFZ).arg(db.sfzCount()));
    buildSfzTree(db.sfzTree.root);

    // Add items to tree
    ui->treeWidget_Library->insertTopLevelItem(0, librarySfTree.rootTreeItem);
    ui->treeWidget_Library->insertTopLevelItem(0, librarySfzTree.rootTreeItem);
    ui->treeWidget_Library->insertTopLevelItem(0, libraryPatchTree.rootTreeItem);
}

void MainWindow::preparePreviewMenu()
{
    previewButtonMenu.clear();

    QMenu* midiInPortMenu = previewButtonMenu.addMenu("MIDI In Port");
    updateMidiInPortsMenu(midiInPortMenu, previewPatchMidiInPort);
    connect(midiInPortMenu, &QMenu::triggered,
            this, &MainWindow::previewButtonMidiInPortMenuTrigger);

    QMenu* midiInChannelMenu = previewButtonMenu.addMenu("MIDI In Channel");
    updateMidiInChannelMenu(midiInChannelMenu, previewPatchMidiInChannel);
    connect(midiInChannelMenu, &QMenu::triggered,
            this, &MainWindow::previewButtonMidiInChannelMenuTrigger);

    QMenu* busMenu = previewButtonMenu.addMenu("Output Bus");
    updateBusMenu(busMenu, previewPatchBus);
    connect(busMenu, &QMenu::triggered,
            this, &MainWindow::previewButtonBusMenuTrigger);
}

/* Menu item has been clicked in the preview button MIDI-In port menu. */
void MainWindow::previewButtonMidiInPortMenuTrigger(QAction *action)
{
    int portId = action->property(PTY_MIDI_IN_PORT).toInt();
    if (portId == -2) {
        // Show current port in connections page
        showConnectionsPage();
        connectionsTreeSelectMidiInPort(previewPatchMidiInPort);
    } else {
        if (portId < 0) {
            // Add new MIDI in port
            portId = addMidiInPort();
            if (portId < 0) { return; }
            // Open the connections page and show port
            showConnectionsPage();
            connectionsTreeSelectMidiInPort(portId);
        }

        previewPatchMidiInPort = portId;
        updatePreviewPatchLayer();
    }
}

/* Menu item has been clicked in the preview button MIDI-In channel menu. */
void MainWindow::previewButtonMidiInChannelMenuTrigger(QAction *action)
{
    int channel = action->property(PTY_MIDI_CHANNEL).toInt();

    previewPatchMidiInChannel = channel;
    updatePreviewPatchLayer();
}

void MainWindow::previewButtonBusMenuTrigger(QAction *action)
{
    int busId = action->property(PTY_AUDIO_OUT_BUS).toInt();
    if (busId == -2) {
        // Show current bus in connections page
        showConnectionsPage();
        connectionsTreeSelectBus(previewPatchBus);
    } else {
        if (busId < 0) {
            // Add new audio output bus
            busId = addBus();
            if (busId < 0) { return; }
            // Open the connections page and show bus
            showConnectionsPage();
            connectionsTreeSelectBus(busId);
        }

        previewPatchBus = busId;
        updatePreviewPatchLayer();
    }
}

void MainWindow::fillLibraryTreeWithSearch(QString search)
{
    mLibrarySearchModeActive = true; // Controls the behaviour when the user selects a tree item
    db.search(search);

    ui->treeWidget_Library->clear();

    QTreeWidgetItem* twiResults = new QTreeWidgetItem();
    twiResults->setText(0, TREE_ITEM_SEARCH_RESULTS);

    // Soundfonts
    resetLibraryTree(librarySfTree, QString("%1 [%2 (%3 programs)]")
                     .arg(TREE_ITEM_SOUNDFONTS).arg(db.getNumSfontsResults())
                     .arg(db.getNumSfontProgramResults()));
    buildSfTree(db.sfontTree_results.root);

    // Patches
    resetLibraryTree(libraryPatchTree, QString("%1 [%2]")
                     .arg(TREE_ITEM_PATCHES).arg(db.getNumPatchesResults()));
    buildPatchTree(db.patchTree_results.root);

    // SFZ
    resetLibraryTree(librarySfzTree, QString("%1 [%2]")
                     .arg(TREE_ITEM_SFZ).arg(db.getNumSfzResults()));
    buildSfzTree(db.sfzTree_results.root);

    // Add items to tree
    twiResults->addChild(libraryPatchTree.rootTreeItem);
    twiResults->addChild(librarySfzTree.rootTreeItem);
    twiResults->addChild(librarySfTree.rootTreeItem);

    ui->treeWidget_Library->insertTopLevelItem(0, twiResults);
    ui->treeWidget_Library->expandItem(twiResults);
    ui->treeWidget_Library->expandItem(libraryPatchTree.rootTreeItem);
    ui->treeWidget_Library->expandItem(librarySfzTree.rootTreeItem);
    ui->treeWidget_Library->expandItem(librarySfTree.rootTreeItem);
}

void MainWindow::setupFilesystemView()
{
    this->fsview_currentPath = QDir::homePath();
    refreshFilesystemView();
    ui->tabWidget_library->setCurrentWidget(ui->tab_library);
}

/* Log a message in the GUI console. */
void MainWindow::print(QString message)
{
    if (mBlockPrint) { return; }

    ui->textBrowser->append(message);

    /* Ensure textBrowser scrolls to maximum when it is filled with text. Usually
     * this is only done when the user explicitely scrolls to the end. We want it to
     * happen from the start. Once it's there, we set 'start=false' as the
     * user / textBrowser can then handle it themselves. */
    QScrollBar* v = ui->textBrowser->verticalScrollBar();
    if (mPrintStart) {
        if (v->value() != v->maximum()) {
            v->setValue(v->maximum());
            mPrintStart = false;
        }
    }

    // Separate console dialog
    emit printSignal(message);
}

void MainWindow::setupKeyboardShortcuts()
{
    shortcut_save = new QShortcut(QKeySequence("Ctrl+S"), this);
    connect(shortcut_save, &QShortcut::activated,
            this, &MainWindow::shortcut_save_activated);

    shortcut_panic = new QShortcut(QKeySequence("Ctrl+P"), this);
    connect(shortcut_panic, &QShortcut::activated,
            this, &MainWindow::shortcut_panic_activated);

    ui->pushButton_Panic->setToolTip( ui->pushButton_Panic->toolTip() +
            " [" + shortcut_panic->key().toString() + "]");
}

void MainWindow::messageBox(QString msg)
{
    QMessageBox msgbox;
    msgbox.setText(msg);
    msgbox.exec();
}

bool MainWindow::dirExists(QString dirname)
{
    if (dirname.isEmpty()) { return false; }

    QDir dir(dirname);
    return dir.exists();
}

QStringList MainWindow::scanDirForFiles(QString dirname, QString filenameExtension)
{
    QStringList ret;

    if (!dirExists(dirname)) {
        print(QString("Scan dir for %1 files: Dir does not exist: %2")
                    .arg(filenameExtension, dirname));
        return ret;
    }

    QDir dir(dirname);

    // Get list of all subfiles and directories. Then for each:
    // If a file, add it if it is a project.
    // If a directory, run this function on it.
    QFileInfoList fil = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (int i = 0; i < fil.count(); i++) {
        QFileInfo fi = fil.at(i);

        if (fi.isFile()) {
            // Check extension and add if project.
            bool pass = false;
            if (filenameExtension.isEmpty()) { pass = true; }
            else {
                QString suffix = "." + fi.suffix();
                if (suffix == filenameExtension) {
                    pass = true;
                }
            }
            if (pass) {
                ret.append(fi.filePath());
            }
        } else if (fi.isDir()) {
            // Scan the dir
            ret.append(scanDirForFiles(fi.filePath(), filenameExtension));
        }
    }

    return ret;
}

void MainWindow::openFileManager(QString path)
{
    if (mFilemanager.length()) {
        QProcess* process = new QProcess();

        connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                [this, process](int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/){
            process->deleteLater();
        });

        process->start(mFilemanager, QStringList() << path);

    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

/* Setup the initial project based on command-line arguments. */
void MainWindow::setupInitialProjectFromCmdLineArgs()
{
    bool defaultProject = false;

    // Find the first project file specified in cmdline arguments, if any
    foreach (QString f, appInfo.filesToLoad) {
        if ( !fileIsPatch(f) && !fileIsSfzOrGig(f) && !fileIsSoundfont(f) ) {
            if (mCurrentProject) {
                print("Project already loaded, skipping cmdline arg: " + f);
                continue;
            }
            // Try to load project
            print("Opening project from cmdline arg: " + f);
            if (loadProjectFromFile(f)) {
                print("Project loaded from cmdline arg.");
            } else {
                print("Failed to load project from cmdline arg.");
            }
        }
    }

    // If no project is loaded yet, create default project
    if (!mCurrentProject) {
        print("Creating default new project.");
        defaultProject = true;
        loadNewProject(); // Create and load new project
        // Since this is a default startup project, mark as not modified.
        mCurrentProject->setModified(false);
    }

    // Load non-project files specified in cmdline arguments, if any
    foreach (QString f, appInfo.filesToLoad) {
        if (fileIsPatch(f)) {
            // Load patch into current project and switch to patch

            addPatchToProjectFromFile(f);

            // Locate in filesystem view
            ui->tabWidget_library->setCurrentWidget(ui->tab_filesystem);
            cdFilesystemView(QFileInfo(f).absoluteFilePath());
            selectItemInFilesystemView(f);

        } else if (fileIsSfzOrGig(f)) {
            // Create new patch and load sfz into patch
            newPatchToProject(); // Create a new patch and add to current project.
            setCurrentPatchByIndex(-1);

            addSfzToCurrentPatch(f);
            // Rename patch
            ui->lineEdit_PatchName->setText( getBaseNameWithoutExtension(f) );
            on_lineEdit_PatchName_editingFinished();

            // Locate in filesystem view
            ui->tabWidget_library->setCurrentWidget(ui->tab_filesystem);
            cdFilesystemView(QFileInfo(f).absoluteFilePath());
            selectItemInFilesystemView(f);

        } else if (fileIsSoundfont(f)) {

            // Locate soundfont in filebrowser, select it and show its programs
            ui->tabWidget_library->setCurrentWidget(ui->tab_filesystem);
            cdFilesystemView(QFileInfo(f).absoluteFilePath());
            selectItemInFilesystemView(f);
            // Load from filesystem view
            on_treeWidget_filesystem_itemDoubleClicked( ui->treeWidget_filesystem->currentItem(), 0 );
        }
    }

    // If no patches have been loaded from cmdline arguments, and the current
    // project is a default created project, add a default patch.
    if ( defaultProject && (mCurrentProject->getNumPatches() == 0) ) {
        newPatchToProject(); // Create a new patch and add to current project.
        setCurrentPatchByIndex(0);
        // Since this is a default startup project, mark as not modified.
        mCurrentProject->setModified(false);
    }
}

QString MainWindow::getBaseNameWithoutExtension(QString filepath)
{
    QFileInfo fi(filepath);
    return fi.baseName();
}

void MainWindow::on_treeWidget_Library_itemClicked(QTreeWidgetItem *item, int /*column*/)
{
    // Expand / unexpand item due to click (makes things a lot easier)
    item->setExpanded(!item->isExpanded());
}

/* Set the current patch, and update the GUI accordingly. */
void MainWindow::setCurrentPatch(KonfytPatch* patch)
{
    mCurrentPatch = patch;
    loadPatchAndUpdateGui();

    if (patch) {
        // Send MIDI events associated with patch layers
        pengine.sendCurrentPatchMidi();
    }
}

/* Set the current patch corresponding to the specified index. If index is -1,
 * the last patch is selected. If index is out of bounds, the first patch is
 * selected. */
void MainWindow::setCurrentPatchByIndex(int index)
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    // If index is -1, select the last patch
    if (index == -1) {
        index = prj->getNumPatches()-1;
    }

    // Make index zero if out of bounds
    if ( (index < 0) || (index >= prj->getNumPatches()) ) {
        index = 0;
    }

    setCurrentPatch(prj->getPatch(index));
}

void MainWindow::on_treeWidget_Library_currentItemChanged(
        QTreeWidgetItem* /*current*/, QTreeWidgetItem* /*previous*/)
{
    if ( librarySelectedTreeItemType() == libTreePatch ) {

        showPatchInLibFsInfoArea();

    } else if ( librarySelectedTreeItemType() == libTreeSoundfont ) {

        selectedSfont = librarySelectedSfont();
        showSelectedSfontProgramList();

    } else if ( librarySelectedTreeItemType() == libTreeSFZ ) {

        showSfzContentInLibFsInfoArea(librarySelectedSfz()->filename);

    } else {
        clearLibFsInfoArea();
    }

    if (mPreviewMode) {
        loadPreviewPatchAndUpdateGui();
    }
}

/* Refresh the library/filesystem program list view according to the selected
 * soundfont. */
void MainWindow::showSelectedSfontProgramList()
{
    ui->listWidget_LibraryBottom->clear();
    ui->stackedWidget_libraryBottom->setCurrentWidget(ui->page_libraryBottom_ProgramList);

    if (selectedSfont.isNull()) { return; }

    // Do not automatically select a preset
    ui->listWidget_LibraryBottom->blockSignals(true);

    foreach (const KonfytSoundPreset preset, selectedSfont->presets) {
        ui->listWidget_LibraryBottom->addItem(QString("%1-%2 %3")
            .arg(preset.bank).arg(preset.program).arg(preset.name));
    }

    ui->listWidget_LibraryBottom->setCurrentRow(-1);
    ui->listWidget_LibraryBottom->blockSignals(false);
}

/* Enter pressed in the search box. */
void MainWindow::on_lineEdit_Search_returnPressed()
{
    QString str = ui->lineEdit_Search->text();
    if (str.trimmed().isEmpty()) {
        on_toolButton_ClearSearch_clicked();
    } else {
        fillLibraryTreeWithSearch(str);
    }
}

/* Clear search button clicked. */
void MainWindow::on_toolButton_ClearSearch_clicked()
{
    ui->lineEdit_Search->clear();
    fillLibraryTreeWithAll();
    ui->lineEdit_Search->setFocus();
}

/* Library/filesystem program list: Soundfont program selected. */
void MainWindow::on_listWidget_LibraryBottom_currentRowChanged(int currentRow)
{
    if (currentRow < 0) { return; }

    KONFYT_ASSERT_RETURN(!selectedSfont.isNull());

    if (mPreviewMode) {
        loadPreviewPatchAndUpdateGui();
    }
}

/* Adds SFZ to current patch in engine and in GUI. */
void MainWindow::addSfzToCurrentPatch(QString sfzPath)
{
    newPatchIfCurrentNull();

    // Add layer to engine
    KfPatchLayerWeakPtr layer = pengine.addSfzLayer(sfzPath);

    // Add layer to GUI
    addPatchLayerToGUI(layer);

    patchModified();
}

/* Adds soundfont program to current patch in engine and in GUI. */
void MainWindow::addSoundfontProgramToCurrentPatch(QString soundfontPath, KonfytSoundPreset p)
{
    newPatchIfCurrentNull();

    // Add program to engine
    KfPatchLayerWeakPtr layer = pengine.addSfProgramLayer(soundfontPath, p);

    // Add layer to GUI
    addPatchLayerToGUI(layer);

    patchModified();
}

/* If the current patch is null, adds a new patch to the project and activates it. */
void MainWindow::newPatchIfCurrentNull()
{
    ProjectPtr prj = mCurrentProject;
    KONFYT_ASSERT_RETURN(prj);

    if (!mCurrentPatch) {
        newPatchToProject();
        // Switch to latest patch
        setCurrentPatchByIndex(-1);
    }
}

/* Adds a midi port to the current patch in engine and GUI. */
void MainWindow::addMidiPortToCurrentPatch(int port)
{
    newPatchIfCurrentNull();

    // Check if the port isn't already in the patch
    QList<int> l = pengine.currentPatch()->getMidiOutputPortListProjectIds();
    if (l.contains(port)) { return; }

    // Add port to current patch in engine
    KfPatchLayerWeakPtr layer = pengine.addMidiOutPortToPatch(port);

    // Add to GUI list
    addPatchLayerToGUI(layer);

    patchModified();
}

/* Adds an audio bus to the current patch in engine and GUI. */
void MainWindow::addAudioInPortToCurrentPatch(int port)
{
    newPatchIfCurrentNull();

    // Check if the port isn't already in the patch
    QList<int> l = pengine.currentPatch()->getAudioInPortListProjectIds();
    if (l.contains(port)) { return; }

    // Add port to current patch in engine
    KfPatchLayerWeakPtr layer = pengine.addAudioInPortToPatch(port);

    // Add to GUI list
    addPatchLayerToGUI(layer);

    patchModified();
}

/* Sets previewMode as specified, and updates the GUI. */
void MainWindow::setPreviewMode(bool previewModeOn)
{
    mPreviewMode = previewModeOn;

    ui->toolButton_LibraryPreview->setChecked(mPreviewMode);
    ui->PatchPage->setEnabled(!mPreviewMode);
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);

    if (mPreviewMode) {
        setMasterGain(previewGain); // To update GUI slider
        loadPreviewPatchAndUpdateGui();
    } else {
        setMasterGain(masterGain); // To update GUI slider
        pengine.unloadPatch(&mPreviewPatch);
        loadPatchAndUpdateGui();
    }
}

/* Master gain slider moved. */
void MainWindow::on_horizontalSlider_MasterGain_sliderMoved(int position)
{
    setMasterGain( (float)position / (float)ui->horizontalSlider_MasterGain->maximum() );
}

void MainWindow::on_lineEdit_PatchName_returnPressed()
{
    on_lineEdit_PatchName_editingFinished();

    // Remove focus to something else to give the impression that the name has been changed.
    ui->label_PatchList->setFocus();
}

void MainWindow::on_lineEdit_PatchName_editingFinished()
{
    // Rename patch
    pengine.setPatchName(ui->lineEdit_PatchName->text());

    // Indicate to the user that the patch has been modified.
    patchModified();

    patchListAdapter.patchModified(pengine.currentPatch());
    updateWindowTitle();
}

void MainWindow::onPatchMidiOutPortsMenu_aboutToShow()
{
    updateMidiOutPortsMenu(&patchMidiOutPortsMenu);
}

void MainWindow::on_lineEdit_ProjectName_editingFinished()
{
    setProjectName(ui->lineEdit_ProjectName->text());
}

/* Save patch to library (in other words, to patchesDir directory.) */
bool MainWindow::savePatchToLibrary(KonfytPatch *patch)
{
    QDir dir(mPatchesDir);
    if (!dir.exists()) {
        print("Patches directory does not exist.");
        return false;
    }

    QString patchName = getUniqueFilename(dir.path(), patch->name(), "." + QString(KONFYT_PATCH_SUFFIX) );
    if (patchName == "") {
        print("Could not find a suitable filename.");
        return false;
    }

    if (patchName != patch->name() + "." + KONFYT_PATCH_SUFFIX) {
        print("Duplicate name exists. Saving patch as:");
        print(patchName);
    }

    // Add directory, and save.
    patchName = mPatchesDir + "/" + patchName;
    if (patch->savePatchToFile(patchName)) {
        print("Patch saved as " + patchName);
        db.addPatch(patchName);
        // Refresh tree view if not in searchmode
        if (!mLibrarySearchModeActive) {
            fillLibraryTreeWithAll();
        }
        // Now save database
        saveDatabase();

        return true;
    } else {
        print("Failed saving patch to file " + patchName);
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
        emit print("getUniqueFilename: Directory does not exist.");
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

/* Add process (external application) to GUI and current project. */
void MainWindow::addProcess(KonfytProcess* process)
{
    if (!mCurrentProject) {
        print("No active project.");
        return;
    }
    // Add to project list
    mCurrentProject->addProcess(process);
    // Add to GUI list
    ui->listWidget_ExtApps->addItem(process->toString());
}

void MainWindow::runProcess(int index)
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    // Abort if process is already running
    if (prj->isProcessRunning(index)) {
        print("Process is already running. Stop it before running it again.");
        return;
    }
    // Start process
    prj->runProcess(index);
    // Indicate in list widget
    if ( (index >=0) && (index < ui->listWidget_ExtApps->count()) ) {
        // Indicate in list widget
        QListWidgetItem* item = ui->listWidget_ExtApps->item(index);
        item->setText("[starting] " + item->text());
    } else {
        print("ERROR: PROCESS INDEX NOT IN GUI LIST: " + n2s(index));
    }
}

void MainWindow::stopProcess(int index)
{
    if (!mCurrentProject) { return; }
    // Stop process
    mCurrentProject->stopProcess(index);
}

void MainWindow::removeProcess(int index)
{
    if (!mCurrentProject) { return; }

    // Remove process from project
    mCurrentProject->removeProcess(index);
    // Remove from GUI list
    QListWidgetItem* item = ui->listWidget_ExtApps->item(index);
    delete item;
}

void MainWindow::processStartedSlot(int index, KonfytProcess *process)
{
    if ( (index >=0) && (index < ui->listWidget_ExtApps->count()) ) {
        // Indicate in list widget
        QListWidgetItem* item = ui->listWidget_ExtApps->item(index);
        item->setText("[running] " + process->toString());
    } else {
        print("ERROR: PROCESS INDEX NOT IN GUI LIST: " + n2s(index));
    }
}

void MainWindow::processFinishedSlot(int index, KonfytProcess *process)
{
    if ( (index >=0) && (index < ui->listWidget_ExtApps->count()) ) {
        // Indicate in list widget
        QListWidgetItem* item = ui->listWidget_ExtApps->item(index);
        item->setText("[stopped] " + process->toString());
    } else {
        print("ERROR: PROCESS INDEX NOT IN GUI LIST: " + n2s(index));
    }
}

void MainWindow::extAppsMenuTriggered(QAction *action)
{
    if (extAppsMenuActions_Append.contains(action)) {
        // Append to external apps text box
        ui->lineEdit_ExtApp->setText( ui->lineEdit_ExtApp->text() + extAppsMenuActions_Append.value(action, ""));
    } else {
        // Replace text of external apps text box
        ui->lineEdit_ExtApp->setText( extAppsMenuActions_Set.value(action, "") );
    }
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
    appInfo.a->installEventFilter(this);
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
    appInfo.a->removeEventFilter(this);
}

void MainWindow::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == waiterTimer.timerId()) {
        // We are busy waiting for something (startWaiter() has been called).
        // Rotate the cool fan in the statusbar.
        QString anim = "|/-\\";
        ui->statusBar->showMessage(waiterMessage + "   " + QString(anim.at(waiterState)));
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
    QString txt = getCompileVersionText();
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

void MainWindow::resizeEvent(QResizeEvent* /*ev*/)
{
    resizeAboutDialog();
}

/* Helper function for scanning things into database. */
void MainWindow::scanForDatabase()
{
    startWaiter("Scanning database directories...");
    // Display waiting screen.
    print("Starting database scan.");
    showWaitingPage("Scanning database directories...");
    // Start scanning for directories.
    db.scan();
    // When the finished signal is received, remove waiting screen.
    // See onDatabaseScanFinished()
    // Periodic status info might be received in onDatabaseScanStatus()
}

void MainWindow::setSoundfontsDir(QString path)
{
    mSoundfontsDir = path;
    db.setSoundfontsDir(path);
}

void MainWindow::setPatchesDir(QString path)
{
    mPatchesDir = path;
    db.setPatchesDir(path);
}

void MainWindow::setSfzDir(QString path)
{
    mSfzDir = path;
    db.setSfzDir(path);
}

/* Creates the settings dir if it doesn't exist. */
void MainWindow::createSettingsDir()
{
    QDir dir(settingsDir);
    if (!dir.exists()) {
        if (dir.mkpath(settingsDir)) {
            print("Created settings directory: " + settingsDir);
        } else {
            print("Failed to create settings directory: " + settingsDir);
        }
    }
}

void MainWindow::onDatabaseScanFinished()
{
    print("Database scanning complete.");
    print("   Found " + n2s(db.soundfontCount()) + " soundfonts.");
    print("   Found " + n2s(db.sfzCount()) + " sfz/gig samples.");
    print("   Found " + n2s(db.patchCount()) + " patches.");

    // Save to database file
    saveDatabase();

    fillLibraryTreeWithAll();
    stopWaiter();
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);
}

void MainWindow::setupDatabase()
{
    connect(&db, &konfytDatabase::print, this, &MainWindow::print);

    connect(&db, &konfytDatabase::scanFinished,
            this, &MainWindow::onDatabaseScanFinished);

    connect(&db, &konfytDatabase::scanStatus,
            this, &MainWindow::onDatabaseScanStatus);

    connect(&db, &konfytDatabase::sfontInfoLoadedFromFile,
            this, &MainWindow::onDatabaseSfontInfoLoaded);

    // Check if database file exists.
    if (db.loadDatabaseFromFile(settingsDir + "/" + DATABASE_FILE)) {
        print("Database loaded from file. Rescan to refresh.");
        print("Database contains:");
        print("   " + n2s(db.soundfontCount()) + " soundfonts.");
        print("   " + n2s(db.patchCount()) + " patches.");
        print("   " + n2s(db.sfzCount()) + " sfz/gig samples.");
    } else {
        print("No database file found.");
        // Check if old database location exists
        QString oldDir = QDir::homePath() + "/.konfyt/konfyt.database";
        if (db.loadDatabaseFromFile(oldDir)) {
            print("Found database file in old location. Saving to new location.");
            db.saveDatabaseToFile(settingsDir + "/" + DATABASE_FILE);
        } else {
            // Still no database file.
            print("You can scan directories to create a database from Settings.");
        }
    }

    fillLibraryTreeWithAll(); // Fill the tree widget with all the database entries
}

bool MainWindow::saveDatabase()
{
    // Save to database file
    if (db.saveDatabaseToFile(settingsDir + "/" + DATABASE_FILE)) {
        print("Saved database to file " + settingsDir + "/" + DATABASE_FILE);
        return true;
    } else {
        print("Failed to save database.");
        return false;
    }
}

void MainWindow::onDatabaseScanStatus(QString msg)
{
    ui->label_WaitingStatus->setText(msg);
}

void MainWindow::onDatabaseSfontInfoLoaded(KfSoundPtr sf)
{
    // Soundfont received from database after request was made from
    // on_treeWidget_filesystem_itemDoubleClicked()
    selectedSfont = sf;
    showSelectedSfontProgramList();

    // Enable GUI again
    stopWaiter();
}

/* Rescan database button pressed. */
void MainWindow::on_pushButtonSettings_RescanLibrary_clicked()
{
    applySettings();
    db.clearDatabase();
    scanForDatabase();
}

/* Quick scan database button clicked. */
void MainWindow::on_pushButtonSettings_QuickRescanLibrary_clicked()
{
    applySettings();
    db.clearDatabase_exceptSoundfonts();
    scanForDatabase();
}

void MainWindow::scanThreadFihishedSlot()
{
    print("ScanThread finished!");
}

void MainWindow::on_toolButton_RemovePatch_clicked()
{
    // Remove patch
    removePatchFromProject(ui->listWidget_Patches->currentRow());
}

void MainWindow::on_toolButton_PatchUp_clicked()
{
    patchListAdapter.moveSelectedPatchUp();
}

void MainWindow::on_toolButton_PatchDown_clicked()
{
    patchListAdapter.moveSelectedPatchDown();
}

void MainWindow::patchModified()
{
    setProjectModified();
}

void MainWindow::setProjectModified()
{
    if (!mCurrentProject) { return; }

    mCurrentProject->setModified(true);
}

/* Set the name of the current project and updates the GUI. */
void MainWindow::setProjectName(QString name)
{
    if (!mCurrentProject) { return; }

    mCurrentProject->setProjectName(name);
    updateProjectNameInGui();
    updateWindowTitle();
}

/* Update GUI with current project name. */
void MainWindow::updateProjectNameInGui()
{
    QString name;
    if (mCurrentProject) {
        name = mCurrentProject->getProjectName();
    }
    ui->lineEdit_ProjectName->setText(name);
}

void MainWindow::onProjectModifiedStateChanged(bool modified)
{
    QString stylesheet_base = "border-top-left-radius: 0;"
            "border-bottom-left-radius: 0;"
            "border-top-right-radius: 0;"
            "border-bottom-right-radius: 0;";

    QString stylesheet_normal = stylesheet_base + "border-top-right-radius: 0; border-bottom-right-radius: 0;";
    QString stylesheet_orange = stylesheet_base + "background-color: qlineargradient(spread:pad, x1:0, y1:1, x2:1, y2:0, stop:0 rgba(95, 59, 28, 255), stop:1 rgba(199, 117, 18, 255));border-top-right-radius: 0; border-bottom-right-radius: 0;";

    if (modified) {
        ui->toolButton_Project->setStyleSheet(stylesheet_orange);
    } else {
        ui->toolButton_Project->setStyleSheet(stylesheet_normal);
    }
}

void MainWindow::onProjectMidiPickupRangeChanged(int range)
{
    ui->spinBox_Triggers_midiPickupRange->setValue(range);
    pengine.setMidiPickupRange(range);
    masterGainMidiCtrlr.pickupRange = range;
}

bool MainWindow::saveCurrentProject()
{
    if (!mCurrentProject) {
        print("No active project.");
        return false;
    }

    return saveProject(mCurrentProject);
}

bool MainWindow::saveProject(ProjectPtr prj)
{
    if (!prj) {
        print("ERROR: saveProject: project null.");
        return false;
    }

    // Try to save. If this fails, it means the project has not been saved
    // yet and we need to create a directory for it.
    if (prj->saveProject()) {
        // Saved successfully.
        print("Project saved.");
        return true;
    } else {
        // We need to find a directory for the project.

        QString saveDir;

        // See if a default projects directory is set
        if (this->projectsDir == "") {
            print("Projects directory is not set.");
            // Inform the user about project directory that is not set
            if (informedUserAboutProjectsDir == false) {
                messageBox("No default projects directory has been set. You can set this in Settings.");
                informedUserAboutProjectsDir = true; // So we only show it once
            }
            // We need to bring up a save dialog.
        } else {
            // Find a unique directory name within our default projects dir
            QString dir = getUniqueFilename(this->projectsDir,sanitiseFilename( prj->getProjectName() ),"");
            if (dir == "") {
                print("Failed to obtain a unique directory name.");
            } else {
                dir = this->projectsDir + "/" + dir;
                // We now have a unique directory filename.
                QMessageBox msgbox;
                msgbox.setText("Do you want to save project \"" + prj->getProjectName() + "\" to the following path? Selecting No will bring up a dialog box to select a location.");
                msgbox.setInformativeText(dir);
                msgbox.setIcon(QMessageBox::Question);
                msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
                int ret = msgbox.exec();
                if (ret == QMessageBox::Yes) {
                    QDir d;
                    if (d.mkdir(dir)) {
                        print("Created project directory: " + dir);
                        saveDir = dir;
                    } else {
                        print("Failed to create project directory: " + dir);
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
        if (mCurrentProject->saveProjectAs(saveDir)) {
            print("Project Saved to " + saveDir);
            ui->statusBar->showMessage("Project saved.", 5000);
            return true;
        } else {
            print("Failed to save project.");
            messageBox("Failed to save project to " + saveDir);
            return false;
        }

    }

} // end of saveProject()

bool MainWindow::requestCurrentProjectClose()
{
    if (!mCurrentProject) { return true; }
    if (!mCurrentProject->isModified()) { return true; }

    QMessageBox msgbox;
    msgbox.setText("Do you want to save the changes to project " +
                   mCurrentProject->getProjectName() + "?");
    msgbox.setIcon(QMessageBox::Question);
    msgbox.setStandardButtons(QMessageBox::Cancel
                              | QMessageBox::Yes
                              | QMessageBox::No);
    msgbox.setDefaultButton(QMessageBox::Cancel);
    int ret = msgbox.exec();
    if (ret == QMessageBox::Yes) {
        bool saved = saveProject(mCurrentProject);
        if (!saved) { return false; }
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }

    return true;
}

void MainWindow::updateGUIWarnings()
{
    ui->listWidget_Warnings->clear();

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    QStringList moports = jack.getMidiOutputPortsList();
    QStringList miports = jack.getMidiInputPortsList();
    QStringList aoports = jack.getAudioOutputPortsList();
    QStringList aiports = jack.getAudioInputPortsList();


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

    // Buses
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
    pengine.panic(panicState);

    // Indicate panic state in GUI
    ui->pushButton_Panic->setChecked(panicState);

    // Clear sustain and pitchbend indicators for all layers
    foreach (KonfytLayerWidget* w, layerWidgetList) {
        if ( ! w->getPatchLayer().toStrongRef()->hasMidiInput() ) { continue; }
        w->indicateSustain(false);
        w->indicatePitchbend(false);
    }

    // Global sustain indicator
    portIndicatorHandler.clearSustain();
    updateGlobalSustainIndicator();

    // Global pitchbend indicator
    portIndicatorHandler.clearPitchbend();
    updateGlobalPitchbendIndicator();
}

void MainWindow::midi_setLayerGain(int layerIndex, int midiValue)
{
    // Set channel gain in engine
    if ((layerIndex>=0) && (layerIndex < pengine.getNumLayers()) ) {
        pengine.setLayerGainByMidi(layerIndex, midiValue);
        // Set channel gain in GUI slider
        this->layerWidgetList.at(layerIndex)->setSliderGain(
            pengine.currentPatch()->layer(layerIndex).toStrongRef()->gain());
    }
}

void MainWindow::midi_setLayerMute(int layer, int midiValue)
{
    if (midiValue > 0) {
        if ((layer>=0) && (layer < pengine.getNumLayers()) ) {
            bool newMute = !(pengine.currentPatch()->layers().at(layer).toStrongRef()->isMute());
            pengine.setLayerMute(layer, newMute);
            // Set in GUI layer item
            this->layerWidgetList.at(layer)->setMuteButton(newMute);
        }
    }
}

void MainWindow::midi_setLayerSolo(int layer, int midiValue)
{
    if (midiValue > 0) {
        if ((layer>=0) && (layer < pengine.getNumLayers()) ) {
            bool newSolo = !(pengine.currentPatch()->layers().at(layer).toStrongRef()->isSolo());
            pengine.setLayerSolo(layer, newSolo);
            // Set in GUI layer item
            this->layerWidgetList.at(layer)->setSoloButton(newSolo);
        }
    }
}

/* MIDI events are waiting in the JACK engine. */
void MainWindow::onJackMidiEventsReceived()
{   
    QList<KfJackMidiRxEvent> events = jack.getMidiRxEvents();
    foreach (KfJackMidiRxEvent event, events) {
        if (event.sourcePort) {
            handlePortMidiEvent(event);
        } else if (event.midiRoute) {
            handleRouteMidiEvent(event);
        } else {
            print("ERROR: MIDI EVENT BOTH PORT AND ROUTE NULL");
        }
    }
}

void MainWindow::onJackAudioEventsReceived()
{
    QList<KfJackAudioRxEvent> events = jack.getAudioRxEvents();
    foreach (KfJackAudioRxEvent event, events) {
        layerIndicatorHandler.jackEventReceived(event);
    }
}

void MainWindow::on_pushButton_ClearConsole_clicked()
{
    ui->textBrowser->clear();
}

/* Patch midi output port menu item has been clicked. */
void MainWindow::onPatchMidiOutPortsMenu_ActionTrigger(QAction *action)
{
    int portId = action->property(PTY_MIDI_OUT_PORT).toInt();
    if (portId < 0) {
        // Add new port
        portId = addMidiOutPort();
        if (portId < 0) { return; }
        // Show the newly created port in the connections tree
        showConnectionsPage();
        connectionsTreeSelectMidiOutPort(portId);
    }

    addMidiPortToCurrentPatch(portId);
}

void MainWindow::onPatchAudioInPortsMenu_aboutToShow()
{
    updateAudioInPortsMenu(&patchAudioInPortsMenu);
}

/* Patch add audio bus menu item has been clicked. */
void MainWindow::onPatchAudioInPortsMenu_ActionTrigger(QAction *action)
{
    int portId = action->property(PTY_AUDIO_IN_PORT).toInt();
    if (portId < 0) {
        // Add new port
        portId = addAudioInPort();
        if (portId >= 0) {
            addAudioInPortToCurrentPatch( portId );
            // Show the newly created port in the connections tree
            showConnectionsPage();
            connectionsTreeSelectAudioInPort(portId);
        }
    }

    addAudioInPortToCurrentPatch( portId );
}

/* Layer midi output channel menu item has been clicked. */
void MainWindow::onLayerMidiOutChannelMenu_ActionTrigger(QAction* action)
{
    int channel = action->property(PTY_MIDI_CHANNEL).toInt();
    KfPatchLayerSharedPtr layer = layerToolMenuSourceitem->getPatchLayer().toStrongRef();

    if (channel == -2) {
        // Get current port id and show it in the connections page
        showConnectionsPage();
        connectionsTreeSelectMidiOutPort(layer->midiOutputPortData.portIdInProject);
    } else {
        // Update layer MIDI filter with output channel
        KonfytMidiFilter filter = layer->midiFilter();
        filter.outChan = channel;
        layer->setMidiFilter(filter);

        // Update layer widget
        layerToolMenuSourceitem->refresh();
        // Update in pengine
        pengine.setLayerFilter(layer.toWeakRef(), filter);

        patchModified();
    }
}

/* Menu item has been clicked in the layer MIDI-In port menu. */
void MainWindow::onLayerMidiInPortsMenu_ActionTrigger(QAction *action)
{
    KfPatchLayerSharedPtr layer = layerToolMenuSourceitem->getPatchLayer().toStrongRef();

    int portId = action->property(PTY_MIDI_IN_PORT).toInt();
    if (portId == -2) {
        // Get current port id and show it in the connections page
        showConnectionsPage();
        connectionsTreeSelectMidiInPort(layer->midiInPortIdInProject());
    } else {
        if (portId == -1) {
            // Add new MIDI in port
            portId = addMidiInPort();
            if (portId < 0) { return; }
            // Open the connections page and show port.
            showConnectionsPage();
            connectionsTreeSelectMidiInPort(portId);
            }

        // Set the MIDI Input port in the GUI layer item
        layer->setMidiInPortIdInProject(portId);

        // Update the layer widget
        layerToolMenuSourceitem->refresh();
        // Update in pengine
        pengine.setLayerMidiInPort(layer.toWeakRef(), portId );

        patchModified();
    }
}

void MainWindow::onLayerMidiInChannelMenu_ActionTrigger(QAction *action)
{
    int channel = action->property(PTY_MIDI_CHANNEL).toInt();

    KfPatchLayerSharedPtr layer = layerToolMenuSourceitem->getPatchLayer().toStrongRef();
    KonfytMidiFilter filter = layer->midiFilter();
    filter.inChan = channel;
    layer->setMidiFilter(filter);

    // Update the layer widget
    layerToolMenuSourceitem->refresh();
    // Update in pengine
    pengine.setLayerFilter(layer.toWeakRef(), filter);

    patchModified();
}

void MainWindow::onLayer_midiSend_clicked(KonfytLayerWidget *layerWidget)
{
    pengine.sendLayerMidi(layerWidget->getPatchLayer());
}

/* Layer bus menu item has been clicked. */
void MainWindow::onLayerBusMenu_ActionTrigger(QAction *action)
{
    KfPatchLayerSharedPtr layer = layerToolMenuSourceitem->getPatchLayer().toStrongRef();

    int busId = action->property(PTY_AUDIO_OUT_BUS).toInt();
    if (busId == -2) {
        // Show current bus in connections page
        showConnectionsPage();
        connectionsTreeSelectBus(layer->busIdInProject());
    } else {
        if (busId < 0) {
            // Add new bus
            busId = addBus();
            if (busId < 0) { return; }
            // Open the connections page and show bus.
            showConnectionsPage();
            connectionsTreeSelectBus(busId);
        }

        // Set the destination bus in gui layer item
        layer->setBusIdInProject(busId);

        // Update the layer widget
        layerToolMenuSourceitem->refresh();
        // Update in pengine
        pengine.setLayerBus(layer.toWeakRef(), busId);

        patchModified();
    }
}

void MainWindow::on_pushButton_ExtApp_add_clicked()
{
    // Create a new process and call addProcess function
    KonfytProcess* p = new KonfytProcess();

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
        print("Select an application.");
        return;
    }
    runProcess(row);
}

void MainWindow::on_pushButton_ExtApp_Stop_clicked()
{
    int row = ui->listWidget_ExtApps->currentRow();
    if (row < 0) {
        print("Select an application.");
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
        print("Select an application.");
        return;
    }
    removeProcess(row);
}

/* Slot: on layer item slider move. */
void MainWindow::onLayer_slider_moved(KonfytLayerWidget *layerWidget, float gain)
{
    pengine.setLayerGain(layerWidget->getPatchLayer(), gain);
}

/* Slot: on layer item solo button clicked. */
void MainWindow::onLayer_solo_clicked(KonfytLayerWidget *layerWidget, bool solo)
{
    pengine.setLayerSolo(layerWidget->getPatchLayer(), solo);
}

/* Slot: on layer item mute button clicked. */
void MainWindow::onLayer_mute_clicked(KonfytLayerWidget *layerWidget, bool mute)
{
    pengine.setLayerMute(layerWidget->getPatchLayer(), mute);
}

/* Slot: on layer item bus button clicked. */
void MainWindow::onLayer_rightToolbutton_clicked(KonfytLayerWidget *layerWidget)
{
    // Save the layer item for future use
    layerToolMenuSourceitem = layerWidget;

    KfPatchLayerSharedPtr layer = layerWidget->getPatchLayer().toStrongRef();

    KonfytPatchLayer::LayerType type = layer->layerType();
    if (type == KonfytPatchLayer::TypeMidiOut) {
        // Show MIDI channel menu
        updateLayerMidiOutChannelMenu(&layerMidiOutChannelMenu,
                layer->midiFilter().outChan);
        layerMidiOutChannelMenu.popup(QCursor::pos());
    } else {
        // Show Buses menu
        updateBusMenu(&layerBusMenu, layer->busIdInProject());
        layerBusMenu.popup(QCursor::pos());

    }
    // The rest will be done in onlayerBusMenu_ActionTrigger() when the user clicked a menu item.
}

void MainWindow::onLayer_leftToolbutton_clicked(KonfytLayerWidget *layerItem)
{
    // Save the layer item for future use
    layerToolMenuSourceitem = layerItem;

    updateLayerToolMenu();
    layerToolMenu.popup(QCursor::pos());

    // The rest will be done in the menu/submenu trigger slots.
}

/* Clears and fills specified menu with items corresponding to project audio
 * output buses as well as a current bus connections entry and a new bus entry.
 * A property is set for each action corresponding to the bus id, -1 for the new
 * bus entry and -2 for the current bus connections entry.
 * The action corresponding to the specified currentBusId is marked to indicate
 * the current setting. */
void MainWindow::updateBusMenu(QMenu *menu, int currentBusId)
{
    menu->clear();

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    QAction* a = menu->addAction("Current Bus Connections...");
    a->setProperty(PTY_AUDIO_OUT_BUS, -2);
    menu->addSeparator();

    QList<int> busIds = prj->audioBus_getAllBusIds();
    foreach (int id, busIds) {
        QAction* action = menu->addAction( n2s(id) + " " + prj->audioBus_getBus(id).busName );
        action->setProperty(PTY_AUDIO_OUT_BUS, id);
        if (id == currentBusId) {
            action->setIcon(QIcon("://icons/right_w_outline.png"));
        }
    }
    menu->addSeparator();
    QAction* b = menu->addAction("New Bus");
    b->setProperty(PTY_AUDIO_OUT_BUS, -1);
}

/* Clears and fills specified menu with items corresponding to the 16 MIDI
 * channels as well as an original channel entry. A property is set for each
 * action corresponding to the MIDI channel (zero based) and -1 for the original
 * channel entry. The action corresponding to the specified currentMidiPort is
 * marked to indicate the current setting. */
void MainWindow::updateLayerMidiOutChannelMenu(QMenu *menu, int currentMidiPort)
{
    menu->clear();

    QAction* a = menu->addAction("Output Port Connections...");
    a->setProperty(PTY_MIDI_CHANNEL, -2);

    menu->addSection("MIDI Out Channel");

    QAction* action = menu->addAction("Original Channel");
    action->setProperty(PTY_MIDI_CHANNEL, -1);
    if (currentMidiPort == -1) {
        action->setIcon(QIcon("://icons/right_w_outline.png"));
    }

    for (int i=0; i <= 15; i++) {
        QAction* action = menu->addAction("Channel " + n2s(i+1));
        action->setProperty(PTY_MIDI_CHANNEL, i);
        if (i == currentMidiPort) {
            action->setIcon(QIcon("://icons/right_w_outline.png"));
        }
    }
}

/* Add a patch layer to the GUI layer list. If index is -1, it is added to the
 * end of the list. */
void MainWindow::addPatchLayerToGUI(KfPatchLayerWeakPtr patchLayer, int index)
{
    // Create new GUI layer item
    KonfytLayerWidget* layerWidget = new KonfytLayerWidget();
    layerWidget->setProject(mCurrentProject);
    QListWidgetItem* item = new QListWidgetItem();
    layerWidget->initLayer(patchLayer, item);

    // Register with MIDI indicator handler, provide corresponding JackEngine route
    KfPatchLayerSharedPtr pl(patchLayer);
    KfJackMidiRoute* midiRoute = nullptr;
    KfJackAudioRoute* audioRoute1 = nullptr;
    KfJackAudioRoute* audioRoute2 = nullptr;
    if (pl->layerType() == KonfytPatchLayer::TypeMidiOut) {
        midiRoute = pl->midiOutputPortData.jackRoute;
    } else if (pl->layerType() == KonfytPatchLayer::TypeSfz) {
        midiRoute = jack.getPluginMidiRoute(pl->sfzData.portsInJackEngine);
        QList<KfJackAudioRoute*> a = jack.getPluginAudioRoutes(pl->sfzData.portsInJackEngine);
        audioRoute1 = a.value(0);
        audioRoute2 = a.value(1);
    } else if (pl->layerType() == KonfytPatchLayer::TypeSoundfontProgram) {
        midiRoute = jack.getPluginMidiRoute(pl->soundfontData.portsInJackEngine);
        QList<KfJackAudioRoute*> a = jack.getPluginAudioRoutes(pl->soundfontData.portsInJackEngine);
        audioRoute1 = a.value(0);
        audioRoute2 = a.value(1);
    } else if (pl->layerType() == KonfytPatchLayer::TypeAudioIn) {
        audioRoute1 = pl->audioInPortData.jackRouteLeft;
        audioRoute2 = pl->audioInPortData.jackRouteRight;
    }
    if (midiRoute) {
        layerIndicatorHandler.layerWidgetAdded(layerWidget, midiRoute);
    }
    if (audioRoute1) {
        layerIndicatorHandler.layerWidgetAdded(layerWidget, audioRoute1, audioRoute2);
    }

    // Add to our internal list
    if (index < 0) { index = layerWidgetList.count(); }
    this->layerWidgetList.insert(index, layerWidget);

    // Add to gui layer list
    ui->listWidget_Layers->insertItem(index, item);
    // and set the item widget
    ui->listWidget_Layers->setItemWidget(item, layerWidget);
    // Put setSizeHint() after setItemWidget() so size is accurate since widget is already displayed.
    // Use zero width so that horizontal scrollbars don't appear when shrinking.
    item->setSizeHint(QSize(0, layerWidget->size().height()));

    // Layer widget connections
    connect(layerWidget, &KonfytLayerWidget::slider_moved_signal,
            this, &MainWindow::onLayer_slider_moved);

    connect(layerWidget, &KonfytLayerWidget::solo_clicked_signal,
            this, &MainWindow::onLayer_solo_clicked);

    connect(layerWidget, &KonfytLayerWidget::mute_clicked_signal,
            this, &MainWindow::onLayer_mute_clicked);

    connect(layerWidget, &KonfytLayerWidget::leftToolbutton_clicked_signal,
            this, &MainWindow::onLayer_leftToolbutton_clicked);

    connect(layerWidget, &KonfytLayerWidget::rightToolbutton_clicked_signal,
            this, &MainWindow::onLayer_rightToolbutton_clicked);

    connect(layerWidget, &KonfytLayerWidget::sendMidiEvents_clicked_signal,
            this, &MainWindow::onLayer_midiSend_clicked);
}

/* Remove a patch layer from the engine, GUI and the internal list. */
void MainWindow::removePatchLayer(KonfytLayerWidget *layerWidget)
{
    pengine.removeLayer(layerWidget->getPatchLayer());

    removePatchLayerFromGuiOnly(layerWidget);

    patchModified();
}

/* Remove a layer item from the GUI (and our internal list) only.
 * This is used if the layers in the engine should not be modified. */
void MainWindow::removePatchLayerFromGuiOnly(KonfytLayerWidget *layerWidget)
{
    // Remove from our internal list (do first since layerWidget will be deleted
    // when QListWidgetItem is removed from list widget)
    layerWidgetList.removeAll(layerWidget);

    // Remove from indicators handler
    layerIndicatorHandler.layerWidgetRemoved(layerWidget);

    // Remove from GUI list
    QListWidgetItem* item = layerWidget->getListWidgetItem();
    delete item;
}

/* Clear patch's layer items from GUI list. */
void MainWindow::clearPatchLayersFromGuiOnly()
{
    while (this->layerWidgetList.count()) {
        removePatchLayerFromGuiOnly(this->layerWidgetList.at(0));
    }
}

void MainWindow::movePatchLayer(int indexFrom, int indexTo)
{
    KONFYT_ASSERT_RETURN(indexFrom >= 0);
    KONFYT_ASSERT_RETURN(indexFrom < layerWidgetList.count());
    KONFYT_ASSERT_RETURN(indexTo >= 0);
    KONFYT_ASSERT_RETURN(indexTo < layerWidgetList.count());

    KonfytLayerWidget* layerWidget = layerWidgetList.value(indexFrom);
    KfPatchLayerWeakPtr patchLayer = layerWidget->getPatchLayer();

    pengine.moveLayer(patchLayer, indexTo);

    removePatchLayerFromGuiOnly(layerWidget);
    addPatchLayerToGUI(patchLayer, indexTo);

    patchModified();
}

KfJackMidiRoute *MainWindow::jackMidiRouteFromLayerWidget(KonfytLayerWidget *layerWidget)
{
    KfJackMidiRoute* route = nullptr;

    KfPatchLayerSharedPtr patchLayer = layerWidget->getPatchLayer();
    if (!patchLayer) { return nullptr; }

    KonfytPatchLayer::LayerType type = patchLayer->layerType();
    if (type == KonfytPatchLayer::TypeMidiOut) {
        route = patchLayer->midiOutputPortData.jackRoute;
    } else if (type == KonfytPatchLayer::TypeSfz) {
        route = jack.getPluginMidiRoute(patchLayer->sfzData.portsInJackEngine);
    } else if (type == KonfytPatchLayer::TypeSoundfontProgram) {
        route = jack.getPluginMidiRoute(patchLayer->soundfontData.portsInJackEngine);
    }

    return route;
}

/* Clears and fills specified menu with items corresponding to project MIDI
 * input ports as well as a current port connections item and a new port item.
 * A property is set for each action corresponding to the port id, -1 for the
 * new port entry and -2 for the current port connections entry.
 * The item corresponding to the specified currentPortId will be marked to
 * indicate that it is the current setting. */
void MainWindow::updateMidiInPortsMenu(QMenu *menu, int currentPortId)
{
    menu->clear();

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    QAction* a = menu->addAction("Current Port Connections...");
    a->setProperty(PTY_MIDI_IN_PORT, -2);
    menu->addSeparator();

    QList<int> midiInIds = prj->midiInPort_getAllPortIds();
    foreach(int id, midiInIds) {
        PrjMidiPort projectPort = prj->midiInPort_getPort(id);
        QAction* action = menu->addAction( n2s(id) + " " + projectPort.portName);
        action->setProperty(PTY_MIDI_IN_PORT, id);
        if (id == currentPortId) {
            action->setIcon(QIcon("://icons/right_w_outline.png"));
        }
    }
    menu->addSeparator();

    QAction* b = menu->addAction("New Port");
    b->setProperty(PTY_MIDI_IN_PORT, -1);
}

/* Clears and fills the specified menu with items corresponding to the 16
 * MIDI channels, as well as an all channels entry. A property is set for each
 * action corresponding to the MIDI channel (zero-based) and -1 for the all
 * channels entry. The action corresponding to the specified currentChannel
 * is marked to indicate the current setting. */
void MainWindow::updateMidiInChannelMenu(QMenu *menu, int currentChannel)
{
    menu->clear();

    // "All" entry
    QAction* action = menu->addAction("All");
    action->setProperty(PTY_MIDI_CHANNEL, -1);
    if (currentChannel == -1) {
        action->setIcon(QIcon("://icons/right_w_outline.png"));
    }

    // MIDI channels
    for (int i=0; i<=15; i++) {
        QAction* action = menu->addAction("Channel " + n2s(i+1));
        action->setProperty(PTY_MIDI_CHANNEL, i);
        if (i == currentChannel) {
            action->setIcon(QIcon("://icons/right_w_outline.png"));
        }
    }
}

void MainWindow::updateLayerToolMenu()
{
    KonfytLayerWidget* layerWidget = layerToolMenuSourceitem;
    KfPatchLayerSharedPtr patchLayer = layerWidget->getPatchLayer().toStrongRef();
    KonfytPatchLayer::LayerType type = patchLayer->layerType();

    layerToolMenu.clear();
    // Menu items for layers with MIDI input
    if (    (type != KonfytPatchLayer::TypeUninitialized)
         && (!patchLayer->hasError())
         && (type != KonfytPatchLayer::TypeAudioIn) )
    {
        updateMidiInPortsMenu(&layerMidiInPortsMenu, patchLayer->midiInPortIdInProject());
        layerToolMenu.addMenu(&layerMidiInPortsMenu);
        updateMidiInChannelMenu(&layerMidiInChannelMenu, patchLayer->midiFilter().inChan);
        layerToolMenu.addMenu(&layerMidiInChannelMenu);
        layerToolMenu.addAction( ui->actionEdit_MIDI_Filter );
    }
    // Menu items for Audio input port layers
    if (type == KonfytPatchLayer::TypeAudioIn) {
        QAction* a = layerToolMenu.addAction("Input Port Connections...");
        connect(a, &QAction::triggered, [=](){
            // Show port in connections page
            showConnectionsPage();
            connectionsTreeSelectAudioInPort(patchLayer->audioInPortData.portIdInProject);
        });
    }
    // Menu items for MIDI output port layers
    if (type == KonfytPatchLayer::TypeMidiOut) {
        layerToolMenu.addAction( ui->actionEdit_MIDI_Send_List );
    }
    // Menu items for instrument layers
    if (    (type == KonfytPatchLayer::TypeSfz)
         || (type == KonfytPatchLayer::TypeSoundfontProgram) )
    {
        layerToolMenu.addAction( ui->actionReload_Layer );
    }
    // Menu items for layers that have a file path
    QString filepath = layerWidget->getFilePath();
    if (!filepath.isEmpty()) {
        layerToolMenu.addAction(ui->actionOpen_In_File_Manager_layerwidget);
    }
    // Remove layer menu item
    if (layerToolMenu.actions().count()) { layerToolMenu.addSeparator(); }
    layerToolMenu.addAction( ui->actionRemove_Layer );
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
    QString path = d.getExistingDirectory( this, "Select projects directory",
                                           ui->comboBox_settings_projectsDir->currentText() );
    if ( !path.isEmpty() ) {
        ui->comboBox_settings_projectsDir->setCurrentText( path );
    }
}

void MainWindow::on_pushButton_Settings_Soundfonts_clicked()
{
    // Show dialog to select soundfonts directory
    QFileDialog d;
    QString path = d.getExistingDirectory( this, "Select soundfonts directory",
                                           ui->comboBox_settings_soundfontDirs->currentText() );
    if ( !path.isEmpty() ) {
        ui->comboBox_settings_soundfontDirs->setCurrentText( path );
    }
}

void MainWindow::on_pushButton_Settings_Patches_clicked()
{
    // Show dialog to select patches directory
    QFileDialog d;
    QString path = d.getExistingDirectory( this, "Select patches directory",
                                           ui->comboBox_settings_patchDirs->currentText() );
    if ( !path.isEmpty() ) {
        ui->comboBox_settings_patchDirs->setCurrentText( path );
    }
}

void MainWindow::on_pushButton_Settings_Sfz_clicked()
{
    // Show dialog to select sfz directory
    QFileDialog d;
    QString path = d.getExistingDirectory( this, "Select sfz directory",
                                           ui->comboBox_settings_sfzDirs->currentText() );
    if ( !path.isEmpty() ) {
        ui->comboBox_settings_sfzDirs->setCurrentText( path );
    }
}

/* Action to save current patch as a copy in the current project. */
void MainWindow::on_actionSave_Patch_As_Copy_triggered()
{
    KonfytPatch* p = pengine.currentPatch();
    KonfytPatch* newPatch = new KonfytPatch();
    newPatch->fromByteArray(p->toByteArray());

    addPatchToProject(newPatch);

    setCurrentPatch(newPatch);

    ui->lineEdit_PatchName->setFocus();
    ui->lineEdit_PatchName->selectAll();
}

/* Action to add current patch to the library. */
void MainWindow::on_actionAdd_Patch_To_Library_triggered()
{
    KonfytPatch* pt = pengine.currentPatch(); // Get current patch

    if (savePatchToLibrary(pt)) {
        print("Saved to library.");
    } else {
        print("Could not save patch to library.");
    }
}

/* Action to save the current patch to file. */
void MainWindow::on_actionSave_Patch_To_File_triggered()
{
    // Save patch to user selected file

    KonfytPatch* pt = pengine.currentPatch(); // Get current patch
    QFileDialog d;
    QString filename = d.getSaveFileName(this,"Save patch as file", mPatchesDir, "*." + QString(KONFYT_PATCH_SUFFIX));
    if (filename=="") {return;} // Dialog was cancelled.

    // Add suffix if not already added (this is not foolproof, but what the hell.)
    if (!filename.contains("." + QString(KONFYT_PATCH_SUFFIX))) { filename = filename + "." + QString(KONFYT_PATCH_SUFFIX); }

    if (pt->savePatchToFile(filename)) {
        print("Patch saved.");
    } else {
        print("Failed saving patch to file.");
    }
}

/* Action to add new patch to project. */
void MainWindow::on_actionNew_Patch_triggered()
{
    KonfytPatch* patch = newPatchToProject();
    setCurrentPatch(patch);
    ui->lineEdit_PatchName->setFocus();
    ui->lineEdit_PatchName->selectAll();
}

/* Action to Let user browse and select a patch file to add to the project. */
void MainWindow::on_actionAdd_Patch_From_File_triggered()
{
    if (!mCurrentProject) {
        print("No project active.");
        return;
    }

    QFileDialog d;
    QString filename = d.getOpenFileName(this,
                                         "Open patch from file",
                                         mPatchesDir,
                                         "*." + QString(KONFYT_PATCH_SUFFIX));
    if (filename.isEmpty()) { return; }

    addPatchToProjectFromFile(filename);
}

/* Add button clicked (not its menu). */
void MainWindow::on_toolButton_AddPatch_clicked()
{
    // Add a new patch to the project.
    on_actionNew_Patch_triggered();
}

void MainWindow::on_pushButton_ShowConsole_clicked()
{
    consoleDialog.show();
}

bool MainWindow::eventFilter(QObject* /*object*/, QEvent *event)
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
            case Qt::Key_Right:
            case Qt::Key_Down:
                // Next patch
                setCurrentPatchByIndex(currentPatchIndex() + 1);
                break;
            case Qt::Key_Left:
            case Qt::Key_Up:
                // Previous patch
                setCurrentPatchByIndex(currentPatchIndex() - 1);
                break;
            case Qt::Key_1:
                setCurrentPatchByIndex( 0 );
                break;
            case Qt::Key_2:
                setCurrentPatchByIndex( 1 );
                break;
            case Qt::Key_3:
                setCurrentPatchByIndex( 2 );
                break;
            case Qt::Key_4:
                setCurrentPatchByIndex( 3 );
                break;
            case Qt::Key_5:
                setCurrentPatchByIndex( 4 );
                break;
            case Qt::Key_6:
                setCurrentPatchByIndex( 5 );
                break;
            case Qt::Key_7:
                setCurrentPatchByIndex( 6 );
                break;
            case Qt::Key_8:
                setCurrentPatchByIndex( 7 );
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
        KONFYT_ASSERT_FAIL("Invalid eventFilterMode");
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

/* Apply clicked on the MIDI filter editor dialog. */
void MainWindow::on_pushButton_midiFilter_Apply_clicked()
{
    KonfytMidiFilter f;
    ProjectPtr prj = mCurrentProject;
    if (!prj) {
        print("ERROR: No current project.");
        return;
    }

    if (midiFilterEditType == MidiFilterEditPort) {
        if (prj->midiInPort_exists(midiFilterEditPort)) {
            f = prj->midiInPort_getPort(midiFilterEditPort).filter;
        } else {
            print("ERROR: Port does not exist in project.");
            return;
        }
    } else {
        f = midiFilterEditItem->getPatchLayer().toStrongRef()->midiFilter();
    }

    // Update the filter from the GUI
    f.setZone( ui->spinBox_midiFilter_LowNote->value(),
               ui->spinBox_midiFilter_HighNote->value(),
               ui->spinBox_midiFilter_Add->value(),
               ui->spinBox_midiFilter_LowVel->value(),
               ui->spinBox_midiFilter_HighVel->value(),
               ui->spinBox_midiFilter_VelLimitMin->value(),
               ui->spinBox_midiFilter_VelLimitMax->value());
    f.ignoreGlobalTranspose = ui->checkBox_midiFilter_ignoreGlobalTranspose->isChecked();
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
        jack.setPortFilter(prj->midiInPort_getPort(midiFilterEditPort).jackPort, f);
    } else {
        // Update filter in gui layer item
        pengine.setLayerFilter(midiFilterEditItem->getPatchLayer(), f);
        midiFilterEditItem->refresh();
    }

    // Indicate project needs to be saved
    setProjectModified();

    // Switch back to previous view
    on_pushButton_midiFilter_Cancel_clicked();
}

void MainWindow::on_toolButton_MidiFilter_lowNote_clicked()
{
    ui->spinBox_midiFilter_LowNote->setValue(midiFilterLastEvent.data1());
}

void MainWindow::on_toolButton_MidiFilter_HighNote_clicked()
{
    ui->spinBox_midiFilter_HighNote->setValue(midiFilterLastEvent.data1());
}

void MainWindow::on_toolButton_MidiFilter_Add_clicked()
{
    ui->spinBox_midiFilter_Add->setValue(midiFilterLastEvent.data1());
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

/* Live mode button clicked. */
void MainWindow::on_pushButton_LiveMode_clicked()
{
    if (ui->pushButton_LiveMode->isChecked()) {
        // Switch to live mode
        ui->stackedWidget_left->setCurrentWidget(ui->page_Live);
        // Install event filter to catch all global key presses
        eventFilterMode = EVENT_FILTER_MODE_LIVE;
        appInfo.a->installEventFilter(this);
    } else {
        // Switch out of live mode to normal
        ui->stackedWidget_left->setCurrentWidget(ui->pageLibrary);
        // Remove event filter
        appInfo.a->removeEventFilter(this);
    }
}

void MainWindow::on_actionMaster_Volume_Up_triggered()
{
    on_horizontalSlider_MasterGain_sliderMoved(
                ui->horizontalSlider_MasterGain->value() + 1);
}

void MainWindow::on_actionMaster_Volume_Down_triggered()
{
    on_horizontalSlider_MasterGain_sliderMoved(
                ui->horizontalSlider_MasterGain->value() - 1);
}

/* External apps list: item double clicked. */
void MainWindow::on_listWidget_ExtApps_doubleClicked(const QModelIndex& /*index*/)
{
    // Run the currently selected process.
    on_pushButton_ExtApp_RunSelected_clicked();
}

void MainWindow::on_listWidget_ExtApps_clicked(const QModelIndex &index)
{
    // Put the contents of the selected item in the External Apps text box
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    KonfytProcess* p = prj->getProcessList()[index.row()];
    ui->lineEdit_ExtApp->setText(p->appname);
}

void MainWindow::on_toolButton_layer_AddMidiPort_clicked()
{
    // Button is configured to popup a menu upon click.
    // See patchMidiOutPortsMenu and on_patchMidiOutPortsMenu_ActionTrigger().
}

/* Library/filesystem soundfont program list: item double clicked. */
void MainWindow::on_listWidget_LibraryBottom_itemDoubleClicked(QListWidgetItem* item)
{
    if (mPreviewMode) { setPreviewMode(false); }

    KONFYT_ASSERT_RETURN(!selectedSfont.isNull());

    addSoundfontProgramToCurrentPatch(selectedSfont->filename,
        selectedSfont->presets.value(ui->listWidget_LibraryBottom->row(item)));
}

/* Library tree: item double clicked. */
void MainWindow::on_treeWidget_Library_itemDoubleClicked(
        QTreeWidgetItem *item, int /*column*/)
{
    if (mPreviewMode) { setPreviewMode(false); }

    if ( libraryTreeItemType(item) == libTreePatch ) {

        addPatchToProjectFromFile(librarySelectedPatch()->filename);

    } else if (libraryTreeItemType(item) == libTreeSoundfont) {

        // Select the first soundfont program
        ui->listWidget_LibraryBottom->setCurrentRow(0);
        KfSoundPtr sf = librarySelectedSfont();
        if (sf) {
            addSoundfontProgramToCurrentPatch(sf->filename, selectedSoundfontProgramInLibOrFs());
        }

    } else if ( libraryTreeItemType(item) == libTreeSFZ ) {

        addSfzToCurrentPatch( librarySelectedSfz()->filename );

    }
}

void MainWindow::on_toolButton_MidiFilter_lowVel_clicked()
{
    ui->spinBox_midiFilter_LowVel->setValue( midiFilterLastEvent.data2() );
}

void MainWindow::on_toolButton_MidiFilter_HighVel_clicked()
{
    ui->spinBox_midiFilter_HighVel->setValue( midiFilterLastEvent.data2() );
}

void MainWindow::on_toolButton_MidiFilter_lastCC_clicked()
{
    ui->lineEdit_MidiFilter_CC->setText( n2s(midiFilterLastEvent.data1()) );
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

/* Change library/filesystem view tab. */
void MainWindow::on_tabWidget_library_currentChanged(int /*index*/)
{
    if (ui->tabWidget_library->currentWidget() == ui->tab_filesystem) {
        // Filesystem tab selected
        refreshFilesystemView();
        on_treeWidget_filesystem_currentItemChanged(
                    ui->treeWidget_filesystem->currentItem(), nullptr);
    } else {
        // Library tab selected
        on_treeWidget_Library_currentItemChanged(
                    ui->treeWidget_Library->currentItem(), nullptr);
    }
}

void MainWindow::refreshFilesystemView()
{
    ui->lineEdit_filesystem_path->setText(fsview_currentPath);

    ProjectPtr prj = mCurrentProject;
    QString project_dir;
    if (prj) {
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
            item->setIcon(0, mFolderIcon);
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
                item->setIcon(0, mFileIcon);
                item->setText(0, info.fileName());
                ui->treeWidget_filesystem->addTopLevelItem(item);
                fsMap.insert(item, info);
            }
        }
    }
}

/* Change filesystem view directory, storing current path for the 'back'
 * functionality. */
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

void MainWindow::on_treeWidget_filesystem_currentItemChanged(
        QTreeWidgetItem *current, QTreeWidgetItem* /*previous*/)
{
    if (current == nullptr) {

        clearLibFsInfoArea();

    } else {

        QFileInfo info = fsMap.value(current);
        if ( fileIsPatch(info.filePath())) {

            showPatchInLibFsInfoArea();

        } else if ( fileIsSoundfont(info.filePath()) ) {

            selectedSfont.clear();
            showSfontInfoInLibFsInfoArea(info.filePath());

        } else if ( fileIsSfzOrGig(info.filePath()) ) {

            showSfzContentInLibFsInfoArea(info.filePath());

        } else {
            clearLibFsInfoArea();
        }
    }

    if (mPreviewMode) {
        loadPreviewPatchAndUpdateGui();
    }
}

/* Filesystem view: double clicked file or folder in file list. */
void MainWindow::on_treeWidget_filesystem_itemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    QFileInfo info = fsMap.value(item);
    if (info.isDir()) {
        // If directory, cd to directory.
        cdFilesystemView(info.filePath());

    } else if ( fileIsPatch(info.filePath()) ) {

        // File is a patch
        if (mPreviewMode) { setPreviewMode(false); }
        addPatchToProjectFromFile(info.filePath());

    } else if ( fileIsSoundfont(info.filePath()) ) {
        // If soundfont, read soundfont and fill program list.

        // Initiate mainwindow waiter (this disables the GUI and indicates to the user
        // that we are busy getting the soundfont in the background)
        startWaiter("Loading soundfont...");
        ui->textBrowser_LibraryBottom->append("Loading soundfont...");
        // Request soundfont from database
        db.loadSfontInfoFromFile(info.filePath());
        // This might take a while. The result will be sent by signal to the
        // onDatabaseSfontInfoLoaded() slot where we will continue.

    } else if ( fileIsSfzOrGig(info.filePath()) ) {

        // If sfz or gig, load file.
        if (mPreviewMode) { setPreviewMode(false); }
        addSfzToCurrentPatch( info.filePath() );

    }
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

/* Add left and right audio output ports to JACK client for a bus, named
 * according to the given bus number. The left and right port references in JACK
 * are assigned to the leftPort and rightPort parameters. */
void MainWindow::addAudioBusToJack(int busNo, KfJackAudioPort **leftPort, KfJackAudioPort **rightPort)
{
    *leftPort = jack.addAudioPort(QString("bus_%1_L").arg(busNo), false);
    *rightPort = jack.addAudioPort(QString("bus_%1_R").arg(busNo), false);
}

/* Adds an audio bus to the current project and Jack. Returns bus index.
   Returns -1 on error. */
int MainWindow::addBus()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) {
        print("Select a project.");
        return -1;
    }

    // Add to project
    QString busName = "AudioBus_" + n2s( prj->audioBus_count() );
    int busId = prj->audioBus_add(busName);

    KfJackAudioPort* left;
    KfJackAudioPort* right;
    addAudioBusToJack( busId, &left, &right );
    if ( (left == nullptr) || (right == nullptr) ) {
        prj->audioBus_remove(busId);
        print("ERROR: Failed to create audio bus. Failed to add Jack port(s).");
        return -1;
    }
    PrjAudioBus bus = prj->audioBus_getBus(busId);
    bus.leftJackPort = left;
    bus.rightJackPort = right;
    prj->audioBus_replace(busId, bus);

    updateBusGainInJackEngine(busId);

    return busId;
}

void MainWindow::on_actionAdd_Bus_triggered()
{
    int busId = addBus();
    if (busId >= 0) {
        showConnectionsPage();
        // Select newly added bus
        connectionsTreeSelectBus(busId);
    }
}

/* Adds an audio input port to current project and Jack and returns port's ID.
   Returns -1 on error. */
int MainWindow::addAudioInPort()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) {
        print("Select a project.");
        return -1;
    }


    int portId = prj->audioInPort_add( "New Audio In Port" );
    KfJackAudioPort* left;
    KfJackAudioPort* right;
    addAudioInPortsToJack( portId, &left, &right );
    if ( (left != nullptr) && (right != nullptr) ) {
        // Update in project
        prj->audioInPort_setJackPorts(portId, left, right);

        return portId;
    } else {
        print("ERROR: Failed to create audio input port. Failed to add Jack port.");
        prj->audioInPort_remove(portId);
        return -1;
    }
}

/* Adds new Midi input port to project and Jack. Returns the port index.
 * Returns -1 on error. */
int MainWindow::addMidiInPort()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) {
        print("Select a project.");
        return -1;
    }

    int prjPortId = prj->midiInPort_addPort("New MIDI In Port");
    KfJackMidiPort* port = addMidiInPortToJack(prjPortId);
    if (port) {

        prj->midiInPort_setJackPort(prjPortId, port);

        // Update filter in Jack
        PrjMidiPort p = prj->midiInPort_getPort(prjPortId);
        jack.setPortFilter(port, p.filter);

        return prjPortId;

    } else {
        // Could not create Jack port. Remove port from project again.
        print("ERROR: Could not add MIDI input port. Failed to create JACK port.");
        prj->midiInPort_removePort(prjPortId);
        return -1;
    }
}

void MainWindow::on_actionAdd_Audio_In_Port_triggered()
{
    int portId = addAudioInPort();
    if (portId >= 0) {
        showConnectionsPage();
        connectionsTreeSelectAudioInPort(portId);
    }
}

void MainWindow::on_actionAdd_MIDI_In_Port_triggered()
{
    int portId = addMidiInPort();
    if (portId >= 0) {
        showConnectionsPage();
        connectionsTreeSelectMidiInPort(portId);
    }
}

/* Adds new midi output port to project and Jack. Returns the port index.
 * Returns -1 on error. */
int MainWindow::addMidiOutPort()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) {
        print("Select a project.");
        return -1;
    }


    int prjPortId = prj->midiOutPort_addPort("New MIDI Out Port"); // Add to current project
    KfJackMidiPort* port = addMidiOutPortToJack(prjPortId);
    if (port) {
        prj->midiOutPort_setJackPort(prjPortId, port);
        return prjPortId;
    } else {
        // Could not create Jack port. Remove port from project again.
        print("ERROR: Could not add MIDI output port. Failed to create JACK port.");
        prj->midiOutPort_removePort(prjPortId);
        return -1;
    }
}

void MainWindow::on_actionAdd_MIDI_Out_Port_triggered()
{
    int portId = addMidiOutPort();
    if (portId >= 0) {
        showConnectionsPage();
        connectionsTreeSelectMidiOutPort(portId);
    }
}

/* An audio input port in the project consists of a left and right Jack port.
 * This function adds the left and right Jack audio input ports, named according
 * to the given port number. The resulting Jack port IDs are assigned to the
 * leftPortId and rightPortId function parameters. */
void MainWindow::addAudioInPortsToJack(int portNo, KfJackAudioPort **leftPort, KfJackAudioPort **rightPort)
{
    *leftPort = jack.addAudioPort(QString("audio_in_%1_L").arg(portNo), true);
    *rightPort = jack.addAudioPort(QString("audio_in_%1_R").arg(portNo), true);
}

/* Adds a new MIDI output port to JACK, named with the specified port ID. The
 * JACK engine port ID is returned, and null if an error occured. */
KfJackMidiPort *MainWindow::addMidiOutPortToJack(int numberLabel)
{
    return jack.addMidiPort(QString("midi_out_%1").arg(numberLabel), false);
}

/* Adds a new MIDI port to JACK, named with the specified port ID. The JACK
 * engine port ID is returned, and null if an error occured. */
KfJackMidiPort *MainWindow::addMidiInPortToJack(int numberLabel)
{
    return jack.addMidiPort(QString("midi_in_%1").arg(numberLabel), true);
}

bool MainWindow::jackPortBelongstoUs(QString jackPortName)
{
    bool ret = false;

    QStringList ourJackClients;
    ourJackClients.append(jack.clientName());
    ourJackClients.append(pengine.ourJackClientNames());

    for (int i=0; i < ourJackClients.count(); i++) {
        QString name = ourJackClients[i] + ":";
        if (jackPortName.startsWith(name)) {
            ret = true;
            break;
        }
    }

    return ret;
}

void MainWindow::handleRouteMidiEvent(KfJackMidiRxEvent rxEvent)
{
    KONFYT_ASSERT_RETURN(rxEvent.midiRoute);

    // Indicate MIDI, sustain and pitchbend for layers
    layerIndicatorHandler.jackEventReceived(rxEvent);

    // Layer MIDI Filter Editor last received
    if (midiFilterEditType == MidiFilterEditLayer) {
        if (midiFilterEditRoute == rxEvent.midiRoute) {
            updateMidiFilterEditorLastRx(rxEvent.midiEvent);
        }
    }

    // List of received MIDI events on MIDI send list editor page
    if (ui->stackedWidget->currentWidget() == ui->midiSendListPage) {
        midiSendListEditorMidiRxList.addMidiEvent(rxEvent.midiEvent);
    }
}

void MainWindow::handlePortMidiEvent(KfJackMidiRxEvent rxEvent)
{
    KONFYT_ASSERT_RETURN(rxEvent.sourcePort);

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    KonfytMidiEvent ev = rxEvent.midiEvent;

    // Indicate global sustain/pitchbend
    portIndicatorHandler.jackEventReceived(rxEvent);
    updateGlobalSustainIndicator();
    updateGlobalPitchbendIndicator();

    // Show in console if enabled.
    int portIdInPrj = prj->midiInPort_getPortIdWithJackId(rxEvent.sourcePort);
    if (portIdInPrj < 0) {
        print("ERROR: NO PORT IN PROJECT MATCHING JACK PORT.");
    }
    if (console_showMidiMessages) {
        QString portName = "UNKNOWN";
        if (portIdInPrj >= 0) {
            PrjMidiPort prt = prj->midiInPort_getPort(portIdInPrj);
            portName = prt.portName;
        }
        print("MIDI EVENT " + ev.toString() + " from port " + portName);
    }

    // Global MIDI indicator "LED"
    ui->MIDI_indicator->setChecked(true);
    midiIndicatorTimer.start(500, this);

    // Route MIDI Filter Editor last received
    if (midiFilterEditType == MidiFilterEditPort) {
        if (portIdInPrj == midiFilterEditPort) {
            updateMidiFilterEditorLastRx(ev);
        }
    }

    // List of received MIDI events in Triggers page
    if (ui->stackedWidget->currentWidget() == ui->triggersPage) {
        triggersPageMidiEventList.addMidiEvent(ev);
        return; // We are on the Triggers page, skip normal processing
    }

    // If program change without bank select, switch patch if checkbox is checked.
    if (ev.type() == MIDI_EVENT_TYPE_PROGRAM) {
        if ( (ev.bankMSB == -1) && (ev.bankLSB == -1) ) {
            if (prj->isProgramChangeSwitchPatches()) {
                setCurrentPatchByIndex(ev.program());
            }
        }
    }

    // Hash midi event to a key
    int key = hashMidiEventToInt(ev.type(), ev.channel, ev.data1(), ev.bankMSB, ev.bankLSB);
    // Determine if event passes as button press
    bool buttonPass = 0;
    if (ev.type() == MIDI_EVENT_TYPE_PROGRAM) {
        buttonPass = true;
    } else {
        buttonPass = ev.data2() > 0;
    }

    // Get the appropriate action based on the key
    QAction* action = triggersMidiActionHash[key];

    // Perform the action
    if (action == ui->actionPanic) {

        if (buttonPass) { ui->actionPanic->trigger(); }

    } else if (action == ui->actionPanicToggle) {

        if (buttonPass) { ui->actionPanicToggle->trigger(); }

    } else if (action == ui->actionNext_Patch) {

        if (buttonPass) { setCurrentPatchByIndex(currentPatchIndex() + 1); }

    } else if (action == ui->actionPrevious_Patch) {

        if (buttonPass) { setCurrentPatchByIndex(currentPatchIndex() - 1); }

    } else if (action == ui->actionMaster_Volume_Slider) {

        if (masterGainMidiCtrlr.midiInput(ev.data2())) {
            setMasterGain((float)masterGainMidiCtrlr.value()/127.0);
        }

    } else if (action == ui->actionMaster_Volume_Up) {

        if (buttonPass) { ui->actionMaster_Volume_Up->trigger(); }

    } else if (action == ui->actionMaster_Volume_Down) {

        if (buttonPass) { ui->actionMaster_Volume_Down->trigger(); }

    } else if (action == ui->actionProject_save) {

        if (buttonPass) { ui->actionProject_save->trigger(); }

    } else if (channelGainActions.contains(action)) {

        midi_setLayerGain( channelGainActions.indexOf(action), ev.data2() );

    } else if (channelSoloActions.contains(action)) {

        midi_setLayerSolo( channelSoloActions.indexOf(action), ev.data2() );

    } else if (channelMuteActions.contains(action)) {

        midi_setLayerMute( channelMuteActions.indexOf(action), ev.data2() );

    } else if (patchActions.contains(action)) {

        setCurrentPatchByIndex( patchActions.indexOf(action) );

    } else if (action == ui->actionGlobal_Transpose_12_Down) {

        if (buttonPass) { setMasterInTranspose(-12, true); }

    } else if (action == ui->actionGlobal_Transpose_12_Up) {

        if (buttonPass) { setMasterInTranspose(12, true); }

    } else if (action == ui->actionGlobal_Transpose_1_Down) {

        if (buttonPass) { setMasterInTranspose(-1, true); }

    } else if (action == ui->actionGlobal_Transpose_1_Up) {

        if (buttonPass) { setMasterInTranspose(1, true); }

    } else if (action == ui->actionGlobal_Transpose_Zero) {

        if (buttonPass) { setMasterInTranspose(0, false); }

    }
}

void MainWindow::onJackPrint(QString msg)
{
    print("JACK Engine: " + msg);
}

void MainWindow::setupExternalAppsMenu()
{
    extAppsMenuActions_Append.insert(
                extAppsMenu.addAction(
                    "Project Directory Reference: " + QString(STRING_PROJECT_DIR)),
                STRING_PROJECT_DIR );

    extAppsMenu.addSeparator();

    extAppsMenuActions_Set.insert(
                extAppsMenu.addAction("a2jmidid -ue (export hardware, without ALSA IDs)"),
                "a2jmidid -ue" );
    extAppsMenuActions_Set.insert(
                extAppsMenu.addAction("zynaddsubfx -l (Load .xmz state file)"),
                "zynaddsubfx -l " );
    extAppsMenuActions_Set.insert(
                extAppsMenu.addAction("zynaddsubfx -L (Load .xiz instrument file)"),
                "zynaddsubfx -L " );
    extAppsMenuActions_Set.insert(
                extAppsMenu.addAction("jack-keyboard"),
                "jack-keyboard" );
    extAppsMenuActions_Set.insert(
                extAppsMenu.addAction("VMPK (Virtual Keyboard)"),
                "vmpk" );
    extAppsMenuActions_Set.insert( extAppsMenu.addAction("Ardour"), "ardour " );
    extAppsMenuActions_Set.insert( extAppsMenu.addAction("Carla"), "carla " );

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

void MainWindow::on_tree_portsBusses_currentItemChanged(
        QTreeWidgetItem *current, QTreeWidgetItem* /*previous*/)
{
    if (!current) { return; }

    // Enable MIDI Filter button if MIDI in port selected
    if ( current->parent() == midiInParent ) {
        // Midi input port is selected
        ui->frame_connectionsPage_MidiFilter->setVisible(true);
    } else {
        ui->frame_connectionsPage_MidiFilter->setVisible(false);
    }

    // Show/hide and update ignore-global-volume checkbox if bus selected
    if (mCurrentProject) {
        if (current->parent() == busParent) {
            ui->checkBox_connectionsPage_ignoreGlobalVolume->setVisible(true);
            int busId = tree_busMap.value(current);
            PrjAudioBus bus = mCurrentProject->audioBus_getBus(busId);
            ui->checkBox_connectionsPage_ignoreGlobalVolume->setChecked(
                        bus.ignoreMasterGain);
        } else {
            ui->checkBox_connectionsPage_ignoreGlobalVolume->setVisible(false);
        }
    }

    updateConnectionsTree();
}

/* Remove the bus/port selected in the connections ports/buses tree widget. */
void MainWindow::on_actionRemove_BusPort_triggered()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

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
    QList<KonfytPatch*> patchList = prj->getPatchList();
    for (int iPatch=0; iPatch < patchList.count(); iPatch++) {
        KonfytPatch* patch = patchList[iPatch];
        QList<KfPatchLayerWeakPtr> layers = patch->layers();
        for (int iLayer=0; iLayer < layers.count(); iLayer++) {
            KfPatchLayerSharedPtr layer = layers[iLayer];
            bool append = false;
            if (busSelected) {
                if ( (layer->layerType() == KonfytPatchLayer::TypeAudioIn)
                     || ( layer->layerType() == KonfytPatchLayer::TypeSfz)
                     || ( layer->layerType() == KonfytPatchLayer::TypeSoundfontProgram) ) {
                    append = layer->busIdInProject() == id;
                }
            }
            if (audioInSelected) {
                if ( layer->layerType() == KonfytPatchLayer::TypeAudioIn ) {
                append = layer->audioInPortData.portIdInProject == id;
                }
            }
            if (midiOutSelected) {
                if ( layer->layerType() == KonfytPatchLayer::TypeMidiOut ) {
                append = layer->midiOutputPortData.portIdInProject == id;
                }
            }
            if (midiInSelected) {
                if ( (layer->layerType() == KonfytPatchLayer::TypeSfz)
                     || ( layer->layerType() == KonfytPatchLayer::TypeMidiOut)
                     || ( layer->layerType() == KonfytPatchLayer::TypeSoundfontProgram) ) {
                    append = layer->midiInPortIdInProject() == id;
                }
            }
            if (append) {
                usingPatches.append(iPatch);
                usingLayers.append(iLayer);
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
                    KonfytPatch* patch = prj->getPatch(usingPatches[i]);
                    KfPatchLayerWeakPtr layer = patch->layers()[usingLayers[i]];
                    pengine.setLayerBus( layer, busToChangeTo );
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
                    KonfytPatch* patch = prj->getPatch(usingPatches[i]);
                    KfPatchLayerWeakPtr layer = patch->layers()[usingLayers[i]];
                    pengine.removeLayer( patch, layer );
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
                    KonfytPatch* patch = prj->getPatch(usingPatches[i]);
                    KfPatchLayerWeakPtr layer = patch->layers()[usingLayers[i]];
                    pengine.removeLayer( patch, layer );
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
                    KonfytPatch* patch = prj->getPatch(usingPatches[i]);
                    KfPatchLayerWeakPtr layer = patch->layers()[usingLayers[i]];
                    pengine.setLayerMidiInPort( layer, portToChangeTo );
                }
                // Removal will be done below.

            } else { return; } // Do not remove port

        }
    } // end usingPatches.count()

    // Remove the bus/port
    if (busSelected) {
        // Remove the bus
        jack.removeAudioPort(bus.leftJackPort);
        jack.removeAudioPort(bus.rightJackPort);
        prj->audioBus_remove(id);
        tree_busMap.remove(item);
    }
    else if (audioInSelected) {
        // Remove the port
        jack.removeAudioPort(audioInPort.leftJackPort);
        jack.removeAudioPort(audioInPort.rightJackPort);
        prj->audioInPort_remove(id);
        tree_audioInMap.remove(item);
    }
    else if (midiOutSelected) {
        // Remove the port
        jack.removeMidiPort(midiOutPort.jackPort);
        portIndicatorHandler.portRemoved(midiOutPort.jackPort);
        prj->midiOutPort_removePort(id);
        tree_midiOutMap.remove(item);
        // Remove port from global indicator handler
        portIndicatorHandler.portRemoved(midiOutPort.jackPort);
    } else if (midiInSelected) {
        // Remove the port
        jack.removeMidiPort(midiInPort.jackPort);
        prj->midiInPort_removePort(id);
        tree_midiInMap.remove(item);
    }

    delete item;
    updatePatchView();
}


/* Prepare and show the filesystem tree view context menu. */
void MainWindow::on_treeWidget_filesystem_customContextMenuRequested(const QPoint &pos)
{
    fsViewContextMenu.clear();

    QList<QAction*> actions;
    actions.append( ui->actionAdd_Path_To_External_App_Box );
    actions.append( ui->actionAdd_Path_to_External_App_Box_Relative_to_Project );
    actions.append( ui->actionOpen_In_File_Manager_fsview );
    fsViewContextMenu.addActions(actions);

    fsViewMenuItem = ui->treeWidget_filesystem->itemAt(pos);

    fsViewContextMenu.popup(QCursor::pos());
}

void MainWindow::on_actionAdd_Path_To_External_App_Box_triggered()
{
    QString path;

    if (!fsViewMenuItem) {
        // No item is selected in the filesystem list. Use current path
        path = fsview_currentPath;
    } else {
        // Get item's path
        QFileInfo info = fsMap.value(fsViewMenuItem);
        path = info.filePath();
    }

    // Add quotes
    path = "\"" + path + "\"";

    ui->lineEdit_ExtApp->setText( ui->lineEdit_ExtApp->text()
                                  + path);
    ui->lineEdit_ExtApp->setFocus();
}

void MainWindow::on_toolButton_filesystem_projectDir_clicked()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }
    if (prj->getDirname().length() == 0) { return; }

    cdFilesystemView( prj->getDirname() );
}

/* Action triggered from filesystem tree view to open item in file manager. */
void MainWindow::on_actionOpen_In_File_Manager_fsview_triggered()
{
    QString path;

    if (!fsViewMenuItem) {
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

void MainWindow::on_actionAdd_Path_to_External_App_Box_Relative_to_Project_triggered()
{
    QString path;

    if (!fsViewMenuItem) {
        // No item is selected in the filesystem list. Use current path
        path = fsview_currentPath;
    } else {
        // Get item's path
        QFileInfo info = fsMap.value(fsViewMenuItem);
        path = info.filePath();
    }

    // Make relative to project directory
    ProjectPtr prj = mCurrentProject;
    if (prj) {
        QString projPath = prj->getDirname();
        QDir projDir(projPath);
        path = QString(STRING_PROJECT_DIR) + "/" + projDir.relativeFilePath(path);
    }

    // Add quotes
    path = "\"" + path + "\"";

    ui->lineEdit_ExtApp->setText( ui->lineEdit_ExtApp->text()
                                  + path);
    ui->lineEdit_ExtApp->setFocus();
}

/* Initialises patch engine. Must be called after JACK engine has been set up. */
void MainWindow::setupPatchEngine()
{
    connect(&pengine, &KonfytPatchEngine::print, this, &MainWindow::print);
    connect(&pengine, &KonfytPatchEngine::statusInfo, [this](QString msg){
        ui->textBrowser_Testing->setText(msg);
    });
    connect(&pengine, &KonfytPatchEngine::patchLayerLoaded,
            this, &MainWindow::onPatchLayerLoaded);

    pengine.initPatchEngine(&jack, appInfo);
}

/* Update the input and output port settings for the preview patch layer. */
void MainWindow::updatePreviewPatchLayer()
{
    foreach (KfPatchLayerSharedPtr layer, mPreviewPatch.layers()) {
        if (layer->hasError()) { continue; }

        // Set the MIDI input channel
        KonfytMidiFilter filter = layer->midiFilter();
        filter.inChan = previewPatchMidiInChannel;
        layer->setMidiFilter(filter);

        // Set the MIDI input port
        layer->setMidiInPortIdInProject(previewPatchMidiInPort);

        // Set the audio output bus
        layer->setBusIdInProject(previewPatchBus);

        // Update in patch engine (if we are in preview mode)
        if (mPreviewMode) {
            pengine.setLayerFilter(layer, filter);
            pengine.setLayerMidiInPort(layer, previewPatchMidiInPort);
            pengine.setLayerBus(layer, previewPatchBus);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Only accept event if user allows
    if (!appInfo.headless) {
        if (!requestCurrentProjectClose()) {
            event->ignore();
            return;
        }
    }

    event->accept();
}

/* Context menu requested for a library tree item. */
void MainWindow::on_treeWidget_Library_customContextMenuRequested(const QPoint &pos)
{
    libraryMenuItem = ui->treeWidget_Library->itemAt(pos);
    LibraryTreeItemType itemType = libraryTreeItemType( libraryMenuItem );

    libraryContextMenu.clear();

    QList<QAction*> actions;

    actions.append( ui->actionOpen_In_File_Manager_library );
    // Disable menu item if no applicable tree widget item is selected
    ui->actionOpen_In_File_Manager_library->setEnabled( itemType != libTreeInvalid );

    libraryContextMenu.addActions(actions);

    libraryContextMenu.popup(QCursor::pos());
}

/* Action triggered from library tree view to open item in file manager. */
void MainWindow::on_actionOpen_In_File_Manager_library_triggered()
{
    if (!libraryMenuItem) { return; }

    QString path;

    LibraryTreeItemType itemType = libraryTreeItemType( libraryMenuItem );

    if ( itemType == libTreeSoundfontRoot ) { path = this->mSoundfontsDir; }
    else if ( itemType == libTreePatchesRoot ) { path = this->mPatchesDir; }
    else if ( itemType == libTreeSFZRoot) { path = this->mSfzDir; }
    else if ( itemType == libTreeSoundfontFolder ) {
        path = librarySfTree.foldersMap.value( libraryMenuItem );
    }
    else if ( itemType == libTreeSoundfont ) {
        path = librarySfTree.soundsMap.value(libraryMenuItem)->filename;
    }
    else if ( itemType == libTreeSFZFolder ) {
        path = librarySfzTree.foldersMap.value( libraryMenuItem );
    }
    else if ( itemType == libTreeSFZ ) {
        path = librarySfzTree.soundsMap.value(libraryMenuItem)->filename;
    }
    else if ( itemType == libTreePatch ) {
        path = this->mPatchesDir;
    } else {
        return;
    }

    // If a file is selected, change path to the folder name of the file
    QFileInfo info(path);
    if (!info.isDir()) { path = info.path(); }

    openFileManager(path);
}

void MainWindow::showSfzContentInLibFsInfoArea(QString filename)
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

        print("File exceeds max allowed size to show contents: " + filename);
        text = "File exceeds max allowed size to show contents.";

    } else if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {

        print("Failed to open file: " + filename);
        text = "Failed to open file.";

    } else {

        text = QString(file.readAll());
        file.close();

    }

    return text;
}

void MainWindow::on_actionRename_BusPort_triggered()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

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
void MainWindow::on_tree_portsBusses_itemChanged(QTreeWidgetItem *item, int /*column*/)
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    if (item->parent() == busParent) {
        // Bus is selected
        int id = tree_busMap.value( item );
        PrjAudioBus bus = prj->audioBus_getBus(id);

        bus.busName = item->text(0);
        prj->audioBus_replace(id, bus);

    } else if (item->parent() == audioInParent) {
        // Audio input port selected
        int id = tree_audioInMap.value(item);
        prj->audioInPort_setName(id, item->text(0));

    } else if (item->parent() == midiOutParent) {
        // MIDI Output port selected
        int id = tree_midiOutMap.value(item);
        prj->midiOutPort_setName(id, item->text(0));

    } else if (item->parent() == midiInParent) {
        // MIDI Input port selected
        int id = tree_midiInMap.value(item);
        prj->midiInPort_setName(id, item->text(0));
    }
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
    if (!item) { return; }

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    int eventRow = ui->listWidget_triggers_eventList->currentRow();
    if (eventRow < 0) { return; }

    KonfytMidiEvent selectedEvent = triggersPageMidiEventList.selectedMidiEvent();
    QAction* action = triggersItemActionHash[item];
    KonfytTrigger trig = KonfytTrigger();

    trig.actionText = action->text();
    trig.bankLSB = selectedEvent.bankLSB;
    trig.bankMSB = selectedEvent.bankMSB;
    trig.channel = selectedEvent.channel;
    trig.data1 = selectedEvent.data1();
    trig.type = selectedEvent.type();

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
    if (!item) { return; }

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    QAction* action = triggersItemActionHash[item];

    // remove from project
    prj->removeTrigger(action->text());

    // Remove action from the quick lookup hash
    int key = triggersMidiActionHash.key(action);
    triggersMidiActionHash.remove(key);

    // Refresh the page
    showTriggersPage();
}

void MainWindow::on_tree_Triggers_itemDoubleClicked(QTreeWidgetItem* /*item*/, int /*column*/)
{
    on_pushButton_triggersPage_assign_clicked();
}

void MainWindow::onTriggersMidiEventListDoubleClicked()
{
    on_pushButton_triggersPage_assign_clicked();
}

void MainWindow::on_checkBox_Triggers_ProgSwitchPatches_clicked()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    prj->setProgramChangeSwitchPatches( ui->checkBox_Triggers_ProgSwitchPatches->isChecked() );
}

void MainWindow::on_checkBox_ConsoleShowMidiMessages_clicked()
{
    setConsoleShowMidiMessages( ui->checkBox_ConsoleShowMidiMessages->isChecked() );
}

void MainWindow::updateGlobalSustainIndicator()
{
    ui->MIDI_indicator_sustain->setChecked(portIndicatorHandler.isSustainDown());
}

void MainWindow::updateGlobalPitchbendIndicator()
{
    ui->MIDI_indicator_pitchbend->setChecked(portIndicatorHandler.isPitchbendNonzero());
}

void MainWindow::setConsoleShowMidiMessages(bool show)
{
    ui->checkBox_ConsoleShowMidiMessages->setChecked( show );
    consoleDialog.setShowMidiEvents( show );
    console_showMidiMessages = show;
}

void MainWindow::setupConsoleDialog()
{
    connect(this, &MainWindow::printSignal, &consoleDialog, &ConsoleDialog::print);
}

void MainWindow::on_pushButton_RestartApp_clicked()
{
    if (requestCurrentProjectClose()) {
        // Restart the app (see code in main.cpp)
        QCoreApplication::exit(APP_RESTART_CODE);
    }
}

void MainWindow::on_actionProject_save_triggered()
{
    // Save project
    saveCurrentProject();
}

void MainWindow::on_actionProject_New_triggered()
{
    if (requestCurrentProjectClose()) {
        loadNewProject(); // Create and load new project
        newPatchToProject(); // Create a new patch and add to current project.
        setCurrentPatchByIndex(0);
        mCurrentProject->setModified(false);
    }
}

void MainWindow::on_actionProject_Open_triggered()
{
    if (!requestCurrentProjectClose()) { return; }

    // Show open dialog box
    QFileDialog* d = new QFileDialog();
    QString filename = d->getOpenFileName(
                this, "Select project to open", projectsDir,
                "*" + QString(PROJECT_FILENAME_EXTENSION) );
    if (filename == "") {
        print("Cancelled.");
        return;
    }

    loadProjectFromFile(filename); // Read project from file and load it
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
        pengine.setPatchNote(ui->textBrowser_patchNote->toPlainText());
        patchModified();
    }
}

void MainWindow::on_toolButton_layer_down_clicked()
{
    int index = ui->listWidget_Layers->currentRow();
    if (index < 0) { return; } // Nothing selected
    int to = wrapIndex(index + 1, layerWidgetList.count());
    movePatchLayer(index, to);
    // Keep moved item selected
    ui->listWidget_Layers->setCurrentRow(to);
}

void MainWindow::on_toolButton_layer_up_clicked()
{
    int index = ui->listWidget_Layers->currentRow();
    if (index < 0) { return; } // Nothing selected
    int to = wrapIndex(index - 1, layerWidgetList.count());
    movePatchLayer(index, to);
    // Keep moved item selected
    ui->listWidget_Layers->setCurrentRow(to);
}

void MainWindow::on_listWidget_Layers_currentItemChanged(
        QListWidgetItem* current, QListWidgetItem* /*previous*/)
{
    foreach (KonfytLayerWidget* w, layerWidgetList) {
        w->setHighlighted(w->getListWidgetItem() == current);
    }
}

void MainWindow::on_toolButton_Project_clicked()
{
    saveCurrentProject();
}

void MainWindow::on_actionProject_SaveAs_triggered()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    QString oldName = prj->getProjectName();
    bool oldModified = prj->isModified();
    QString oldDirname = prj->getDirname();

    // Query user for new project name
    QString newName = QInputDialog::getText(this, "Save Project As",
                                            "New Project Name",
                                            QLineEdit::Normal,
                                            prj->getProjectName());
    if (newName.isEmpty()) { return; }

    // Clear the project dir name so it can be saved as new project
    prj->setDirname("");

    setProjectName(newName);

    bool saved = saveProject(prj);
    if (saved) {
        print("Saved project as new project.");
    } else {
        print("Project not saved as new project.");
        messageBox("Project was not saved as a new project.");
        // Restore original project state
        setProjectName(oldName);
        prj->setDirname(oldDirname);
        prj->setModified(oldModified);
    }
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
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    int startPatch = currentPatchIndex();

    for (int i=0; i<prj->getNumPatches(); i++) {
        setCurrentPatchByIndex(i);
    }

    setCurrentPatchByIndex(startPatch);
}

void MainWindow::on_pushButton_ExtApp_Replace_clicked()
{
    int row = ui->listWidget_ExtApps->currentRow();
    if (row < 0) { return; }

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    KonfytProcess* process = prj->getProcessList()[row];
    process->appname = ui->lineEdit_ExtApp->text();

    QListWidgetItem* item = ui->listWidget_ExtApps->item(row);
    item->setText(ui->lineEdit_ExtApp->text());

    setProjectModified();
}

void MainWindow::on_MIDI_indicator_clicked()
{
    ui->MIDI_indicator->setChecked(false);
}

void MainWindow::on_MIDI_indicator_sustain_clicked()
{
    ui->MIDI_indicator_sustain->setChecked(false);
}

void MainWindow::on_MIDI_indicator_pitchbend_clicked()
{
    ui->MIDI_indicator_pitchbend->setChecked(false);
}

void MainWindow::on_toolButton_MidiFilter_inChan_last_clicked()
{
    ui->comboBox_midiFilter_inChannel->setCurrentIndex(midiFilterLastEvent.channel+1);
}

void MainWindow::setMasterInTranspose(int transpose, bool relative)
{
    if (relative) {
        transpose += ui->spinBox_MasterIn_Transpose->value();
    }
    ui->spinBox_MasterIn_Transpose->setValue( transpose );
    // See also on_spinBox_MasterIn_Transpose_valueChanged() slot.
}

void MainWindow::on_spinBox_MasterIn_Transpose_valueChanged(int arg1)
{
    jack.setGlobalTranspose(arg1);
}

void MainWindow::on_pushButton_MasterIn_TransposeSub12_clicked()
{
    setMasterInTranspose(-12, true);
}

void MainWindow::on_pushButton_MasterIn_TransposeAdd12_clicked()
{
    setMasterInTranspose(12, true);
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

    updateJackPage();
}

void MainWindow::updateJackPage()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    QStringList outPorts;
    QStringList inPorts;
    QList<KonfytJackConPair> conList;
    if (jackPage_audio) {
        // Audio Jack ports
        outPorts = jack.getAudioOutputPortsList();
        inPorts = jack.getAudioInputPortsList();
        conList = prj->getJackAudioConList();
    } else {
        // MIDI Jack ports
        outPorts = jack.getMidiOutputPortsList();
        inPorts = jack.getMidiInputPortsList();
        conList = prj->getJackMidiConList();
    }

    // Update JACK output ports
    ui->treeWidget_jackPortsOut->clear();
    for (int i=0; i < outPorts.count(); i++) {
        QString client_port = outPorts[i];
        if (jackPortBelongstoUs(client_port)) { continue; }
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0,client_port);
        ui->treeWidget_jackPortsOut->addTopLevelItem(item);
    }

    // Update JACK input ports
    ui->treeWidget_jackportsIn->clear();
    for (int i=0; i < inPorts.count(); i++) {
        QString client_port = inPorts[i];
        if (jackPortBelongstoUs(client_port)) { continue; }
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
            item->setBackground(QBrush(Qt::red));
        }
        ui->listWidget_jackConnections->addItem( item );
    }
}

void MainWindow::on_pushButton_jackConAdd_clicked()
{
    QTreeWidgetItem* itemOut = ui->treeWidget_jackPortsOut->currentItem();
    QTreeWidgetItem* itemIn = ui->treeWidget_jackportsIn->currentItem();
    if ( (!itemOut) || (!itemIn) ) { return; }

    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    KonfytJackConPair p;
    if (jackPage_audio) {
        // Add Audio Jack port to project
        p = prj->addJackAudioCon(itemOut->text(0), itemIn->text(0));
    } else {
        // Add MIDI Jack port to project
        p = prj->addJackMidiCon(itemOut->text(0), itemIn->text(0));
    }

    // Add to Jack engine
    jack.addOtherJackConPair(p);
    // Add to jack connections GUI list
    ui->listWidget_jackConnections->addItem( p.toString() );

    updateGUIWarnings();
}

void MainWindow::on_pushButton_jackConRemove_clicked()
{
    int row = ui->listWidget_jackConnections->currentRow();
    if (row<0) { return; }
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

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
    jack.removeOtherJackConPair(p);
    // Remove from GUI
    delete ui->listWidget_jackConnections->item(row);

    updateGUIWarnings();
}

void MainWindow::on_checkBox_filesystem_ShowOnlySounds_toggled(bool /*checked*/)
{
    refreshFilesystemView();
}

void MainWindow::on_pushButton_LavaMonster_clicked()
{
    showAboutDialog();
}

void MainWindow::on_toolButton_PatchListMenu_clicked()
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

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
    if (!mCurrentProject) { return; }

    bool visible = !mCurrentProject->getShowPatchListNumbers();
    mCurrentProject->setShowPatchListNumbers(visible);
    patchListAdapter.setPatchNumbersVisible(visible);
}

void MainWindow::toggleShowPatchListNotes()
{
    if (!mCurrentProject) { return; }

    bool visible = !mCurrentProject->getShowPatchListNotes();
    mCurrentProject->setShowPatchListNotes(visible);
    patchListAdapter.setPatchNotesVisible(visible);
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

void MainWindow::setupSettings()
{
    // Settings dir is standard (XDG) config dir
    settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    print("Settings path: " + settingsDir);
    // Check if settings file exists
    if (loadSettingsFile(settingsDir)) {
        print("Settings loaded.");
    } else {
        print("Could not load settings.");
        // Check if old settings file exists.
        QString oldDir = QDir::homePath() + "/.konfyt";
        if (loadSettingsFile(oldDir)) {
            print("Loaded settings from old location: " + settingsDir);
            print("Saving to new settings location.");
            if (saveSettingsFile()) {
                print("Saved settings file to new location: " + settingsDir);
            } else {
                print("Could not save settings to new location: " + settingsDir);
            }
        } else {
            // If settings file does not exist, it's probably the first run.
            // Show about dialog and settings.
            createSettingsDir();
            mSettingsFirstRun = true;
            showAboutDialog();
        }
    }

    // Set up settings dialog
    ui->label_SettingsPath->setText(QString("%1\n%2")
                                    .arg(ui->label_SettingsPath->text())
                                    .arg(settingsDir));

    QString docsPath = QString("%1/%2")
            .arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .arg(APP_NAME);
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    ui->comboBox_settings_projectsDir->addItem(docsPath + "/Projects");
    ui->comboBox_settings_projectsDir->addItem(appDataPath + "/Projects");

    ui->comboBox_settings_soundfontDirs->addItem(docsPath + "/Soundfonts");
    ui->comboBox_settings_soundfontDirs->addItem(appDataPath + "/Soundfonts");

    ui->comboBox_settings_patchDirs->addItem(docsPath + "/Patches");
    ui->comboBox_settings_patchDirs->addItem(appDataPath + "/Patches");

    ui->comboBox_settings_sfzDirs->addItem(docsPath + "/sfz");
    ui->comboBox_settings_sfzDirs->addItem(appDataPath + "/sfz");

    // Initialise default settings
    if (projectsDir.isEmpty()) {
        projectsDir = ui->comboBox_settings_projectsDir->itemText(0);
    }
    if (mPatchesDir.isEmpty()) {
        setPatchesDir(ui->comboBox_settings_patchDirs->itemText(0));
    }
    if (mSoundfontsDir.isEmpty()) {
        setSoundfontsDir(ui->comboBox_settings_soundfontDirs->itemText(0));
    }
    if (mSfzDir.isEmpty()) {
        setSfzDir(ui->comboBox_settings_sfzDirs->itemText(0));
    }
}

void MainWindow::on_toolButton_MidiFilter_VelLimitMin_last_clicked()
{
    ui->spinBox_midiFilter_VelLimitMin->setValue( midiFilterLastEvent.data2() );
}

/* User right-clicked on panic button. */
void MainWindow::on_pushButton_Panic_customContextMenuRequested(const QPoint& /*pos*/)
{
    // Momentary panic
    on_actionPanic_triggered();
}

void MainWindow::on_toolButton_MidiFilter_VelLimitMax_last_clicked()
{
    ui->spinBox_midiFilter_VelLimitMax->setValue( midiFilterLastEvent.data2() );
}

void MainWindow::setupMidiSendListEditor()
{
    connect(&midiSendListEditorMidiRxList, &MidiEventListWidgetAdapter::itemClicked,
            this, &MainWindow::onMidiSendListEditorMidiRxListItemClicked);
    midiSendListEditorMidiRxList.init(ui->listWidget_midiSendList_lastReceived);
}

void MainWindow::showMidiSendListEditor()
{
    midiSendList = midiSendListEditItem->getPatchLayer().toStrongRef()->midiSendList;

    ui->listWidget_midiSendList->clear();
    foreach (MidiSendItem item, midiSendList) {
        ui->listWidget_midiSendList->addItem(item.toString());
    }

    // Set default event in editor
    MidiSendItem e;
    midiEventToMidiSendEditor(e);

    // Switch to MIDI send list editor page
    ui->stackedWidget->setCurrentWidget(ui->midiSendListPage);
}

void MainWindow::midiEventToMidiSendEditor(MidiSendItem item)
{
    int comboIndex = midiSendTypeComboItems.indexOf(item.midiEvent.type());
    if (comboIndex < 0) { comboIndex = 0; }
    ui->comboBox_midiSendList_type->setCurrentIndex(comboIndex);

    // Channel in GUI is 1-based, but in event it is 0-based.
    ui->spinBox_midiSendList_channel->setValue(item.midiEvent.channel + 1);

    // Note/CC page
    ui->spinBox_midiSendList_cc_data1->setValue(item.midiEvent.data1());
    ui->spinBox_midiSendList_cc_data2->setValue(item.midiEvent.data2());

    // Program page
    ui->spinBox_midiSendList_program->setValue(item.midiEvent.program());
    ui->checkBox_midiSendList_bank->setChecked(item.midiEvent.bankMSB >= 0);
    ui->spinBox_midiSendList_msb->setValue(item.midiEvent.bankMSB);
    ui->spinBox_midiSendList_lsb->setValue(item.midiEvent.bankLSB);

    // Pitchbend page
    ui->spinBox_midiSendList_pitchbend->setValue(item.midiEvent.pitchbendValueSigned());

    // Sysex page
    ui->lineEdit_midiSendList_sysex_bytes->setText(item.midiEvent.dataToHexString());

    // Description
    ui->lineEdit_midiSendList_Description->setText(item.description);
}

MidiSendItem MainWindow::midiEventFromMidiSendEditor()
{
    KonfytMidiEvent e;

    int type = midiSendTypeComboItems.value(
                ui->comboBox_midiSendList_type->currentIndex(),
                MIDI_EVENT_TYPE_CC);
    int data1 = ui->spinBox_midiSendList_cc_data1->value();
    int data2 = ui->spinBox_midiSendList_cc_data2->value();

    // Channel in GUI is 1-based, but in event it is 0-based.
    e.channel = ui->spinBox_midiSendList_channel->value() - 1;

    switch (type) {
    case MIDI_EVENT_TYPE_PITCHBEND:
        e.setPitchbend(ui->spinBox_midiSendList_pitchbend->value());
        break;
    case MIDI_EVENT_TYPE_PROGRAM:
        e.setProgram( ui->spinBox_midiSendList_program->value() );
        if (ui->checkBox_midiSendList_bank->isChecked()) {
            e.bankMSB = ui->spinBox_midiSendList_msb->value();
            e.bankLSB = ui->spinBox_midiSendList_lsb->value();
        }
        break;
    case MIDI_EVENT_TYPE_NOTEON:
        e.setNoteOn(data1, data2);
        break;
    case MIDI_EVENT_TYPE_NOTEOFF:
        e.setNoteOff(data1, data2);
        break;
    case MIDI_EVENT_TYPE_CC:
        e.setCC(data1, data2);
        break;
    case MIDI_EVENT_TYPE_SYSTEM:
        // For sysex, force channel to zero.
        e.channel = 0;
        e.setType(type);
        e.setDataFromHexString( ui->lineEdit_midiSendList_sysex_bytes->text() );
        break;
    }

    MidiSendItem item;
    item.midiEvent = e;
    item.description = ui->lineEdit_midiSendList_Description->text();

    return item;
}

void MainWindow::on_pushButton_jackCon_OK_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);
}

/* Action to toggle always-active for current patch. */
void MainWindow::on_actionAlways_Active_triggered()
{
    KonfytPatch* p = pengine.currentPatch();
    p->alwaysActive = !p->alwaysActive;
    ui->actionAlways_Active->setChecked(p->alwaysActive);
    ui->label_patch_alwaysActive->setVisible(p->alwaysActive);

    mCurrentProject->setModified(true);
}

void MainWindow::on_actionEdit_MIDI_Filter_triggered()
{
    midiFilterEditType = MidiFilterEditLayer;
    midiFilterEditItem = layerToolMenuSourceitem;
    midiFilterEditRoute = jackMidiRouteFromLayerWidget(midiFilterEditItem);
    showMidiFilterEditor();
}

void MainWindow::on_actionReload_Layer_triggered()
{
    pengine.reloadLayer( layerToolMenuSourceitem->getPatchLayer() );
    layerToolMenuSourceitem->refresh();
}

void MainWindow::on_actionOpen_In_File_Manager_layerwidget_triggered()
{
    QString filepath = layerToolMenuSourceitem->getFilePath();
    // If path is a file, change path to the folder name of the file
    QFileInfo info(filepath);
    if (!info.isDir()) { filepath = info.path(); }
    openFileManager(filepath);
}

void MainWindow::on_actionRemove_Layer_triggered()
{
    removePatchLayer( layerToolMenuSourceitem );
}

void MainWindow::on_pushButton_midiSendList_apply_clicked()
{
    midiSendListEditItem->getPatchLayer().toStrongRef()->midiSendList = midiSendList;

    // Update in GUI layer item
    midiSendListEditItem->refresh();

    // Indicate project needs to be saved
    setProjectModified();

    // Switch back to patch view
    on_pushButton_midiSendList_cancel_clicked();
}

void MainWindow::on_pushButton_midiSendList_cancel_clicked()
{
    // Switch back to patch view
    ui->stackedWidget->setCurrentWidget(ui->PatchPage);
}

void MainWindow::on_pushButton_midiSendList_add_clicked()
{
    MidiSendItem item = midiEventFromMidiSendEditor();

    // Add event
    midiSendList.append(item);
    ui->listWidget_midiSendList->addItem(item.toString());
}

void MainWindow::on_comboBox_midiSendList_type_currentIndexChanged(int index)
{
    int type = midiSendTypeComboItems.value(index, MIDI_EVENT_TYPE_CC);
    switch (type) {
    case MIDI_EVENT_TYPE_CC:
        ui->stackedWidget_midiSend->setCurrentWidget(ui->page_midiSend_cc);
        break;
    case MIDI_EVENT_TYPE_PROGRAM:
        ui->stackedWidget_midiSend->setCurrentWidget(ui->page_midiSend_program);
        break;
    case MIDI_EVENT_TYPE_NOTEON:
        ui->stackedWidget_midiSend->setCurrentWidget(ui->page_midiSend_cc);
        break;
    case MIDI_EVENT_TYPE_NOTEOFF:
        ui->stackedWidget_midiSend->setCurrentWidget(ui->page_midiSend_cc);
        break;
    case MIDI_EVENT_TYPE_PITCHBEND:
        ui->stackedWidget_midiSend->setCurrentWidget(ui->page_midiSend_pitchbend);
        break;
    case MIDI_EVENT_TYPE_SYSTEM:
        ui->stackedWidget_midiSend->setCurrentWidget(ui->page_midiSend_sysex);
        break;
    }
}

/* MIDI send list program bank checkbox: enable/disable bank select boxes. */
void MainWindow::on_checkBox_midiSendList_bank_stateChanged(int arg1)
{
    bool enabled = (arg1 == Qt::Checked);
    ui->spinBox_midiSendList_msb->setEnabled(enabled);
    ui->spinBox_midiSendList_lsb->setEnabled(enabled);
    // If disabled, set MSB and LSB to -1 to indicate no bank select.
    if (!enabled) {
        ui->spinBox_midiSendList_msb->setValue(-1);
        ui->spinBox_midiSendList_lsb->setValue(-1);
    }
}

void MainWindow::on_listWidget_midiSendList_currentRowChanged(int currentRow)
{
    if (currentRow >= 0) {
        // Valid item selected
        midiEventToMidiSendEditor(midiSendList.value(currentRow));
    } else {
        // No item selected
        MidiSendItem item;
        midiEventToMidiSendEditor(item);
    }
}

void MainWindow::on_listWidget_midiSendList_itemClicked(QListWidgetItem *item)
{
    on_listWidget_midiSendList_currentRowChanged(
                ui->listWidget_midiSendList->row(item) );
}

void MainWindow::on_pushButton_midiSendList_pbmin_clicked()
{
    ui->spinBox_midiSendList_pitchbend->setValue(MIDI_PITCHBEND_SIGNED_MIN);
}

void MainWindow::on_pushButton_midiSendList_pbzero_clicked()
{
    ui->spinBox_midiSendList_pitchbend->setValue(0);
}

void MainWindow::on_pushButton_midiSendList_pbmax_clicked()
{
    ui->spinBox_midiSendList_pitchbend->setValue(MIDI_PITCHBEND_SIGNED_MAX);
}

void MainWindow::on_actionEdit_MIDI_Send_List_triggered()
{
    midiSendListEditItem = layerToolMenuSourceitem;
    midiSendListEditRoute = jackMidiRouteFromLayerWidget(midiSendListEditItem);
    showMidiSendListEditor();
}

void MainWindow::setupSavedMidiSendItems()
{
    savedMidiListDir = settingsDir + "/" + SAVED_MIDI_SEND_ITEMS_DIR;

    // Create directory if it doesn't exist
    QDir dir(savedMidiListDir);
    if (!dir.exists()) {
        if (dir.mkpath(savedMidiListDir)) {
            print("Created Saved-MIDI-Send-Items directory: " + savedMidiListDir);
        } else {
            print("Failed to create Saved-MIDI-Send-Items directory: " + savedMidiListDir);
        }
    }

    loadSavedMidiSendItems(savedMidiListDir);
}

void MainWindow::addSavedMidiSendItem(MidiSendItem item)
{
    savedMidiSendItems.append(item);
    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    treeItem->setText(0, item.toString());
    ui->treeWidget_savedMidiMessages->addTopLevelItem(treeItem);
}

void MainWindow::loadSavedMidiSendItems(QString dirname)
{
    print("Scanning for saved MIDI Send Items...");
    QStringList files = scanDirForFiles(dirname);

    foreach (QString filename, files) {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            print("Failed to open MIDI Send Item file: " + filename);
            continue;
        }
        QXmlStreamReader r(&file);
        r.setNamespaceProcessing(false);
        MidiSendItem item;
        QString error = item.readFromXmlStream(&r);
        if (!error.isEmpty()) {
            print("Errors for MIDI Send Item File " + filename + ":");
            print(error);
        }
        item.filename = filename;
        addSavedMidiSendItem(item);
    }

    print(QString("Saved MIDI send items loaded: %1 items.")
          .arg(savedMidiSendItems.count()));
}

bool MainWindow::saveMidiSendItemToFile(QString filename, MidiSendItem item)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        print("Failed to open MIDI Send Item file for writing: " + filename);
        return false;
    }

    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument();

    item.writeToXMLStream(&stream);

    stream.writeEndDocument();

    file.close();

    return true;
}

void MainWindow::on_pushButton_savedMidiMsgs_save_clicked()
{
    MidiSendItem item = midiEventFromMidiSendEditor();

    // Get unique filename
    QString filename = "event";
    if (!item.description.isEmpty()) {
        filename = item.description;
    }
    filename = getUniqueFilename(savedMidiListDir, filename, ".midiSendEvent");
    filename = savedMidiListDir + "/" + filename;

    // Save
    if (saveMidiSendItemToFile(filename, item)) {
        print("Saved MIDI Send Event to file: " + filename);
        item.filename = filename;
    } else {
        print("Failed to save MIDI Send event to file.");
    }

    // Add to GUI
    addSavedMidiSendItem(item);
}

void MainWindow::on_pushButton_savedMidiMsgs_remove_clicked()
{
    QTreeWidgetItem* selected = ui->treeWidget_savedMidiMessages->currentItem();
    if (selected == nullptr) { return; }

    int index = ui->treeWidget_savedMidiMessages->indexOfTopLevelItem(selected);
    MidiSendItem item = savedMidiSendItems[index];

    if (item.filename.isEmpty()) {
        print("Error removing saved MIDI send item: No filename associated with item.");
        return;
    }

    // Ask whether user is sure
    QMessageBox msgbox;
    msgbox.setText("Are you sure you want to delete the MIDI Send Event '" + item.toString() + "'?");
    msgbox.setInformativeText(item.filename);
    msgbox.setIcon(QMessageBox::Question);
    msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    int ret = msgbox.exec();
    if (ret == QMessageBox::Yes) {
        QFile f(item.filename);
        bool removed = f.remove();
        if (removed) {
            print("Removed MIDI Send Event file " + f.fileName());
            // Remove from list and GUI
            delete selected;
            savedMidiSendItems.removeAt(index);
        } else {
            print("Failed to remove MIDI Send Event file " + f.fileName());
        }
    }
}

void MainWindow::on_treeWidget_savedMidiMessages_itemClicked(QTreeWidgetItem *item, int /*column*/)
{
    int index = ui->treeWidget_savedMidiMessages->indexOfTopLevelItem(item);
    midiEventToMidiSendEditor(savedMidiSendItems[index]);
}

QString MainWindow::xrunTimeString(qint64 ms)
{
    int min = ms/1000/60;
    int sec = (ms/1000) % 60;
    int msec = ms % 1000;
    QString str = QString("+%1:%2:%L3")
            .arg(min)
            .arg(sec, 2, 10, QChar('0'))
            .arg(msec, 3, 10, QChar('0'));

    return str;
}

void MainWindow::setupJack()
{
    connect(&jack, &KonfytJackEngine::print, this, &MainWindow::onJackPrint);

    connect(&jack, &KonfytJackEngine::jackPortRegisteredOrConnected,
            this, &MainWindow::onJackPortRegisteredOrConnected);

    connect(&jack, &KonfytJackEngine::midiEventsReceived,
            this, &MainWindow::onJackMidiEventsReceived);

    connect(&jack, &KonfytJackEngine::audioEventsReceived,
            this, &MainWindow::onJackAudioEventsReceived);

    connect(&jack, &KonfytJackEngine::xrunOccurred, this, &MainWindow::onJackXrunOccurred);

    QString jackClientName = appInfo.jackClientName;
    if (jackClientName.isEmpty()) {
        jackClientName = KONFYT_JACK_DEFAULT_CLIENT_NAME;
    }
    if ( jack.initJackClient(jackClientName) ) {
        // Jack client initialised.
        print("Initialised JACK client with name " + jack.clientName());
    } else {
        // not.
        print("Could not initialise JACK client.");

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
}

void MainWindow::onMidiSendListEditorMidiRxListItemClicked()
{
    // User clicked on an item in the list of received MIDI events.
    // Load the event in the editor

    MidiSendItem m;
    m.midiEvent = midiSendListEditorMidiRxList.selectedMidiEvent();
    midiEventToMidiSendEditor(m);
}

void MainWindow::on_pushButton_midiSendList_replace_clicked()
{
    int index = ui->listWidget_midiSendList->currentRow();
    if (index < 0) { return; }

    MidiSendItem item = midiEventFromMidiSendEditor();
    midiSendList.replace(index, item);
    ui->listWidget_midiSendList->item(index)->setText(item.toString());
}

void MainWindow::on_toolButton_midiSendList_down_clicked()
{
    int index = ui->listWidget_midiSendList->currentRow();
    if (index < 0) { return; }

    int nexti = index + 1;
    if (nexti >= midiSendList.count()) {
        nexti = 0;
    }

    midiSendList.move(index, nexti);
    ui->listWidget_midiSendList->insertItem(
                nexti, ui->listWidget_midiSendList->takeItem(index));
    ui->listWidget_midiSendList->setCurrentRow(nexti);
}

void MainWindow::on_toolButton_midiSendList_up_clicked()
{
    int index = ui->listWidget_midiSendList->currentRow();
    if (index < 0) { return; }

    int nexti = index - 1;
    if (nexti < 0) {
        nexti = midiSendList.count() - 1;
    }

    midiSendList.move(index, nexti);
    ui->listWidget_midiSendList->insertItem(
                nexti, ui->listWidget_midiSendList->takeItem(index));
    ui->listWidget_midiSendList->setCurrentRow(nexti);
}

void MainWindow::on_pushButton_midiSendList_remove_clicked()
{
    int index = ui->listWidget_midiSendList->currentRow();
    if (index < 0) { return; }

    delete ui->listWidget_midiSendList->item(index);
    midiSendList.removeAt(index);
}

void MainWindow::on_pushButton_midiSendList_sendSelected_clicked()
{
    // Send the MIDI message currently being edited

    KonfytMidiEvent event = midiEventFromMidiSendEditor().midiEvent;
    KfPatchLayerSharedPtr layer = midiSendListEditItem->getPatchLayer();
    if (!layer->hasError()) {
        jack.sendMidiEventsOnRoute(layer->midiOutputPortData.jackRoute, {event});
    }
}

void MainWindow::on_pushButton_midiSendList_sendAll_clicked()
{
    KfPatchLayerSharedPtr layer = midiSendListEditItem->getPatchLayer();
    if (!layer->hasError()) {
        QList<KonfytMidiEvent> events;
        foreach (MidiSendItem item, midiSendList) {
            events.append(item.midiEvent);
        }
        jack.sendMidiEventsOnRoute(layer->midiOutputPortData.jackRoute, events);
    }
}

void MainWindow::on_stackedWidget_currentChanged(int /*arg1*/)
{
    QWidget* currentWidget = ui->stackedWidget->currentWidget();
    if (lastCenterWidget == ui->midiSendListPage) {
        // Changed away from MIDI Send List page
        ui->stackedWidget_left->setCurrentWidget(lastSidebarWidget);
    } else if (currentWidget == ui->midiSendListPage) {
        // Save current sidebar widget and change to saved MIDI send list
        lastSidebarWidget = ui->stackedWidget_left->currentWidget();
        ui->stackedWidget_left->setCurrentWidget(ui->page_savedMidiMsges);
    }
    lastCenterWidget = currentWidget;
}

void MainWindow::on_toolButton_LibraryPreview_clicked()
{
    setPreviewMode( ui->toolButton_LibraryPreview->isChecked() );
}

void MainWindow::on_spinBox_Triggers_midiPickupRange_valueChanged(int arg1)
{
    ProjectPtr prj = mCurrentProject;
    if (!prj) { return; }

    prj->setMidiPickupRange(arg1);
}

void MainWindow::on_checkBox_connectionsPage_ignoreGlobalVolume_clicked()
{
    if (!mCurrentProject) { return; }
    QTreeWidgetItem* currentItem = ui->tree_portsBusses->currentItem();
    if (currentItem->parent() != busParent) { return; }

    int busId = tree_busMap.value(currentItem);
    PrjAudioBus bus = mCurrentProject->audioBus_getBus(busId);
    bus.ignoreMasterGain = ui->checkBox_connectionsPage_ignoreGlobalVolume->isChecked();

    // Update in project
    mCurrentProject->audioBus_replace(busId, bus);

    // Update in JACK engine
    updateBusGainInJackEngine(busId);
}
