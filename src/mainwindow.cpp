// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Main window

#include <fstream>
#include <set>

#include <QTranslator>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFontDatabase>
#include <QInputDialog>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QStatusBar>
#include <QToolButton>
#include <QDebug>
#include <QActionGroup>
#include <QMenu>

#include "mainwindow.h"
#include "convertdialog.h"
#include "fileparamdialog.h"
#include "formatdialog.h"
#include "viewdialog.h"

#include "./ui_mainwindow.h"
#include "./ui_aboutdlg.h"
#include "./ui_fileinfodialog.h"

#include "dsk_tools/dsk_tools.h"

#include "globals.h"
#include "mainutils.h"

#define INI_FILE_NAME "/dsk_com.ini"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , leftFilesModel(this)
    , rightFilesModel(this)
{
    QFontDatabase::addApplicationFont(":/fonts/consolas");
    QFontDatabase::addApplicationFont(":/fonts/dos");

    QString app_path = QApplication::applicationDirPath();
    QString current_path = QDir::currentPath();
    QString ini_path, ini_file;

#if defined(__linux__)
    ini_path = QString(getenv("HOME")) + "/.config";
    ini_file = ini_path + INI_FILE_NAME;
#elif defined(__APPLE__)
    ini_path = QString(getenv("HOME"));
    ini_file = ini_path + INI_FILE_NAME;
#elif defined(_WIN32)
    QFileInfo ini_fi(current_path + INI_FILE_NAME);
    if (ini_fi.exists() && ini_fi.isFile()) {
        ini_path = current_path;
    } else {
        ini_path = app_path;
    }
    ini_file = ini_path + INI_FILE_NAME;
#else
#error "Unknown platform"
#endif

    settings = new QSettings(ini_file, QSettings::IniFormat);

    QString ini_lang = settings->value("interface/language", "").toString();

    if (ini_lang.length() == 0) {
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = QLocale(locale).name().toLower();
            switch_language(baseName, true);
            break;
        }
    } else {
        switch_language(ini_lang, true);
    }

    ui->setupUi(this);

    resize(1000, 600);

    QList<int> sizes;
    int halfWidth = this->width() / 2;
    sizes << halfWidth << halfWidth;

    setWindowTitle(windowTitle() + " " + PROJECT_VERSION);

    // QFont font("PxPlus IBM VGA9", 12, 400);
    QFont font("Consolas", 10, 400);
    // QFont font("Iosevka Fixed", 10, 400);
    // font.setStretch(QFont::Expanded);

    load_config();

// New interface
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    leftPanel = new FilePanel(this, settings, "left", file_formats, file_types, file_systems);
    rightPanel = new FilePanel(this, settings, "right", file_formats, file_types, file_systems);

    connect(leftPanel,  &FilePanel::activated, this, &MainWindow::setActivePanel);
    connect(rightPanel, &FilePanel::activated, this, &MainWindow::setActivePanel);

    // Handle Tab key to switch between panels
    connect(leftPanel, &FilePanel::switchPanelRequested, this, [this]() {
        FilePanel* target = otherPanel();
        setActivePanel(target);
        target->focusList();
    });
    connect(rightPanel, &FilePanel::switchPanelRequested, this, [this]() {
        FilePanel* target = otherPanel();
        setActivePanel(target);
        target->focusList();
    });

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    createActions();
    layout->addWidget(splitter);
    layout->addWidget(createBottomPanel());
    layout->setStretch(0, 1); // file panels stretch
    layout->setStretch(1, 0); // bottom panel takes minimum space
    setCentralWidget(central);

    statusLabel = new QLabel(this);
    statusLabel->setText("Готово");
    statusBar()->addWidget(statusLabel);

    // setWindowTitle("Двухпанельный файловый менеджер (Qt6)");
    // resize(1200, 700);

    setActivePanel(leftPanel);
    updateViewButtonState();

    // Initialize new menu system
    initializeMainMenu();

    // Connect panel sorting signals to update menu checkmarks
    connect(leftPanel, &FilePanel::sortOrderChanged, this, [this]() {
        updateSortingMenu(leftPanel);
    });
    connect(rightPanel, &FilePanel::sortOrderChanged, this, [this]() {
        updateSortingMenu(rightPanel);
    });

    // Connect panel mode change signals to update button states
    connect(leftPanel, &FilePanel::panelModeChanged, this, &MainWindow::updateViewButtonState);
    connect(rightPanel, &FilePanel::panelModeChanged, this, &MainWindow::updateViewButtonState);
}

