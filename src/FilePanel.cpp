// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File panel UI

#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QUrl>
#include <QJsonArray>
#include <QMessageBox>
#include <QIcon>
#include <QDebug>
#include <QLocale>
#include <QFont>
#include <QFontDatabase>
#include <QDateTime>
#include <memory>
#include <fstream>
#include <QScrollBar>

#include "./ui_fileinfodialog.h"

#include "FilePanel.h"
#include "mainutils.h"
#include "viewdialog.h"
#include "fileparamdialog.h"
#include "convertdialog.h"
#include "FileOperations.h"
#include "fs_host.h"
#include "host_helpers.h"

#include "dsk_tools/dsk_tools.h"

// ============================================================================
// HostModel implementation
// ============================================================================

HostModel::HostModel(QObject *parent)
    : QStandardItemModel(parent)
{
    // Set up 3 columns with translatable headers
    setColumnCount(3);
    setHorizontalHeaderItem(0, new QStandardItem(FilePanel::tr("Name")));
    setHorizontalHeaderItem(1, new QStandardItem(FilePanel::tr("Size")));
    setHorizontalHeaderItem(2, new QStandardItem(FilePanel::tr("Date")));
}

void HostModel::setRootPath(const QString& path) {
    QDir dir(path);
    if (!dir.exists()) return;

    m_currentPath = dir.absolutePath();

    // Check if we're at a root directory
    QDir parentDir = dir;
    m_isRoot = !parentDir.cdUp();

    populateModel();
}

void HostModel::setNameFilters(const QStringList& filters) {
    m_nameFilters = filters;
}

void HostModel::setSortOrder(SortOrder order, bool ascending) {
    m_sortOrder = order;
    m_sortAsc = ascending;
    refresh();
}

void HostModel::refresh() {
    populateModel();
}

void HostModel::goUp() {
    if (m_isRoot) return;

    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        setRootPath(dir.absolutePath());
    }
}

QString HostModel::filePath(const QModelIndex& index) const {
    if (!index.isValid()) return QString();

    QStandardItem* item = itemFromIndex(this->index(index.row(), 0));
    if (!item) return QString();

    return item->data(Qt::UserRole).toString();
}

QFileInfo HostModel::fileInfo(const QModelIndex& index) const {
    QString path = filePath(index);
    return QFileInfo(path);
}

bool HostModel::isDir(const QModelIndex& index) const {
    if (!index.isValid()) return false;

    QStandardItem* item = itemFromIndex(this->index(index.row(), 0));
    if (!item) return false;

    return item->data(Qt::UserRole + 1).toBool();
}

void HostModel::populateModel() {
    // Clear existing rows
    removeRows(0, rowCount());

    QDir dir(m_currentPath);

    // Add [..] entry if not at root
    if (!m_isRoot) {
        QList<QStandardItem*> items;

        // Column 0: Name [..]
        QStandardItem* nameItem = new QStandardItem("[..]");
        nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        nameItem->setData(QString(), Qt::UserRole);  // Empty path for [..]
        nameItem->setData(true, Qt::UserRole + 1);   // Mark as directory
        nameItem->setData(QIcon(":/icons/folder_open"), Qt::DecorationRole);  // Folder icon
        items.append(nameItem);

        // Column 1: Size (empty for [..])
        QStandardItem* sizeItem = new QStandardItem("");
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(sizeItem);

        // Column 2: Date (empty for [..])
        QStandardItem* dateItem = new QStandardItem("");
        dateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(dateItem);

        appendRow(items);
    }

    // Get directory contents
    QDir::Filters filters = QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::AllDirs;
    QFileInfoList entries = dir.entryInfoList(m_nameFilters, filters, QDir::NoSort);

    // Separate directories and files
    QFileInfoList directories;
    QFileInfoList files;

    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            directories.append(info);
        } else {
            files.append(info);
        }
    }

    // Sort directories and files separately
    auto sortByName = [this](const QFileInfo& a, const QFileInfo& b) {
        return m_sortAsc ? QString::localeAwareCompare(a.fileName(), b.fileName()) < 0 : QString::localeAwareCompare(a.fileName(), b.fileName()) >= 0;
    };
    auto sortBySize = [this](const QFileInfo& a, const QFileInfo& b) {
        return m_sortAsc ? a.size() < b.size() : a.size() >= b.size();
    };

    if (m_sortOrder == ByName) {
        std::sort(directories.begin(), directories.end(), sortByName);
        std::sort(files.begin(), files.end(), sortByName);
    } else if (m_sortOrder == BySize) {
        std::sort(directories.begin(), directories.end(), sortByName);  // Dirs still by name
        std::sort(files.begin(), files.end(), sortBySize);
    }
    // For NoOrder, use filesystem order (don't sort)

    // Add directories first
    for (const QFileInfo& info : directories) {
        QList<QStandardItem*> items;

        // Column 0: Name
        QStandardItem* nameItem = new QStandardItem("[" + info.fileName() + "]");
        nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        nameItem->setData(info.absoluteFilePath(), Qt::UserRole);
        nameItem->setData(true, Qt::UserRole + 1);
        nameItem->setData(QIcon(":/icons/folder_open"), Qt::DecorationRole);  // Folder icon
        items.append(nameItem);

        // Column 1: Size
        QStandardItem* sizeItem = new QStandardItem("<DIR>");
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(sizeItem);

        // Column 2: Date
        QStandardItem* dateItem = new QStandardItem(formatDate(info.lastModified()));
        dateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(dateItem);

        appendRow(items);
    }

    // Then add files
    for (const QFileInfo& info : files) {
        QList<QStandardItem*> items;

        // Column 0: Name
        QStandardItem* nameItem = new QStandardItem(info.fileName());
        nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        nameItem->setData(info.absoluteFilePath(), Qt::UserRole);
        nameItem->setData(false, Qt::UserRole + 1);
        nameItem->setData(QIcon(":/icons/file_image"), Qt::DecorationRole);  // File icon
        items.append(nameItem);

        // Column 1: Size
        QStandardItem* sizeItem = new QStandardItem(formatSize(info.size()));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(sizeItem);

        // Column 2: Date
        QStandardItem* dateItem = new QStandardItem(formatDate(info.lastModified()));
        dateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(dateItem);

        appendRow(items);
    }
}

