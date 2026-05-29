// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Main window

#include <fstream>
#include <set>

#include <QTranslator>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSplitter>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QStatusBar>
#include <QDebug>
#include <QActionGroup>
#include <QMenuBar>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QFontDialog>

#include "mainwindow.h"
#include "mainutils.h"
#include "convertdialog.h"
#include "fileparamdialog.h"
#include "formatdialog.h"
#include "FileOperations.h"

#include "./ui_aboutdlg.h"
#include "./ui_fileinfodialog.h"

#include "dsk_tools/dsk_tools.h"
#include "fs_host.h"

#include "globals.h"

#define INI_FILE_NAME "/dsk_com.ini"

// Global settings reference and callback for dsk_tools library
static QSettings* g_mainwindow_settings = nullptr;

static bool check_use_recycle_bin() {
    if (g_mainwindow_settings) {
        return g_mainwindow_settings->value("files/use_recycle_bin", true).toBool();
    }
    return true;  // Default to safe behavior
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
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

    settings = dsk_tools::make_unique<QSettings>(ini_file, QSettings::IniFormat);

    // Register callback for dsk_tools library to check recycle bin setting
    g_mainwindow_settings = settings.get();
    dsk_tools::fsHost::use_recycle_bin = check_use_recycle_bin;

    QString ini_lang = settings->value("interface/language", "").toString();
    if (ini_lang.length() == 0) {
        // Auto-detect: pick the first preferred UI language we actually ship a translation for.
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = QLocale(locale).name().toLower();
            if (QFile::exists(":/i18n/" + baseName + ".qm")) {
                switch_language(baseName, true);
                settings->setValue("interface/language", baseName);
                break;
            }
        }
    } else {
        switch_language(ini_lang, true);
    }

    const QByteArray savedGeometry = settings->value("interface/main_window_geometry").toByteArray();
    if (savedGeometry.isEmpty() || !restoreGeometry(savedGeometry)) {
        resize(1000, 600);
    }

    // QList<int> sizes;
    // int halfWidth = this->width() / 2;
    // sizes << halfWidth << halfWidth;

    setWindowTitle(windowTitle() + " " + PROJECT_VERSION);

    // QFont font("PxPlus IBM VGA9", 12, 400);
    // QFont font("Consolas", 10, 400);
    // QFont font("Iosevka Fixed", 10, 400);
    // font.setStretch(QFont::Expanded);

    load_config();

// New interface
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    leftPanel = new FilePanel(this, settings.get(), "left", file_formats, file_types, file_systems, m_diskdefs);
    rightPanel = new FilePanel(this, settings.get(), "right", file_formats, file_types, file_systems, m_diskdefs);

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
    statusLabel->setText(tr("Ready"));
    statusBar()->addWidget(statusLabel);

    setActivePanel(leftPanel);
    updateViewButtonState();

    QTimer::singleShot(0, this, [this]() { leftPanel->focusList(); });

    // Initialize new menu system
    initializeMainMenu();

    // Connect panel sorting signals to update menu checkmarks
    connect(leftPanel, &FilePanel::sortOrderChanged, this, [this]() {
        updateSortingMenu(leftPanel);
    });
    connect(rightPanel, &FilePanel::sortOrderChanged, this, [this]() {
        updateSortingMenu(rightPanel);
    });

    // Connect panel mode change signals to update button states.
    // setMode replaces the model via tableView->setModel(), which destroys the
    // old QItemSelectionModel and creates a new one — breaking our
    // selectionChanged/currentChanged connections. When the active panel's mode
    // changes, re-run setActivePanel to reconnect to the new selection model.
    connect(leftPanel, &FilePanel::panelModeChanged, this, [this]() {
        if (activePanel == leftPanel) {
            setActivePanel(leftPanel);
        } else {
            updateViewButtonState();
        }
    });
    connect(rightPanel, &FilePanel::panelModeChanged, this, [this]() {
        if (activePanel == rightPanel) {
            setActivePanel(rightPanel);
        } else {
            updateViewButtonState();
        }
    });
}

