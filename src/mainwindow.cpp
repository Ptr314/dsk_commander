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
#include <QMenuBar>

#include "mainwindow.h"
#include "convertdialog.h"
#include "fileparamdialog.h"
#include "formatdialog.h"
#include "viewdialog.h"

#include "./ui_aboutdlg.h"
#include "./ui_fileinfodialog.h"

#include "dsk_tools/dsk_tools.h"

#include "globals.h"
#include "mainutils.h"

#define INI_FILE_NAME "/dsk_com.ini"

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

    settings = new QSettings(ini_file, QSettings::IniFormat);

    QString ini_lang = settings->value("interface/language", "").toString();
    if (ini_lang.length() == 0) {
        // Auto-detect language from system locale
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = QLocale(locale).name().toLower();
            switch_language(baseName, true);
            // Save auto-detected language to settings so menu can show correct checkmark
            settings->setValue("interface/language", baseName);
            break;
        }
    } else {
        switch_language(ini_lang, true);
    }

    resize(1000, 600);

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
    statusLabel->setText(tr("Ready"));
    statusBar()->addWidget(statusLabel);

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
    actExit   = new QAction(this);

    // actSave->setShortcut(Qt::Key_F2);
    // actSave->setShortcutContext(Qt::WindowShortcut);
    // actView->setShortcut(Qt::Key_F3);
    // actView->setShortcutContext(Qt::WindowShortcut);
    // actEdit->setShortcut(Qt::Key_F4);
    // actEdit->setShortcutContext(Qt::WindowShortcut);
    // actCopy->setShortcut(Qt::Key_F5);
    // actCopy->setShortcutContext(Qt::WindowShortcut);
    // actRename->setShortcut(Qt::Key_F6);
    // actRename->setShortcutContext(Qt::WindowShortcut);
    // actMkdir->setShortcut(Qt::Key_F7);
    // actMkdir->setShortcutContext(Qt::WindowShortcut);
    // actDelete->setShortcut(Qt::Key_F8);
    // actDelete->setShortcutContext(Qt::WindowShortcut);
    // actExit->setShortcut(Qt::Key_F10);
    // actExit->setShortcutContext(Qt::WindowShortcut);

    connect(actHelp,   &QAction::triggered, this, &MainWindow::onAbout);
    connect(actSave,   &QAction::triggered, this, &MainWindow::onImageSave);
    connect(actView,   &QAction::triggered, this, &MainWindow::onView);
    connect(actEdit,   &QAction::triggered, this, &MainWindow::onEdit);
    connect(actCopy,   &QAction::triggered, this, &MainWindow::onCopy);
    connect(actRename, &QAction::triggered, this, &MainWindow::onRename);
    connect(actMkdir,  &QAction::triggered, this, &MainWindow::onMkdir);
    connect(actDelete, &QAction::triggered, this, &MainWindow::onDelete);
    connect(actExit,   &QAction::triggered, this, &MainWindow::onExit);

    addActions({actHelp, actSave, actView, actEdit, actCopy, actRename, actMkdir, actDelete, actExit});

    // Set initial text (unified method)
    updateActionTexts();
}

void MainWindow::updateActionTexts() {
    // All action text in ONE place - called both on init and retranslation
    actHelp->setText(tr("F1 Help"));
    actSave->setText(tr("F2 Save"));
    actView->setText(tr("F3 View"));
    actEdit->setText(tr("F4 Open"));
    actCopy->setText(tr("F5 Copy"));
    actRename->setText(tr("F6 Rename"));
    actMkdir->setText(tr("F7 MkDir"));
    actDelete->setText(tr("F8 Delete"));
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
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setDefaultAction(act);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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

    actImageSave = imageMenu->addAction(QIcon(":/icons/icon"), MainWindow::tr("Save"));
    actImageSave->setShortcut(QKeySequence(Qt::Key_F2));
    connect(actImageSave, &QAction::triggered, this, &MainWindow::onImageSave);

    actImageSaveAs = imageMenu->addAction(QIcon(":/icons/convert"), MainWindow::tr("Save as..."));
    actImageSaveAs->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F2));
    connect(actImageSaveAs, &QAction::triggered, this, &MainWindow::onImageSaveAs);

    imageMenu->addSeparator();

    actImageInfo = imageMenu->addAction(QIcon(":/icons/info"), MainWindow::tr("Container Info..."));
    connect(actImageInfo, &QAction::triggered, this, &MainWindow::onImageInfo);

    actFSInfo = imageMenu->addAction(QIcon(":/icons/info"), MainWindow::tr("Filesystem Info..."));
    connect(actFSInfo, &QAction::triggered, this, &MainWindow::onFSInfo);

    imageMenu->addSeparator();

    actImageOpen = imageMenu->addAction(QIcon(":/icons/open"), MainWindow::tr("Open"));
    connect(actImageOpen, &QAction::triggered, this, &MainWindow::onEdit);

    // === FILES MENU ===
    QMenu *filesMenu = menuBar()->addMenu(MainWindow::tr("Files"));

    menuViewAction = filesMenu->addAction(QIcon(":/icons/info"), MainWindow::tr("View"));
    // menuViewAction->setShortcut(QKeySequence(Qt::Key_F3));
    connect(menuViewAction, &QAction::triggered, this, &MainWindow::onView);

    menuFileInfoAction = filesMenu->addAction(QIcon(":/icons/info"), MainWindow::tr("File Info"));
    menuFileInfoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F3));
    connect(menuFileInfoAction, &QAction::triggered, this, &MainWindow::onFileInfo);

    menuEditAction = filesMenu->addAction(QIcon(":/icons/view"), MainWindow::tr("Edit Metadata"));
    connect(menuEditAction, &QAction::triggered, this, &MainWindow::onEdit);

    menuCopyAction = filesMenu->addAction(QIcon(":/icons/text_copy"), MainWindow::tr("Copy"));
    menuCopyAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(menuCopyAction, &QAction::triggered, this, &MainWindow::onCopy);

    menuRenameAction = filesMenu->addAction(QIcon(":/icons/edit"), MainWindow::tr("Rename"));
    menuRenameAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(menuRenameAction, &QAction::triggered, this, &MainWindow::onRename);

    menuMkdirAction = filesMenu->addAction(QIcon(":/icons/new_dir"), MainWindow::tr("F7 Make dir"));
    menuMkdirAction->setShortcut(QKeySequence(Qt::Key_F7));
    connect(menuMkdirAction, &QAction::triggered, this, &MainWindow::onMkdir);

    menuDeleteAction = filesMenu->addAction(QIcon(":/icons/delete"), MainWindow::tr("F8 Delete"));
    menuDeleteAction->setShortcut(QKeySequence(Qt::Key_F8));
    connect(menuDeleteAction, &QAction::triggered, this, &MainWindow::onDelete);

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

    QAction *aboutAction = optionsMenu->addAction(QIcon(":/icons/help"), MainWindow::tr("About..."));
    aboutAction->setShortcut(QKeySequence(Qt::Key_F1));
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
        statusLabel->setText(MainWindow::tr("No active panel"));
        return;
    }
    const QString dir = activePanel->currentDir();
    const int selCount = activePanel->selectedPaths().size();
    statusLabel->setText(QString(MainWindow::tr("Active panel: %1 | Selected: %2")).arg(dir).arg(selCount));
}

