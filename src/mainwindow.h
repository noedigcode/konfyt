/******************************************************************************
 *
 * Copyright 2020 Gideon van der Kolf
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "aboutdialog.h"
#include "consoledialog.h"
#include "indicatorHandlers.h"
#include "konfytDatabase.h"
#include "konfytDefines.h"
#include "konfytFluidsynthEngine.h"
#include "konfytJackEngine.h"
#include "konfytLayerWidget.h"
#include "konfytMidiFilter.h"
#include "konfytPatchEngine.h"
#include "konfytPatchLayer.h"
#include "konfytProcess.h"
#include "konfytProject.h"
#include "patchListWidgetAdapter.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QShortcut>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTimer>
#include <QTreeWidgetItem>

#include <iostream>


#define APP_RESTART_CODE 1000

#define SETTINGS_DIR ".konfyt"
#define SETTINGS_FILE "konfyt.settings"
#define DATABASE_FILE "konfyt.database"

#define SAVED_MIDI_SEND_ITEMS_DIR "savedMidiSendItems"

#define EVENT_FILTER_MODE_LIVE 0
#define EVENT_FILTER_MODE_WAITER 1

#define returnSfontRequester_on_treeWidget_filesystem_itemDoubleClicked 0

#define TREE_ITEM_SEARCH_RESULTS "Search Results:"
#define TREE_ITEM_SOUNDFONTS "Soundfonts"
#define TREE_ITEM_PATCHES "Patches"
#define TREE_ITEM_SFZ "SFZ"

#define TREECON_COL_PORT 0
#define TREECON_COL_L 1
#define TREECON_COL_R 2

#define XML_SETTINGS "settings"
#define XML_SETTINGS_PRJDIR "projectsDir"
#define XML_SETTINGS_SFDIR "soundfontsDir"
#define XML_SETTINGS_PATCHESDIR "patchesDir"
#define XML_SETTINGS_SFZDIR "sfzDir"
#define XML_SETTINGS_FILEMAN "filemanager"

#define PTY_MIDI_CHANNEL "midiChannel"
#define PTY_MIDI_IN_PORT "midiInPort"
#define PTY_AUDIO_OUT_BUS "audioOutBus"
#define PTY_AUDIO_IN_PORT "audioInPort"
#define PTY_MIDI_OUT_PORT "midiOutPort"

namespace Ui {
class MainWindow;
}



enum LibraryTreeItemType { libTreeInvalid,
                           libTreePatchesRoot,
                           libTreePatch,
                           libTreeSFZRoot,
                           libTreeSFZFolder,
                           libTreeSFZ,
                           libTreeSoundfontRoot,
                           libTreeSoundfontFolder,
                           libTreeSoundfont };

enum MidiFilterEditType {
    MidiFilterEditPort,
    MidiFilterEditLayer
};



class MainWindow : public QMainWindow
{
    Q_OBJECT
    
    // ========================================================================
    // MainWindow
    // ========================================================================
public:
    explicit MainWindow(QWidget *parent, KonfytAppInfo appInfoArg);
    ~MainWindow();

    KonfytAppInfo appInfo;

private:
    Ui::MainWindow *ui;
    int eventFilterMode;
    bool mPrintStart = true;
    void setupGuiMenuButtons();
    void setupGuiDefaults();
    void printArgumentsInfo();

private slots:
    void closeEvent(QCloseEvent *);
    bool eventFilter(QObject *object, QEvent *event);

    void print(QString message);

    // Keyboard shortcuts
private:
    QShortcut* shortcut_save;
    QShortcut* shortcut_panic;
    void setupKeyboardShortcuts();
private slots:
    void shortcut_save_activated();
    void shortcut_panic_activated();

    // General helper functions
private:
    QString getBaseNameWithoutExtension(QString filepath);
    void error_abort(QString msg);
    void messageBox(QString msg);
    bool dirExists(QString dirname);
    QStringList scanDirForFiles(QString dirname, QString filenameExtension = "");

    // ========================================================================
    // Project related
    // ========================================================================
private:
    typedef QSharedPointer<KonfytProject> ProjectPtr;

    QStringList projectDirList;
    ProjectPtr mCurrentProject;
    void setupInitialProjectFromCmdLineArgs();
    void scanDirForProjects(QString dirname);
    void loadNewProject();
    bool loadProjectFromFile(QString filename);
    void loadProject(ProjectPtr prj);
    void unloadCurrentProject();
    bool saveCurrentProject();
    bool saveProject(ProjectPtr prj);
    bool informedUserAboutProjectsDir = false;
    bool requestCurrentProjectClose();

    KonfytPatch* newPatchToProject();
    void removePatchFromProject(int i);
    void addPatchToProject(KonfytPatch* patch);
    KonfytPatch* addPatchToProjectFromFile(QString filename);
    bool savePatchToLibrary(KonfytPatch* patch);

    QMenu projectsMenu;
    QMap<QAction*, QFileInfo> projectsMenuMap;
    void updateProjectsMenu();

    // Misc helper functions
    QString getUniqueFilename(QString dirname, QString name, QString extension);

    void setPatchModified(bool modified);
    void setProjectModified();
    void setProjectName(QString name);
    void gui_updateProjectName();

private slots:
    // Project modified
    void projectModifiedStateChanged(bool modified);

    // Projects Menu
    void onprojectMenu_ActionTrigger(QAction* action);

    // ========================================================================
    // Library and filesystem view
    // ========================================================================
private:
    bool library_isProgramSelected();
    KonfytSoundfontProgram library_getSelectedProgram();

    LibraryTreeItemType library_getTreeItemType(QTreeWidgetItem* item);
    LibraryTreeItemType library_getSelectedTreeItemType();

    KonfytPatch library_getSelectedPatch();
    KonfytSoundfont* library_getSelectedSfont();
    QString library_selectedSfz;

    // Library tree items
    QTreeWidgetItem*                         library_sfRoot;
    QMap<QTreeWidgetItem*, QString>          library_sfFolders; // All intermediate (non-root, non-bottom) items in soundfont tree and item path
    QMap<QTreeWidgetItem*, KonfytSoundfont*> library_sfMap;     // Bottom-most items in soundfont tree
    QTreeWidgetItem*                    library_patchRoot;
    QMap<QTreeWidgetItem*, KonfytPatch> library_patchMap;
    QTreeWidgetItem*                 library_sfzRoot;
    QMap<QTreeWidgetItem*, QString>  library_sfzFolders; // All the non-root and non-bottom items in the sfz tree and item path
    QMap<QTreeWidgetItem*, QString>  library_sfzMap;     // Bottom-most items in the sfz tree with corresponding path
    void buildSfzTree(QTreeWidgetItem* twi, KonfytDbTreeItem* item);
    void buildSfTree(QTreeWidgetItem* twi, KonfytDbTreeItem* item);

    // Preview button & menu
private:
    QMenu previewButtonMenu;
private slots:
    void preparePreviewMenu();
    void previewButtonMidiInPortMenuTrigger(QAction* action);
    void previewButtonMidiInChannelMenuTrigger(QAction* action);
    void previewButtonBusMenuTrigger(QAction* action);
    void on_toolButton_LibraryPreview_clicked();

    // Database
private:
    konfytDatabase db;
    void setupDatabase();
    bool saveDatabase();
    int returnSfontRequester;
    QList<KonfytSoundfontProgram> programList; // List of programs currently displayed in program list view in library.
    void library_refreshGUIProgramList();      // Refresh the GUI program list to match programList
private slots:
    void database_scanDirsFinished();
    void database_scanDirsStatus(QString msg);
    void database_returnSfont(KonfytSoundfont* sf);

private:
    bool searchMode;
    void fillTreeWithAll();
    void fillTreeWithSearch(QString search);
    void gui_updateTree();

    QMenu libraryMenu;
    QTreeWidgetItem* libraryMenuItem;

    // Filesystem view in library
    QString fsview_currentPath;
    QStringList fsview_back;
    void setupFilesystemView();
    void refreshFilesystemView();
    void cdFilesystemView(QString newpath);
    void selectItemInFilesystemView(QString path);
    QMap<QTreeWidgetItem*, QFileInfo> fsMap;
    QMenu fsViewMenu;
    QTreeWidgetItem* fsViewMenuItem;

    void openFileManager(QString path);

    void showSfzContentsBelowLibrary(QString filename);
    QString loadSfzFileText(QString filename);

private slots:
    void on_tabWidget_library_currentChanged(int index);

    void on_treeWidget_Library_itemClicked(QTreeWidgetItem *item, int column);
    void on_treeWidget_Library_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_treeWidget_Library_itemDoubleClicked(QTreeWidgetItem *item, int column);
    /* When the user right-clicks on the library tree. */
    void on_treeWidget_Library_customContextMenuRequested(const QPoint &pos);

    void on_listWidget_LibraryBottom_currentRowChanged(int currentRow);
    void on_listWidget_LibraryBottom_itemDoubleClicked(QListWidgetItem *item);

    void on_lineEdit_Search_returnPressed();
    void on_toolButton_ClearSearch_clicked();

    void on_treeWidget_filesystem_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_treeWidget_filesystem_itemDoubleClicked(QTreeWidgetItem *item, int column);
    /* When the user right-clicks on the filesystem tree. */
    void on_treeWidget_filesystem_customContextMenuRequested(const QPoint &pos);

    void on_toolButton_filesystem_up_clicked();
    void on_toolButton_filesystem_refresh_clicked();
    void on_toolButton_filesystem_home_clicked();
    void on_toolButton_filesystem_back_clicked();
    void on_lineEdit_filesystem_path_returnPressed();
    void on_toolButton_filesystem_projectDir_clicked();
    void on_checkBox_filesystem_ShowOnlySounds_toggled(bool checked);

    void on_actionOpen_In_File_Manager_library_triggered();
    void on_actionAdd_Path_To_External_App_Box_triggered();
    void on_actionOpen_In_File_Manager_fsview_triggered();
    void on_actionAdd_Path_to_External_App_Box_Relative_to_Project_triggered();

    // ========================================================================
    // Patches
    // ========================================================================
