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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
#include <QKeyEvent>
#include <QCheckBox>
#include <QSignalMapper>
#include <QDesktopServices>
#include <QCloseEvent>
#include <QAction>
#include <QShortcut>
#include <QScrollBar>
#include <QTreeWidgetItem>
#include <QFileDialog>
#include <QMessageBox>

#include <fluidsynth.h>
#include <carla/CarlaHost.h>

#include "konfytDatabase.h"
#include "konfytFluidsynthEngine.h"
#include "konfytPatchEngine.h"
#include "konfytProject.h"
#include "konfytJackEngine.h"
#include "konfytProcess.h"
#include "konfytPatchLayer.h"
#include "konfytMidiFilter.h"
#include "konfytLayerWidget.h"
#include "consoledialog.h"
#include "konfytDefines.h"
#include "konfytsfloader.h"
#include "aboutdialog.h"


CARLA_BACKEND_USE_NAMESPACE

#define APP_RESTART_CODE 1000

#define SETTINGS_DIR ".konfyt"
#define SETTINGS_FILE "konfyt.settings"
#define DATABASE_FILE "konfyt.database"

#define EVENT_FILTER_MODE_LIVE 0
#define EVENT_FILTER_MODE_WAITER 1

#define returnSfontRequester_on_treeWidget_filesystem_itemDoubleClicked 0

#define TREE_ITEM_SEARCH_RESULTS "Search Results:"
#define TREE_ITEM_SOUNDFONTS "Soundfonts"
#define TREE_ITEM_PATCHES "Patches"
#define TREE_ITEM_SFZ "SFZ"

#define STACKED_WIDGET_LEFT_LIBRARY 0
#define STACKED_WIDGET_LEFT_LIVE 1

#define STACKED_WIDGET_PATCHLAYERS_PATCH 0
#define STACKED_WIDGET_PATCHLAYERS_NOPATCH 1

#define LIBRARY_TAB_LIBRARY     0
#define LIBRARY_TAB_FILESYSTEM  1

#define TREECON_COL_PORT 0
#define TREECON_COL_L 1
#define TREECON_COL_R 2

#define XML_SETTINGS "settings"
#define XML_SETTINGS_PRJDIR "projectsDir"
#define XML_SETTINGS_SFDIR "soundfontsDir"
#define XML_SETTINGS_PATCHESDIR "patchesDir"
#define XML_SETTINGS_SFZDIR "sfzDir"
#define XML_SETTINGS_FILEMAN "filemanager"


namespace Ui {
class MainWindow;
}