void MainWindow::switch_language(const QString & lang, bool init)
{
    if (translator.load(":/i18n/" + lang)) {
        qApp->installTranslator(&translator);

        QString t = QString(":/i18n/qtbase_%1.qm").arg(lang.split("_")[0]);
        if (qtTranslator.load(t)) {
            qApp->installTranslator(&qtTranslator);
        }
        if (!init) {
            ui->retranslateUi(this);
            settings->setValue("interface/language", lang);
        }
    } else {
        QMessageBox::warning(this, MainWindow::tr("Error"), MainWindow::tr("Failed to load language file for: ") + lang);
    }
}


void MainWindow::load_config()
{
    QFile file(":/files/config");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(0, MainWindow::tr("Error"), MainWindow::tr("Error reading config file"));
        return;
    }
    QByteArray config_contents = file.readAll();
    file.close();
    QJsonParseError err;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(config_contents, &err);
    if (jsonDoc.isNull()) {
        QMessageBox::critical(0, qApp->translate("Main", "Error"), qApp->translate("Main", "Config parse error: %1").arg(err.errorString()));
        return;
    }
    QJsonObject jsonRoot = jsonDoc.object();
    file_formats = jsonRoot["file_formats"].toObject();
    file_types = jsonRoot["file_types"].toObject();
    file_systems = jsonRoot["file_systems"].toObject();

    // Fill FILE_SUPPORTED

    QString all_filters = "";
    foreach (const QString & ff_id, file_formats.keys()) {
        QJsonObject format = file_formats[ff_id].toObject();
        if (format["source"].toBool()) {
            if (ff_id != "FILE_ANY" && ff_id != "FILE_SUPPORTED") {
                if (all_filters.size() > 0) all_filters += ";";
                all_filters += format["extensions"].toString();
            }
        }
    }

    QJsonArray all_types;
    const QStringList &all_types_sl = file_types.keys();
    for (const auto &str : all_types_sl) {
        all_types.append(str);
    }

    // Update FILE_ANY
    QJsonObject fileAny = file_formats["FILE_SUPPORTED"].toObject();
    fileAny["extensions"] = all_filters;
    fileAny["types"] = all_types;
    file_formats["FILE_SUPPORTED"] = fileAny;
}

MainWindow::~MainWindow()
{
    // delete settings;
    delete ui;
}