private:
    KonfytPatchEngine pengine;
    void setupPatchEngine();
    KonfytPatch* masterPatch = nullptr; // Current patch being played
    float masterGain = 1.0;     // Master gain when not in preview mode

    KonfytPatch previewPatch;   // Patch played when in preview mode
    float previewGain = 1.0;    // Gain when in preview mode
    int previewPatchMidiInPort = 0;
    int previewPatchMidiInChannel = -1;
    int previewPatchBus = 0;
    void updatePreviewPatchLayer();

    bool fileSuffixIs(QString file, QString suffix);
    bool fileIsPatch(QString file);
    bool fileIsSoundfont(QString file);
    bool fileIsSfzOrGig(QString file);

    void setMasterGain(float gain);

    // Current patch functions
    int mCurrentPatchIndex = -1;
    void setCurrentPatch(KonfytPatch *patch);
    void setCurrentPatchByIndex(int index);

    void newPatchIfMasterNull();
    void addSfzToCurrentPatch(QString sfzPath);
    void addProgramToCurrentPatch(KonfytSoundfontProgram p);
    void addMidiPortToCurrentPatch(int port);
    void addAudioInPortToCurrentPatch(int port);

    void loadPatchForModeAndUpdateGUI();

    // GUI patch functions
    bool previewMode = false;           // Use setPreviewMode() to change.
    void setPreviewMode(bool choice);   // Set preview mode on or off and updates GUI.
    void gui_updatePatchView();
    void gui_updateWindowTitle();
    PatchListWidgetAdapter patchListAdapter;
    void setupPatchListAdapter();
    bool patchNote_ignoreChange = false;
