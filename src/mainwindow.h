/******************************************************************************
 *
 * Copyright 2024 Gideon van der Kolf
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

#include "biqmap.h"
#include "consolewindow.h"
#include "indicatorHandlers.h"
#include "konfytAudio.h"
#include "konfytDatabase.h"
#include "konfytUtils.h"
#include "konfytFluidsynthEngine.h"
#include "konfytJackEngine.h"
#include "konfytJs.h"
#include "konfytLayerWidget.h"
#include "konfytMidiFilter.h"
#include "konfytPatchEngine.h"
#include "konfytPatchLayer.h"
#include "konfytProcess.h"
#include "konfytProject.h"
#include "menuEntryWidget.h"
#include "midiEventListWidgetAdapter.h"
#include "patchListWidgetAdapter.h"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QElapsedTimer>
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
#include <QWidgetAction>

#include <iostream>


#define APP_RESTART_CODE 1000

#define SETTINGS_FILE "konfyt.settings"
#define DATABASE_FILE "konfyt.database"
#define MIDI_MAP_PRESETS_FILE "konfytMidiMapPresets"

#define SAVED_MIDI_SEND_ITEMS_DIR "savedMidiSendItems"

#define EVENT_FILTER_MODE_LIVE 0
#define EVENT_FILTER_MODE_WAITER 1

#define TREE_ITEM_SEARCH_RESULTS "Search Results:"
#define TREE_ITEM_SOUNDFONTS "Soundfonts"
#define TREE_ITEM_PATCHES "Patches"
#define TREE_ITEM_SFZ "SFZ"

#define TREECON_COL_PORT 0
#define TREECON_COL_L 1
#define TREECON_COL_R 2
#define TREECON_COL_MIDI 3

#define XML_SETTINGS "settings"
#define XML_SETTINGS_PRJDIR "projectsDir"
#define XML_SETTINGS_SFDIR "soundfontsDir"
#define XML_SETTINGS_PATCHESDIR "patchesDir"
#define XML_SETTINGS_SFZDIR "sfzDir"
#define XML_SETTINGS_FILEMAN "filemanager"
#define XML_SETTINGS_PROMPT_ON_QUIT "promptOnQuit"
#define XML_SETTINGS_START_MAXIMIZED "startMaximized"
#define XML_SETTINGS_OPEN_LAST_PROJECT "openLastProject"
#define XML_SETTINGS_LAST_PROJECT_FILEPATH "lastProjectFilePath"
#define XML_SETTINGS_DEFAULT_RESET_OPTION "defaultResetOption"

#define XML_MIDI_MAP_PRESETS "midiMapPresets"
#define XML_MIDI_MAP_PRESET "midiMapPreset"
#define XML_MIDI_MAP_PRESET_NAME "name"
#define XML_MIDI_MAP_PRESET_DATA "data"

#define PTY_MIDI_CHANNEL "midiChannel"
#define PTY_MIDI_IN_PORT "midiInPort"
#define PTY_AUDIO_OUT_BUS "audioOutBus"
#define PTY_AUDIO_IN_PORT "audioInPort"
#define PTY_MIDI_OUT_PORT "midiOutPort"

#define REPLACE_TXT_APP_VERSION "%APP_VERSION%"
#define REPLACE_TXT_APP_YEAR "%APP_YEAR%"
#define REPLACE_TXT_MORE_VERSION "%MORE_VERSION_TEXT%"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent, KonfytAppInfo appInfoArg);
    ~MainWindow();

    KonfytAppInfo appInfo;

private:
    Ui::MainWindow *ui;
    int eventFilterMode;
    bool mPrintStart = true;
    void setupGuiStyle();
    void setupGuiMenuButtons();
    void setupGuiDefaults();
    void printArgumentsInfo();
    bool mBlockPrint = false;

private slots:
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *object, QEvent *event);
    void keyPressEvent(QKeyEvent *event) override;

    void print(QString message);

    // Keyboard shortcuts
private:
    void setupKeyboardShortcuts();
private slots:
    void shortcut_save_activated();
    void shortcut_panic_activated();

    // General helper functions
private:
    QString baseFilenameWithoutExtension(QString filepath);
    bool fileExtensionIs(QString filepath, QString extension);
    bool fileExtensionIsPatch(QString filepath);
    bool fileExtensionIsSoundfont(QString filepath);
    bool fileExtensionIsSfzOrGig(QString filepath);

    void msgBox(QString msg, QString infoText = "");
    int msgBoxYesNo(QString text, QString infoText = "");
    int msgBoxYesNoCancel(QString text, QString infoText = "");
    bool dirExists(QString dirname);
    QStringList scanDirForFilesSkipBackupSubdirs(QString dirname,
                                                 QString filenameExtension = "");
    void openFileManager(QString path);

    struct ResetOptionComboBox
    {
        void initComboBox(QComboBox* comboBox);
        KonfytReset selectedValue();
        void updateSelectedOption(KonfytReset option);
    private:
        QComboBox* mComboBox = nullptr;
        QList<KonfytReset> mValues;
        void addItem(QString text, KonfytReset value);
    };

    struct ResetOptionMenu
    {
        ResetOptionMenu();
        QMenu* menu();
        void setInheritActionText(QString text);
        void updateMenu(KonfytReset selected, KonfytReset inherited);
        KonfytReset actionValue(QAction* action);
    private:
        QMenu mMenu;
        QString mInheritActionText {"Inherit"};
        QList<KonfytReset> mValues;
        void addAction(QString text, KonfytReset value);
        QAction* actionWithValue(KonfytReset value);
    };

    // Widget helper functions
private:
    void highlightButton(QAbstractButton* button, bool highlight);

    // -----------------------------------------------------------------------
    // Project related
private:
    ProjectPtr mCurrentProject;
    void setupInitialProjectFromCmdLineArgs();
    ProjectPtr newProjectPtr();
    void loadNewProject();
    bool loadProjectFromFile(QString filename);
    void loadProject(ProjectPtr prj);
    void unloadCurrentProject();
    bool saveCurrentProject();
    bool saveProject(ProjectPtr prj);
    bool saveProjectInNewDir(ProjectPtr prj);
    bool informedUserAboutProjectsDir = false;
    bool requestCurrentProjectClose();

    void updateProjectNameInGui();

    PatchPtr addNewPatch();
    void removePatch(int index);
    void addPatch(PatchPtr patch);
    PatchPtr addPatchFromFile(QString filename);
    bool savePatchToLibrary(PatchPtr patch);

    // Projects menu
private:
    QMenu projectsMenu;
    QMap<QAction*, QFileInfo> projectsMenuMap;
    void updateProjectsMenu();
private slots:
    void onProjectMenu_ActionTrigger(QAction* action);

    void on_actionProject_save_triggered();
    void on_actionProject_New_triggered();
    void on_actionProject_Open_triggered();
    void on_actionProject_OpenDirectory_triggered();
    void on_actionProject_SaveAs_triggered();
    void on_toolButton_Project_clicked();

    // Project modified
private:
    void patchModified();
    void setProjectModified();
private slots:
    void onProjectModifiedStateChanged(bool modified);
    void onProjectNameChanged();
    void onProjectMidiPickupRangeChanged(int range);

    // -----------------------------------------------------------------------
    // Library and filesystem view

    // Library/filesystem common
private:
    KfSoundPtr selectedSfont;
    KfSoundPtr selectedSoundInLibOrFs();
    bool isSoundfontProgramSelectedInLibOrFs();
    KonfytSoundPreset selectedSoundfontProgramInLibOrFs();
    void clearLibFsInfoArea();
    void showPatchInLibFsInfoArea();
    void showSfontInfoInLibFsInfoArea(QString filename);
    void showSelectedSfontProgramList();
    void showSfzContentInLibFsInfoArea(QString filename);
    QString loadSfzFileText(QString filename);

    QMenu libraryBottomContextMenu;

private slots:
    void on_tabWidget_library_currentChanged(int index);
    void on_listWidget_LibraryBottom_currentRowChanged(int currentRow);
    void on_listWidget_LibraryBottom_itemDoubleClicked(QListWidgetItem *item);
    void on_listWidget_LibraryBottom_customContextMenuRequested(const QPoint &pos);

    // Preview button & menu
private:
    QMenu previewButtonMenu;
private slots:
    void preparePreviewMenu();
    void previewButtonMidiInPortMenuTrigger(QAction* action);
    void previewButtonMidiInChannelMenuTrigger(QAction* action);
    void previewButtonBusMenuTrigger(QAction* action);
    void on_toolButton_LibraryPreview_clicked();

    // Library tree
private:
    struct LibraryTree {
        QTreeWidgetItem* rootTreeItem = nullptr;
        QMap<QTreeWidgetItem*, QString> foldersMap; // Intermediate (non-root, non-bottom) items
        QMap<QTreeWidgetItem*, KfSoundPtr> soundsMap; // Bottom-most items in tree
    };
    LibraryTree librarySfontTree;
    LibraryTree librarySfzTree;
    LibraryTree libraryPatchTree;
    void resetLibraryTree(LibraryTree &libTree, QString name);

    QIcon mFolderIcon {":/icons/folder.png"};
    QIcon mFileIcon {":/icons/picture.png"};
    void buildLibraryTree(QTreeWidgetItem* twi, KfDbTreeItemPtr dbTreeItem, LibraryTree* libTree);
    void buildSfzTree(KfDbTreeItemPtr dbTreeItem);
    void buildSfTree(KfDbTreeItemPtr dbTreeItem);
    void buildPatchTree(KfDbTreeItemPtr dbTreeItem);

    enum LibraryTreeItemType {
        libTreeInvalid,
        libTreePatchesRoot,
        libTreePatch,
        libTreeSFZRoot,
        libTreeSFZFolder,
        libTreeSFZ,
        libTreeSoundfontRoot,
        libTreeSoundfontFolder,
        libTreeSoundfont
    };
    LibraryTreeItemType libraryTreeItemType(QTreeWidgetItem* item);
    LibraryTreeItemType librarySelectedTreeItemType();
    KfSoundPtr librarySelectedPatch();
    KfSoundPtr librarySelectedSfont();
    KfSoundPtr librarySelectedSfz();

    bool mLibrarySearchModeActive;
    void fillLibraryTreeWithAll();
    void refreshLibraryPatchTree();
    void fillLibraryTreeWithSearch(QString search);

    QMenu libraryContextMenu;
    QTreeWidgetItem* libraryMenuItem = nullptr;
    QAction* actionRemoveLibraryPatch = nullptr;
    void setupLibraryContextMenu();

private slots:
    void onActionRemoveLibraryPatchTriggered();
    void on_treeWidget_Library_itemClicked(QTreeWidgetItem *item, int column);
    void on_treeWidget_Library_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_treeWidget_Library_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_treeWidget_Library_customContextMenuRequested(const QPoint &pos);
    void on_lineEdit_Search_returnPressed();
    void on_toolButton_ClearSearch_clicked();
    void on_actionOpen_In_File_Manager_library_triggered();

    // Database
private:
    KonfytDatabase db;
    void setupDatabase();
    void loadDatabase();
    void saveDatabase();
private slots:
    void onDatabaseScanFinished();
    void onDatabaseScanStatus(QString msg);
    void onDatabaseSfontInfoLoaded(KfSoundPtr sf);

    // Filesystem view
private:
    QString fsview_currentPath;
    QStringList fsview_back;
    void setupFilesystemView();
    void refreshFilesystemView();
    void cdFilesystemView(QString newpath);
    void selectItemInFilesystemView(QString path);
    bool isSfzSelectedInFilesystem();
    bool isPatchSelectedInFilesystem();
    bool isSoundfontSelectedInFilesystem();
    QMap<QTreeWidgetItem*, QFileInfo> fsMap;

    QMenu fsViewContextMenu;
    QTreeWidgetItem* fsViewMenuItem;
    QMenu fsPathMenu;

private slots:
    void on_treeWidget_filesystem_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_treeWidget_filesystem_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_treeWidget_filesystem_customContextMenuRequested(const QPoint &pos);

    void on_toolButton_filesystem_up_clicked();
    void on_toolButton_filesystem_refresh_clicked();
    void on_toolButton_filesystem_back_clicked();
    void on_lineEdit_filesystem_path_returnPressed();
    void on_toolButton_filesystemPathDropdown_clicked();
    void on_checkBox_filesystem_ShowOnlySounds_toggled(bool checked);
    void on_checkBox_filesystem_hiddenFiles_toggled(bool checked);

    void on_actionOpen_In_File_Manager_fsview_triggered();
    void on_actionAdd_Path_To_External_App_Box_triggered();
    void on_actionAdd_Path_to_External_App_Box_Relative_to_Project_triggered();
    void on_action_CopyFilesystemPath_triggered();

    // -----------------------------------------------------------------------
    // Patches
private:
    KonfytPatchEngine pengine;
    void setupPatchEngine();
    PatchPtr mCurrentPatch;

    // Patch preview
    PatchPtr mPreviewPatch {new Patch()};  // Patch played when in preview mode
    float previewGain = 1.0;    // Gain when in preview mode
    int previewPatchMidiInPort = 0;
    int previewPatchMidiInChannel = -1;
    int previewPatchBus = 0;
    void updatePreviewPatchLayer();

    // Current patch functions
    int currentPatchIndex();
    void setCurrentPatch(PatchPtr patch);
    void setCurrentPatchByIndex(int index);

    void newPatchIfCurrentNull();
    void addSfzToCurrentPatch(QString sfzPath);
    void addSoundfontProgramToCurrentPatch(QString soundfontPath, KonfytSoundPreset p);
    void addMidiPortToCurrentPatch(int port);
    void addAudioInPortToCurrentPatch(int port);

    void loadCurrentPatchAndUpdateGui();
    void loadPreviewPatchAndUpdateGui();
    void loadAllPatches();

    // GUI patch functions
    bool mPreviewMode = false;               // Set by setPreviewMode()
    void setPreviewMode(bool previewModeOn); // Set preview mode on or off and updates GUI.
    void updatePatchView();
    void updateWindowTitle();
    PatchListWidgetAdapter patchListAdapter;
    void setupPatchListAdapter();
    bool patchNote_ignoreChange = false;

private slots:
    void onPatchSelected(PatchPtr patch);
    void onPatchLayerLoaded(PatchLayerPtr patchLayer);

    // Patch menu
private:
    void setupPatchMenu();

    // Patch menu reset option submenu
private:
    ResetOptionMenu patchResetOptionMenu;
    void setupPatchResetOptionMenu();
private slots:
    void onPatchResetOptionMenuAboutToShow();
    void onPatchResetOptionMenuTriggered(QAction* action);

    // Layers
private:
    QList<PatchLayerWidget*> layerWidgetList;
    void addPatchLayerToGUI(PatchLayerPtr patchLayer, int index = -1);
    void addPatchLayerToIndicatorHandler(PatchLayerWidget* layerWidget,
                                         PatchLayerPtr patchLayer);
    void removePatchLayer(PatchLayerWidget *layerWidget);
    void removePatchLayerFromGuiOnly(PatchLayerWidget *layerWidget);
    void clearPatchLayersFromGuiOnly();
    void movePatchLayer(int indexFrom, int indexTo);
    KfJackMidiRoute* jackMidiRouteFromLayerWidget(PatchLayerWidget* layerWidget);

    QByteArray mCopiedLayerData;

private slots:
    void onLayer_slider_moved(PatchLayerWidget* layerWidget, float gain);
    void onLayer_solo_clicked(PatchLayerWidget* layerWidget, bool solo);
    void onLayer_mute_clicked(PatchLayerWidget* layerWidget, bool mute);
    void onLayer_midiSend_clicked(PatchLayerWidget* layerWidget);
    void onLayer_rightToolbutton_clicked(PatchLayerWidget* layerWidget);

    // Menu to add MIDI out port layer
private:
    void updateMidiOutPortsMenu(QMenu* menu);
    QMenu patchMidiOutPortsMenu;
private slots:
    void onPatchMidiOutPortsMenu_aboutToShow();
    void onPatchMidiOutPortsMenu_ActionTrigger(QAction* action);

    // Menu to add audio input port layer
private:
    void updateAudioInPortsMenu(QMenu* menu);
    QMenu patchAudioInPortsMenu;
private slots:
    void onPatchAudioInPortsMenu_aboutToShow();
    void onPatchAudioInPortsMenu_ActionTrigger(QAction* action);

    // Layer left toolbutton menu
private:
    QMenu layerToolMenu;
    QAction* audioInLayerInputPortConnectionsAction;
    ResetOptionMenu layerResetOptionMenu;
    void setupLayerToolMenu();
    PatchLayerWidget* layerToolMenuSourceitem = nullptr;
    void updateLayerToolMenu();
private slots:
    void onAudioInLayerInputPortConnectionActionTrigger();
    void onLayerResetOptionMenuActionTrigger(QAction* action);
    void onLayer_leftToolbutton_clicked(PatchLayerWidget* layerItem);
    void on_actionReload_Layer_triggered();
    void on_actionRemove_Layer_triggered();
    void on_actionCopy_Layer_triggered();
    void on_actionPaste_Layer_triggered();
    void on_actionOpen_In_File_Manager_layerwidget_triggered();

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

    // Layer bus menu
private:
    void updateBusMenu(QMenu* menu, int currentBusId = -1);
    QMenu layerBusMenu;
private slots:
    void onLayerBusMenu_ActionTrigger(QAction* action);

    // Layer MIDI out channel menu
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
    QScopedPointer<QMenu> patchRemoveMenu;

private slots:
    // Patch view area
    void on_lineEdit_PatchName_returnPressed();
    void on_lineEdit_PatchName_editingFinished();
    void on_lineEdit_ProjectName_editingFinished();
    void on_textBrowser_patchNote_textChanged();
    void on_toolButton_layer_down_clicked();
    void on_toolButton_layer_up_clicked();
    void on_listWidget_Layers_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_actionSave_Patch_As_Copy_triggered();
    void on_actionAdd_Patch_To_Library_triggered();
    void on_actionSave_Patch_To_File_triggered();
    void on_actionAlways_Active_triggered();

    // Patch List
    void on_actionNew_Patch_triggered();
    void on_actionAdd_Patch_From_File_triggered();
    void on_toolButton_RemovePatch_clicked();
    void on_toolButton_PatchUp_clicked();
    void on_toolButton_PatchDown_clicked();
    void on_toolButton_AddPatch_clicked();
    void on_toolButton_PatchListMenu_clicked();
    void toggleShowPatchListNumbers();
    void toggleShowPatchListNotes();

    // -----------------------------------------------------------------------
    // Connections screen (Ports and buses)
private:
    QMenu portsBusesTreeMenu;
    void setupPortsBusesTreeMenu();
    void showPortsBusesTreeMenu();

    void setupConnectionsPage();
    void showConnectionsPage();
    void connectionsTreeSelectBus(int busId);
    void connectionsTreeSelectAndEditBus(int busId);
    void connectionsTreeSelectAudioInPort(int portId);
    void connectionsTreeSelectAndEditAudioInPort(int portId);
    void connectionsTreeSelectMidiInPort(int portId);
    void connectionsTreeSelectAndEditMidiInPort(int portId);
    void connectionsTreeSelectMidiOutPort(int portId);
    void connectionsTreeSelectAndEditMidiOutPort(int portId);
    void updatePortsBussesTree();
    void updateConnectionsTree();
    void clearPortsBussesConnectionsData();
    bool connectionsTreeIsMidiInPortSelected();
    int connectionsTreeGetSelectedMidiInPortId();

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
    void onPortsBusesTreeMenuRequested();
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
    void on_pushButton_connectionsPage_editScript_clicked();
    void on_checkBox_connectionsPage_ignoreGlobalVolume_clicked();
    void on_toolButton_connectionsPage_portsBussesListOptions_clicked();

    // -----------------------------------------------------------------------
    // Settings
private:
    bool mSettingsFirstRun = false;
    ResetOptionComboBox mProjectResetOptionComboBox;
    ResetOptionComboBox mSettingsDefaultResetOptionComboBox;
    void setupSettings();
    void showSettingsDialog();
    void applySettings();
    void scanForDatabase();
    QString mSettingsDir;
    QString mProjectsDir;
    QString mSoundfontsDir;
    bool mPromptOnQuit = true;
    bool mStartMaximized = false;
    bool mOpenLastProjectAtStartup = false;
    QString mLastProjectFilePath;
    void setSoundfontsDir(QString path);
    QString mPatchesDir;
    void setPatchesDir(QString path);
    QString mSfzDir;
    void setSfzDir(QString path);
    QString mFilemanager;
    KonfytReset mDefaultResetOption = KonfytReset::NoReset;

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
     * (Currently the message is not displayed anywhere, as the status bar has
     * been removed.)
     * All GUI input is disabled.
     * When the signal is received which completes the operation, call stopWaiter(),
     * which will reset everything back to normal. */
    void startWaiter(QString msg);
    void stopWaiter();
    QString waiterMessage;