QString HostModel::formatSize(qint64 size) {
    QString numStr = QString::number(size);
    QString result;
    int count = 0;
    for (int i = numStr.length() - 1; i >= 0; --i) {
        if (count == 3) {
            result.prepend('.');
            count = 0;
        }
        result.prepend(numStr[i]);
        count++;
    }
    return result;
}

QString HostModel::formatDate(const QDateTime& dt){
    return QLocale().toString(dt, QLocale::ShortFormat);
}


FilePanel::FilePanel(QWidget *parent, QSettings *settings, QString ini_label, const QJsonObject & file_formats, const QJsonObject & file_types, const QJsonObject & file_systems) :
      QWidget(parent)
    , m_settings(settings)
    , m_ini_label(ini_label)
    , m_file_formats(file_formats)
    , m_file_types(file_types)
    , m_file_systems(file_systems)
{
    setupPanel();
}

void FilePanel::setupPanel() {
    QFont font;
#ifdef Q_OS_WIN
    font.setFamily("Consolas");
    font.setPointSize(10);
#else
    font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
#endif

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(3);

    // Model for the file system
    host_model = new HostModel(this);

    image_model = new QStandardItemModel(this);

    // Upper tooldar
    topToolBar = new QToolBar(this);

    upButton = new QToolButton(this);
    upButton->setText(FilePanel::tr("Up"));
    upButton->setIcon(QIcon(":/icons/up"));
    upButton->setToolTip(FilePanel::tr("Up"));
    upButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    upButton->setIconSize(QSize(24, 24));

    dirEdit = new QLineEdit(this);
    dirEdit->setPlaceholderText(FilePanel::tr("Enter path and press Enter..."));

    dirButton = new QToolButton(this);
    dirButton->setText(FilePanel::tr("Choose..."));
    dirButton->setIcon(QIcon(":/icons/folder_open"));
    dirButton->setToolTip(FilePanel::tr("Choose..."));
    dirButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    dirButton->setIconSize(QSize(24, 24));

    historyMenu = new QMenu(this);
    dirButton->setMenu(historyMenu);
    dirButton->setPopupMode(QToolButton::MenuButtonPopup);

    // Image name label (shown in Image mode instead of dirEdit/dirButton)
    imageLabel = new QLabel(this);
    imageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    imageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    imageLabel->hide();  // Initially hidden (starts in Host mode)

    // Save button (shown in Image mode)
    saveButton = new QToolButton(this);
    saveButton->setText(FilePanel::tr("Save"));
    saveButton->setIcon(QIcon(":/icons/icon"));
    saveButton->setToolTip(FilePanel::tr("Save disk image"));
    saveButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    saveButton->setIconSize(QSize(24, 24));
    saveButton->hide();  // Initially hidden (starts in Host mode)

    // Save As button (shown in Image mode)
    saveAsButton = new QToolButton(this);
    saveAsButton->setText(FilePanel::tr("Save as..."));
    saveAsButton->setIcon(QIcon(":/icons/convert"));
    saveAsButton->setToolTip(FilePanel::tr("Save disk image as..."));
    saveAsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    saveAsButton->setIconSize(QSize(24, 24));
    saveAsButton->hide();  // Initially hidden (starts in Host mode)

    auto *topContainer = new QWidget(topToolBar);
    auto *topLayout = new QHBoxLayout(topContainer);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(5);
    topLayout->addWidget(upButton);
    topLayout->addWidget(dirEdit, 1);
    topLayout->addWidget(saveButton);
    topLayout->addWidget(saveAsButton);
    topLayout->addWidget(imageLabel, 1);  // Same stretch as dirEdit, both hidden/shown based on mode
    topLayout->addWidget(dirButton);

    topToolBar->addWidget(topContainer);

    connect(upButton,  &QToolButton::clicked, this, &FilePanel::onGoUp);
    connect(dirButton, &QToolButton::clicked, this, &FilePanel::chooseDirectory);
    connect(dirEdit,   &QLineEdit::returnPressed, this, &FilePanel::onPathEntered);
    connect(historyMenu, &QMenu::triggered, this, &FilePanel::onHistoryMenuTriggered);
    connect(saveButton, &QToolButton::clicked, this, &FilePanel::saveImage);
    connect(saveAsButton, &QToolButton::clicked, this, &FilePanel::saveImageAs);

    // Load and initialize directory history
    loadDirectoryHistory();
    updateHistoryMenu();

    // Main table with files using custom FileTable
    tableView = new FileTable(this);
    tableView->setFont(font);

    // Connect FileTable signals to FilePanel slots
    connect(tableView, &FileTable::focusReceived, this, [this]() { emit activated(this); });
    connect(tableView, &FileTable::switchPanelRequested, this, &FilePanel::switchPanelRequested);
    connect(tableView, &FileTable::doubleClicked, this, &FilePanel::onItemDoubleClicked);
    connect(tableView, &FileTable::goUpRequested, this, &FilePanel::onGoUp);

    setMode(panelMode::Host);

    // Bottom toolbar
    filterToolBar = new QToolBar(this);
    typeToolBar = new QToolBar(this);
    setupFilters();

    // Install event filters for controls
    for (QObject* obj : {
                         static_cast<QObject*>(tableView),
                         static_cast<QObject*>(tableView->viewport()),
                         static_cast<QObject*>(filterCombo),
                         static_cast<QObject*>(dirButton),
                         static_cast<QObject*>(upButton),
                         static_cast<QObject*>(dirEdit),
                         static_cast<QObject*>(saveButton),
                         static_cast<QObject*>(saveAsButton),
                         static_cast<QObject*>(imageLabel)})
    {
        obj->installEventFilter(this);
    }

    // Adding elements
    layout->addWidget(topToolBar, 0);
    layout->addWidget(tableView, 1);
    layout->addWidget(filterToolBar, 0);
    layout->addWidget(typeToolBar, 0);

    layout->setStretch(1, 1); // Table consumes maximim
    setLayout(layout);

    // Starting path
    setDirectory(m_settings->value("directory/"+m_ini_label, QDir::currentPath()).toString());
}