private slots:
    void onPatchSelected(KonfytPatch* patch);

    // Layers
private:
    QList<KonfytLayerWidget*> layerWidgetList;
    void addPatchLayerToGUI(KfPatchLayerWeakPtr patchLayer);
    void removePatchLayer(KonfytLayerWidget *layerWidget);
    void removePatchLayerFromGuiOnly(KonfytLayerWidget *layerWidget);
    void clearPatchLayersFromGuiOnly();
private slots:
    void onLayer_slider_moved(KonfytLayerWidget* layerWidget, float gain);
    void onLayer_solo_clicked(KonfytLayerWidget* layerWidget, bool solo);
    void onLayer_mute_clicked(KonfytLayerWidget* layerWidget, bool mute);
    void onLayer_midiSend_clicked(KonfytLayerWidget* layerWidget);
    void onLayer_rightToolbutton_clicked(KonfytLayerWidget* layerWidget);

    // patchMidiOutPortsMenu: Context menu when user clicks button to add a new MIDI output layer.
private:
    void updateMidiOutPortsMenu(QMenu* menu);
    QMenu patchMidiOutPortsMenu;
private slots:
    void onPatchMidiOutPortsMenu_aboutToShow();
    void onPatchMidiOutPortsMenu_ActionTrigger(QAction* action);

    // patchAudioInPortsMenu: Context menu when user clicks button to add new audio input port layer.