private slots:
    void scanThreadFihishedSlot();

    // -----------------------------------------------------------------------
    // MIDI filter editor
private:
    enum MidiFilterEditType {
        MidiFilterEditPort,
        MidiFilterEditLayer,
        MidiFilterEditPatch
    };
    MidiFilterEditType midiFilterEditType;
    PatchLayerWidget* midiFilterEditItem = nullptr;
    KfJackMidiRoute* midiFilterEditRoute = nullptr;
    int midiFilterEditPort;
    PatchPtr midiFilterEditPatch;
    void showMidiFilterEditor();
    KonfytMidiEvent midiFilterLastEvent;
    void updateMidiFilterEditorLastRx(KonfytMidiEvent ev);
    QList<int> textToNonRepeatedUint7List(QString text);
    QString intListToText(QList<int> lst);
    MidiFilter midiFilterFromGuiEditor();
    void updateMidiFilterBeingEdited(MidiFilter newFilter);
    bool blockMidiFilterEditorModified = false;

    MidiFilter midiFilterUnderEdit;
    MidiFilter midiFilterEditorOriginalFilter;

    // MIDI map presets
    struct MidiMapPreset {
        QString name = "Preset";
        QString data;
    };
    QList<MidiMapPreset*> midiMapUserPresets;
    QMenu midiMapPresetMenu;
    QWidget* midiMapPresetMenuRequester = nullptr;
    QString midiMapPresetsFilename;
    void setupMidiMapPresets();
    void addMidiMapFactoryPresetMenuAction(QString text, QString data);
    void addMidiMapUserPresetMenuAction(MidiMapPreset* preset);
    void popupMidiMapPresetMenu(QWidget* requester);
    void saveMidiMapPresets();
    void loadMidiMapPresets();