void MainWindow::updateViewButtonState() {
    if (!activePanel) return;

    updateImageMenuState();
    updateFileMenuState();
}

void MainWindow::onView() {
    if (!activePanel) return;
    activePanel->onView();
}

void MainWindow::onFileInfo() {
    if (!activePanel) return;
    activePanel->onFileInfo();
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

void MainWindow::onFSInfo()
{
    if (!activePanel) return;
    activePanel->showFSInfo();
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

void MainWindow::updateImageMenuState() const
{
    if (!activePanel) return;

    const bool is_host = activePanel->getMode() == panelMode::Host;

    if (actImageSave) actImageSave->setEnabled(!is_host);
    if (actImageSaveAs) actImageSaveAs->setEnabled(!is_host);
    if (actSave) actSave->setDisabled(!is_host);
    if (actFSInfo) actFSInfo->setEnabled(!is_host);


    if (is_host) {
        if (activePanel->isIndexValid()) {
            const auto path = activePanel->currentFilePath();
            const QFileInfo info(path);
            if (menuViewAction) menuViewAction->setShortcut(QKeySequence());
            if (actImageInfo) {
                actImageInfo->setEnabled(!info.isDir());
                actImageInfo->setShortcut(QKeySequence(Qt::Key_F3));
            }
            if (menuEditAction) menuEditAction->setShortcut(QKeySequence());
            if (actImageOpen) {
                actImageOpen->setEnabled(!info.isDir());
                actImageOpen->setShortcut(QKeySequence(Qt::Key_F4));
            }
        }
    } else {
        const bool canSave = (activePanel->getFileSystem() != nullptr) &&
                             (activePanel->getFileSystem()->get_changed());

        if (actImageSave) actImageSave->setEnabled(canSave);
        if (actSave) actSave->setEnabled(canSave);
    }
}
void MainWindow::updateFileMenuState() const
{
    if (!activePanel) return;

    const bool is_host = activePanel->getMode() == panelMode::Host;

    if (menuViewAction) menuViewAction->setEnabled(!is_host);
    if (menuFileInfoAction) menuFileInfoAction->setEnabled(!is_host);
    if (menuEditAction) menuEditAction->setEnabled(!is_host);

    if (is_host) {
        actView->setText(MainWindow::tr("F3 Image Info"));
        actEdit->setText(MainWindow::tr("F4 Open"));

        // if (menuEditAction) menuEditAction->setText(MainWindow::tr("Open"));

        qDebug() << activePanel->currentFilePath();
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
            // menuEditAction->setText(MainWindow::tr("View"));
            menuEditAction->setShortcut(QKeySequence(Qt::Key_F4));
        }
    }

    const dsk_tools::FSCaps sourceCaps = activePanel->getFileSystem()->getCaps();
    const dsk_tools::FSCaps targetCaps = otherPanel()->getFileSystem()->getCaps();

    // F5
    const bool canCopy = hasFlag(targetCaps,dsk_tools::FSCaps::Add);
    if (menuCopyAction) menuCopyAction->setEnabled(canCopy);
    actCopy->setEnabled(canCopy);

    //F6
    const bool canRename = hasFlag(sourceCaps,dsk_tools::FSCaps::Rename);
    if (menuRenameAction) menuRenameAction->setEnabled(canRename);
    actRename->setEnabled(canRename);

    //F7
    const bool canMkdir = hasFlag(sourceCaps,dsk_tools::FSCaps::MkDir);
    if (menuMkdirAction) menuMkdirAction->setEnabled(canMkdir);
    actMkdir->setEnabled(canMkdir);

    //F8
    const bool canDelete = hasFlag(sourceCaps,dsk_tools::FSCaps::Delete);
    if (menuDeleteAction) menuDeleteAction->setEnabled(canDelete);
    actDelete->setEnabled(canDelete);

}