enum libraryTreeItemType { libTreeInvalid,
                           libTreePatchesRoot,
                           libTreePatch,
                           libTreeSFZRoot,
                           libTreeSFZFolder,
                           libTreeSFZ,
                           libTreeSoundfontRoot,
                           libTreeSoundfontFolder,
                           libTreeSoundfont };

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:

    explicit MainWindow(QWidget *parent, QApplication* application, QStringList filesToLoad, QString jackClientName);
    ~MainWindow();

    QApplication* app;

    bool eventFilter(QObject *object, QEvent *event);
    int eventFilterMode;

    // ========================================================================
    // Project related
    // ========================================================================
    QList<QFileInfo> projectDirList;
    QList<konfytProject*> projectList;
    int currentProject;
    bool startupProject;
    konfytProject* getCurrentProject();
    bool scanDirForProjects(QString dirname);
    void newProject();
    bool openProject(QString filename);
    void addProject(konfytProject *prj);
    void removeProject(int i);
    void setCurrentProject(int i);
    bool saveCurrentProject();
    bool saveProject(konfytProject *p);

    void newPatchToProject();
    void removePatchFromProject(int i);
    void addPatchToProject(konfytPatch *newPatch);
    bool savePatchToLibrary(konfytPatch* patch);

    QMenu projectsMenu;
    QMap<QAction*, QFileInfo> projectsMenuMap;
    void updateProjectsMenu();

    // Misc helper functions
    QString getUniqueFilename(QString dirname, QString name, QString extension);

    void setPatchModified(bool modified);
    void setProjectModified();

    // ========================================================================
    // GUI Representation of library
    // ========================================================================
    bool library_isProgramSelected();
    konfytSoundfontProgram library_getSelectedProgram();

    libraryTreeItemType library_getTreeItemType(QTreeWidgetItem* item);
    libraryTreeItemType library_getSelectedTreeItemType();

    konfytPatch library_getSelectedPatch();
    konfytSoundfont* library_getSelectedSfont();
    QString library_selectedSfz;

    // Library tree items
    QTreeWidgetItem*                         library_sfRoot;
    QMap<QTreeWidgetItem*, QString>          library_sfFolders; // All intermediate (non-root, non-bottom) items in soundfont tree and item path
    QMap<QTreeWidgetItem*, konfytSoundfont*> library_sfMap;     // Bottom-most items in soundfont tree
    QTreeWidgetItem*                    library_patchRoot;
    QMap<QTreeWidgetItem*, konfytPatch> library_patchMap;
    QTreeWidgetItem*                 library_sfzRoot;
    QMap<QTreeWidgetItem*, QString>  library_sfzFolders; // All the non-root and non-bottom items in the sfz tree and item path
    QMap<QTreeWidgetItem*, QString>  library_sfzMap;     // Bottom-most items in the sfz tree with corresponding path
    void buildSfzTree(QTreeWidgetItem* twi, konfytDbTreeItem* item);
    void buildSfTree(QTreeWidgetItem* twi, konfytDbTreeItem* item);

    konfytDatabase db;
    bool saveDatabase();
    int returnSfontRequester;
    QList<konfytSoundfontProgram> programList; // List of programs currently displayed in program list view in library.
    void library_refreshGUIProgramList();      // Refresh the GUI program list to match programList

    bool searchMode;
    void fillTreeWithAll();
    void fillTreeWithSearch(QString search);
    void gui_updateTree();

    QMenu libraryMenu;
    QTreeWidgetItem* libraryMenuItem;

    // Filesystem view in library
    QString fsview_currentPath;
    QStringList fsview_back;
    void refreshFilesystemView();
    void cdFilesystemView(QString newpath);
    void selectItemInFilesystemView(QString path);
    QMap<QTreeWidgetItem*, QFileInfo> fsMap;
    QMenu fsViewMenu;
    QTreeWidgetItem* fsViewMenuItem;
    QSignalMapper filemanProcessSignalMapper;

    void openFileManager(QString path);

    void showSfzContentsBelowLibrary(QString filename);
    QString loadSfzFileText(QString filename);

    // ========================================================================
    // Patches
    // ========================================================================
    konfytPatchEngine* pengine;
    konfytPatch* masterPatch;   // Current patch being played
    float masterGain;           // Master gain when not in preview mode
    konfytPatch* previewPatch;  // Patch played when in preview mode
    float previewGain;          // Gain when in preview mode

    bool fileSuffixIs(QString file, QString suffix);
    bool fileIsPatch(QString file);
    bool fileIsSoundfont(QString file);
    bool fileIsSfzOrGig(QString file);

    void setMasterGain(float gain);

    // Current patch functions
    int currentPatchIndex;
    void setCurrentPatch(int index);
    void setCurrentPatch(konfytPatch *newPatch);
    void setCurrentPatchIcon();
    void unsetCurrentPatchIcon();

    void newPatchIfMasterNull();
    void addSfzToCurrentPatch(QString sfzPath);
    void addProgramToCurrentPatch(konfytSoundfontProgram p);
    void addMidiPortToCurrentPatch(int port);
    void addAudioInPortToCurrentPatch(int port);
    void addLV2ToCurrentPatch(QString lv2Path);
    void addCarlaInternalToCurrentPatch(QString URI);

    void loadPatchForModeAndUpdateGUI();

    // GUI patch functions
    bool previewMode;                   // Use setPreviewMode() to change.
    void setPreviewMode(bool choice);   // Set preview mode on or off and updates GUI.
    void gui_updatePatchView();
    void gui_updateWindowTitle();
    void gui_updatePatchList();
    bool patchNote_ignoreChange;

    // Layers
    QList<konfytLayerWidget*> guiLayerItemList;
    void addLayerItemToGUI(konfytPatchLayer layerItem);
    void removeLayerItem(konfytLayerWidget *layerItem);
    void removeLayerItem_GUIonly(konfytLayerWidget *layerItem);
    void clearLayerItems_GUIonly();

    // patchMidiOutPortsMenu: Context menu when user clicks button to add a new MIDI output layer.
    //   When an item is clicked: on_patchMidiOutPortsMenu_ActionTrigger()
    QMenu patchMidiOutPortsMenu;
    QAction* patchMidiOutPortsMenu_NewPortAction;
    QMap<QAction*, int> patchMidiOutPortsMenu_map; // Map menu actions to project port ids
    void gui_updatePatchMidiOutPortsMenu();

    // patchAudioInPortsMenu: Context menu when user clicks button to add new audio input port layer.
    //    When item is clicked: on_patchAudioInPortsMenu_ActionTrigger()
    QMenu patchAudioInPortsMenu;
    QAction* patchAudioInPortsMenu_NewPortAction;
    QMap<QAction*, int> patchAudioInPortsMenu_map; // Map menu actions to project port ids
    void gui_updatePatchAudioInPortsMenu();

    // layerBusMenu: Bus menu in the layers in patch view.
    //   Opened when user clicks on layer item bus button, see onlayer_bus_clicked()
    //   When a menu item is clicked: onlayerBusMenu_ActionTrigger
    void gui_updateLayerBusMenu();
    void gui_updateLayerMidiOutChannelMenu();
    QMenu layerBusMenu;
    QMenu layerMidiOutChannelMenu;
    QAction* layerBusMenu_NewBusAction;
    konfytLayerWidget* layerBusMenu_sourceItem;
    QMap<QAction*, int> layerBusMenu_actionsBusIdsMap; // Map menu actions to bus ids
    QMap<QAction*, int> layerMidiOutChannelMenu_map;

    // ========================================================================
    // Connections page (Ports and busses)
    // ========================================================================

    // portsBussesTreeMenu: Context menu when item in the tree is right-clicked on
    //   Opened when user right-clicks on tree_portsBusses, see tree_portsBusses_Menu()
    //   When an item is clicked: slot corresponding to action is called.
    QMenu portsBussesTreeMenu;
    QTreeWidgetItem* portsBussesTreeMenuItem; // The item that was right-clicked on

    void showConnectionsPage();
    void gui_updatePortsBussesTree();
    void gui_updateConnectionsTree();

    QTreeWidgetItem* busParent;
    QTreeWidgetItem* audioInParent;
    QTreeWidgetItem* midiOutParent;
    QTreeWidgetItem* midiInParent;
    QMap<QTreeWidgetItem*, int> tree_busMap; // Maps tree item to bus id
    QMap<QTreeWidgetItem*, int> tree_midiOutMap;
    QMap<QTreeWidgetItem*, int> tree_audioInMap;
    QMap<QTreeWidgetItem*, int> tree_midiInMap;

    void addClientPortToTree(QString jackport, bool active);

    QSignalMapper conSigMap;
    QMap<QTreeWidgetItem*, QString> conPortsMap; // Mapping of tree items to Jack port strings
    QMap<QString, QTreeWidgetItem*> conClientsMap; // Mapping of Jack clients to tree items
    QMap<QCheckBox*, QTreeWidgetItem*> conChecksMap1; // Map column 1 checkboxes to tree items
    QMap<QCheckBox*, QTreeWidgetItem*> conChecksMap2; // Map column 2 checkboxes to tree items
    QTreeWidgetItem* notRunningParent; // Parent of ports which clients are not currently running
    QMap<QString, QTreeWidgetItem*> notRunningClientsMap;

    // ========================================================================
    // Settings
    // ========================================================================
    void showSettingsDialog();
    void applySettings();
    void scanForDatabase();
    QString settingsDir;
    QString projectsDir;
    QString soundfontsDir;
    QString patchesDir;
    QString sfzDir;
    QString filemanager;
    bool loadSettingsFile();
    bool saveSettingsFile();

    // ========================================================================
    // MIDI filter editor
    // ========================================================================
    int midiFilter_lastChan;
    int midiFilter_lastData1;
    int midiFilter_lastData2;
    konfytLayerWidget* midiFilterEditItem;
    void showMidiFilterEditor();
    void updateMidiFilterEditorLastRx();

    // ========================================================================
    // Jack
    // ========================================================================
    konfytJackEngine* jack;
    void addAudioBusToJack(int busNo, konfytJackPort **leftPort, konfytJackPort **rightPort);
    void addAudioInPortsToJack(int portNo, konfytJackPort **leftPort, konfytJackPort **rightPort);
    void addMidiOutPortToJack(int portId, konfytJackPort **jackPort);

    // ========================================================================
    // Processes (External apps)
    // ========================================================================
    QHash<QAction*, QString> extAppsMenuActions;
    QMenu extAppsMenu;
    void setupExtAppMenu();
    void addProcess(konfytProcess *process);
    void runProcess(int index);
    void stopProcess(int index);
    void removeProcess(int index);

    // ========================================================================
    // Busses, audio and midi ports
    // ========================================================================
    int addBus();
    int addAudioInPort();
    int addMidiOutPort();

    // ========================================================================
    // Triggers
    // ========================================================================

    /* To add a new trigger, create an action with unique text in the mainwindow.ui editor.
     * Add the action to the list in initTriggers().
     * In midiEventSlot(), handle the action.
     * Saving and loading to the project is done automatically. */

    QHash<QTreeWidgetItem*, QAction*> triggersItemActionHash;
    QHash<int, QAction*> triggersMidiActionHash; // Map midi status and data1 bytes to action for fast midi event to action lookup
    void initTriggers();
    void showTriggersPage();
    konfytMidiEvent triggersLastEvent;
    int lastBankSelectMSB;
    int lastBankSelectLSB;
    QList<konfytMidiEvent> triggersLastEvents;

    QList<QAction*> channelGainActions;
    QList<QAction*> channelSoloActions;
    QList<QAction*> channelMuteActions;
    QList<QAction*> patchActions;
    void midi_setLayerGain(int layer, int midiValue);
    void midi_setLayerSolo(int layer, int midiValue);
    void midi_setLayerMute(int layer, int midiValue);

    // ========================================================================
    // Other JACK connections
    // ========================================================================
    void showJackPage();

    // ========================================================================
    // Warnings
    // ========================================================================

    void updateGUIWarnings();
    void addWarning(QString warning);

    // ========================================================================
    // Other
    // ========================================================================

    bool panicState;

    // Thread for scanning folders
    konfytSfLoader sfLoader;
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

    AboutDialog aboutDialog;
    void initAboutDialog();
    void showAboutDialog();
    void resizeAboutDialog();
    void resizeEvent(QResizeEvent *ev);

    // Console
    ConsoleDialog* consoleDiag; // Console dialog (seperate console window)
    void setConsoleShowMidiMessages(bool show);
    bool console_showMidiMessages;

    // MIDI Indicator
    QBasicTimer midiIndicatorTimer;

    // Keyboard shortcuts
    QShortcut* shortcut_save;
    QShortcut* shortcut_panic;

    void setMasterInTranspose(int transpose, bool relative);

    // General utilities
    QString getBaseNameWithoutExtension(QString filepath);


    void error_abort(QString msg);
    void messageBox(QString msg);
    