private:
    void updateAudioInPortsMenu(QMenu* menu);
    QMenu patchAudioInPortsMenu;
private slots:
    void onPatchAudioInPortsMenu_aboutToShow();
    void onPatchAudioInPortsMenu_ActionTrigger(QAction* action);

    // Layer left toolbutton menu
private:
    QMenu layerToolMenu;
    KonfytLayerWidget* layerToolMenuSourceitem;
    void gui_updateLayerToolMenu();
private slots:
    void onLayer_leftToolbutton_clicked(KonfytLayerWidget* layerItem);

    // Layer MIDI input ports menu
private:
    void updateMidiInPortsMenu(QMenu* menu, int currentPortId = -1);
    QMenu layerMidiInPortsMenu;
private slots:
    void onLayerMidiInPortsMenu_ActionTrigger(QAction* action);

    // Layer MIDI input channel menu
private:
    void updateMidiInChannelMenu(QMenu* menu, int currentChannel = -2);
    QMenu layerMidiInChannelMenu;
private slots:
    void onLayerMidiInChannelMenu_ActionTrigger(QAction* action);

    // layerBusMenu: Opened when user clicks on bus button of a layer which has audio output.
private:
    void updateBusMenu(QMenu* menu, int currentBusId = -1);
    QMenu layerBusMenu;
private slots:
    void onLayerBusMenu_ActionTrigger(QAction* action);

    // Layer MIDI out channel menu: Opened when user clicks on bus button of a layer which as MIDI output.
private:
    void updateLayerMidiOutChannelMenu(QMenu* menu, int currentMidiPort = -2);
    QMenu layerMidiOutChannelMenu;
private slots:
    void onLayerMidiOutChannelMenu_ActionTrigger(QAction* action);

    // Patch list menu
private:
    QMenu patchListMenu;
    QAction* patchListMenu_NumbersAction;
    QAction* patchListMenu_NotesAction;

private slots:
    // Patch view area

    // Patch related
    void on_lineEdit_PatchName_returnPressed();
    void on_lineEdit_PatchName_editingFinished();
    void on_lineEdit_ProjectName_editingFinished();
    void on_textBrowser_patchNote_textChanged();

    // Patch List
    void on_toolButton_RemovePatch_clicked();
    void on_toolButton_PatchUp_clicked();
    void on_toolButton_PatchDown_clicked();
    void on_toolButton_AddPatch_clicked();
    void on_pushButton_LoadAll_clicked();
    void on_toolButton_PatchListMenu_clicked();
    void toggleShowPatchListNumbers();
    void toggleShowPatchListNotes();

    // ========================================================================
    // Connections page (Ports and buses)
    // ========================================================================
