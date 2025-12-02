// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File panel UI

#include <QFileDialog>
#include <QHeaderView>
#include <QUrl>
#include <QJsonArray>
#include <QMessageBox>
#include <QIcon>
#include <QDebug>
#include <QJsonObject>
#include <QLocale>
#include <QDesktopServices>
#include <QInputDialog>
#include <QDialog>
#include <QFont>
#include <memory>
#include <fstream>
#include "placeholders.h"

#include "./ui_fileinfodialog.h"

#include "FilePanel.h"
#include "mainutils.h"
#include "viewdialog.h"
#include "fileparamdialog.h"
#include "convertdialog.h"
#include "fs_host.h"

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

QString HostModel::formatSize(qint64 size) const {
    QString numStr = QString::number(size);
    QString result;
    int count = 0;
    for (int i = numStr.length() - 1; i >= 0; --i) {
        if (count == 3) {
            result.prepend(' ');
            count = 0;
        }
        result.prepend(numStr[i]);
        count++;
    }
    return result;
}

QString HostModel::formatDate(const QDateTime& dt) const {
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
    QFont font("Consolas", 10, 400);

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

    auto *topContainer = new QWidget(topToolBar);
    auto *topLayout = new QHBoxLayout(topContainer);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(10);
    topLayout->addWidget(upButton);
    topLayout->addWidget(dirEdit, 1);
    topLayout->addWidget(dirButton);

    topToolBar->addWidget(topContainer);

    connect(upButton,  &QToolButton::clicked, this, &FilePanel::onGoUp);
    connect(dirButton, &QToolButton::clicked, this, &FilePanel::chooseDirectory);
    connect(dirEdit,   &QLineEdit::returnPressed, this, &FilePanel::onPathEntered);

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
                         static_cast<QObject*>(dirEdit)})
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
        }
    } else {
        if (m_filesystem->is_root()) {
            setMode(panelMode::Host);
            setDirectory(currentPath);
        } else {
            m_filesystem->cd_up();
            dir();
        }
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

    if (!index.isValid())
        return;

    if (mode == panelMode::Host) {
        QString displayName = host_model->data(index, Qt::DisplayRole).toString();

        // Process [..] navigation
        if (displayName == "[..]") {
            // Store current directory name for cursor restoration
            m_lastDirName = QDir(currentPath).dirName();
            host_model->goUp();
            currentPath = host_model->currentPath();
            dirEdit->setText(currentPath);
            m_settings->setValue("directory/"+m_ini_label, currentPath);

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
            return;
        }

        const QString path = host_model->filePath(index);
        if (path.isEmpty()) return;

        QFileInfo info(path);

        if (info.isDir()) {
            setDirectory(path);
        } else {
            int res = openImage(path);
            if (res != 0 ) {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Can't detect type of the file automatically"));
            }
        }
    } else {
        auto f = m_files[index.row()];
        if (f.is_dir){
            m_filesystem->cd(f);
            dir();
        } else {
            dsk_tools::BYTES data;
            m_filesystem->get_file(f, "", data);

            if (data.size() > 0) {
                QDialog * w = new ViewDialog(this, m_settings, QString::fromStdString(f.name), data, f.type_preferred, f.is_deleted, m_image, m_filesystem);
                w->setAttribute(Qt::WA_DeleteOnClose);
                w->setWindowTitle(w->windowTitle() + " (" + QString::fromStdString(f.name) + ")");
                w->show();
            } else {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("File reading error!"));
            }
        }
    }
}