void MainWindow::switch_language(const QString & lang, bool init)
{
    // Remove old translators before installing new ones
    if (!init) {
        qApp->removeTranslator(&translator);
        qApp->removeTranslator(&qtTranslator);
    }

    if (translator.load(":/i18n/" + lang)) {
        qApp->installTranslator(&translator);

        const QString t = QString(":/i18n/qtbase_%1.qm").arg(lang.split("_")[0]);
        if (qtTranslator.load(t)) {
            qApp->installTranslator(&qtTranslator);
        }
        if (!init) {
            settings->setValue("interface/language", lang);
            // Qt automatically posts QEvent::LanguageChange after installTranslator()
        }
    } else {
        QMessageBox::warning(this, MainWindow::tr("Error"), MainWindow::tr("Failed to load language file for: ") + lang);
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        setWindowTitle(tr("DISK Commander") + " " + PROJECT_VERSION);

        updateActionTexts();  // Unified action text updates
        initializeMainMenu(); // Rebuilds menus (updateSortingMenu at end preserves states)
        updateStatusBarInfo(); // Updates status bar with tr()

        if (leftPanel) leftPanel->retranslateUi();
        if (rightPanel) rightPanel->retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Check both panels for unsaved changes
    bool hasUnsavedLeft = (leftPanel && leftPanel->getMode() == panelMode::Image &&
                           leftPanel->getFileSystem() &&
                           leftPanel->getFileSystem()->get_changed());

    bool hasUnsavedRight = (rightPanel && rightPanel->getMode() == panelMode::Image &&
                            rightPanel->getFileSystem() &&
                            rightPanel->getFileSystem()->get_changed());

    if (hasUnsavedLeft || hasUnsavedRight) {
        QString message;
        if (hasUnsavedLeft && hasUnsavedRight) {
            message = tr("Both panels have unsaved disk image changes. Close anyway?");
        } else {
            message = tr("One panel has unsaved disk image changes. Close anyway?");
        }

        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No  // Default to No (safe)
        );

        if (reply == QMessageBox::No) {
            event->ignore();  // Cancel close
            return;
        }
    }

    settings->setValue("interface/main_window_geometry", saveGeometry());

    event->accept();  // Proceed with close
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

    // Loading CP/M diskdefs
    QFile ddf(":/files/diskdefs");
    if (!ddf.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(0, MainWindow::tr("Error"), MainWindow::tr("Error reading diskdefs file"));
        return;
    }

    QByteArray ddf_contents = ddf.readAll();
    ddf.close();

    m_diskdefs = dsk_tools::parse_diskdefs(ddf_contents.toStdString());

}

MainWindow::~MainWindow()
{
    // delete settings;
}

// New interface elements ---------------------------

void MainWindow::createActions() {
    // Bottom toolbar actions - created here and used in createBottomPanel
    actHelp   = new QAction(this);
    actSave   = new QAction(this);
    actView   = new QAction(this);
    actEdit   = new QAction(this);
    actCopy   = new QAction(this);
    actRename = new QAction(this);
    actMkdir  = new QAction(this);
    actDelete = new QAction(this);
    actRestore= new QAction(this);
    actExit   = new QAction(this);

    connect(actHelp,   &QAction::triggered, this, &MainWindow::onHotkeys);
    connect(actSave,   &QAction::triggered, this, &MainWindow::onImageSave);
    connect(actView,   &QAction::triggered, this, &MainWindow::onView);
    connect(actEdit,   &QAction::triggered, this, &MainWindow::onEdit);
    connect(actCopy,   &QAction::triggered, this, &MainWindow::onCopy);
    connect(actRename, &QAction::triggered, this, &MainWindow::onRename);
    connect(actMkdir,  &QAction::triggered, this, &MainWindow::onMkdir);
    connect(actDelete, &QAction::triggered, this, &MainWindow::onDelete);
    connect(actRestore, &QAction::triggered, this, &MainWindow::onRestore);
    connect(actExit,   &QAction::triggered, this, &MainWindow::onExit);

    addActions({actHelp, actSave, actView, actEdit, actCopy, actRename, actMkdir, actDelete, actRestore, actExit});

    // Set initial text (unified method)
    updateActionTexts();
}

void MainWindow::updateActionTexts() {
    // All action text in ONE place - called both on init and retranslation
    actHelp->setText(tr("F1 Hotkeys"));
    actSave->setText(tr("F2 Save"));
    actView->setText(tr("F3 View"));
    actEdit->setText(tr("F4 Open"));
    actCopy->setText(tr("F5 Copy"));
    actRename->setText(tr("F6 Rename"));
    actMkdir->setText(tr("F7 MkDir"));
    actDelete->setText(tr("F8 Delete"));
    actRestore->setText(tr("F9 Restore"));
    actExit->setText(tr("F10 Exit"));

    // Update dynamic F3/F4 based on current panel mode
    if (activePanel) {
        updateViewButtonState();
    }
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
        btn->setObjectName(QStringLiteral("bottomBtn"));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setDefaultAction(act);
        btn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        btn->setMinimumSize(60, 0);
        layout->addWidget(btn);
        buttons << btn;
    };

    makeButton(actHelp);   // F1
    makeButton(actSave);   // F2
    makeButton(actView);   // F3
    makeButton(actEdit);   // F4
    makeButton(actCopy);   // F5
    makeButton(actRename); // F6
    makeButton(actMkdir);  // F7
    makeButton(actDelete); // F8
    makeButton(actRestore); // F9
    makeButton(actExit);   // F10

    // buttons have equal widths
    for (int i = 0; i < buttons.size(); ++i)
        layout->setStretch(i, 1);

    // Expand panel to the window
    layout->setAlignment(Qt::AlignJustify);

    return panel;
}