void FilePanel::populateFilterCombo()
{
    filterCombo->clear();

    QVector<QPair<QString, QJsonObject>> entries;

    const QStringList keys = m_file_formats.keys();
    for (const QString& key : keys) {
        entries.append(qMakePair(key, m_file_formats[key].toObject()));
    }

    std::sort(entries.begin(), entries.end(), [](const QPair<QString, QJsonObject>& a, const QPair<QString, QJsonObject>& b) {
        int orderA = a.second["order"].toInt();
        int orderB = b.second["order"].toInt();
        if (orderA != orderB)
            return orderA < orderB;

        return a.second["name"].toString().toLower() < b.second["name"].toString().toLower();
    });


    for (const auto& pair : entries) {
        QString ff_id = pair.first;
        QJsonObject obj = m_file_formats[ff_id].toObject();
        if (obj["source"].toBool()) {
            QString name = QCoreApplication::translate("config", obj["name"].toString().toUtf8().constData());
            filterCombo->addItem(
                QString("%1 (%2)").arg(name, obj["extensions"].toString().replace(";", "; ")),
                ff_id
                );
        }
    }
}

void FilePanel::setupFilters()
{
    QString filter_def = m_settings->value("directory/"+m_ini_label+"_file_filter", "").toString();
    QString type_def = m_settings->value("directory/"+m_ini_label+"_type_filter", "").toString();
    QString filesystem_def = m_settings->value("directory/"+m_ini_label+"_filesystem", "").toString();

    // Extensions filter ------------------------------------------------------
    filterCombo = new QComboBox(this);

    populateFilterCombo();
    filterCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    filterToolBar->addWidget(filterCombo);

    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilePanel::onFilterChanged);

    // Type & filesystem -----------------------------------------
    typeCombo = new QComboBox(this);
    typeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    fsCombo = new QComboBox(this);
    fsCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    autoCheck = new QCheckBox(FilePanel::tr("Auto"), this);
    autoCheck->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    auto *container = new QWidget(typeToolBar);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(typeCombo, 1);
    layout->addWidget(fsCombo, 1);
    layout->addWidget(autoCheck, 0);

    typeToolBar->addWidget(container);

    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilePanel::onTypeChanged);
    connect(fsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilePanel::onFsChanged);
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    connect(autoCheck, QOverload<Qt::CheckState>::of(&QCheckBox::checkStateChanged),
            this, &FilePanel::onAutoChanged);
#else
    connect(autoCheck, &QCheckBox::stateChanged,
            this, [this](int state) { onAutoChanged(static_cast<Qt::CheckState>(state)); });
#endif

    QSignalBlocker block(filterCombo);
    setComboBoxByItemData(filterCombo, filter_def);
    onFilterChanged(filterCombo->currentIndex());   // Doing it manually for the first time

    setComboBoxByItemData(typeCombo, type_def);
    setComboBoxByItemData(fsCombo, filesystem_def);

    autoCheck->setChecked(m_settings->value("directory/"+m_ini_label+"_auto", 1).toInt()==1);

    // Load show_deleted preference (default to true)
    m_show_deleted = m_settings->value("directory/" + m_ini_label + "_show_deleted", 1).toInt() == 1;

    // Load sort order and ascending preference (default to NoOrder, ascending)
    m_sort_order = static_cast<HostModel::SortOrder>(m_settings->value("directory/" + m_ini_label + "_sort_order", static_cast<int>(HostModel::SortOrder::NoOrder)).toInt());
    m_sort_ascending = m_settings->value("directory/" + m_ini_label + "_sort_ascending", true).toBool();
    if (host_model) {
        host_model->setSortOrder(m_sort_order, m_sort_ascending);
    }
}

void FilePanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void FilePanel::retranslateUi()
{
    // Retranslate HostModel column headers
    if (host_model) {
        host_model->setHorizontalHeaderItem(0, new QStandardItem(tr("Name")));
        host_model->setHorizontalHeaderItem(1, new QStandardItem(tr("Size")));
        host_model->setHorizontalHeaderItem(2, new QStandardItem(tr("Date")));
    }

    // Retranslate toolbar elements
    upButton->setToolTip(tr("Up"));
    dirButton->setToolTip(tr("Choose..."));
    dirEdit->setPlaceholderText(tr("Enter path and press Enter..."));
    autoCheck->setText(tr("Auto"));

    // Retranslate toolbar button labels
    if (saveButton) {
        saveButton->setText(tr("Save"));
        saveButton->setToolTip(tr("Save disk image"));
    }
    if (saveAsButton) {
        saveAsButton->setText(tr("Save as..."));
        saveAsButton->setToolTip(tr("Save disk image as..."));
    }

    // Refresh history menu to update translated strings
    updateHistoryMenu();

    // Refresh filterCombo - this will cascade to typeCombo and fsCombo
    QString savedFilter = filterCombo->currentData().toString();

    QSignalBlocker block(filterCombo);
    populateFilterCombo();
    setComboBoxByItemData(filterCombo, savedFilter);

    // Manually trigger filter change to cascade updates to type and fs combos
    onFilterChanged(filterCombo->currentIndex());
}