QString MainWindow::replace_placeholders(const QString & in)
{
    return QString(in)
        .replace("{$DIRECTORY_ENTRY}", MainWindow::tr("Directory Entry"))
        .replace("{$FILE_NAME}", MainWindow::tr("File Name"))
        .replace("{$SIZE}", MainWindow::tr("File Size"))
        .replace("{$BYTES}", MainWindow::tr("byte(s)"))
        .replace("{$SIDES}", MainWindow::tr("Sides"))
        .replace("{$TRACKS}", MainWindow::tr("Tracks"))
        .replace("{$SECTORS}", MainWindow::tr("sector(s)"))
        .replace("{$ATTRIBUTES}", MainWindow::tr("Attributes"))
        .replace("{$DATE}", MainWindow::tr("Date"))
        .replace("{$TYPE}", MainWindow::tr("Type"))
        .replace("{$PROTECTED}", MainWindow::tr("Protected"))
        .replace("{$YES}", MainWindow::tr("Yes"))
        .replace("{$NO}", MainWindow::tr("No"))
        .replace("{$TS_LIST_LOCATION}", MainWindow::tr("T/S List Location"))
        .replace("{$TS_LIST_DATA}", MainWindow::tr("T/S List Contents"))
        .replace("{$INCORRECT_TS_DATA}", MainWindow::tr("Incorrect T/S data, stopping iterations"))
        .replace("{$NEXT_TS}", MainWindow::tr("Next T/S List Location"))
        .replace("{$FILE_END_REACHED}", MainWindow::tr("File End Reached"))
        .replace("{$FILE_DELETED}", MainWindow::tr("The file is marked as deleted, the data below may be incorrect"))
        .replace("{$ERROR_PARSING}", MainWindow::tr("File parsing error"))
        .replace("{$TRACK}", MainWindow::tr("Track"))
        .replace("{$TRACK_SHORT}", MainWindow::tr("T"))
        .replace("{$SIDE_SHORT}", MainWindow::tr("H"))
        .replace("{$PHYSICAL_SECTOR}", MainWindow::tr("S"))
        .replace("{$LOGICAL_SECTOR}", MainWindow::tr("S"))
        .replace("{$PARSING_FINISHED}", MainWindow::tr("Parsing finished"))
        .replace("{$VOLUME_ID}", MainWindow::tr("V"))
        .replace("{$SECTOR_INDEX}", MainWindow::tr("Index"))
        .replace("{$INDEX_MARK}", MainWindow::tr("Index Mark"))
        .replace("{$DATA_MARK}", MainWindow::tr("Data Mark"))
        .replace("{$DATA_FIELD}", MainWindow::tr("Sector Data"))
        .replace("{$SECTOR_INDEX_END_OK}", MainWindow::tr("End Mark: OK"))
        .replace("{$SECTOR_INDEX_END_ERROR}", MainWindow::tr("End Mark: Not Detected"))
        .replace("{$SECTOR_CRC_OK}", MainWindow::tr("CRC: OK"))
        .replace("{$SECTOR_CRC_ERROR}", MainWindow::tr("CRC: Error"))
        .replace("{$CRC_EXPECTED}", MainWindow::tr("Expected"))
        .replace("{$CRC_FOUND}", MainWindow::tr("Found"))
        .replace("{$SECTOR_ERROR}", MainWindow::tr("Data Error"))
        .replace("{$INDEX_CRC_OK}", MainWindow::tr("CRC: OK"))
        .replace("{$INDEX_CRC_ERROR}", MainWindow::tr("CRC: Error"))
        .replace("{$INDEX_EPILOGUE_OK}", MainWindow::tr("Epilogue: OK"))
        .replace("{$INDEX_EPILOGUE_ERROR}", MainWindow::tr("Epilogue: Error"))
        .replace("{$DATA_EPILOGUE_OK}", MainWindow::tr("Epilogue: OK"))
        .replace("{$DATA_EPILOGUE_ERROR}", MainWindow::tr("Epilogue: Error"))
        .replace("{$TRACKLIST_OFFSET}", MainWindow::tr("Track List Offset"))
        .replace("{$TRACK_OFFSET}", MainWindow::tr("Track Offset"))
        .replace("{$TRACK_SIZE}", MainWindow::tr("Track Size"))
        .replace("{$HEADER}", MainWindow::tr("Header"))
        .replace("{$SIGNATURE}", MainWindow::tr("Signature"))
        .replace("{$NO_SIGNATURE}", MainWindow::tr("No known signature found, aborting"))
        .replace("{$FORMAT_REVISION}", MainWindow::tr("Format revision"))
        .replace("{$SIDE}", MainWindow::tr("Side"))
        .replace("{$CUSTOM_DATA}", MainWindow::tr("Custom Data"))
        .replace("{$VTOC_FOUND}", MainWindow::tr("DOS 3.3 VTOC found"))
        .replace("{$VTOC_NOT_FOUND}", MainWindow::tr("DOS 3.3 VTOC not found"))
        .replace("{$VTOC_CATALOG_TRACK}", MainWindow::tr("Root Catalog Track"))
        .replace("{$VTOC_CATALOG_SECTOR}", MainWindow::tr("Root Catalog Sector"))
        .replace("{$VTOC_DOS_RELEASE}", MainWindow::tr("DOS Release"))
        .replace("{$VTOC_VOLUME_ID}", MainWindow::tr("Volume ID"))
        .replace("{$VTOC_VOLUME_NAME}", MainWindow::tr("Volume name"))
        .replace("{$VTOC_PAIRS_ON_SECTOR}", MainWindow::tr("Pairs per T/S list"))
        .replace("{$VTOC_LAST_TRACK}", MainWindow::tr("Last track"))
        .replace("{$VTOC_DIRECTION}", MainWindow::tr("Direction"))
        .replace("{$VTOC_TRACKS_TOTAL}", MainWindow::tr("Tracks total"))
        .replace("{$VTOC_SECTORS_ON_TRACK}", MainWindow::tr("Sectors on track"))
        .replace("{$VTOC_BYTES_PER_SECTOR}", MainWindow::tr("Bytes per sector"))
        .replace("{$ERROR_OPENING}", MainWindow::tr("Error opening the file"))
        .replace("{$ERROR_LOADING}", MainWindow::tr("Error loading, check if the file type is correct"))

        .replace("{$DPB_INFO}", MainWindow::tr("DPB Information"))
        .replace("{$DPB_VOLUME_ID}", MainWindow::tr("Volume ID"))
        .replace("{$DPB_TYPE}", MainWindow::tr("Device type"))
        .replace("{$DPB_DSIDE}", MainWindow::tr("DSIDE"))
        .replace("{$DPB_TSIZE}", MainWindow::tr("Blocks on track"))
        .replace("{$DPB_DSIZE}", MainWindow::tr("Tracks on disk"))
        .replace("{$DPB_MAXBLOK}", MainWindow::tr("Last block"))
        .replace("{$DPB_VTOCADR}", MainWindow::tr("VTOC block"))

        .replace("{$EXTENT}", MainWindow::tr("Extent"))
        .replace("{$FREE_SECTORS}", MainWindow::tr("Free sectors"))
        .replace("{$FREE_BYTES}", MainWindow::tr("Free bytes"))

        .replace("{$META_FILENAME}", MainWindow::tr("File Name"))
        .replace("{$META_PROTECTED}", MainWindow::tr("Protected"))
        .replace("{$META_TYPE}", MainWindow::tr("Type"))
        .replace("{$META_EXTENDED}", MainWindow::tr("Extended"))

        .replace("{$AGAT_VR_FOUND}", MainWindow::tr("Agat image VR block found"))
        .replace("{$AGAT_VR_MODE}", MainWindow::tr("Video mode"))
        .replace("{$AGAT_VR_AGAT_GMODES}", MainWindow::tr("Agat graphic"))
        .replace("{$AGAT_VR_AGAT_TMODES}", MainWindow::tr("Agat text"))
        .replace("{$AGAT_VR_A2_MODES}", MainWindow::tr("Apple II modes"))
        .replace("{$AGAT_VR_GIGA_MODES}", MainWindow::tr("Agat GigaScreen"))
        .replace("{$AGAT_VR_MAIN_PALETTE}", MainWindow::tr("Main palette"))
        .replace("{$AGAT_VR_ATL_PALETTE}", MainWindow::tr("Alternative palette"))
        .replace("{$AGAT_VR_CUSTOM_PALETTE}", MainWindow::tr("Custom palette"))
        .replace("{$AGAT_VR_COMMENT}", MainWindow::tr("Comment block"))
        .replace("{$AGAT_VR_FONT}", MainWindow::tr("Font ID"))
        .replace("{$AGAT_VR_CUSTOM_FONT}", MainWindow::tr("Custom font"))
        ;
}