void MainWindow::createPanelMenu(FilePanel* panel, PanelMenuActions& actions, const QString& panelName, int fkey) {
    QMenu *panelMenu = menuBar()->addMenu(panelName);

    QAction *goUp = panelMenu->addAction(QIcon(":/icons/up"), MainWindow::tr("Go Up"));
    connect(goUp, &QAction::triggered, this, [this, panel]() { onGoUp(panel); });

    QAction *openDir = panelMenu->addAction(QIcon(":/icons/folder_open"), MainWindow::tr("Open directory..."));
    openDir->setShortcut(QKeySequence(Qt::ALT | fkey));
    connect(openDir, &QAction::triggered, this, [this, panel]() { onOpenDirectory(panel); });

    QAction *dirHistory = panelMenu->addAction(QIcon(":/icons/folder_open"), MainWindow::tr("Directory history"));
    dirHistory->setShortcut(QKeySequence(Qt::CTRL | fkey));
    connect(dirHistory, &QAction::triggered, this, [panel]() {
        if (panel) panel->showDirectoryHistory();
    });

    panelMenu->addSeparator();

    QMenu *sortMenu = panelMenu->addMenu(QIcon(":/icons/sort"), MainWindow::tr("Sorting"));
    QActionGroup *sortGroup = new QActionGroup(this);
    sortGroup->setExclusive(true);

    actions.sortByName = sortMenu->addAction(MainWindow::tr("Sort by name"));
    actions.sortByName->setCheckable(true);
    actions.sortByName->setActionGroup(sortGroup);
    connect(actions.sortByName, &QAction::triggered, this, [this, panel]() { onSetSorting(panel, HostModel::ByName); });

    actions.sortBySize = sortMenu->addAction(MainWindow::tr("Sort by size"));
    actions.sortBySize->setCheckable(true);
    actions.sortBySize->setActionGroup(sortGroup);
    connect(actions.sortBySize, &QAction::triggered, this, [this, panel]() { onSetSorting(panel, HostModel::BySize); });

    actions.noSort = sortMenu->addAction(MainWindow::tr("No sorting"));
    actions.noSort->setCheckable(true);
    actions.noSort->setActionGroup(sortGroup);
    connect(actions.noSort, &QAction::triggered, this, [this, panel]() { onSetSorting(panel, HostModel::NoOrder); });

    panelMenu->addSeparator();

    actions.showDeleted = panelMenu->addAction(QIcon(":/icons/deleted"), MainWindow::tr("Show deleted"));
    actions.showDeleted->setCheckable(true);
    actions.showDeleted->setChecked(panel->getShowDeleted());
    connect(actions.showDeleted, &QAction::triggered, this, [this, panel](bool checked) {
        onSetShowDeleted(panel, checked);
    });
}