void FilePanel::setDirectory(const QString& path, bool restoreCursor) {
    if (path.isEmpty()) return;
    QDir dir(path);
    if (!dir.exists()) return;

    QString oldDirName;
    if (restoreCursor && !m_lastDirName.isEmpty()) {
        oldDirName = m_lastDirName;
        m_lastDirName.clear();
    }

    currentPath = dir.absolutePath();
    dirEdit->setText(currentPath);
    host_model->setRootPath(currentPath);
    tableView->setRootIndex(QModelIndex());  // QStandardItemModel doesn't use root index

    // Update filesystem if it's an fsHost instance
    if (m_filesystem && m_filesystem->getFS() == dsk_tools::FS::Host) {
        m_filesystem->cd(_toStdString(currentPath));
    }

    // Restore cursor position or set to first item
    if (!oldDirName.isEmpty()) {
        // Search for the directory we came from
        for (int row = 0; row < host_model->rowCount(); ++row) {
            QModelIndex idx = host_model->index(row, 0);
            QString displayName = host_model->data(idx, Qt::DisplayRole).toString();
            // Remove brackets from directory name: [dirname] -> dirname
            if (displayName.startsWith("[") && displayName.endsWith("]")) {
                QString dirName = displayName.mid(1, displayName.length() - 2);
                if (dirName == oldDirName && dirName != "..") {
                    // Clear selection and set cursor without selecting
                    tableView->selectionModel()->clearSelection();
                    tableView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::NoUpdate);
                    tableView->scrollTo(idx);
                    break;
                }
            }
        }
    } else {
        // Default: set cursor to first item
        QModelIndex firstIndex = host_model->index(0, 0);
        if (firstIndex.isValid()) {
            // Clear selection and set cursor without selecting
            tableView->selectionModel()->clearSelection();
            tableView->selectionModel()->setCurrentIndex(firstIndex, QItemSelectionModel::NoUpdate);
        }
    }

    m_settings->setValue("directory/"+m_ini_label, currentPath);

    // Update directory history
    addToDirectoryHistory(currentPath);
    updateHistoryMenu();
}

void FilePanel::chooseDirectory()
{
    emit activated(this);
    QString dir = QFileDialog::getExistingDirectory(this, FilePanel::tr("Choose a path"), currentPath);
    if (!dir.isEmpty()) setDirectory(dir);
}

void FilePanel::onFilterChanged(int index)
{
    emit activated(this);

    typeCombo->clear();
    QString ff_id = filterCombo->itemData(index).toString();
    QJsonObject filter = m_file_formats[ff_id].toObject();
    QJsonArray types = filter["types"].toArray();
    if (types.empty()) {
        foreach (const QJsonValue & type, m_file_types.keys()) {
                types.append(type.toString());
        }
    }
    foreach (const QJsonValue & value, types) {
        QString type_id = value.toString();
        QJsonObject type = m_file_types[type_id].toObject();
        QString name = QCoreApplication::translate("config", type["name"].toString().toUtf8().constData());

        typeCombo->addItem(name, type_id);
    }

    QStringList filters = filter["extensions"].toString().split(";");
    host_model->setNameFilters(filters);
    host_model->refresh();  // Refresh to apply new filters

    m_settings->setValue("directory/"+m_ini_label+"_file_filter", ff_id);
}

void FilePanel::onTypeChanged(int index)
{
    emit activated(this);

    QString type_id = typeCombo->itemData(index).toString();
    m_settings->setValue("directory/"+m_ini_label+"_type_filter", type_id);

    fsCombo->clear();
    QJsonObject type = m_file_types[type_id].toObject();
    foreach (const QJsonValue & fs_val, type["filesystems"].toArray()) {
        QString fs_id = fs_val.toString();
        QJsonObject fs = m_file_systems[fs_id].toObject();
        QString name = QCoreApplication::translate("config", fs["name"].toString().toUtf8().constData());
        fsCombo->addItem(name, fs_id);
    }
}

void FilePanel::onFsChanged(int index)
{
    emit activated(this);
    QString fs_id = fsCombo->itemData(index).toString();
    m_settings->setValue("directory/"+m_ini_label+"_filesystem", fs_id);

}

void FilePanel::setComboBoxByItemData(QComboBox* comboBox, const QVariant& value) {
    if (!comboBox || value.toString().isEmpty()) return;  // Защита от nullptr

    for (int i = 0; i < comboBox->count(); ++i) {
        if (comboBox->itemData(i) == value) {
            comboBox->setCurrentIndex(i);
            return;
        }
    }
}

void FilePanel::onAutoChanged(Qt::CheckState checked)
{
    emit activated(this);
    m_settings->setValue("directory/"+m_ini_label+"_auto", checked?1:0);
}