// New interface elements ---------------------------

void MainWindow::createActions() {
    // Bottom toolbar actions - created here and used in createBottomPanel
    actSave   = new QAction(MainWindow::tr("F2 Save"), this);
    actView   = new QAction(MainWindow::tr("F3 Information"), this);
    actEdit   = new QAction(MainWindow::tr("F4 Open"), this);
    actCopy   = new QAction(MainWindow::tr("F5 Copy"), this);
    actRename = new QAction(MainWindow::tr("F6 Rename"), this);
    actMkdir  = new QAction(MainWindow::tr("F7 MkDir"), this);
    actDelete = new QAction(MainWindow::tr("F8 Delete"), this);
    actExit   = new QAction(MainWindow::tr("F10 Exit"), this);

    actSave->setShortcut(Qt::Key_F2);
    actSave->setShortcutContext(Qt::WindowShortcut);
    actView->setShortcut(Qt::Key_F3);
    actView->setShortcutContext(Qt::WindowShortcut);
    actEdit->setShortcut(Qt::Key_F4);
    actEdit->setShortcutContext(Qt::WindowShortcut);
    actCopy->setShortcut(Qt::Key_F5);
    actCopy->setShortcutContext(Qt::WindowShortcut);
    actRename->setShortcut(Qt::Key_F6);
    actRename->setShortcutContext(Qt::WindowShortcut);
    actMkdir->setShortcut(Qt::Key_F7);
    actMkdir->setShortcutContext(Qt::WindowShortcut);
    actDelete->setShortcut(Qt::Key_F8);
    actDelete->setShortcutContext(Qt::WindowShortcut);
    actExit->setShortcut(Qt::Key_F10);
    actExit->setShortcutContext(Qt::WindowShortcut);

    connect(actSave,   &QAction::triggered, this, &MainWindow::onImageSave);
    connect(actView,   &QAction::triggered, this, &MainWindow::onView);
    connect(actEdit,   &QAction::triggered, this, &MainWindow::onEdit);
    connect(actCopy,   &QAction::triggered, this, &MainWindow::onCopy);
    connect(actRename, &QAction::triggered, this, &MainWindow::onRename);
    connect(actMkdir,  &QAction::triggered, this, &MainWindow::onMkdir);
    connect(actDelete, &QAction::triggered, this, &MainWindow::onDelete);
    connect(actExit,   &QAction::triggered, this, &MainWindow::onExit);

    addActions({actSave, actView, actEdit, actCopy, actRename, actMkdir, actDelete, actExit});
}

