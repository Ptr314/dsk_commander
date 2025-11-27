// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Main window

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QStandardItemModel>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include <QComboBox>
#include <QTranslator>
#include <QItemSelection>
#include <QLabel>

#include "FilePanel.h"

#include "dsk_tools/dsk_tools.h"


struct PanelMenuActions {
    QAction *sortByName;
    QAction *sortBySize;
    QAction *noSort;
    QAction *showDeleted;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onView();
    void onEdit();
    void onCopy();
    void onRename();
    void onMkdir();
    void onDelete();
    void onExit();

    void doCopy(bool copy);

    void setActivePanel(FilePanel* panel);
    void updateStatusBarInfo();
    void updateViewButtonState();

    // New menu system slots
    void onGoUp(FilePanel* panel);
    void onOpenDirectory(FilePanel* panel);
    void onSetSorting(FilePanel* panel, HostModel::SortOrder order);
    void onSetShowDeleted(FilePanel* panel, bool show);
    void updateSortingMenu(FilePanel* panel);
    void onAbout();

protected:
    void changeEvent(QEvent* event) override;

private:
    QTranslator translator;
    QTranslator qtTranslator;

    QString directory;
    QSettings * settings;
    QFileSystemModel leftFilesModel;
    QStandardItemModel rightFilesModel;

    QJsonObject file_formats;
    QJsonObject file_types;
    QJsonObject file_systems;

    dsk_tools::diskImage * image = nullptr;
    dsk_tools::fileSystem * filesystem = nullptr;

    bool is_loaded = false;

    void load_config();
    void switch_language(const QString &lang, bool init);
    void update_info();

    void initializeMainMenu();

private:
    FilePanel* leftPanel {nullptr};
    FilePanel* rightPanel {nullptr};
    FilePanel* activePanel {nullptr};

    QAction* actSave {nullptr};  // F2 button action
    QAction* actView {nullptr};
    QAction* actEdit {nullptr};
    QAction* actCopy {nullptr};
    QAction* actRename {nullptr};
    QAction* actMkdir {nullptr};
    QAction* actDelete {nullptr};
    QAction* actExit {nullptr};

    QAction* menuViewAction {nullptr};
    QAction* menuEditAction {nullptr};

    // Image menu actions
    QAction* actImageInfo {nullptr};
    QAction* actImageSave {nullptr};
    QAction* actImageSaveAs {nullptr};

    QLabel* statusLabel {nullptr};

    PanelMenuActions leftMenuActions;
    PanelMenuActions rightMenuActions;

    void createActions();
    void updateActionTexts();
    QWidget* createBottomPanel();
    FilePanel* otherPanel() const;

    // Image menu slots
    void onImageInfo();
    void onImageSave();
    void onImageSaveAs();
    void updateImageMenuState();
};
#endif // MAINWINDOW_H