private slots:
    void onMidiFilterEditorModified();
    void onMidiMapPresetMenuTrigger(QAction* action);
    void on_actionEdit_MIDI_Filter_triggered();
    void on_pushButton_midiFilter_Cancel_clicked();
    void on_pushButton_midiFilter_Apply_clicked();
    void on_toolButton_MidiFilter_lowNote_clicked();
    void on_toolButton_MidiFilter_HighNote_clicked();
    void on_toolButton_MidiFilter_Add_Plus12_clicked();
    void on_toolButton_MidiFilter_Add_Minus12_clicked();
    void on_toolButton_MidiFilter_inChan_last_clicked();
    void on_toolButton_MidiFilter_pitchDownFull_clicked();
    void on_toolButton_MidiFilter_pitchDownHalf_clicked();
    void on_toolButton_MidiFilter_pitchDownLast_clicked();
    void on_spinBox_midiFilter_pitchDownRange_valueChanged(int value);
    void on_spinBox_midiFilter_pitchUpRange_valueChanged(int value);
    void on_toolButton_MidiFilter_pitchUpFull_clicked();
    void on_toolButton_MidiFilter_pitchUpHalf_clicked();
    void on_toolButton_MidiFilter_pitchUpLast_clicked();
    void on_toolButton_MidiFilter_ccAllowedLast_clicked();
    void on_toolButton_MidiFilter_ccBlockedLast_clicked();
    void on_lineEdit_MidiFilter_velocityMap_textChanged(const QString &text);
    void on_toolButton_MidiFilter_velocityMap_presets_clicked();
    void on_toolButton_MidiFilter_velocityMap_save_clicked();
    void on_comboBox_midiFilter_inChannel_currentIndexChanged(int index);
    void on_spinBox_midiFilter_LowNote_valueChanged(int arg1);
    void on_spinBox_midiFilter_HighNote_valueChanged(int arg1);
    void on_spinBox_midiFilter_Add_valueChanged(int arg1);
    void on_checkBox_midiFilter_ignoreGlobalTranspose_toggled(bool checked);
    void on_checkBox_midiFilter_AllCCs_toggled(bool checked);
    void on_lineEdit_MidiFilter_ccAllowed_textChanged(const QString &arg1);
    void on_lineEdit_MidiFilter_ccBlocked_textChanged(const QString &arg1);
    void on_checkBox_midiFilter_pitchbend_toggled(bool checked);
    void on_checkBox_midiFilter_Prog_toggled(bool checked);
    void on_actionPatch_MIDI_Filter_triggered();

    // -----------------------------------------------------------------------
    // MIDI send list

    // MIDI send list editor