QWidget* MainWindow::createBottomPanel() {
    auto *panel = new QWidget(this);
    auto *layout = new QHBoxLayout(panel);

    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(10);

    // Expand to the window
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QList<QToolButton*> buttons;

    auto makeButton = [&](QAction* act) {
        auto *btn = new QToolButton(this);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setDefaultAction(act);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(btn);
        buttons << btn;
    };

    makeButton(actSave);   // F2
    makeButton(actView);   // F3
    makeButton(actEdit);   // F4
    makeButton(actCopy);   // F5
    makeButton(actRename); // F6
    makeButton(actMkdir);  // F7
    makeButton(actDelete); // F8
    makeButton(actExit);   // F10

    // buttons have equal widths
    for (int i = 0; i < buttons.size(); ++i)
        layout->setStretch(i, 1);

    // Expand panel to the window
    layout->setAlignment(Qt::AlignJustify);

    return panel;
}

void MainWindow::initializeMainMenu() {
    // Clear existing menu
    menuBar()->clear();

    // === LEFT PANEL MENU ===
    QMenu *leftMenu = menuBar()->addMenu(MainWindow::tr("Left panel"));

    QAction *leftGoUp = leftMenu->addAction(QIcon(":/icons/up"), MainWindow::tr("Go Up"));
    connect(leftGoUp, &QAction::triggered, this, [this]() { onGoUp(leftPanel); });

    QAction *leftOpenDir = leftMenu->addAction(QIcon(":/icons/folder_open"), MainWindow::tr("Open directory..."));
    connect(leftOpenDir, &QAction::triggered, this, [this]() { onOpenDirectory(leftPanel); });

    leftMenu->addSeparator();

    QMenu *leftSortMenu = leftMenu->addMenu(MainWindow::tr("Sorting"));
    QActionGroup *leftSortGroup = new QActionGroup(this);
    leftSortGroup->setExclusive(true);

    leftMenuActions.sortByName = leftSortMenu->addAction(QIcon(":/icons/sort"), MainWindow::tr("Sort by name"));
    leftMenuActions.sortByName->setCheckable(true);
    leftMenuActions.sortByName->setActionGroup(leftSortGroup);
    connect(leftMenuActions.sortByName, &QAction::triggered, this, [this]() { onSetSorting(leftPanel, HostModel::ByName); });

    leftMenuActions.sortBySize = leftSortMenu->addAction(QIcon(":/icons/sort"), MainWindow::tr("Sort by size"));
    leftMenuActions.sortBySize->setCheckable(true);
    leftMenuActions.sortBySize->setActionGroup(leftSortGroup);
    connect(leftMenuActions.sortBySize, &QAction::triggered, this, [this]() { onSetSorting(leftPanel, HostModel::BySize); });

    leftMenuActions.noSort = leftSortMenu->addAction(QIcon(":/icons/sort"), MainWindow::tr("No sorting"));
    leftMenuActions.noSort->setCheckable(true);
    leftMenuActions.noSort->setActionGroup(leftSortGroup);
    connect(leftMenuActions.noSort, &QAction::triggered, this, [this]() { onSetSorting(leftPanel, HostModel::NoOrder); });

    leftMenu->addSeparator();

    leftMenuActions.showDeleted = leftMenu->addAction(QIcon(":/icons/deleted"), MainWindow::tr("Show deleted"));
    leftMenuActions.showDeleted->setCheckable(true);
    leftMenuActions.showDeleted->setChecked(leftPanel->getShowDeleted());
    connect(leftMenuActions.showDeleted, &QAction::triggered, this, [this](bool checked) {
        onSetShowDeleted(leftPanel, checked);
    });

    // === IMAGE MENU === (NEW)
    QMenu *imageMenu = menuBar()->addMenu(MainWindow::tr("Image"));

    actImageInfo = imageMenu->addAction(QIcon(":/icons/info"), MainWindow::tr("Information..."));
    connect(actImageInfo, &QAction::triggered, this, &MainWindow::onImageInfo);                  

    imageMenu->addSeparator();

    actImageSave = imageMenu->addAction(QIcon(":/icons/icon"), MainWindow::tr("Save"));
    actImageSave->setShortcut(QKeySequence::Save);  // Ctrl+S
    connect(actImageSave, &QAction::triggered, this, &MainWindow::onImageSave);                  
                                                                 
    actImageSaveAs = imageMenu->addAction(QIcon(":/icons/convert"), MainWindow::tr("Save as..."));
    actImageSaveAs->setShortcut(QKeySequence::SaveAs);  // Ctrl+Shift+S                          
    connect(actImageSaveAs, &QAction::triggered, this, &MainWindow::onImageSaveAs);

    // === FILES MENU ===
    QMenu *filesMenu = menuBar()->addMenu(MainWindow::tr("Files"));

    menuViewAction = filesMenu->addAction(QIcon(":/icons/info"), MainWindow::tr("F3 View"));
    connect(menuViewAction, &QAction::triggered, this, &MainWindow::onView);

    menuEditAction = filesMenu->addAction(QIcon(":/icons/view"), MainWindow::tr("F4 Edit"));
    connect(menuEditAction, &QAction::triggered, this, &MainWindow::onEdit);

    QAction *copyAction = filesMenu->addAction(QIcon(":/icons/text_copy"), MainWindow::tr("F5 Copy"));
    connect(copyAction, &QAction::triggered, this, &MainWindow::onCopy);

    QAction *renameAction = filesMenu->addAction(QIcon(":/icons/edit"), MainWindow::tr("F6 Rename"));
    connect(renameAction, &QAction::triggered, this, &MainWindow::onRename);

    QAction *mkdirAction = filesMenu->addAction(QIcon(":/icons/new_dir"), MainWindow::tr("F7 Make dir"));
    connect(mkdirAction, &QAction::triggered, this, &MainWindow::onMkdir);

    QAction *deleteAction = filesMenu->addAction(QIcon(":/icons/delete"), MainWindow::tr("F8 Delete"));
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDelete);

    // === OPTIONS MENU ===
    QMenu *optionsMenu = menuBar()->addMenu(MainWindow::tr("Options"));

    // Language submenu (reuse add_languages logic)
    QAction *langAction = optionsMenu->addAction(QIcon(":/icons/lang"), MainWindow::tr("Language"));
    QMenu *langSubmenu = new QMenu(MainWindow::tr("Languages"), this);

    QAction *langRu = langSubmenu->addAction(QIcon(":/icons/ru"), MainWindow::tr("Русский"));
    connect(langRu, &QAction::triggered, this, [this]() { switch_language("ru_ru", false); });

    QAction *langEn = langSubmenu->addAction(QIcon(":/icons/en"), MainWindow::tr("English"));
    connect(langEn, &QAction::triggered, this, [this]() { switch_language("en_us", false); });

    langAction->setMenu(langSubmenu);

    QAction *aboutAction = optionsMenu->addAction(QIcon(":/icons/help"), MainWindow::tr("About..."));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    // === RIGHT PANEL MENU ===
    QMenu *rightMenu = menuBar()->addMenu(MainWindow::tr("Right panel"));

    QAction *rightGoUp = rightMenu->addAction(QIcon(":/icons/up"), MainWindow::tr("Go Up"));
    connect(rightGoUp, &QAction::triggered, this, [this]() { onGoUp(rightPanel); });

    QAction *rightOpenDir = rightMenu->addAction(QIcon(":/icons/folder_open"), MainWindow::tr("Open directory..."));
    connect(rightOpenDir, &QAction::triggered, this, [this]() { onOpenDirectory(rightPanel); });

    rightMenu->addSeparator();

    QMenu *rightSortMenu = rightMenu->addMenu(MainWindow::tr("Sorting"));
    QActionGroup *rightSortGroup = new QActionGroup(this);
    rightSortGroup->setExclusive(true);

    rightMenuActions.sortByName = rightSortMenu->addAction(QIcon(":/icons/sort"), MainWindow::tr("Sort by name"));
    rightMenuActions.sortByName->setCheckable(true);
    rightMenuActions.sortByName->setActionGroup(rightSortGroup);
    connect(rightMenuActions.sortByName, &QAction::triggered, this, [this]() { onSetSorting(rightPanel, HostModel::ByName); });

    rightMenuActions.sortBySize = rightSortMenu->addAction(QIcon(":/icons/sort"), MainWindow::tr("Sort by size"));
    rightMenuActions.sortBySize->setCheckable(true);
    rightMenuActions.sortBySize->setActionGroup(rightSortGroup);
    connect(rightMenuActions.sortBySize, &QAction::triggered, this, [this]() { onSetSorting(rightPanel, HostModel::BySize); });

    rightMenuActions.noSort = rightSortMenu->addAction(QIcon(":/icons/sort"), MainWindow::tr("No sorting"));
    rightMenuActions.noSort->setCheckable(true);
    rightMenuActions.noSort->setActionGroup(rightSortGroup);
    connect(rightMenuActions.noSort, &QAction::triggered, this, [this]() { onSetSorting(rightPanel, HostModel::NoOrder); });

    rightMenu->addSeparator();

    rightMenuActions.showDeleted = rightMenu->addAction(QIcon(":/icons/deleted"), MainWindow::tr("Show deleted"));
    rightMenuActions.showDeleted->setCheckable(true);
    rightMenuActions.showDeleted->setChecked(rightPanel->getShowDeleted());
    connect(rightMenuActions.showDeleted, &QAction::triggered, this, [this](bool checked) {
        onSetShowDeleted(rightPanel, checked);
    });

    // Initialize sorting menu states
    updateSortingMenu(leftPanel);
    updateSortingMenu(rightPanel);
}