int FilePanel::openImage(QString path)
{
    // Detecting formats if necessary
    std::string format_id;
    std::string type_id;
    std::string filesystem_id;

    QFileInfo fileInfo(path);
    std::string file_name = _toStdString(fileInfo.absoluteFilePath());
    QString selected_format = filterCombo->itemData(filterCombo->currentIndex()).toString();
    if (autoCheck->isChecked()) {
        int res = dsk_tools::detect_fdd_type(file_name, format_id, type_id, filesystem_id);
        if (res != FDD_DETECT_OK ) {
            return res;
        } else {
            setComboBoxByItemData(filterCombo, (selected_format != "FILE_ANY")?QString::fromStdString(format_id):"");
            setComboBoxByItemData(typeCombo, QString::fromStdString(type_id));
            setComboBoxByItemData(fsCombo, QString::fromStdString(filesystem_id));
        }
    } else {
        type_id = "";
        filesystem_id = "";
        if (selected_format != "FILE_ANY") {
            format_id = filterCombo->itemData(filterCombo->currentIndex()).toString().toStdString();
        } else {
            dsk_tools::detect_fdd_type(file_name, format_id, type_id, filesystem_id, true);
            type_id = "";
            filesystem_id = "";
        };
        if (type_id.size() == 0) type_id = typeCombo->itemData(typeCombo->currentIndex()).toString().toStdString();
        if (filesystem_id.size() == 0) filesystem_id = fsCombo->itemData(fsCombo->currentIndex()).toString().toStdString();

    }

    // Load file

    image_model->removeRows(0, image_model->rowCount());

    if (m_image != nullptr) delete m_image;
    m_image = dsk_tools::prepare_image(file_name, format_id, type_id);

    if (m_image != nullptr) {
        auto check_result = m_image->check();
        if (check_result == FDD_LOAD_OK) {
            auto load_result = m_image->load();
            if (load_result == FDD_LOAD_OK) {
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

    return FDD_LOAD_OK;
}

void FilePanel::processImage(std::string filesystem_type)
{
    if (m_filesystem != nullptr) delete m_filesystem;
    m_filesystem = dsk_tools::prepare_filesystem(m_image, filesystem_type);
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
    if (mode==panelMode::Host) {
        tableView->setModel(host_model);
        tableView->setupForHostMode();
        if (m_filesystem != nullptr) delete m_filesystem;
        m_filesystem = new dsk_tools::fsHost(nullptr);
    } else {
        tableView->setModel(image_model);
        tableView->setupForImageMode(m_filesystem->getCaps());
    }
    emit panelModeChanged(mode);
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
        size_item->setText((file_name != "..")?QString::number(f.size):"");
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

void FilePanel::onView()
{
    emit activated(this);

    if (mode==panelMode::Host) {
        QString path = currentFilePath();
        if (path.isEmpty()) return;  // Skip [..] entry or invalid index

        QFileInfo fi(path);

        if (fi.isDir()) {
            // Show directory information
            QDir dir(path);
            QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);

            int fileCount = 0;
            int dirCount = 0;
            qint64 totalSize = 0;

            for (const QFileInfo& entry : entries) {
                if (entry.isDir()) {
                    dirCount++;
                } else {
                    fileCount++;
                    totalSize += entry.size();
                }
            }

            QString info = FilePanel::tr("Directory: %1\n\n").arg(fi.fileName());
            info += FilePanel::tr("Path: %1\n").arg(fi.absoluteFilePath());
            info += FilePanel::tr("Subdirectories: %1\n").arg(dirCount);
            info += FilePanel::tr("Files: %1\n").arg(fileCount);
            info += FilePanel::tr("Total size: %1 bytes\n").arg(totalSize);
            info += FilePanel::tr("Last modified: %1").arg(QLocale().toString(fi.lastModified(), QLocale::ShortFormat));

            QMessageBox::information(this, FilePanel::tr("Directory Information"), info);
        } else {
            // Show file information
            std::string file_name = _toStdString(fi.absoluteFilePath());
            std::string type_id = "";
            std::string  filesystem_id = "";
            std::string format_id = filterCombo->itemData(filterCombo->currentIndex()).toString().toStdString();
            if (format_id == "FILE_ANY") {
                int res = dsk_tools::detect_fdd_type(file_name, format_id, type_id, filesystem_id, true);
            }
            if (type_id.size()==0) type_id = typeCombo->itemData(typeCombo->currentIndex()).toString().toStdString();

            std::unique_ptr<dsk_tools::Loader> loader;

            if (format_id == "FILE_RAW_MSB") {
                loader = std::make_unique<dsk_tools::LoaderRAW>(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_AIM") {
                loader = std::make_unique<dsk_tools::LoaderAIM>(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_MFM_NIC") {
                loader = std::make_unique<dsk_tools::LoaderNIC>(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_MFM_NIB") {
                loader = std::make_unique<dsk_tools::LoaderNIB>(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_HXC_MFM") {
                loader = std::make_unique<dsk_tools::LoaderHXC_MFM>(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_HXC_HFE") {
                loader = std::make_unique<dsk_tools::LoaderHXC_HFE>(file_name, format_id, type_id);
            } else {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Not supported yet"));
                return;
            }
            QDialog * file_info = new QDialog(this);

            Ui_FileInfo fileinfoUi;
            fileinfoUi.setupUi(file_info);

            QString info = replacePlaceholders(QString::fromStdString(loader->file_info()));
            QFont font("Consolas", 10, 400);
            fileinfoUi.textBox->setFont(font);
            fileinfoUi.textBox->setPlainText(info);
            file_info->exec();
        }
    } else {
        // Image mode: behave like double-click/Enter
        QModelIndex index = tableView->currentIndex();
        if (index.isValid()) {
            onItemDoubleClicked(index);
        }
    }
}

void FilePanel::onFileInfo()
{
    emit activated(this);

    if (mode==panelMode::Host) {
    } else {
        const auto file_info = new QDialog(this);

        Ui_FileInfo fileinfoUi{};
        fileinfoUi.setupUi(file_info);

        QModelIndex index = tableView->currentIndex();
        if (index.isValid()) {
            auto f = m_files[index.row()];

            auto info = replacePlaceholders(QString::fromStdString(m_filesystem->file_info(f)));

            QFont font("Consolas", 10, 400);
            fileinfoUi.textBox->setFont(font);
            fileinfoUi.textBox->setPlainText(info);
            file_info->exec();
        }
    }
}

void FilePanel::onEdit()
{
    emit activated(this);

    if (mode==panelMode::Host) {
        // QString path = currentFilePath();
        // if (path.isEmpty()) return;  // Skip [..] entry or invalid index
        //
        // // Open file with system default application
        // QUrl fileUrl = QUrl::fromLocalFile(path);
        // if (!QDesktopServices::openUrl(fileUrl)) {
        //     QMessageBox::warning(this, FilePanel::tr("Error"),
        //                        FilePanel::tr("Could not open file with system default application"));
        // }
        QModelIndex index = tableView->currentIndex();
        if (index.isValid()) {
            onItemDoubleClicked(index);
        }
    } else {
        QModelIndex index = tableView->currentIndex();
        if (index.isValid()) {
            auto f = m_files[index.row()];
            std::vector<dsk_tools::ParameterDescription> params = m_filesystem->file_get_metadata(f);

            for (auto & param : params) {
                param.name = replacePlaceholders(QString::fromStdString(param.name)).toStdString();
            }

            FileParamDialog dialog(params);
            if (dialog.exec() == QDialog::Accepted) {
                const auto values = dialog.getParameters();
                m_filesystem->file_set_metadata(f, values);
                dir();
            }
        }
    }
}

void FilePanel::onMkDir()
{
    dsk_tools::FSCaps funcs = m_filesystem->getCaps();

    if (dsk_tools::hasFlag(funcs, dsk_tools::FSCaps::MkDir)) {
        bool ok{};
        const QString text = QInputDialog::getText(this, FilePanel::tr("Add a directory"),
                                             FilePanel::tr("Directory name:"),
                                             QLineEdit::Normal,
                                             "New",
                                             &ok);
        if (ok && !text.isEmpty()) {
            dsk_tools::UniversalFile new_dir;
            const dsk_tools::Result res = m_filesystem->mkdir(text.toStdString(), new_dir);
            if (!res) {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error creating directory: ") + decodeError(res));
            }
            refresh();
        }
    }

}


void FilePanel::setSortOrder(HostModel::SortOrder order)
{
    if (m_sort_order == order)
        m_sort_ascending = !m_sort_ascending;
    else
        m_sort_ascending = true;

    m_sort_order = order;
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

QString FilePanel::decodeError(const dsk_tools::Result & result)
{
    QString error;
    switch (result.code) {
        case dsk_tools::ErrorCode::Ok:
            error = tr("No error");
            break;
        case dsk_tools::ErrorCode::NotImplementedYet:
            error = tr("Not implemented yet");
            break;
        case dsk_tools::ErrorCode::FileAddErrorSpace:
            error = tr("No enough free space");
            break;
        case dsk_tools::ErrorCode::FileAddErrorAllocateDirEntry:
            error = tr("Can't allocate a directory entry");
            break;
        case dsk_tools::ErrorCode::FileAddErrorAllocateSector:
            error = tr("Can't allocate a sector");
            break;
        case dsk_tools::ErrorCode::DirNotEmpty:
            error = tr("Directory is not empty");
            break;
        case dsk_tools::ErrorCode::DirErrorSpace:
            error = tr("No enough free space");
            break;
        case dsk_tools::ErrorCode::DirErrorAllocateDirEntry:
            error = tr("Can't allocate a directory entry");
            break;
        case dsk_tools::ErrorCode::DirErrorAllocateSector:
            error = tr("Can't allocate a sector");
            break;
        case dsk_tools::ErrorCode::FileAlreadyExists:
            error = tr("File already exists");
            break;
        case dsk_tools::ErrorCode::DirAlreadyExists:
            error = tr("Directory already exists");
            break;
        case dsk_tools::ErrorCode::DirError:
            error = tr("Error creating a directory");
            break;
        default:
            error = tr("Unknown error");
            break;
    }

    if (!result.message.empty()) error += ": " + QString::fromStdString(result.message);

    return error;
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

void FilePanel::putFiles(dsk_tools::fileSystem* sourceFs, const dsk_tools::Files & files, const QString & format, const bool copy)
{
    const std::string std_format = format.toStdString();
    foreach (const dsk_tools::UniversalFile & f, files) {
        if (f.is_dir) {
            if (f.name != "..") {
                dsk_tools::UniversalFile new_dir;
                const auto mkdir_result = m_filesystem->mkdir(f, new_dir);
                if (mkdir_result) {
                    // Getting files
                    dsk_tools::Files dir_files;
                    sourceFs->cd(f);
                    sourceFs->dir(dir_files, false);
                    sourceFs->cd_up();

                    // Putting files
                    m_filesystem->cd(new_dir);
                    putFiles(sourceFs, dir_files, format, copy);
                    m_filesystem->cd_up();
                } else {
                    const QMessageBox::StandardButton res = QMessageBox::critical(
                        this,
                        FilePanel::tr("Error"),
                        FilePanel::tr("Error creating directory '%1': %2. Continue?").arg(QString::fromUtf8(f.name), decodeError(mkdir_result)),
                        QMessageBox::Yes | QMessageBox::No
                    );
                    if (res != QMessageBox::Yes) break;
                }
            }
        } else {
            dsk_tools::BYTES data;
            auto get_result = sourceFs->get_file(f, std_format, data);
            if (get_result) {
                auto put_result = m_filesystem->put_file(f, std_format, data, false);
                if (!put_result) {
                    // Check if file already exists
                    if (put_result.code == dsk_tools::ErrorCode::FileAlreadyExists) {
                        const QMessageBox::StandardButton res = QMessageBox::question(
                            this,
                            FilePanel::tr("File exists"),
                            FilePanel::tr("File '%1' already exists. Overwrite?").arg(QString::fromStdString(f.name)),
                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                        );

                        if (res == QMessageBox::Yes) {
                            // Retry with force_replace flag
                            put_result = m_filesystem->put_file(f, std_format, data, true);
                            if (!put_result) {
                                QString error_message = decodeError(put_result);
                                QMessageBox::critical(this,
                                    FilePanel::tr("Error"),
                                    FilePanel::tr("Error writing file '%1': %2")
                                            .arg(QString::fromStdString(f.name), error_message)
                                );
                            }
                        } else if (res == QMessageBox::Cancel) {
                            // Stop all operations
                            break;
                        }
                        // If No: continue to next file (implicit - loop continues)
                    } else if (put_result.code == dsk_tools::ErrorCode::NotImplementedYet) {
                        QMessageBox::critical(
                            this,
                            FilePanel::tr("Error"),
                            FilePanel::tr("Writing for this type of file system is not implemented yet")
                        );
                        break;
                    } else {
                        QString error_message = decodeError(put_result);
                        QMessageBox::critical(this,
                            FilePanel::tr("Error"),
                            FilePanel::tr("Error writing file '%1': %2")
                                    .arg(QString::fromStdString(f.name), error_message)
                        );
                    }
                }
            } else {
                QMessageBox::critical(
                    this,
                    FilePanel::tr("Error"),
                    FilePanel::tr("Error reading file '%1'").arg(QString::fromStdString(f.name))
                );
            }
        }
    }
    // refresh();
}

void FilePanel::deleteRecursively(const dsk_tools::UniversalFile & f)
{
    if (mode == panelMode::Host) {
        // Extract full directory path from metadata
        QString path_to_delete = QString::fromStdString(dsk_tools::bytesToString(f.metadata));

        // Create QDir object and validate
        QDir dir(path_to_delete);
        if (!dir.exists()) {
            QMessageBox::critical(
                this,
                FilePanel::tr("Error"),
                FilePanel::tr("Directory '%1' not found").arg(path_to_delete)
            );
            return;
        }

        // Perform recursive deletion
        if (!dir.removeRecursively()) {
            QMessageBox::critical(
                this,
                FilePanel::tr("Error"),
                FilePanel::tr("Error deleting directory '%1'").arg(QString::fromStdString(f.name))
            );
        }
    } else {
        // Image mode: not implemented yet
        QMessageBox::information(
            this,
            FilePanel::tr("Not Implemented"),
            FilePanel::tr("Recursive directory deletion in image mode is not yet implemented")
        );
    }
}

void FilePanel::deleteFiles()
{
    dsk_tools::Files files = getSelectedFiles();
    if (!files.empty()) {
        const QMessageBox::StandardButton reply_all = QMessageBox::question(this,
                            FilePanel::tr("Deleting files"),
                            FilePanel::tr("Delete %1 files?").arg(files.size()),
                            QMessageBox::Yes|QMessageBox::No
        );
        if (reply_all == QMessageBox::Yes) {
            bool recursively = false;
            foreach (const dsk_tools::UniversalFile & f, files) {
                if (f.is_dir) {
                    if (!recursively) {
                        const QMessageBox::StandardButton reply_dir = QMessageBox::question(this,
                                            FilePanel::tr("Deleting directories"),
                                            FilePanel::tr("'%1' is a directory. Delete it recursively?").arg(QString::fromUtf8(f.name)),
                                            QMessageBox::Yes|QMessageBox::No
                        );
                        if (reply_dir == QMessageBox::Yes) {
                            recursively = true;
                            deleteRecursively(f);
                        }
                    } else {
                        // User already confirmed recursive deletion for a previous directory
                        deleteRecursively(f);
                    }
                } else {
                    auto result = m_filesystem->delete_file(f);
                    if (!result) {
                        QMessageBox::critical(
                            this,
                            FilePanel::tr("Error"),
                            FilePanel::tr("Error deleting file '%1'").arg(QString::fromStdString(f.name))
                        );
                    }
                }
            }
            refresh();
        }
    }
}

void FilePanel::onRename()
{
    if (!m_filesystem) return;

    const dsk_tools::Files files = getSelectedFiles();

    // Rename only works on single files
    if (files.size() != 1) {
        QMessageBox::information(this,
                                FilePanel::tr("Rename"),
                                FilePanel::tr("Please select exactly one file to rename."));
        return;
    }

    const dsk_tools::UniversalFile & file = files[0];

    // Don't allow renaming parent directory entry
    if (file.name == "..") {
        return;
    }

    QString old_name = QString::fromStdString(file.name);
    bool ok{};
    QString new_name = QInputDialog::getText(
        this,
        FilePanel::tr("Rename"),
        FilePanel::tr("New name:"),
        QLineEdit::Normal,
        old_name,
        &ok
    );

    if (ok && !new_name.isEmpty()) {
        // Don't rename if the name hasn't changed
        if (new_name == old_name) {
            return;
        }

        auto result = m_filesystem->rename_file(file, new_name.toStdString());
        if (!result) {
            QMessageBox::critical(
                this,
                FilePanel::tr("Error"),
                FilePanel::tr("Error renaming file '%1': %2").arg(
                    old_name,
                    QString::fromStdString(decodeError(result).toStdString())
                )
            );
        } else {
            refresh();
        }
    }
}

// ============================================================================
// Image menu operations
// ============================================================================

void FilePanel::showImageInfo()
{
    emit activated(this);

    if (mode == panelMode::Host) {
        // Host mode: show information about selected disk image file
        QString path = currentFilePath();
        if (path.isEmpty()) return;

        QFileInfo fi(path);
        if (fi.isDir()) return;

        std::string file_name = _toStdString(fi.absoluteFilePath());
        std::string type_id = "";
        std::string filesystem_id = "";
        std::string format_id = filterCombo->itemData(filterCombo->currentIndex()).toString().toStdString();

        // Auto-detect format if necessary
        if (format_id == "FILE_ANY") {
            int res = dsk_tools::detect_fdd_type(file_name, format_id, type_id, filesystem_id, true);
        }
        if (type_id.size() == 0) {
            type_id = typeCombo->itemData(typeCombo->currentIndex()).toString().toStdString();
        }

        // Create appropriate loader
        dsk_tools::Loader* loader = createLoader(file_name, format_id, type_id);

        if (!loader) {
            QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Not supported yet"));
            return;
        }

        // Display info
        showInfoDialog(loader->file_info());

        delete loader;
    } else {
    }
}

void FilePanel::showFSInfo()
{
    emit activated(this);

    if (mode == panelMode::Host) {
    } else {
        // Image mode: show information about currently loaded disk image
        if (!m_image) return;
        const std::string info = m_filesystem->information();
        showInfoDialog(info);
    }
}

dsk_tools::Loader* FilePanel::createLoader(const std::string& file_name,
                                           const std::string& format_id,
                                           const std::string& type_id)
{
    if (format_id == "FILE_RAW_MSB") {
        return new dsk_tools::LoaderRAW(file_name, format_id, type_id);
    } else if (format_id == "FILE_AIM") {
        return new dsk_tools::LoaderAIM(file_name, format_id, type_id);
    } else if (format_id == "FILE_MFM_NIC") {
        return new dsk_tools::LoaderNIC(file_name, format_id, type_id);
    } else if (format_id == "FILE_MFM_NIB") {
        return new dsk_tools::LoaderNIB(file_name, format_id, type_id);
    } else if (format_id == "FILE_HXC_MFM") {
        return new dsk_tools::LoaderHXC_MFM(file_name, format_id, type_id);
    } else if (format_id == "FILE_HXC_HFE") {
        return new dsk_tools::LoaderHXC_HFE(file_name, format_id, type_id);
    }
    return nullptr;
}

void FilePanel::showInfoDialog(const std::string& info)
{
    QDialog* file_info = new QDialog(this);

    Ui_FileInfo fileinfoUi;
    fileinfoUi.setupUi(file_info);

    QString text = replacePlaceholders(QString::fromStdString(info));
    QFont font("Consolas", 10, 400);
    fileinfoUi.textBox->setFont(font);
    fileinfoUi.textBox->setPlainText(text);

    file_info->exec();
    delete file_info;
}

void FilePanel::saveImage()
{
    emit activated(this);

    qDebug() << "FilePanel::saveImage()";

    // TODO: Implement save to original file
    QMessageBox::information(this, FilePanel::tr("Save"),
        FilePanel::tr("Save functionality not yet implemented"));
}

void FilePanel::saveImageAs()
{
    emit activated(this);

    if (mode != panelMode::Image || !m_image || !m_filesystem) {
        QMessageBox::critical(this, FilePanel::tr("Error"),
            FilePanel::tr("No disk image loaded"));
        return;
    }

    QString type_id = typeCombo->itemData(typeCombo->currentIndex()).toString();
    QString target_id;
    QString output_file;
    QString template_file;
    int numtracks;
    uint8_t volume_id;

    ConvertDialog dialog(this, m_settings, &m_file_types, &m_file_formats, m_image, type_id, m_filesystem->get_volume_id());
    if (dialog.exec(target_id, output_file, template_file, numtracks, volume_id) == QDialog::Accepted) {
        dsk_tools::Writer* writer = nullptr;

        std::set<QString> mfm_formats = {"FILE_HXC_MFM", "FILE_MFM_NIB", "FILE_MFM_NIC"};

        if (mfm_formats.find(target_id) != mfm_formats.end()) {
            writer = new dsk_tools::WriterHxCMFM(target_id.toStdString(), m_image, volume_id);
        } else if (target_id == "FILE_HXC_HFE") {
            writer = new dsk_tools::WriterHxCHFE(target_id.toStdString(), m_image, volume_id);
        } else if (target_id == "FILE_RAW_MSB") {
            writer = new dsk_tools::WriterRAW(target_id.toStdString(), m_image);
        } else {
            QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Not implemented!"));
            return;
        }

        dsk_tools::BYTES buffer;
        int result = writer->write(buffer);

        if (result != FDD_WRITE_OK) {
            QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error generating file"));
            delete writer;
            return;
        }

        if (numtracks > 0) {
            dsk_tools::BYTES tmplt;

            std::ifstream tf(template_file.toStdString(), std::ios::binary);
            if (!tf.good()) {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error opening template file"));
                delete writer;
                return;
            }
            tf.seekg(0, tf.end);
            auto tfsize = tf.tellg();
            tf.seekg(0, tf.beg);
            tmplt.resize(tfsize);
            tf.read(reinterpret_cast<char*>(tmplt.data()), tfsize);
            if (!tf.good()) {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error reading template file"));
                delete writer;
                return;
            }

            result = writer->substitute_tracks(buffer, tmplt, numtracks);
            if (result != FDD_WRITE_OK) {
                if (result == FDD_WRITE_INCORECT_TEMPLATE)
                    QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("The selected template cannot be used - it must be the same type and size as the target."));
                else if (result == FDD_WRITE_INCORECT_SOURCE)
                    QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Incorrect source data for tracks replacement."));
                else
                    QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error inserting tracks from template"));
                delete writer;
                return;
            }
        }

        std::ofstream file(output_file.toStdString(), std::ios::binary);

        if (file.good()) {
            file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
            QMessageBox::information(this, FilePanel::tr("Success"),
                FilePanel::tr("File saved successfully"));
        } else {
            QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error writing file to disk"));
        }

        delete writer;
    }
}