private:
    PatchLayerWidget* midiSendListEditItem = nullptr;
    KfJackMidiRoute* midiSendListEditRoute = nullptr;
    QList<MidiSendItem> midiSendList;
    void setupMidiSendListEditor();
    void updateMidiSendListEditorButtonStates();
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
    MidiEventListWidgetAdapter midiSendListEditorMidiRxList;

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
    void onMidiSendListEditorMidiRxListItemClicked();
    void on_pushButton_midiSendList_replace_clicked();
    void on_toolButton_midiSendList_down_clicked();
    void on_toolButton_midiSendList_up_clicked();
    void on_pushButton_midiSendList_remove_clicked();
    void on_pushButton_midiSendList_sendSelected_clicked();
    void on_pushButton_midiSendList_sendAll_clicked();
    void on_listWidget_midiSendList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_treeWidget_savedMidiMessages_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    // Saved MIDI send items (presets)
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

    // -----------------------------------------------------------------------
    // JACK / MIDI
private:
    KonfytJackEngine jack;
    int mJackXrunCount = 0;
    QElapsedTimer mXrunTimer;
    QString xrunTimeString(qint64 ms);
    void setupJack();
    void addAudioBusToJack(int busNo, KfJackAudioPort** leftPort, KfJackAudioPort** rightPort);
    void addAudioInPortsToJack(int portNo, KfJackAudioPort** leftPort, KfJackAudioPort** rightPort);
    KfJackMidiPort* addMidiOutPortToJack(int numberLabel);
    KfJackMidiPort* addMidiInPortToJack(int numberLabel);
    bool jackPortBelongstoUs(QString jackPortName);
    void handleRouteMidiEvent(KfJackMidiRxEvent rxEvent);
    void handlePortMidiEvent(KfJackMidiRxEvent rxEvent);