void MainWindow::initializeMainMenu() {
    // Clear existing menu
    menuBar()->clear();

    // === LEFT PANEL MENU ===
    createPanelMenu(leftPanel, leftMenuActions, MainWindow::tr("Left panel"), Qt::Key_F1);

    // === IMAGE MENU === (NEW)
    QMenu *imageMenu = menuBar()->addMenu(MainWindow::tr("Image"));

    actImageSave = imageMenu->addAction(QIcon(":/icons/icon"), MainWindow::tr("Save"));
    actImageSave->setShortcut(QKeySequence(Qt::Key_F2));
    connect(actImageSave, &QAction::triggered, this, &MainWindow::onImageSave);

    actImageSaveAs = imageMenu->addAction(QIcon(":/icons/convert"), MainWindow::tr("Save as..."));
    actImageSaveAs->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F2));
    connect(actImageSaveAs, &QAction::triggered, this, &MainWindow::onImageSaveAs);

    imageMenu->addSeparator();

    actImageInfo = imageMenu->addAction(QIcon(":/icons/info"), MainWindow::tr("Container Info..."));
    connect(actImageInfo, &QAction::triggered, this, &MainWindow::onImageInfo);

    actFSInfo = imageMenu->addAction(QIcon(":/icons/fs_info"), MainWindow::tr("Filesystem Info..."));
    actFSInfo->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F3));
    connect(actFSInfo, &QAction::triggered, this, &MainWindow::onFSInfo);

    imageMenu->addSeparator();

    actImageOpen = imageMenu->addAction(QIcon(":/icons/open"), MainWindow::tr("Open"));
    connect(actImageOpen, &QAction::triggered, this, &MainWindow::onEdit);

    actImageClose = imageMenu->addAction(QIcon(":/icons/up"), MainWindow::tr("Close"));
    actImageClose->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(actImageClose, &QAction::triggered, this, &MainWindow::onImageClose);

    actImageReload = imageMenu->addAction(QIcon(":/icons/revert_"), MainWindow::tr("Reload"));
    actImageReload->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(actImageReload, &QAction::triggered, this, &MainWindow::onImageReload);

    // === FILES MENU ===
    QMenu *filesMenu = menuBar()->addMenu(MainWindow::tr("Files"));

    menuViewAction = filesMenu->addAction(QIcon(":/icons/image_view"), MainWindow::tr("View"));
    connect(menuViewAction, &QAction::triggered, this, &MainWindow::onView);

    menuFileInfoAction = filesMenu->addAction(QIcon(":/icons/info"), MainWindow::tr("File Info"));
    menuFileInfoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F3));
    connect(menuFileInfoAction, &QAction::triggered, this, &MainWindow::onFileInfo);

    menuEditAction = filesMenu->addAction(QIcon(":/icons/view"), MainWindow::tr("Edit Metadata"));
    connect(menuEditAction, &QAction::triggered, this, &MainWindow::onEdit);

    menuCopyAction = filesMenu->addAction(QIcon(":/icons/text_copy"), MainWindow::tr("Copy"));
    menuCopyAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(menuCopyAction, &QAction::triggered, this, &MainWindow::onCopy);

    menuRenameAction = filesMenu->addAction(QIcon(":/icons/rename"), MainWindow::tr("Rename"));
    menuRenameAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(menuRenameAction, &QAction::triggered, this, &MainWindow::onRename);

    menuMkdirAction = filesMenu->addAction(QIcon(":/icons/new_dir"), MainWindow::tr("F7 Make dir"));
    menuMkdirAction->setShortcut(QKeySequence(Qt::Key_F7));
    connect(menuMkdirAction, &QAction::triggered, this, &MainWindow::onMkdir);

    menuDeleteAction = filesMenu->addAction(QIcon(":/icons/delete"), MainWindow::tr("F8 Delete"));
    menuDeleteAction->setShortcuts({QKeySequence(Qt::Key_F8), QKeySequence(Qt::Key_Delete)});
    connect(menuDeleteAction, &QAction::triggered, this, &MainWindow::onDelete);

    menuRestoreAction = filesMenu->addAction(QIcon(":/icons/restore"), MainWindow::tr("F9 Restore"));
    menuRestoreAction->setShortcut(QKeySequence(Qt::Key_F9));
    connect(menuRestoreAction, &QAction::triggered, this, &MainWindow::onRestore);

    // === OPTIONS MENU ===
    QMenu *optionsMenu = menuBar()->addMenu(MainWindow::tr("Options"));

    // Language submenu
    QAction *langAction = optionsMenu->addAction(QIcon(":/icons/lang"), MainWindow::tr("Language"));
    QMenu *langSubmenu = new QMenu(MainWindow::tr("Languages"), this);

    // Read current language to set checkmarks
    QString currentLang = settings->value("interface/language", "en_us").toString();

    QAction *langRu = langSubmenu->addAction(QIcon(":/icons/ru"), MainWindow::tr("Русский"));
    langRu->setCheckable(true);
    langRu->setChecked(currentLang == "ru_ru");
    connect(langRu, &QAction::triggered, this, [this]() { switch_language("ru_ru", false); });

    QAction *langEn = langSubmenu->addAction(QIcon(":/icons/en"), MainWindow::tr("English"));
    langEn->setCheckable(true);
    langEn->setChecked(currentLang == "en_us");
    connect(langEn, &QAction::triggered, this, [this]() { switch_language("en_us", false); });

    langAction->setMenu(langSubmenu);

    optionsMenu->addSeparator();

    // Recycle bin / Trash option
    optUseRecycleBin = optionsMenu->addAction(QIcon(":/icons/deleted"), MainWindow::tr("Use Recycle Bin for host"));
    optUseRecycleBin->setCheckable(true);

    // Read setting - default to true (safer)
    bool useRecycleBin = settings->value("files/use_recycle_bin", true).toBool();
    optUseRecycleBin->setChecked(useRecycleBin);

    connect(optUseRecycleBin, &QAction::triggered, this, [this](bool checked) {
        settings->setValue("files/use_recycle_bin", checked);
    });

    // Make backups on save option
    optMakeBackups = optionsMenu->addAction(QIcon(":/icons/backup"), MainWindow::tr("Make backups on save"));
    optMakeBackups->setCheckable(true);

    // Read setting - default to true (safer)
    bool makeBackups = settings->value("files/make_backups_on_save", true).toBool();
    optMakeBackups->setChecked(makeBackups);

    connect(optMakeBackups, &QAction::triggered, this, [this](bool checked) {
        settings->setValue("files/make_backups_on_save", checked);
    });

    // Watch for external image changes option
    optWatchImageChanges = optionsMenu->addAction(QIcon(":/icons/watch"), MainWindow::tr("Watch for external image changes"));
    optWatchImageChanges->setCheckable(true);

    const bool watchEnabled = settings->value("files/watch_image_changes", true).toBool();
    optWatchImageChanges->setChecked(watchEnabled);

    connect(optWatchImageChanges, &QAction::triggered, this, [this](bool checked) {
        settings->setValue("files/watch_image_changes", checked);
        if (leftPanel) leftPanel->setImageWatchEnabled(checked);
        if (rightPanel) rightPanel->setImageWatchEnabled(checked);
    });

    optionsMenu->addSeparator();

    // Font submenu for viewer (dumps and text view)
    QAction *fontAction = optionsMenu->addAction(QIcon(":/icons/font"), MainWindow::tr("Viewer font"));
    QMenu *fontSubmenu = new QMenu(MainWindow::tr("Viewer font"), this);

    optFontChoose = fontSubmenu->addAction(MainWindow::tr("Choose..."));
    connect(optFontChoose, &QAction::triggered, this, &MainWindow::onChooseFont);

    optFontDefault = fontSubmenu->addAction(MainWindow::tr("Default"));
    connect(optFontDefault, &QAction::triggered, this, &MainWindow::onDefaultFont);

    fontAction->setMenu(fontSubmenu);

    optionsMenu->addSeparator();

    QAction *hotkeysAction = optionsMenu->addAction(QIcon(":/icons/hotkeys"), MainWindow::tr("Hotkeys..."));
    hotkeysAction->setShortcut(QKeySequence(Qt::Key_F1));
    connect(hotkeysAction, &QAction::triggered, this, &MainWindow::onHotkeys);

    QAction *aboutAction = optionsMenu->addAction(QIcon(":/icons/help"), MainWindow::tr("About..."));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    // === RIGHT PANEL MENU ===
    createPanelMenu(rightPanel, rightMenuActions, MainWindow::tr("Right panel"), Qt::Key_F2);

    // Initialize sorting menu states
    updateSortingMenu(leftPanel);
    updateSortingMenu(rightPanel);
}