void MainWindow::setActivePanel(FilePanel* panel) {
    if (!panel) return;
    activePanel = panel;
    leftPanel->setActive(leftPanel == activePanel);
    rightPanel->setActive(rightPanel == activePanel);

    if (activePanel->tableSelectionModel()) {
        // Status bar is upadted from the active panel
        connect(activePanel->tableSelectionModel(), &QItemSelectionModel::selectionChanged,
                this, &MainWindow::updateStatusBarInfo);
        // Bottom panel buttons are upadted from the active panel
        connect(activePanel->tableSelectionModel(), &QItemSelectionModel::selectionChanged,
                this, &MainWindow::updateViewButtonState);
    }
    // Force updating after changing panels
    updateStatusBarInfo();
    updateViewButtonState();
}

void MainWindow::updateStatusBarInfo() {
    if (!activePanel) {
        statusLabel->setText(MainWindow::tr("No active panel"));
        return;
    }
    const QString dir = activePanel->currentDir();
    const int selCount = activePanel->selectedPaths().size();
    statusLabel->setText(QString(MainWindow::tr("Active panel: %1 | Selected: %2")).arg(dir).arg(selCount));
}

void MainWindow::updateViewButtonState() {
    if (!activePanel) return;

    // Get the current panel mode
    panelMode mode = activePanel->getMode();

    // Update button and menu action text based on mode
    if (mode == panelMode::Host) {
        // Host mode: F3 Info, F4 Open
        actView->setText(MainWindow::tr("F3 Info"));
        actEdit->setText(MainWindow::tr("F4 Open"));

        if (menuViewAction) menuViewAction->setText(MainWindow::tr("F3 Info"));
        if (menuEditAction) menuEditAction->setText(MainWindow::tr("F4 Open"));

        qDebug() << activePanel->currentFilePath();
        // actView->setEnabled(activePanel->currentFilePath().
    } else {
        // Image mode: F3 View, F4 Meta
        actView->setText(MainWindow::tr("F3 View"));
        actEdit->setText(MainWindow::tr("F4 Meta"));

        if (menuViewAction) menuViewAction->setText(MainWindow::tr("F3 View"));
        if (menuEditAction) menuEditAction->setText(MainWindow::tr("F4 Meta"));
    }
}