private slots:
    void onJackPrint(QString msg);
    void onJackMidiEventsReceived();
    void onJackAudioEventsReceived();
    void onJackXrunOccurred();
    void onJackPortRegisteredOrConnected();

    // -----------------------------------------------------------------------
    // Scripting / Script editor
private:
    KonfytJSEngine scriptEngine;
    QThread scriptingThread;
    void setupScripting();
    PatchLayerPtr mScriptEditLayer;
    Project::MidiPortPtr mScriptEditPort;
    QWidget* stackedWidgetBeforeScriptEditor = nullptr;
    void showScriptEditorForPatchLayer(PatchLayerPtr patchLayer);
    void showScriptEditorForPort(Project::MidiPortPtr prjPort);
    void showScriptEditor();
    bool scriptEditorIgnoreChanged = false;
    QTimer scriptInfoTimer;
    void updateScriptEditorScriptProcessTimeText(float perEventMs, float totalProcessMs);
    void updateScriptEditorTotalProcessTimeText(float processTimeMs);
    void updateScriptEditorErrorText(QString errorString);
private slots:
    void onScriptInfoTimer();
    void onJsEnginePrint(QString msg);
    void onLayerScriptPrint(PatchLayerPtr patchLayer, QString msg);
    void onPortScriptPrint(Project::MidiPortPtr prjPort, QString msg);
    void onLayerScriptErrorStatusChanged(PatchLayerPtr patchLayer,
                                         QString errorString);
    void onPortScriptErrorStatusChanged(Project::MidiPortPtr prjPort,
                                        QString errorString);
    void on_action_Edit_Script_triggered();
    void on_pushButton_script_update_clicked();
    void on_checkBox_script_enable_toggled(bool checked);
    void on_checkBox_script_passMidiThrough_toggled(bool checked);
    void on_pushButton_scriptEditor_OK_clicked();
    void on_plainTextEdit_script_textChanged();

    // Listing scripts from projects