void MainWindow::setActivePanel(FilePanel* panel) {
    if (!panel) return;

    // Disconnect from previous panel's signals before switching
    if (activePanel && activePanel->tableSelectionModel()) {
        disconnect(activePanel->tableSelectionModel(), nullptr, this, nullptr);
    }

    activePanel = panel;
    leftPanel->setActive(leftPanel == activePanel);
    rightPanel->setActive(rightPanel == activePanel);

    if (activePanel->tableSelectionModel()) {
        // Status bar is updated from the active panel
        connect(activePanel->tableSelectionModel(), &QItemSelectionModel::selectionChanged,
                this, &MainWindow::updateStatusBarInfo);
        // Bottom panel buttons are updated from the active panel
        connect(activePanel->tableSelectionModel(), &QItemSelectionModel::selectionChanged,
                this, &MainWindow::updateViewButtonState);
        connect(activePanel->tableSelectionModel(), &QItemSelectionModel::currentChanged,
                this, &MainWindow::updateViewButtonState);

    }
    // Force updating after changing panels
    updateStatusBarInfo();
    updateViewButtonState();
}

void MainWindow::updateStatusBarInfo() {
    if (!activePanel) {
        statusLabel->clear();
        return;
    }

    int selCount = 0;
    qint64 selSize = 0;
    if (activePanel->getExplicitSelectionStats(selCount, selSize)) {
        statusLabel->setText(tr("Selected files: %1, total size: %2")
                                 .arg(selCount)
                                 .arg(HostModel::formatSize(selSize)));
        return;
    }

    if (activePanel->getMode() == panelMode::Host) {
        statusLabel->clear();
        return;
    }

    // Image mode, no selection — show filesystem stats.
    dsk_tools::fileSystem * fs = activePanel->getFileSystem();
    if (!fs) {
        statusLabel->clear();
        return;
    }

    const auto stats = fs->get_stats();
    const auto & v = stats.int_values;

    QStringList parts;
    const auto add = [&](const char * key, const QString & label) {
        const auto it = v.find(key);
        if (it != v.end())
            parts << QString("%1: %2").arg(label, HostModel::formatSize(it->second));
    };
    add("image_size",     tr("Image size"));
    add("total_space",    tr("Total space"));
    add("occupied_space", tr("Occupied"));
    add("free_space",     tr("Available"));

    statusLabel->setText(parts.join(QStringLiteral(", ")));
}