private:
    // portsBussesTreeMenu: Context menu when item in the tree is right-clicked on
    //   Opened when user right-clicks on tree_portsBusses, see tree_portsBusses_Menu()
    //   When an item is clicked: slot corresponding to action is called.
    QMenu portsBussesTreeMenu;
    QTreeWidgetItem* portsBussesTreeMenuItem; // The item that was right-clicked on

    void setupConnectionsPage();
    void showConnectionsPage();
    void connectionsTreeSelectBus(int busId);
    void connectionsTreeSelectAudioInPort(int portId);
    void connectionsTreeSelectMidiInPort(int portId);
    void connectionsTreeSelectMidiOutPort(int portId);
    void gui_updatePortsBussesTree();
    void gui_updateConnectionsTree();
    void clearPortsBussesConnectionsData();

    QTreeWidgetItem* busParent = nullptr;
    QTreeWidgetItem* audioInParent = nullptr;
    QTreeWidgetItem* midiOutParent = nullptr;
    QTreeWidgetItem* midiInParent = nullptr;
    QMap<QTreeWidgetItem*, int> tree_busMap; // Maps tree item to bus id
    QMap<QTreeWidgetItem*, int> tree_midiOutMap;
    QMap<QTreeWidgetItem*, int> tree_audioInMap;
    QMap<QTreeWidgetItem*, int> tree_midiInMap;

    void clearConnectionsTree();
    void addClientPortToTree(QString jackport);

    QMap<QTreeWidgetItem*, QString> conPortsMap; // Mapping of tree items to Jack port strings
    QMap<QString, QTreeWidgetItem*> conClientsMap; // Mapping of Jack clients to tree items
    QMap<QCheckBox*, QTreeWidgetItem*> conChecksMap1; // Map column 1 checkboxes to tree items
    QMap<QCheckBox*, QTreeWidgetItem*> conChecksMap2; // Map column 2 checkboxes to tree items
    QTreeWidgetItem* notRunningParent; // Parent of ports which clients are not currently running
    QMap<QString, QTreeWidgetItem*> notRunningClientsMap;

private slots:
    void checkboxes_clicked_slot(QCheckBox* c);
    void tree_portsBusses_Menu(const QPoint &pos);
    void on_pushButton_connectionsPage_OK_clicked();
    void on_pushButton_ShowConnections_clicked();
    void on_tree_portsBusses_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_actionAdd_Bus_triggered();
    void on_actionAdd_Audio_In_Port_triggered();
    void on_actionAdd_MIDI_In_Port_triggered();
    void on_actionAdd_MIDI_Out_Port_triggered();
    void on_actionRemove_BusPort_triggered();
    void on_actionRename_BusPort_triggered();
    void on_tree_portsBusses_itemChanged(QTreeWidgetItem *item, int column);
    void on_pushButton_connectionsPage_MidiFilter_clicked();

    // ========================================================================
    // Settings
    // ========================================================================
private:
    bool mSettingsFirstRun = false;
    void setupSettings();
    void showSettingsDialog();
    void applySettings();
    void scanForDatabase();
    QString settingsDir;
    QString projectsDir;
    QString soundfontsDir;
    QString patchesDir;
    QString sfzDir;
    QString filemanager;
    void createSettingsDir();
    bool loadSettingsFile(QString dir);
    bool saveSettingsFile();

private slots:
    // Settings Dialog
    void on_toolButton_Settings_clicked();
    void on_pushButtonSettings_RescanLibrary_clicked();
    void on_pushButton_Settings_Cancel_clicked();
    void on_pushButton_Settings_Apply_clicked();
    void on_pushButton_settings_Projects_clicked();
    void on_pushButton_Settings_Soundfonts_clicked();
    void on_pushButton_Settings_Patches_clicked();
    void on_pushButton_Settings_Sfz_clicked();
    void on_pushButtonSettings_QuickRescanLibrary_clicked();

    // Thread for scanning folders
private:
    void showWaitingPage(QString title);

    /* Waiter
     * When a long operation is going to be performed which waits for a signal
     * to complete, call startWaiter() with an informative message.
     * The message appears in the status bar. All GUI input is disabled.
     * The waiterTimer is started which displays a waiting animation in the statusbar.
     * When the signal is received which completes the operation, call stopWaiter(),
     * which will reset everything back to normal. */
    void startWaiter(QString msg);
    void stopWaiter();
    int waiterState;
    QString waiterMessage;
    QBasicTimer waiterTimer;
    void timerEvent(QTimerEvent *ev);
private slots:
    void scanThreadFihishedSlot();

    // ========================================================================
    // MIDI filter editor
    // ========================================================================