private:
    typedef QMap<QTreeWidgetItem*, QString> ItemScriptMap;
    ItemScriptMap mItemScriptMap;

    QTreeWidgetItem* scanProjectScripts(QString path, ItemScriptMap* map);
private slots:
    void on_pushButton_scripts_rescan_clicked();
    void on_treeWidget_scripts_currentItemChanged(QTreeWidgetItem *current,
                                                  QTreeWidgetItem *previous);
    void on_tabWidget_scripting_currentChanged(int index);
    void on_treeWidget_scripts_itemClicked(QTreeWidgetItem *item, int column);

    // -----------------------------------------------------------------------
    // External apps
private:
    void setupExternalApps();
    void setupExternalAppsForCurrentProject();

    ExternalAppRunner externalAppRunner;
    QScopedPointer<ExternalAppsListAdapter> externalAppListAdapter;

    // External apps editor
private:
    int externalAppEditorCurrentId = -1;
    void externalAppToEditor(Project::ExternalApp app);
    Project::ExternalApp externalAppFromEditor();

    void showExternalAppEditor(int id);
    void hideExternalAppEditor();

    void appendToExternalAppEditorCommandBox(QString text);
private slots:
    void externalAppEditor_onExternalAppRemoved(int id);

    // External apps presets menu
private:
    QMenu extAppsMenu;
    void setupExternalAppsMenu();