void MainWindow::onView() {
    if (!activePanel) return;
    activePanel->onView();
}

void MainWindow::onEdit() {
    if (!activePanel) return;
    activePanel->onEdit();
}

void MainWindow::onCopy() {
    doCopy(true);
}

void MainWindow::onRename()
{
    if (!activePanel) return;
    activePanel->onRename();
}

void MainWindow::doCopy(bool copy) {
    if (!activePanel || !otherPanel()->allowPutFiles()) return;

    if (otherPanel()->getFileSystem()->getFS() == dsk_tools::FS::Host && activePanel->getFileSystem()->getFS() != dsk_tools::FS::Host) {
        // Extracting files to host. Need to ask for format
        std::vector<std::string> formats = activePanel->getFileSystem()->get_save_file_formats();

        QMap<QString, QString> fil_map;

        foreach (const std::string & v, formats) {
            QJsonObject fil = file_formats[QString::fromStdString(v)].toObject();
            fil_map[QString::fromStdString(v)] = QCoreApplication::translate("config", fil["name"].toString().toUtf8().constData());
        }


        // Restore last used format from settings
        const QString fs_string = QString::number(static_cast<int>(activePanel->getFileSystem()->getFS()));
        const QString defaultFormat = settings->value("export/extract_format_"+fs_string, "").toString();

        // Show format selection dialog
        auto *dialog = new FormatDialog(this, fil_map,
                                                defaultFormat,
                                                MainWindow::tr("Selected files: %1").arg(activePanel->selectedCount()),
                                                MainWindow::tr("Choose output file format:"),
                                                MainWindow::tr("Choose the format"));
        dialog->setWindowTitle(copy?MainWindow::tr("Copying files"):MainWindow::tr("Moving files"));

        if (dialog->exec() == QDialog::Accepted) {
            const QString selectedFormat = dialog->getSelectedFormat();

            // Save selected format to settings for next time
            settings->setValue("export/extract_format_"+fs_string, selectedFormat);

            dsk_tools::Files files = activePanel->getSelectedFiles();
            otherPanel()->putFiles(activePanel->getFileSystem(), files, selectedFormat, copy);

            qDebug() << "User selected format:" << selectedFormat;
        }

        delete dialog;

    } else {
        // Moving files between FSs in other cases
        QMessageBox::StandardButton reply;
        if (copy) {
            reply = QMessageBox::question(this,
                                        MainWindow::tr("Copying files"),
                                        MainWindow::tr("Copy %1 files?").arg(activePanel->selectedCount()),
                                        QMessageBox::Yes|QMessageBox::No
                    );
        } else {
            reply = QMessageBox::question(this,
                                        MainWindow::tr("Moving files"),
                                        MainWindow::tr("Move %1 files?").arg(activePanel->selectedCount()),
                                        QMessageBox::Yes|QMessageBox::No
                    );
        }
        if (reply == QMessageBox::Yes) {
            dsk_tools::Files files = activePanel->getSelectedFiles();
            otherPanel()->putFiles(activePanel->getFileSystem(), files, "", copy);
        }
    }
}