void FilePanel::onGoUp() {
    emit activated(this);
    if (mode==panelMode::Host) {
        QDir dir(currentPath);
        if (dir.cdUp()) {
            // Store current directory name for cursor restoration
            m_lastDirName = QDir(currentPath).dirName();
            setDirectory(dir.absolutePath(), true);

            // Restore cursor position to the directory we came from
            for (int row = 0; row < host_model->rowCount(); ++row) {
                QModelIndex idx = host_model->index(row, 0);
                QString checkName = host_model->data(idx, Qt::DisplayRole).toString();
                // Remove brackets from directory name: [dirname] -> dirname
                if (checkName.startsWith("[") && checkName.endsWith("]")) {
                    QString dirName = checkName.mid(1, checkName.length() - 2);
                    if (dirName == m_lastDirName && dirName != "..") {
                        // Clear selection and set cursor without selecting
                        tableView->selectionModel()->clearSelection();
                        tableView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::NoUpdate);
                        tableView->scrollTo(idx);
                        m_lastDirName.clear();
                        break;
                    }
                }
            }
        }
    } else {
        if (m_filesystem->is_root()) {
            // Check for unsaved changes before switching to Host mode
            if (!checkUnsavedChanges()) {
                return;  // User cancelled
            }
            setMode(panelMode::Host);
            setDirectory(currentPath);
        } else {
            m_filesystem->cd_up();
            dir();
        }
    }
}

bool FilePanel::checkUnsavedChanges()
{
    // Check if we're in Image mode and have unsaved changes
    if (mode != panelMode::Image || !m_filesystem || !m_filesystem->get_changed()) {
        return true;  // No unsaved changes, operation can proceed
    }

    // Show three-button dialog with Save as default
    const QMessageBox::StandardButton result = QMessageBox::question(
        this,
        FilePanel::tr("Unsaved Changes"),
        FilePanel::tr("The disk image has unsaved changes. Save before closing?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes  // Default button
    );

    if (result == QMessageBox::Save || result == QMessageBox::Yes) {
        // Save changes using ConvertDialog
        saveImageAs();

        // Check if save was successful
        if (m_filesystem && m_filesystem->get_changed()) {
            // Save was cancelled or failed, abort the mode switch
            return false;
        }
        return true;

    } else if (result == QMessageBox::Discard || result == QMessageBox::No) {
        return true;  // Discard changes and proceed

    } else {  // Cancel
        return false;  // Abort the operation
    }
}

void FilePanel::updateImageStatusIndicator() const
{
    if (mode == panelMode::Image && m_filesystem && m_filesystem->get_changed()) {
        // Add asterisk to indicate unsaved changes
        QString text = imageLabel->text();
        if (!text.startsWith("* ")) {
            imageLabel->setText("* " + text);
        }
        // Apply bold font to indicate modification
        QFont font = imageLabel->font();
        font.setBold(true);
        imageLabel->setFont(font);
    } else {
        // Remove asterisk if present
        if (mode == panelMode::Host) {
            QString path = dirEdit->text();
            if (path.startsWith("* ")) {
                dirEdit->setText(path.mid(2));
            }
        } else {
            QString text = imageLabel->text();
            if (text.startsWith("* ")) {
                imageLabel->setText(text.mid(2));
            }
            // Restore normal font
            QFont font = imageLabel->font();
            font.setBold(false);
            imageLabel->setFont(font);
        }
    }

    // Update Save button enabled state based on changes
    if (saveButton) {
        saveButton->setEnabled(
               mode == panelMode::Image
            && m_filesystem
            && m_filesystem->get_changed()
            && m_current_format_id=="FILE_RAW_MSB"
        );
    }
    // Save As is always enabled in Image mode
    if (saveAsButton) {
        saveAsButton->setEnabled(mode == panelMode::Image && m_filesystem != nullptr);
    }
}

void FilePanel::onPathEntered() {
    emit activated(this);
    QString entered = dirEdit->text().trimmed();
    QDir dir(entered);
    if (dir.exists()) setDirectory(dir.absolutePath());
}

void FilePanel::onItemDoubleClicked(const QModelIndex& index) {
    emit activated(this);  // broadcast the activation
    if (!index.isValid()) return;
    FileOperations::openItem(this, this, index);
}

int FilePanel::openImage(QString path)
{
    // Detecting formats if necessary
    std::string format_id;
    std::string type_id;
    std::string filesystem_id;

    const QFileInfo fileInfo(path);
    const std::string file_name = _toStdString(fileInfo.absoluteFilePath());
    const QString selected_format = filterCombo->itemData(filterCombo->currentIndex()).toString();
    if (autoCheck->isChecked()) {
        const auto res = dsk_tools::detect_fdd_type(file_name, format_id, type_id, filesystem_id);
        if (!res) return -1;

        setComboBoxByItemData(filterCombo, (selected_format != "FILE_ANY")?QString::fromStdString(format_id):"");
        setComboBoxByItemData(typeCombo, QString::fromStdString(type_id));
        setComboBoxByItemData(fsCombo, QString::fromStdString(filesystem_id));
    } else {
        type_id = "";
        filesystem_id = "";
        if (selected_format != "FILE_ANY") {
            format_id = filterCombo->itemData(filterCombo->currentIndex()).toString().toStdString();
        } else {
            dsk_tools::Result res = dsk_tools::detect_fdd_type(file_name, format_id, type_id, filesystem_id, true);
            type_id = "";
            filesystem_id = "";
        };
        if (type_id.empty()) type_id = typeCombo->itemData(typeCombo->currentIndex()).toString().toStdString();
        if (filesystem_id.empty()) filesystem_id = fsCombo->itemData(fsCombo->currentIndex()).toString().toStdString();

    }

    // Load file

    image_model->removeRows(0, image_model->rowCount());

    m_image = dsk_tools::prepare_image(file_name, format_id, type_id);

    if (m_image != nullptr) {
        const auto check_result = m_image->check();
        if (check_result) {
            const auto load_result = m_image->load();
            if (load_result) {
                processImage(filesystem_id);
            } else {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("File loading error. Check your disk type settings or try auto-detection."));
                return FDD_LOAD_ERROR;
            }
        } else {
            QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error checking file parameters"));
            return FDD_LOAD_ERROR;
        }
    } else {
        QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error preparing image data"));
        return FDD_LOAD_ERROR;
    }

    // Store loaded image metadata for later use (e.g., Save to original format)
    m_current_format_id = format_id;
    m_current_type_id = type_id;
    m_current_filesystem_id = filesystem_id;

    updateImageStatusIndicator();

    return FDD_LOAD_OK;
}