private slots:
    void on_toolButton_ExtAppsMenu_clicked();

    // External Apps widgets
    void onExternalAppItemDoubleClicked(int id);
    void onExternalAppItemSelectionChanged(int id);
    void onExternalAppItemCountChanged(int count);
    void on_pushButton_ExtApp_RunSelected_clicked();
    void on_pushButton_ExtApp_Stop_clicked();
    void on_pushButton_ExtApp_RunAll_clicked();
    void on_pushButton_ExtApp_StopAll_clicked();
    void on_pushButton_extAppEditor_apply_clicked();
    void on_pushButton_extAppEditor_Cancel_clicked();
    void on_pushButton_extApp_add_clicked();
    void on_pushButton_extApp_edit_clicked();
    void on_pushButton_extApp_remove_clicked();

    // -----------------------------------------------------------------------
    // Buses, audio and MIDI ports
private:
    int addBus();
    int addAudioInPort();
    int addMidiInPort();
    int addMidiOutPort();

    void removeAudioBusFromEngines(Project::AudioPortPtr bus);
    void removeMidiInPortFromEngines(Project::MidiPortPtr prjPort);
    void removeMidiOutPortFromEngines(Project::MidiPortPtr prjPort);
    void removeAudioInPortFromEngines(Project::AudioPortPtr prjPort);

signals:
    void audioBusRemoved(Project::AudioPortPtr bus);
    void midiInPortRemoved(Project::MidiPortPtr prjPort);
    void midiOutPortRemoved(Project::MidiPortPtr prjPort);
    void audioInPortRemoved(Project::AudioPortPtr prjPort);

    // Regex connections
private:
    void updateGuiForJackConRegexPreview();

private slots:
    void on_lineEdit_jackCon_regex_client_textChanged(const QString &arg1);
    void on_lineEdit_jackCon_regex_port_textChanged(const QString &arg1);

    // -----------------------------------------------------------------------
    // Triggers
private:
    /* To add a new trigger, create an action with unique text in the mainwindow.ui editor.
     * Add the action to the list in initTriggers().
     * In midiEventSlot(), handle the action.
     * Saving and loading to the project is done automatically. */

    QHash<QTreeWidgetItem*, QAction*> triggersItemActionHash;
    // Map an int hash of MIDI events to actions for fast lookup
    QHash<int, QAction*> triggersMidiActionHash;
    void initTriggers();
    void setupTriggersPage();
    void showTriggersPage();
    MidiEventListWidgetAdapter triggersPageMidiEventList;

    QList<QAction*> channelGainActions;
    QList<QAction*> channelSoloActions;
    QList<QAction*> channelMuteActions;
    QList<QAction*> patchActions;
    enum SetGainType {SetGainAbsolute, SetGainRelative};
    void midi_setLayerGain(int layerIndex, int midiValue,
                           SetGainType setGainType = SetGainAbsolute);
    void midi_setLayerSolo(int layer, int midiValue);
    void midi_setLayerMute(int layer, int midiValue);

private slots:
    void on_pushButton_ShowTriggersPage_clicked();
    void on_pushButton_triggersPage_OK_clicked();
    void on_pushButton_triggersPage_assign_clicked();
    void on_pushButton_triggersPage_clear_clicked();
    void on_tree_Triggers_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void onTriggersMidiEventListDoubleClicked();
    void on_checkBox_Triggers_ProgSwitchPatches_clicked();
    void on_listWidget_triggers_eventList_currentItemChanged(
            QListWidgetItem *current=nullptr, QListWidgetItem *previous=nullptr);
    void on_tree_Triggers_currentItemChanged(
            QTreeWidgetItem *current=nullptr, QTreeWidgetItem *previous=nullptr);
    void on_spinBox_Triggers_midiPickupRange_valueChanged(int arg1);

    // -----------------------------------------------------------------------
    // Other JACK connections
private:
    bool jackPage_audio = true; // True to display audio ports, false for MIDI
    void setupOtherJackConsPage();
    void showOtherJackConsPage();
    void showOtherJackConsPageMidi();
    void showOtherJackConsPageAudio();
    void updateOtherJackConsPage();
    void updateOtherJackConsPageButtonStates();
    void otherJackConsPage_addSelectedConnection(bool makeNotBreak);