private:
    int midiFilter_lastChan = 0;
    int midiFilter_lastData1 = 0;
    int midiFilter_lastData2 = 0;
    MidiFilterEditType midiFilterEditType;
    KonfytLayerWidget* midiFilterEditItem;
    int midiFilterEditPort;
    void showMidiFilterEditor();
    void updateMidiFilterEditorLastRx();
private slots:
    void on_pushButton_midiFilter_Cancel_clicked();
    void on_pushButton_midiFilter_Apply_clicked();
    void on_toolButton_MidiFilter_lowNote_clicked();
    void on_toolButton_MidiFilter_HighNote_clicked();
    void on_toolButton_MidiFilter_Add_clicked();
    void on_toolButton_MidiFilter_Add_Plus12_clicked();
    void on_toolButton_MidiFilter_Add_Minus12_clicked();
    void on_toolButton_MidiFilter_lowVel_clicked();
    void on_toolButton_MidiFilter_HighVel_clicked();
    void on_toolButton_MidiFilter_lastCC_clicked();
    void on_toolButton_MidiFilter_Add_CC_clicked();
    void on_toolButton_MidiFilter_removeCC_clicked();
    void on_toolButton_MidiFilter_inChan_last_clicked();
    void on_toolButton_MidiFilter_VelLimitMin_last_clicked();
    void on_toolButton_MidiFilter_VelLimitMax_last_clicked();

    // ========================================================================
    // MIDI send list
    // ========================================================================

    // MIDI send list editor
private:
    KonfytLayerWidget* midiSendListEditItem;
    QList<MidiSendItem> midiSendList;
    void showMidiSendListEditor();
    void midiEventToMidiSendEditor(MidiSendItem item);
    MidiSendItem midiEventFromMidiSendEditor();
    // Map combo box index to MIDI type
    QList<int> midiSendTypeComboItems{
                MIDI_EVENT_TYPE_CC,
                MIDI_EVENT_TYPE_PROGRAM,
                MIDI_EVENT_TYPE_NOTEON,
                MIDI_EVENT_TYPE_NOTEOFF,
                MIDI_EVENT_TYPE_PITCHBEND,
                MIDI_EVENT_TYPE_SYSTEM};
    QList<KonfytMidiEvent> midiSendEditorLastEvents;

private slots:
    void on_pushButton_midiSendList_apply_clicked();
    void on_pushButton_midiSendList_cancel_clicked();
    void on_pushButton_midiSendList_add_clicked();
    void on_comboBox_midiSendList_type_currentIndexChanged(int index);
    void on_checkBox_midiSendList_bank_stateChanged(int arg1);
    void on_listWidget_midiSendList_currentRowChanged(int currentRow);
    void on_listWidget_midiSendList_itemClicked(QListWidgetItem *item);
    void on_pushButton_midiSendList_pbmin_clicked();
    void on_pushButton_midiSendList_pbzero_clicked();
    void on_pushButton_midiSendList_pbmax_clicked();
    void on_actionEdit_MIDI_Send_List_triggered();
    void on_listWidget_midiSendList_lastReceived_itemClicked(QListWidgetItem *item);
    void on_pushButton_midiSendList_replace_clicked();
    void on_toolButton_midiSendList_down_clicked();
    void on_toolButton_midiSendList_up_clicked();
    void on_pushButton_midiSendList_remove_clicked();
    void on_pushButton_midiSendList_sendSelected_clicked();
    void on_pushButton_midiSendList_sendAll_clicked();

    // Saved MIDI send items
private:
    QString savedMidiListDir;
    QList<MidiSendItem> savedMidiSendItems;
    void setupSavedMidiSendItems();
    void addSavedMidiSendItem(MidiSendItem item);
    void loadSavedMidiSendItems(QString dirname);
    bool saveMidiSendItemToFile(QString filename, MidiSendItem item);
private slots:
    void on_pushButton_savedMidiMsgs_save_clicked();
    void on_pushButton_savedMidiMsgs_remove_clicked();
    void on_treeWidget_savedMidiMessages_itemClicked(QTreeWidgetItem *item, int column);

    // ========================================================================
    // JACK / MIDI
    // ========================================================================