void MainWindow::onMkdir() {
    if (!activePanel) return;
    activePanel->onMkDir();
}

void MainWindow::onDelete() {
    if (!activePanel) return;
    QMessageBox::StandardButton reply = QMessageBox::question(this,
                                MainWindow::tr("Deleting files"),
                                MainWindow::tr("Delete %1 files?").arg(activePanel->selectedCount()),
                                QMessageBox::Yes|QMessageBox::No
            );
    if (reply == QMessageBox::Yes) {
        activePanel->deleteFiles(activePanel->getSelectedFiles());
    }
}

void MainWindow::onExit() {
    close();
}

void MainWindow::onAbout() {
    QDialog * about = new QDialog(this);

    Ui_About aboutUi;
    aboutUi.setupUi(about);

    aboutUi.info_label->setText(
        aboutUi.info_label->text()
            .replace("{$PROJECT_VERSION}", PROJECT_VERSION)
            .replace("{$BUILD_ARCHITECTURE}", QSysInfo::buildCpuArchitecture())
            .replace("{$OS}", QSysInfo::productType())
            .replace("{$OS_VERSION}", QSysInfo::productVersion())
            .replace("{$CPU_ARCHITECTURE}", QSysInfo::currentCpuArchitecture())
        );

    about->exec();
}

// Universal panel methods
void MainWindow::onGoUp(FilePanel* panel) {
    if (panel) {
        panel->onGoUp();
    }
}

void MainWindow::onOpenDirectory(FilePanel* panel) {
    if (panel) {
        panel->chooseDirectory();
    }
}

void MainWindow::onSetSorting(FilePanel* panel, HostModel::SortOrder order) {
    if (panel) {
        panel->setSortOrder(order);
    }
}

void MainWindow::onSetShowDeleted(FilePanel* panel, bool show) {
    if (panel) {
        panel->setShowDeleted(show);
    }
}

void MainWindow::updateSortingMenu(FilePanel* panel) {
    if (!panel) return;

    int sortOrder = panel->getSortOrder();
    PanelMenuActions* actions = nullptr;

    // Determine which panel's menu to update
    if (panel == leftPanel) {
        actions = &leftMenuActions;
    } else if (panel == rightPanel) {
        actions = &rightMenuActions;
    }

    if (!actions) return;

    // Update checkmarks based on current sort order
    switch (sortOrder) {
        case 0: // HostModel::ByName
            if (actions->sortByName) actions->sortByName->setChecked(true);
            break;
        case 1: // HostModel::BySize
            if (actions->sortBySize) actions->sortBySize->setChecked(true);
            break;
        case 2: // HostModel::NoOrder
            if (actions->noSort) actions->noSort->setChecked(true);
            break;
        default:
            break;
    }
}

FilePanel* MainWindow::otherPanel() const {
    return (activePanel == leftPanel) ? rightPanel : leftPanel;
}

void MainWindow::onImageInfo()
{
    if (!activePanel) return;
    activePanel->showImageInfo();
}

void MainWindow::onImageSave()
{
    if (!activePanel) return;
    activePanel->saveImage();
}

void MainWindow::onImageSaveAs()
{
    if (!activePanel) return;
    activePanel->saveImageAs();
}

void MainWindow::updateImageMenuState()
{
    if (!activePanel) {
        actImageSave->setEnabled(false);
        actImageSaveAs->setEnabled(false);
        actSave->setEnabled(false);
        return;
    }

    bool canSave = (activePanel->getMode() == panelMode::Image) &&
                   (activePanel->getFileSystem() != nullptr) &&
                   (activePanel->getFileSystem()->get_changed());

    actImageSave->setEnabled(canSave);
    actImageSaveAs->setEnabled(canSave);
    actSave->setEnabled(canSave);
}