private slots:
    void on_pushButton_ShowJackPage_clicked();
    void on_pushButton_jackConAdd_clicked();
    void on_pushButton_jackConAddBreak_clicked();
    void on_pushButton_jackConRemove_clicked();
    void on_pushButton_JackAudioPorts_clicked();
    void on_pushButton_JackMidiPorts_clicked();
    void on_pushButton_jackCon_OK_clicked();
    void on_treeWidget_jackPortsOut_currentItemChanged(
            QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_treeWidget_jackportsIn_currentItemChanged(
            QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_listWidget_jackConnections_currentItemChanged(
            QListWidgetItem *current, QListWidgetItem *previous);

    // -----------------------------------------------------------------------
    // Warnings

    // Port connection warnings
private:
    void setupPortConnectionWarnings();
    void setupWarningConnectionsForProject(ProjectPtr prj);

    void updatePortConnectionWarnings();
    void updateAudioBusWarnings();
    void updateMidiInPortsWarnings();
    void updateMidiOutPortWarnings();
    void updateAudioInPortWarnings();
    void updateOtherJackMidiConnectionWarnings();
    void updateOtherJackAudioConnectionWarnings();

    BiQMap<Project::AudioPortPtr, QListWidgetItem*> audioBusWarningMap;
    BiQMap<Project::MidiPortPtr, QListWidgetItem*> midiPortWarningMap; // MIDI in and out ports
    BiQMap<Project::AudioPortPtr, QListWidgetItem*> audioInPortWarningMap;

    QListWidgetItem* otherJackMidiConsWarningHeader = nullptr;
    BiQMap<QListWidgetItem*, QString> otherJackMidiConsWarningMap;

    QListWidgetItem* otherJackAudioConsWarningHeader = nullptr;
    BiQMap<QListWidgetItem*, QString> otherJackAudioConsWarningMap;

private slots:
    void audioBusWarnings_onBusRemoved(Project::AudioPortPtr bus);
    void midiPortWarnings_onPortRemoved(Project::MidiPortPtr prjPort);
    void audioInPortWarnings_onPortRemoved(Project::AudioPortPtr prjPort);
    void portWarnings_onItemDoubleClicked(QListWidgetItem* item);

    // Script warnings
private:
    void setupScriptingWarnings();

    BiQMap<PatchLayerPtr, QListWidgetItem*> scriptWarningLayerMap;
    BiQMap<Project::MidiPortPtr, QListWidgetItem*> scriptWarningPortMap;
private slots:
    void scriptWarningsOnScriptEngineLayerErrorStatusChanged(
            PatchLayerPtr patchLayer, QString errorString);
    void scriptWarningsOnScriptEnginePortErrorStatusChanged(
            Project::MidiPortPtr prjPort, QString errorString);
    void scriptWarningsOnPatchLayerUnloaded(PatchLayerPtr patchLayer);
    void scriptWarningsOnPortRemoved(Project::MidiPortPtr prjPort);
    void scriptWarningsOnItemDoubleClicked(QListWidgetItem* item);

    // SFZ engine warnings
private:
    void setupSfzEngineWarnings();
    QListWidgetItem* sfzEngineWarningItem = nullptr;
private slots:
    void sfzEngineWarnings_onSfzEngineErrorChanged(QString error);
    void sfzEngineWarnings_onItemDoubleClicked(QListWidgetItem* item);

    // -----------------------------------------------------------------------
    // Other

private:
    // Center-view and sidebar (For changing to and from Saved MIDI Send List sidebar)
    QWidget* lastCenterWidget = nullptr;
    QWidget* lastSidebarWidget = nullptr;
private slots:
    void on_stackedWidget_currentChanged(int arg1);

    // Panic
private:
    bool panicState = false;
    void triggerPanic(bool panic);
private slots:
    void on_pushButton_Panic_clicked();
    void on_pushButton_Panic_customContextMenuRequested(const QPoint &pos);
    void on_actionPanic_triggered();
    void on_actionPanicToggle_triggered();

    // About Dialog
private:
    void initAboutDialog();
    void showAboutDialog();
private slots:
    void on_pushButton_about_ok_clicked();

    // Console
private:
    ConsoleWindow consoleWindow {this};
    void setupConsoleDialog();
    void setConsoleShowMidiMessages(bool show);
    bool mConsoleShowMidiMessages = false;
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

    QTimer midiIndicatorTimer;
    void setupMidiIndicatorTimer();
    void updateGlobalSustainIndicator();
    void updateGlobalPitchbendIndicator();
private slots:
    void onMidiIndicatorTimerTick();
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

    // Global volume
private:
    float masterGain = 1.0; // Master gain when not in preview mode
    MidiValueController masterGainMidiCtrlr;
    void setMasterGainFloat(float gain);
    void setMasterGainMidi(int value);
    void updateMasterGainCommon(float gain);
    void updateBusGainInJackEngine(int busId);
private slots:
    void on_horizontalSlider_MasterGain_valueChanged(int value);
    void on_actionMaster_Volume_Up_triggered();
    void on_actionMaster_Volume_Down_triggered();
    
    // Other widget/action slots
private slots:
    void on_pushButton_LiveMode_clicked();
    void on_pushButton_RestartApp_clicked();
    void on_pushButton_LavaMonster_clicked();

    void on_pushButton_jackCon_regex_add_clicked();
};

#endif // MAINWINDOW_H