void MainWindow::updateViewButtonState() {
    if (!activePanel) return;

    updateImageMenuState();
    updateFileMenuState();
}

void MainWindow::onView() {
    if (!activePanel) return;
    FileOperations::viewFile(activePanel, this);
}

void MainWindow::onFileInfo() {
    if (!activePanel) return;
    FileOperations::viewFileInfo(activePanel, this);
}

void MainWindow::onEdit() {
    if (!activePanel) return;
    FileOperations::editFile(activePanel, this);
}

void MainWindow::onCopy() {
    FileOperations::copyFiles(activePanel, otherPanel(), this);
    // doCopy(true);
}

void MainWindow::onRename()
{
    if (!activePanel) return;
    FileOperations::renameFile(activePanel, this);
}

void MainWindow::onMkdir() {
    if (!activePanel) return;
    FileOperations::createDirectory(activePanel, this);
}

void MainWindow::onDelete() {
    if (!activePanel) return;
    FileOperations::deleteFiles(activePanel, this);
}

void MainWindow::onRestore() {
    if (!activePanel) return;
    FileOperations::restoreFiles(activePanel, this);
}

void MainWindow::onExit() {
    close();
}

void MainWindow::onChooseFont() {
    QFont current = getViewerFont(settings.get());
    bool ok = false;
    QFont chosen = QFontDialog::getFont(&ok, current, this, tr("Viewer font"),
                                        QFontDialog::MonospacedFonts);
    if (ok) {
        settings->setValue("viewer/font", chosen.toString());
    }
}

void MainWindow::onDefaultFont() {
    settings->remove("viewer/font");
}

void MainWindow::onAbout() {
    QDialog about(this);

    Ui_About aboutUi;
    aboutUi.setupUi(&about);

    // Determine compiler information
    QString compilerInfo;
#if defined(_MSC_VER)
    compilerInfo = QString("MSVC %1").arg(_MSC_VER);
#elif defined(__clang__)
    compilerInfo = QString("Clang %1.%2.%3")
        .arg(__clang_major__)
        .arg(__clang_minor__)
        .arg(__clang_patchlevel__);
#elif defined(__GNUC__)
    compilerInfo = QString("GCC %1.%2.%3")
        .arg(__GNUC__)
        .arg(__GNUC_MINOR__)
        .arg(__GNUC_PATCHLEVEL__);
#else
    compilerInfo = "Unknown";
#endif

    aboutUi.info_label->setText(
        aboutUi.info_label->text()
            .replace("{$PROJECT_VERSION}", PROJECT_VERSION)
            .replace("{$BUILD_ARCHITECTURE}", QSysInfo::buildCpuArchitecture())
            .replace("{$OS}", QSysInfo::productType())
            .replace("{$OS_VERSION}", QSysInfo::productVersion())
            .replace("{$CPU_ARCHITECTURE}", QSysInfo::currentCpuArchitecture())
            .replace("{$COMPILER}", compilerInfo)
        );

    about.exec();
}