private:
    KonfytJackEngine jack;
    int mJackXrunCount = 0;
    void setupJack();
    void addAudioBusToJack(int busNo, KfJackAudioPort** leftPort, KfJackAudioPort** rightPort);
    void addAudioInPortsToJack(int portNo, KfJackAudioPort** leftPort, KfJackAudioPort** rightPort);
    KfJackMidiPort* addMidiOutPortToJack(int numberLabel);
    KfJackMidiPort* addMidiInPortToJack(int numberLabel);
    bool jackPortBelongstoUs(QString jackPortName);
    void handleMidiEvent(KfJackMidiRxEvent rxEvent);
private slots:
    void onJackMidiEventsReceived();
    void onJackAudioEventsReceived();
    void onJackXrunOccurred();
    void onJackPortRegisteredOrConnected();

    // ========================================================================
    // Processes (External apps)
    // ========================================================================
private:
    QHash<QAction*, QString> extAppsMenuActions_Append;
    QHash<QAction*, QString> extAppsMenuActions_Set;
    QMenu extAppsMenu;
    void setupExternalAppsMenu();
    void addProcess(konfytProcess *process);
    void runProcess(int index);
    void stopProcess(int index);
    void removeProcess(int index);
private slots:
    // Processes (external apps)
    void processStartedSlot(int index, konfytProcess* process);
    void processFinishedSlot(int index, konfytProcess* process);

    // External apps menu
    void extAppsMenuTriggered(QAction* action);
    void on_toolButton_ExtAppsMenu_clicked();

    // External Apps widgets
    void on_listWidget_ExtApps_doubleClicked(const QModelIndex &index);
    void on_listWidget_ExtApps_clicked(const QModelIndex &index);
    void on_toolButton_layer_AddMidiPort_clicked();
    void on_pushButton_ExtApp_add_clicked();
    void on_lineEdit_ExtApp_returnPressed();
    void on_pushButton_ExtApp_RunSelected_clicked();
    void on_pushButton_ExtApp_Stop_clicked();
    void on_pushButton_ExtApp_RunAll_clicked();
    void on_pushButton_ExtApp_StopAll_clicked();
    void on_pushButton_ExtApp_remove_clicked();
    void on_pushButton_ExtApp_Replace_clicked();

    // ========================================================================
    // Buses, audio and MIDI ports
    // ========================================================================
private:
    int addBus();
    int addAudioInPort();
    int addMidiInPort();
    int addMidiOutPort();

    // ========================================================================
    // Triggers
    // ========================================================================
private:
    /* To add a new trigger, create an action with unique text in the mainwindow.ui editor.
     * Add the action to the list in initTriggers().
     * In midiEventSlot(), handle the action.
     * Saving and loading to the project is done automatically. */

    QHash<QTreeWidgetItem*, QAction*> triggersItemActionHash;
    QHash<int, QAction*> triggersMidiActionHash; // Map midi status and data1 bytes to action for fast midi event to action lookup
    void initTriggers();
    void setupTriggersPage();
    void showTriggersPage();
    KonfytMidiEvent triggersLastEvent;
    int lastBankSelectMSB = -1;
    int lastBankSelectLSB = -1;
    QList<KonfytMidiEvent> triggersLastEvents;

    QList<QAction*> channelGainActions;
    QList<QAction*> channelSoloActions;
    QList<QAction*> channelMuteActions;
    QList<QAction*> patchActions;
    void midi_setLayerGain(int layer, int midiValue);
    void midi_setLayerSolo(int layer, int midiValue);
    void midi_setLayerMute(int layer, int midiValue);

private slots:
    void on_pushButton_ShowTriggersPage_clicked();
    void on_pushButton_triggersPage_OK_clicked();
    void on_pushButton_triggersPage_assign_clicked();
    void on_pushButton_triggersPage_clear_clicked();
    void on_tree_Triggers_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_listWidget_triggers_eventList_itemDoubleClicked(QListWidgetItem *item);
    void on_checkBox_Triggers_ProgSwitchPatches_clicked();

    // ========================================================================
    // Other JACK connections
    // ========================================================================