private slots:

    // Keyboard Shortcuts
    void shortcut_save_activated();
    void shortcut_panic_activated();

    // Database
    void database_scanDirsFinished();
    void database_scanDirsStatus(QString msg);
    void database_returnSfont(konfytSoundfont* sf);

    // Display info to user
    void userMessage(QString message);

    // Midi / Jack
    void midiEventSlot(konfytMidiEvent ev);
    void jackXrun();
    void jackPortRegisterOrConnectCallback();

    // Project modified
    void projectModifiedStateChanged(bool modified);

    // Projects Menu
    void onprojectMenu_ActionTrigger(QAction* action);

    // ========================================================================
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

    // Thread for scanning folders
    void scanThreadFihishedSlot();

    // Filemanager process
    void filemanProcessSignalMapperSlot(QObject *object);

    // Handle application closing
    void closeEvent(QCloseEvent *);

    // ========================================================================
    // Library and filesystem view

    void on_treeWidget_Library_itemClicked(QTreeWidgetItem *item, int column);
    void on_treeWidget_Library_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_treeWidget_Library_itemDoubleClicked(QTreeWidgetItem *item, int column);
    /* When the user right-clicks on the library tree. */
    void on_treeWidget_Library_customContextMenuRequested(const QPoint &pos);

    void on_listWidget_LibraryBottom_currentRowChanged(int currentRow);
    void on_listWidget_LibraryBottom_itemDoubleClicked(QListWidgetItem *item);

    void on_lineEdit_Search_returnPressed();
    void on_pushButton_ClearSearch_clicked();
    void on_pushButton_LibraryPreview_clicked();

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

    void on_actionOpen_In_File_Manager_library_triggered();
    void on_actionAdd_Path_To_External_App_Box_triggered();
    void on_actionOpen_In_File_Manager_fsview_triggered();

    // ========================================================================
    // Patch view area

    // Patch related
    void on_lineEdit_PatchName_returnPressed();
    void on_lineEdit_PatchName_editingFinished();
    void onPatchMidiOutPortsMenu_ActionTrigger(QAction* action);
    void onPatchAudioInPortsMenu_ActionTrigger(QAction* action);
    void on_lineEdit_ProjectName_editingFinished();
    void on_toolButton_SavePatch_clicked();
    void on_textBrowser_patchNote_textChanged();

    // Layers
    void onLayer_remove_clicked(konfytLayerWidget* layerItem);
    void onLayer_filter_clicked(konfytLayerWidget* layerItem);
    void onLayer_slider_moved(konfytLayerWidget* layerItem, float gain);
    void onLayer_solo_clicked(konfytLayerWidget* layerItem, bool solo);
    void onLayer_mute_clicked(konfytLayerWidget* layerItem, bool mute);
    void onLayer_bus_clicked(konfytLayerWidget* layerItem);
    void onLayer_reload_clicked(konfytLayerWidget* layerItem);
    void onLayer_openInFileManager_clicked(konfytLayerWidget* layerItem, QString filepath);
    void onLayerBusMenu_ActionTrigger(QAction* action);
    void onLayerMidiOutChannelMenu_ActionTrigger(QAction* action);

    // Patch List
    void on_pushButton_RemovePatch_clicked();
    void on_pushButton_PatchUp_clicked();
    void on_pushButton_PatchDown_clicked();
    void on_toolButton_AddPatch_clicked();
    void on_listWidget_Patches_itemClicked(QListWidgetItem *item);
    void on_pushButton_LoadAll_clicked();

    void on_listWidget_Patches_indexesMoved(const QModelIndexList &indexes);

    // ========================================================================
    // Midi Filter Dialog

    void on_pushButton_midiFilter_Cancel_clicked();

    void on_pushButton_midiFilter_Apply_clicked();

    void on_toolButton_MidiFilter_lowNote_clicked();

    void on_toolButton_MidiFilter_HighNote_clicked();

    void on_toolButton_MidiFilter_Multiply_clicked();

    void on_toolButton_MidiFilter_Add_clicked();

    void on_toolButton_MidiFilter_Add_Plus12_clicked();

    void on_toolButton_MidiFilter_Add_Minus12_clicked();

    void on_toolButton_MidiFilter_lowVel_clicked();

    void on_toolButton_MidiFilter_HighVel_clicked();

    void on_toolButton_MidiFilter_lastCC_clicked();

    void on_toolButton_MidiFilter_Add_CC_clicked();

    void on_toolButton_MidiFilter_removeCC_clicked();

    void on_toolButton_MidiFilter_inChan_last_clicked();

    // ========================================================================
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

    // ========================================================================
    // Actions

    void on_actionSave_Patch_triggered();

    void on_actionSave_Patch_As_Copy_triggered();

    void on_actionAdd_Patch_To_Library_triggered();

    void on_actionSave_Patch_To_File_triggered();

    void on_actionNew_Patch_triggered();

    void on_actionAdd_Patch_From_Library_triggered();

    void on_actionAdd_Patch_From_File_triggered();

    void on_actionMaster_Volume_Up_triggered();

    void on_actionMaster_Volume_Down_triggered();

    void on_actionPanic_triggered();

    void on_actionProject_save_triggered();

    void on_actionProject_New_triggered();

    void on_actionProject_Open_triggered();

    void on_actionProject_OpenDirectory_triggered();

    void on_actionProject_SaveAs_triggered();

    // ========================================================================
    // Connections (ports and busses) Page

    void checkboxes_signalmap_slot(QWidget* widget);

    void tree_portsBusses_Menu(const QPoint &pos);

    void on_pushButton_connectionsPage_OK_clicked();

    void on_pushButton_ShowConnections_clicked();

    void on_tree_portsBusses_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    void on_actionAdd_Bus_triggered();

    void on_actionAdd_Audio_In_Port_triggered();

    void on_actionAdd_MIDI_Out_Port_triggered();

    void on_actionRemove_BusPort_triggered();

    void on_actionRename_BusPort_triggered();

    void on_tree_portsBusses_itemChanged(QTreeWidgetItem *item, int column);

    // ========================================================================
    // Triggers page

    void on_pushButton_ShowTriggersPage_clicked();

    void on_pushButton_triggersPage_OK_clicked();

    void on_pushButton_triggersPage_assign_clicked();

    void on_pushButton_triggersPage_clear_clicked();

    void on_tree_Triggers_itemDoubleClicked(QTreeWidgetItem *item, int column);

    // ========================================================================
    // Console

    void on_pushButton_ClearConsole_clicked();

    void on_pushButton_ShowConsole_clicked();

    void on_checkBox_ConsoleShowMidiMessages_clicked();

    // ========================================================================
    // Global transpose

    void on_spinBox_MasterIn_Transpose_valueChanged(int arg1);

    void on_pushButton_MasterIn_TransposeSub12_clicked();

    void on_pushButton_MasterIn_TransposeAdd12_clicked();

    void on_pushButton_MasterIn_TransposeZero_clicked();

    // ========================================================================

    void on_horizontalSlider_MasterGain_sliderMoved(int position);

    void on_pushButton_LiveMode_clicked();

    void on_tabWidget_Projects_currentChanged(int index);

    void on_tabWidget_library_currentChanged(int index);

    void on_pushButton_RestartApp_clicked();

    void on_toolButton_Project_clicked();

    void on_pushButton_Panic_clicked();

    void on_MIDI_indicator_clicked();

    void on_pushButton_ShowJackPage_clicked();

    void on_pushButton_jackConRefresh_clicked();

    void on_pushButton_jackConAdd_clicked();

    void on_pushButton_jackConRemove_clicked();

    void on_checkBox_filesystem_ShowOnlySounds_toggled(bool checked);

    void on_pushButton_LavaMonster_clicked();




private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