void MainWindow::onHotkeys() {
    struct Entry { QString key; QString desc; };
    struct Group { QString title; std::vector<Entry> entries; };

    const std::vector<Group> groups = {
        { tr("File operations"), {
            { tr("F2"),               tr("Save modified image") },
            { tr("Ctrl+Alt+F2"),      tr("Export image to another format") },
            { tr("F3"),               tr("View file / show image info") },
            { tr("Ctrl+F3"),          tr("File info inside image") },
            { tr("Ctrl+Alt+F3"),      tr("Filesystem info") },
            { tr("F4"),               tr("Open image / edit metadata") },
            { tr("F5"),               tr("Copy") },
            { tr("F6"),               tr("Rename") },
            { tr("F7"),               tr("Make directory") },
            { tr("F8, Del"),          tr("Delete") },
            { tr("F9"),               tr("Restore deleted") },
            { tr("F10"),              tr("Exit") },
        }},
        { tr("Navigation"), {
            { tr("Tab"),              tr("Switch panel") },
            { tr("Enter"),            tr("Open file or directory") },
            { tr("Backspace"),        tr("Go up one level / close image at root") },
            { tr("Esc"),              tr("Close image, return to folder") },
            { tr("Alt+F1, Alt+F2"),   tr("Choose directory for left/right panel") },
            { tr("Ctrl+F1, Ctrl+F2"), tr("Directory history for left/right panel") },
        }},
        { tr("Selection"), {
            { tr("Insert"),           tr("Toggle selection, move to next") },
            { tr("+"),                tr("Select all") },
            { tr("-"),                tr("Deselect all") },
            { tr("*"),                tr("Invert selection") },
            { tr("Up / Down"),        tr("Move cursor") },
            { tr("Home / End"),       tr("Jump to first / last row") },
            { tr("PgUp / PgDn"),      tr("Scroll by page") },
        }},
        { tr("Image"), {
            { tr("Ctrl+R"),           tr("Reload current directory / opened image") },
        }},
        { tr("Help"), {
            { tr("F1"),               tr("This window") },
        }},
    };

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Hotkeys"));

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QTableWidget *table = new QTableWidget(&dlg);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({ tr("Key"), tr("Action") });
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setFocusPolicy(Qt::NoFocus);
    table->setShowGrid(false);
    table->horizontalHeader()->setStretchLastSection(true);

    int totalRows = 0;
    for (const auto &g : groups) {
        totalRows += 1 + static_cast<int>(g.entries.size());
    }
    table->setRowCount(totalRows);

    QFont headerFont = table->font();
    headerFont.setBold(true);
    const QBrush headerBg = dlg.palette().alternateBase();

    int row = 0;
    for (const auto &g : groups) {
        QTableWidgetItem *headerItem = new QTableWidgetItem(g.title);
        headerItem->setFont(headerFont);
        headerItem->setBackground(headerBg);
        headerItem->setTextAlignment(Qt::AlignCenter);
        headerItem->setFlags(Qt::ItemIsEnabled);
        table->setItem(row, 0, headerItem);
        table->setSpan(row, 0, 1, 2);
        ++row;

        for (const auto &e : g.entries) {
            QTableWidgetItem *keyItem = new QTableWidgetItem(e.key);
            QTableWidgetItem *descItem = new QTableWidgetItem(e.desc);
            keyItem->setFlags(Qt::ItemIsEnabled);
            descItem->setFlags(Qt::ItemIsEnabled);
            table->setItem(row, 0, keyItem);
            table->setItem(row, 1, descItem);
            ++row;
        }
    }

    table->resizeColumnToContents(0);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    layout->addWidget(table);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    layout->addWidget(buttons);

    dlg.resize(520, 520);
    dlg.exec();
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
    FileOperations::viewFile(activePanel, this);
}

void MainWindow::onFSInfo()
{
    if (!activePanel) return;
    FileOperations::viewFilesystemInfo(activePanel, this);
}

void MainWindow::onImageSave()
{
    if (!activePanel) return;
    FileOperations::saveImage(activePanel, this);
}

void MainWindow::onImageSaveAs()
{
    if (!activePanel) return;
    FileOperations::saveImageAs(activePanel, this);
}

void MainWindow::onImageClose()
{
    if (!activePanel) return;
    activePanel->closeImage();
}

void MainWindow::onImageReload()
{
    if (!activePanel) return;
    if (activePanel->getMode() == panelMode::Host) {
        activePanel->reloadDirectory();
    } else {
        activePanel->reloadImage();
    }
}