void FilePanel::processImage(const std::string &filesystem_type)
{
    m_filesystem = dsk_tools::prepare_filesystem(m_image.get(), filesystem_type);
    if (m_filesystem != nullptr) {
        int open_res = m_filesystem->open();

        if (open_res != FDD_OPEN_OK) {
            QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Unrecognized disk format or disk is damaged!"));
            return;
        }

        setMode(panelMode::Image);

        // Clear selection immediately after model switch to prevent selection indices
        // from carrying over from the previous model (host mode) to the new model (image mode)
        tableView->clearSelection();

        dir();
    } else {
        QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("File system initialization error!"));
    }
}

void FilePanel::setMode(panelMode new_mode)
{
    mode = new_mode;

    // Update toolbar widget visibility based on mode
    updateToolbarVisibility();

    if (mode==panelMode::Host) {
        // Clear loaded image metadata when closing image
        m_current_format_id.clear();
        m_current_type_id.clear();
        m_current_filesystem_id.clear();

        tableView->setModel(host_model);
        tableView->setupForHostMode();
        m_filesystem = dsk_tools::make_unique<dsk_tools::fsHost>(nullptr);
    } else {
        tableView->setModel(image_model);
        tableView->setupForImageMode(m_filesystem->getCaps());
    }
    emit panelModeChanged(mode);
}

void FilePanel::updateToolbarVisibility() const
{
    if (mode == panelMode::Host) {
        // Host mode: show path input controls, hide image label
        dirEdit->show();
        dirButton->show();
        imageLabel->hide();
        saveButton->hide();
        saveAsButton->hide();
    } else {
        // Image mode: hide path input controls, show image label
        dirEdit->hide();
        dirButton->hide();
        imageLabel->show();
        saveButton->show();
        saveAsButton->show();

        // Update image label text
        if (m_image != nullptr) {
            QString fullPath = QString::fromStdString(m_image->file_name());
            QFileInfo fi(fullPath);
            imageLabel->setText(fi.fileName());  // Show basename only
            imageLabel->setToolTip(fullPath);    // Full path in tooltip
        } else {
            // Defensive: should not happen in normal flow
            imageLabel->setText(FilePanel::tr("(No image)"));
            imageLabel->setToolTip("");
        }
    }
}

void FilePanel::updateTable()
{
    const dsk_tools::FSCaps funcs = m_filesystem->getCaps();

    image_model->removeRows(0, image_model->rowCount());

    foreach (const dsk_tools::UniversalFile & f, m_files) {
        QList<QStandardItem*> items;

        if (dsk_tools::hasFlag(funcs, dsk_tools::FSCaps::Protect)) {
            auto * protect_item = new QStandardItem();
            protect_item->setText((f.is_protected)?"*":"");
            protect_item->setTextAlignment(Qt::AlignCenter);
            items.append(protect_item);
        }

        if (dsk_tools::hasFlag(funcs, dsk_tools::FSCaps::Types)) {
            auto * type_item = new QStandardItem();
            type_item->setText(QString::fromStdString(f.type_label));
            type_item->setTextAlignment(Qt::AlignCenter);
            items.append(type_item);
        }

        QString file_name = QString::fromStdString(f.name);

        auto * size_item = new QStandardItem();
        size_item->setText((file_name != "..")?HostModel::formatSize(f.size):"");
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(size_item);

        QStandardItem *nameItem;
        if (f.is_dir) {
            nameItem = new QStandardItem("[" + file_name + "]");
            QFont dirFont;
            dirFont.setBold(true);
            if (f.is_deleted) dirFont.setStrikeOut(true);
            nameItem->setFont(dirFont);
        } else {
            nameItem = new QStandardItem(file_name);
            if (f.is_deleted) {
                QFont fileFont;
                fileFont.setStrikeOut(true);
                nameItem->setFont(fileFont);
            }
        }
        items.append(nameItem);
        image_model->appendRow( items );
    }


#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    for (int row = 0; row < tableView->model()->rowCount(); ++row) {
        tableView->setRowHeight(row, 24);
    }
#endif


    // Set current index (focus) to first row, but don't select it
    // Selection should only happen through explicit user actions (Insert, +/-, right-click)
    int rowCount = image_model->rowCount();
    if (rowCount > 0) {
        QModelIndex firstIndex = image_model->index(0, 0);
        if (firstIndex.isValid()) {
            tableView->setCurrentIndex(firstIndex);
            tableView->setFocus(Qt::OtherFocusReason);
        }
    }

    // Ensure no accidental selection after populating model
    tableView->clearSelection();
}