private:
    bool jackPage_audio = true; // True to display audio ports, false for MIDI
    void showJackPage();
    void updateJackPage();
private slots:
    void on_pushButton_ShowJackPage_clicked();
    void on_pushButton_jackConAdd_clicked();
    void on_pushButton_jackConRemove_clicked();
    void on_pushButton_JackAudioPorts_clicked();
    void on_pushButton_JackMidiPorts_clicked();
    void on_pushButton_jackCon_OK_clicked();

    // ========================================================================
    // Other
    // ========================================================================

private:
    // Center view and sidebar (For changing to and from Saved MIDI Send List sidebar)
    QWidget* lastCenterWidget = nullptr;
    QWidget* lastSidebarWidget = nullptr;

    // Warnings
private:
    void updateGUIWarnings();
    void addWarning(QString warning);

    // Panic
private:
    bool panicState = false;
    void triggerPanic(bool panic);
private slots:
    void on_pushButton_Panic_clicked();
    void on_pushButton_Panic_customContextMenuRequested(const QPoint &pos);

    // About Dialog
private:
    AboutDialog aboutDialog;
    void initAboutDialog();
    void showAboutDialog();
    void resizeAboutDialog();
    void resizeEvent(QResizeEvent *ev);

    // Console
public:
    void setConsoleShowMidiMessages(bool show);
private:
    ConsoleDialog consoleDialog {this}; // Separate console window
    void setupConsoleDialog();
    bool console_showMidiMessages = false;
private slots:
    void on_pushButton_ClearConsole_clicked();
    void on_pushButton_ShowConsole_clicked();
    void on_checkBox_ConsoleShowMidiMessages_clicked();
signals:
    void printSignal(QString msg);

    // MIDI Indicators
private:
    PortIndicatorHandler portIndicatorHandler;
    LayerIndicatorHandler layerIndicatorHandler;

    QBasicTimer midiIndicatorTimer;
    void updateGlobalSustainIndicator();
    void updateGlobalPitchbendIndicator();
private slots:
    void on_MIDI_indicator_clicked();
    void on_MIDI_indicator_sustain_clicked();
    void on_MIDI_indicator_pitchbend_clicked();

    // Global transpose
private:
    void setMasterInTranspose(int transpose, bool relative);
private slots:
    void on_spinBox_MasterIn_Transpose_valueChanged(int arg1);
    void on_pushButton_MasterIn_TransposeSub12_clicked();
    void on_pushButton_MasterIn_TransposeAdd12_clicked();
    void on_pushButton_MasterIn_TransposeZero_clicked();
    
    // ========================================================================
    // Actions

private slots:
    void on_actionSave_Patch_As_Copy_triggered();

    void on_actionAdd_Patch_To_Library_triggered();

    void on_actionSave_Patch_To_File_triggered();

    void on_actionNew_Patch_triggered();

    void on_actionAdd_Patch_From_Library_triggered();

    void on_actionAdd_Patch_From_File_triggered();

    void on_actionMaster_Volume_Up_triggered();

    void on_actionMaster_Volume_Down_triggered();

    void on_actionPanic_triggered();

    void on_actionPanicToggle_triggered();

    void on_actionProject_save_triggered();

    void on_actionProject_New_triggered();

    void on_actionProject_Open_triggered();

    void on_actionProject_OpenDirectory_triggered();

    void on_actionProject_SaveAs_triggered();

    void on_actionAlways_Active_triggered();

    void on_actionEdit_MIDI_Filter_triggered();

    void on_actionReload_Layer_triggered();

    void on_actionOpen_In_File_Manager_layerwidget_triggered();

    void on_actionRemove_Layer_triggered();

    // ========================================================================

    void on_horizontalSlider_MasterGain_sliderMoved(int position);

    void on_pushButton_LiveMode_clicked();

    void on_pushButton_RestartApp_clicked();

    void on_toolButton_Project_clicked();

    void on_pushButton_LavaMonster_clicked();    

    void on_stackedWidget_currentChanged(int arg1);

};

#endif // MAINWINDOW_H