void MainWindow::updateImageMenuState() const
{
    if (!activePanel) return;

    const bool is_host = activePanel->getMode() == panelMode::Host;

    if (actImageSave) actImageSave->setEnabled(!is_host);
    if (actImageSaveAs) actImageSaveAs->setEnabled(!is_host);
    if (actSave) actSave->setEnabled(!is_host);
    if (actFSInfo) actFSInfo->setEnabled(!is_host);
    if (actImageClose) actImageClose->setEnabled(!is_host);
    if (actImageReload) actImageReload->setEnabled(true);

    const bool has_index = activePanel->getCurrentIndex().isValid();

    if (is_host) {
        if (activePanel->isIndexValid()) {
            const auto path = activePanel->currentFilePath();
            const QFileInfo info(path);
            const bool is_image = !path.isEmpty() && !info.isDir();

            if (menuViewAction) menuViewAction->setShortcut(QKeySequence());
            if (actImageInfo) {
                actImageInfo->setEnabled(is_image);
                actImageInfo->setShortcut(QKeySequence(Qt::Key_F3));
            }
            if (menuEditAction) menuEditAction->setShortcut(QKeySequence());
            if (actImageOpen) {
                actImageOpen->setEnabled(is_image);
                actImageOpen->setShortcut(QKeySequence(Qt::Key_F4));
            }
        } else {
            if (actImageInfo) actImageInfo->setEnabled(false);
            if (actImageOpen) actImageOpen->setEnabled(false);
            if (actSave) actSave->setEnabled(false);
            if (actImageInfo) actImageInfo->setEnabled(false);
            if (actImageOpen) actImageOpen->setEnabled(false);
        }
    } else {
        const auto fs = activePanel->getFileSystem();
        if (fs) {
            const bool canSave = fs->get_changed();
            if (actImageSave) actImageSave->setEnabled(canSave);
            if (actSave) actSave->setEnabled(canSave);

            const bool canExport = dsk_tools::hasFlag(fs->get_caps(), dsk_tools::FSCaps::Export);
            if (actImageSaveAs) actImageSaveAs->setEnabled(canExport);
        }
        if (actImageOpen) actImageOpen->setEnabled(false);
    }
}
void MainWindow::updateFileMenuState() const
{
    if (!activePanel) return;

    const bool is_host = activePanel->getMode() == panelMode::Host;
    const dsk_tools::FSCaps sourceCaps = activePanel->getFileSystem()->get_caps();
    const dsk_tools::FSCaps targetCaps = otherPanel()->getFileSystem()->get_caps();
    const bool has_metadata = dsk_tools::hasFlag(sourceCaps, dsk_tools::FSCaps::Metadata);

    if (menuViewAction) menuViewAction->setEnabled(!is_host);
    if (menuFileInfoAction) menuFileInfoAction->setEnabled(!is_host);
    if (menuEditAction) menuEditAction->setEnabled(!is_host && has_metadata);

    if (is_host) {
        actView->setText(MainWindow::tr("F3 Image Info"));
        actEdit->setText(MainWindow::tr("F4 Open"));
    } else {
        if (actImageInfo) actImageInfo->setEnabled(false);
        // Image mode: F3 View, F4 Meta
        actView->setText(MainWindow::tr("F3 View"));
        actEdit->setText(MainWindow::tr("F4 Meta"));

        if (actImageInfo) actImageInfo->setShortcut(QKeySequence()); // Clears F3 on image functions

        if (menuViewAction) {
            menuViewAction->setText(MainWindow::tr("View"));
            menuViewAction->setShortcut(QKeySequence(Qt::Key_F3));
        }

        if (actImageOpen) actImageOpen->setShortcut(QKeySequence()); // Clears F3 on image functions

        if (menuEditAction) {
            menuEditAction->setShortcut(QKeySequence(Qt::Key_F4));
        }
    }

    const bool has_index = activePanel->isIndexValid();

    //F3
    actView->setEnabled(has_index);

    // F4
    actEdit->setEnabled(has_index && (is_host || has_metadata));

    // F5
    const bool canCopy = hasFlag(targetCaps,dsk_tools::FSCaps::Add);
    if (menuCopyAction) menuCopyAction->setEnabled(canCopy && has_index);
    actCopy->setEnabled(canCopy && has_index);

    //F6
    const bool canRename = hasFlag(sourceCaps,dsk_tools::FSCaps::Rename);
    if (menuRenameAction) menuRenameAction->setEnabled(canRename && has_index);
    actRename->setEnabled(canRename && has_index);

    //F7
    const bool canMkdir = hasFlag(sourceCaps,dsk_tools::FSCaps::MkDir);
    if (menuMkdirAction) menuMkdirAction->setEnabled(canMkdir && has_index);
    actMkdir->setEnabled(canMkdir && has_index);

    //F8
    const bool canDelete = hasFlag(sourceCaps,dsk_tools::FSCaps::Delete);
    if (menuDeleteAction) menuDeleteAction->setEnabled(canDelete && has_index);
    actDelete->setEnabled(canDelete && has_index);

    //F9
    const bool canRestore = hasFlag(sourceCaps,dsk_tools::FSCaps::Restore);
    if (menuRestoreAction) menuRestoreAction->setEnabled(canRestore && has_index);
    actRestore->setEnabled(canRestore && has_index);

}