void FilePanel::dir()
{
    dsk_tools::Files files;

    const dsk_tools::Result res = m_filesystem->dir(files, m_show_deleted);
    if (!res) {
        QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error reading files list!"));
    }

    m_files.clear();
    if (getSortOrder() != HostModel::SortOrder::NoOrder) {
        // Separate directories and files
        dsk_tools::Files dirs_only;
        dsk_tools::Files files_only;

        for (const dsk_tools::UniversalFile& f : files) {
            if (f.is_dir) {
                dirs_only.push_back(f);
            } else {
                files_only.push_back(f);
            }
        }

        // Sort directories and files separately
        auto sortByName = [this](const dsk_tools::UniversalFile& a, const dsk_tools::UniversalFile& b) {
            return m_sort_ascending? a.name<b.name : a.name>=b.name;
        };
        auto sortBySize = [this](const dsk_tools::UniversalFile& a, const dsk_tools::UniversalFile& b) {
            return m_sort_ascending? a.size<b.size : a.size>=b.size;
        };

        if (getSortOrder() == HostModel::SortOrder::ByName) {
            std::sort(dirs_only.begin(), dirs_only.end(), sortByName);
            std::sort(files_only.begin(), files_only.end(), sortByName);
        } else {
            std::sort(dirs_only.begin(), dirs_only.end(), sortByName);  // Dirs still by name
            std::sort(files_only.begin(), files_only.end(), sortBySize);
        }
        m_files.insert(m_files.end(), dirs_only.begin(), dirs_only.end());
        m_files.insert(m_files.end(), files_only.begin(), files_only.end());
    }  else {
        m_files = files;
    }
    updateTable();
}

QStringList FilePanel::selectedPaths() const {
    QStringList paths;
    if (mode != panelMode::Host) return paths;  // Only works in Host mode
    if (!tableView->selectionModel()) return paths;
    const auto rows = tableView->selectionModel()->selectedRows(0);
    for (const auto& idx : rows) {
        QString path = host_model->filePath(idx);
        if (!path.isEmpty()) {  // Skip [..] entry which has empty path
            paths << path;
        }
    }
    if (paths.empty()) {
        QModelIndex current = tableView->currentIndex();
        if (current.isValid())
            paths << host_model->filePath(current);
    }
    return paths;
}

int FilePanel::selectedCount() const {
    if (mode == panelMode::Host) {
        const int selected_count = selectedPaths().size();
        if (selected_count > 0)
            return selected_count;
    } else {
        QItemSelectionModel * selection = tableView->selectionModel();
        if (selection->hasSelection()) {
            QModelIndexList rows = selection->selectedRows();
            return rows.size();
        }
    }
    return tableView->currentIndex().isValid()?1:0;
}

bool FilePanel::isIndexValid() const {
        return tableView->currentIndex().isValid();
}

QString FilePanel::currentFilePath() const {
    if (mode != panelMode::Host) return QString();  // Only works in Host mode
    if (!tableView->selectionModel()) return QString();

    QModelIndex current = tableView->currentIndex();
    if (!current.isValid()) return QString();

    return host_model->filePath(current);
}

void FilePanel::refresh() {
    if (mode == panelMode::Host) {
        setDirectory(currentPath);
    } else {
        dir();
    }
    updateImageStatusIndicator();
}

void FilePanel::setActive(bool active) {
    if (tableView) {
        tableView->setActive(active);
    }
}

void FilePanel::focusList() {
    if (tableView) tableView->setFocus();
}

bool FilePanel::eventFilter(QObject* obj, QEvent* ev) {
    // Handle activation events for non-table controls
    // FileView handles its own events internally
    if ((ev->type() == QEvent::FocusIn || ev->type() == QEvent::MouseButtonPress)) {
        // Only emit activated for controls other than tableView
        // (tableView emits its own activated signal)
        if (obj != tableView && obj != tableView->viewport()) {
            emit activated(this);
        }
    }

    return QWidget::eventFilter(obj, ev);
}

void FilePanel::setSortOrder(HostModel::SortOrder order)
{
    if (m_sort_order == order)
        m_sort_ascending = !m_sort_ascending;
    else
        m_sort_ascending = true;

    m_sort_order = order;
    m_settings->setValue("directory/" + m_ini_label + "_sort_order", static_cast<int>(order));
    m_settings->setValue("directory/" + m_ini_label + "_sort_ascending", m_sort_ascending);
    if (mode == panelMode::Host && host_model) {
        host_model->setSortOrder(order, m_sort_ascending);
    } else {
        dir();
    }
    emit sortOrderChanged(order);
 }

int FilePanel::getSortOrder() const
{
    return m_sort_order;
}

void FilePanel::setShowDeleted(bool show) {
    if (m_show_deleted == show) return;  // No change

    m_show_deleted = show;
    m_settings->setValue("directory/" + m_ini_label + "_show_deleted", show ? 1 : 0);

    // Only refresh if in Image mode and filesystem is loaded
    if (mode == panelMode::Image && m_filesystem) {
        dir();
    }
}

bool FilePanel::allowPutFiles() const {
    return getMode() == panelMode::Host || dsk_tools::hasFlag(m_filesystem->getCaps(), dsk_tools::FSCaps::Add);
}

dsk_tools::Files FilePanel::getSelectedFiles() const {
    dsk_tools::Files files;

    if (mode == panelMode::Host) {
        QStringList paths = selectedPaths();
        foreach (const QString & path, paths) {
            QFileInfo fi(path);
            std::string fn = _toStdString(fi.fileName());
            dsk_tools::UniversalFile f;
            f.fs = dsk_tools::FS::Host;
            f.name = fn;
            f.original_name = dsk_tools::strToBytes(fn);
            if (fi.isDir()) {
                f.is_dir = true;
            } else {
                f.size = fi.size();
                f.is_dir = false;
                f.is_protected = false;
                f.is_deleted = false;
                f.type_preferred = dsk_tools::PreferredType::Binary;
                f.attributes = 0;
            }
            // Metadata stores a full path
            f.metadata = dsk_tools::strToBytes(_toStdString(path));
            files.push_back(f);
        }
    } else {
        QItemSelectionModel * selection = tableView->selectionModel();
        if (selection->hasSelection()) {
            QModelIndexList rows = selection->selectedRows();
            if (!rows.empty()) {
                for (auto index : rows) {
                    files.push_back(m_files[index.row()]);
                }
            }
        } else {
            QModelIndex index = tableView->currentIndex();
            if (index.isValid()) {
                files.push_back(m_files[index.row()]);
            }
        }
    }

    return files;
}

void FilePanel::saveImage()
{
    emit activated(this);
    FileOperations::saveImage(this, this);
}

void FilePanel::saveImageAs()
{
    emit activated(this);
    FileOperations::saveImageAs(this, this);
}

// ============================================================================
// Directory history implementation
// ============================================================================

void FilePanel::loadDirectoryHistory() {
    QString historyKey = "directory/" + m_ini_label + "_history";
    m_directoryHistory = m_settings->value(historyKey, QStringList()).toStringList();

    // Ensure max 10 items
    if (m_directoryHistory.size() > 10) {
        m_directoryHistory = m_directoryHistory.mid(0, 10);
    }
}

void FilePanel::saveDirectoryHistory() {
    QString historyKey = "directory/" + m_ini_label + "_history";
    m_settings->setValue(historyKey, m_directoryHistory);
}

void FilePanel::addToDirectoryHistory(const QString& path) {
    if (path.isEmpty()) return;

    // Remove if already exists (to move it to front)
    m_directoryHistory.removeAll(path);

    // Add to front
    m_directoryHistory.prepend(path);

    // Keep max 10 items
    if (m_directoryHistory.size() > 10) {
        m_directoryHistory.removeLast();
    }

    saveDirectoryHistory();
}

void FilePanel::updateHistoryMenu() {
    historyMenu->clear();

    // Remove non-existent directories from history
    QStringList validHistory;
    bool historyChanged = false;
    for (const QString& path : m_directoryHistory) {
        if (QDir(path).exists()) {
            validHistory.append(path);
        } else {
            historyChanged = true;
        }
    }

    if (historyChanged) {
        m_directoryHistory = validHistory;
        saveDirectoryHistory();
    }

    if (m_directoryHistory.isEmpty()) {
        QAction* emptyAction = historyMenu->addAction(FilePanel::tr("(No history)"));
        emptyAction->setEnabled(false);
        return;
    }

    // Add each directory to menu with shortened display
    for (const QString& path : m_directoryHistory) {
        // Shorten path with ellipsis in middle if too long
        QString displayPath = path;
        const int maxLength = 60;
        if (displayPath.length() > maxLength) {
            // Find a good split point (after drive letter on Windows, after / on Unix)
            int splitPos = displayPath.indexOf('/', 3);
            if (splitPos == -1) splitPos = displayPath.indexOf('\\', 3);
            if (splitPos == -1) splitPos = 10;

            QString start = displayPath.left(splitPos + 1);
            QString end = displayPath.right(maxLength - splitPos - 4);
            displayPath = start + "..." + end;
        }

        QAction* action = historyMenu->addAction(displayPath);
        action->setData(path);  // Store full path in data
        action->setToolTip(path);  // Show full path in tooltip
    }

    // Add separator and clear history option
    historyMenu->addSeparator();
    QAction* clearAction = historyMenu->addAction(FilePanel::tr("Clear history"));
    clearAction->setData("__clear__");
}

void FilePanel::onHistoryMenuTriggered(QAction* action) {
    if (!action) return;

    QString data = action->data().toString();

    if (data == "__clear__") {
        m_directoryHistory.clear();
        saveDirectoryHistory();
        updateHistoryMenu();
    } else if (!data.isEmpty()) {
        // Navigate to selected directory
        setDirectory(data);
    }
}

QString FilePanel::getSelectedFormat() const {
    return filterCombo->itemData(filterCombo->currentIndex()).toString();
}

QString FilePanel::getSelectedType() const {
    return typeCombo->itemData(typeCombo->currentIndex()).toString();
}

void FilePanel::storeTableState()
{
    if (!tableView) return;

    // Store current row index
    const QModelIndex currentIdx = tableView->currentIndex();
    const int row = currentIdx.isValid() ? currentIdx.row() : 0;

    // Store vertical scroll bar position
    const int scroll = tableView->verticalScrollBar() ? tableView->verticalScrollBar()->value() : 0;

    // Push state onto the stack
    m_tableStateStack.push_back({row, scroll});
}

void FilePanel::restoreTableState()
{
    if (!tableView || !tableView->model() || m_tableStateStack.empty()) return;

    // Pop state from the stack
    const auto savedState = m_tableStateStack.back();
    const int savedRow = savedState.first;
    const int savedScroll = savedState.second;
    m_tableStateStack.pop_back();

    // Restore current row if valid
    const int maxRow = tableView->model()->rowCount() - 1;
    if (maxRow >= 0) {
        const int rowToRestore = std::min(savedRow, maxRow);
        const QModelIndex index = tableView->model()->index(rowToRestore, 0);
        if (index.isValid()) {
            tableView->selectionModel()->clearSelection();
            tableView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        }
    }

    // Restore vertical scroll bar position
    if (tableView->verticalScrollBar()) {
        tableView->verticalScrollBar()->setValue(savedScroll);
    }
}

