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

#include "./ui_fileinfodialog.h"

#include "FilePanel.h"
#include "mainutils.h"
#include "viewdialog.h"

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

void HostModel::setSortOrder(SortOrder order) {
    m_sortOrder = order;
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
    auto sortByName = [](const QFileInfo& a, const QFileInfo& b) {
        return QString::localeAwareCompare(a.fileName(), b.fileName()) < 0;
    };
    auto sortBySize = [](const QFileInfo& a, const QFileInfo& b) {
        return a.size() < b.size();
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

void FilePanel::setupFilters()
{
    QString filter_def = m_settings->value("directory/"+m_ini_label+"_file_filter", "").toString();
    QString type_def = m_settings->value("directory/"+m_ini_label+"_type_filter", "").toString();
    QString filesystem_def = m_settings->value("directory/"+m_ini_label+"_filesystem", "").toString();

    // Extensions filter ------------------------------------------------------
    filterCombo = new QComboBox(this);

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

        QString path = host_model->filePath(index);
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
        dsk_tools::fileData f = m_files[index.row()];

        if (f.is_dir){
            m_filesystem->cd(f);
            dir();
        } else {
            dsk_tools::BYTES data = m_filesystem->get_file(f);

            if (data.size() > 0) {
                QDialog * w = new ViewDialog(this, m_settings, QString::fromStdString(f.name), data, f.preferred_type, f.is_deleted, m_image, m_filesystem);
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

        dir();
        tableView->setModel(image_model);
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
    } else {
        tableView->setModel(image_model);
        tableView->setupForImageMode(m_filesystem->get_capabilities());
    }
}

void FilePanel::updateTable()
{
    int funcs = m_filesystem->get_capabilities();

    image_model->removeRows(0, image_model->rowCount());

    foreach (const dsk_tools::fileData & f, m_files) {
        QList<QStandardItem*> items;

        if (funcs & FILE_PROTECTION) {
            QStandardItem * protect_item = new QStandardItem();
            protect_item->setText((f.is_protected)?"*":"");
            protect_item->setTextAlignment(Qt::AlignCenter);
            items.append(protect_item);
        }

        if (funcs & FILE_TYPE) {
            QStandardItem * type_item = new QStandardItem();
            type_item->setText(QString::fromStdString(f.type_str_short));
            type_item->setTextAlignment(Qt::AlignCenter);
            items.append(type_item);
        }

        QString file_name = QString::fromStdString(f.name);

        QStandardItem * size_item = new QStandardItem();
        size_item->setText((file_name != "..")?QString::number(f.size):"");
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(size_item);

        QStandardItem *nameItem;
        if (f.is_dir) {
            nameItem = new QStandardItem("<" + file_name + ">");
            QFont dirFont;
            dirFont.setBold(true);
            if (f.is_deleted) dirFont.setStrikeOut(true);
            nameItem->setFont(dirFont);
            // nameItem->setForeground(QBrush(Qt::blue));
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

    int rowCount = image_model->rowCount();
    if (rowCount == 1) {
        QModelIndex index = image_model->index(0, 0);
        if (index.isValid()) {
            tableView->setCurrentIndex(index);
            tableView->selectionModel()->select(
                index,
                QItemSelectionModel::Select | QItemSelectionModel::Rows
                );
            tableView->setFocus(Qt::OtherFocusReason);
        }
    } else {
        // setup_buttons(false);
    }

}

void FilePanel::dir()
{
    // bool show_deleted = ui->deletedBtn->isChecked();
    bool show_deleted = true;

    int dir_res;
    try {
        dir_res = m_filesystem->dir(&m_files, show_deleted);
    } catch (...) {
        dir_res = FDD_OP_ERROR;
    }

    if (dir_res != FDD_OP_OK) {
        QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Error reading files list!"));
    }

    // if (ui->sortBtn->isChecked()) {
    //     std::sort(m_files.begin(), m_files.end(), [](const dsk_tools::fileData &a, const dsk_tools::fileData &b) {
    //         if (a.is_dir != b.is_dir)
    //             return a.is_dir > b.is_dir;
    //         return a.name < b.name;
    //     });
    // }

    updateTable();
    // setup_buttons(false);
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
    return paths;
}

QString FilePanel::currentFilePath() const {
    if (mode != panelMode::Host) return QString();  // Only works in Host mode
    if (!tableView->selectionModel()) return QString();

    QModelIndex current = tableView->currentIndex();
    if (!current.isValid()) return QString();

    return host_model->filePath(current);
}

void FilePanel::refresh() {
    setDirectory(currentPath);
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

QString FilePanel::replace_placeholders(const QString & in)
{
    return QString(in)
    .replace("{$DIRECTORY_ENTRY}", FilePanel::tr("Directory Entry"))
        .replace("{$FILE_NAME}", FilePanel::tr("File Name"))
        .replace("{$SIZE}", FilePanel::tr("File Size"))
        .replace("{$BYTES}", FilePanel::tr("byte(s)"))
        .replace("{$SIDES}", FilePanel::tr("Sides"))
        .replace("{$TRACKS}", FilePanel::tr("Tracks"))
        .replace("{$SECTORS}", FilePanel::tr("sector(s)"))
        .replace("{$ATTRIBUTES}", FilePanel::tr("Attributes"))
        .replace("{$DATE}", FilePanel::tr("Date"))
        .replace("{$TYPE}", FilePanel::tr("Type"))
        .replace("{$PROTECTED}", FilePanel::tr("Protected"))
        .replace("{$YES}", FilePanel::tr("Yes"))
        .replace("{$NO}", FilePanel::tr("No"))
        .replace("{$TS_LIST_LOCATION}", FilePanel::tr("T/S List Location"))
        .replace("{$TS_LIST_DATA}", FilePanel::tr("T/S List Contents"))
        .replace("{$INCORRECT_TS_DATA}", FilePanel::tr("Incorrect T/S data, stopping iterations"))
        .replace("{$NEXT_TS}", FilePanel::tr("Next T/S List Location"))
        .replace("{$FILE_END_REACHED}", FilePanel::tr("File End Reached"))
        .replace("{$FILE_DELETED}", FilePanel::tr("The file is marked as deleted, the data below may be incorrect"))
        .replace("{$ERROR_PARSING}", FilePanel::tr("File parsing error"))
        .replace("{$TRACK}", FilePanel::tr("Track"))
        .replace("{$TRACK_SHORT}", FilePanel::tr("T"))
        .replace("{$SIDE_SHORT}", FilePanel::tr("H"))
        .replace("{$PHYSICAL_SECTOR}", FilePanel::tr("S"))
        .replace("{$LOGICAL_SECTOR}", FilePanel::tr("S"))
        .replace("{$PARSING_FINISHED}", FilePanel::tr("Parsing finished"))
        .replace("{$VOLUME_ID}", FilePanel::tr("V"))
        .replace("{$SECTOR_INDEX}", FilePanel::tr("Index"))
        .replace("{$INDEX_MARK}", FilePanel::tr("Index Mark"))
        .replace("{$DATA_MARK}", FilePanel::tr("Data Mark"))
        .replace("{$DATA_FIELD}", FilePanel::tr("Sector Data"))
        .replace("{$SECTOR_INDEX_END_OK}", FilePanel::tr("End Mark: OK"))
        .replace("{$SECTOR_INDEX_END_ERROR}", FilePanel::tr("End Mark: Not Detected"))
        .replace("{$SECTOR_CRC_OK}", FilePanel::tr("CRC: OK"))
        .replace("{$SECTOR_CRC_ERROR}", FilePanel::tr("CRC: Error"))
        .replace("{$CRC_EXPECTED}", FilePanel::tr("Expected"))
        .replace("{$CRC_FOUND}", FilePanel::tr("Found"))
        .replace("{$SECTOR_ERROR}", FilePanel::tr("Data Error"))
        .replace("{$INDEX_CRC_OK}", FilePanel::tr("CRC: OK"))
        .replace("{$INDEX_CRC_ERROR}", FilePanel::tr("CRC: Error"))
        .replace("{$INDEX_EPILOGUE_OK}", FilePanel::tr("Epilogue: OK"))
        .replace("{$INDEX_EPILOGUE_ERROR}", FilePanel::tr("Epilogue: Error"))
        .replace("{$DATA_EPILOGUE_OK}", FilePanel::tr("Epilogue: OK"))
        .replace("{$DATA_EPILOGUE_ERROR}", FilePanel::tr("Epilogue: Error"))
        .replace("{$TRACKLIST_OFFSET}", FilePanel::tr("Track List Offset"))
        .replace("{$TRACK_OFFSET}", FilePanel::tr("Track Offset"))
        .replace("{$TRACK_SIZE}", FilePanel::tr("Track Size"))
        .replace("{$HEADER}", FilePanel::tr("Header"))
        .replace("{$SIGNATURE}", FilePanel::tr("Signature"))
        .replace("{$NO_SIGNATURE}", FilePanel::tr("No known signature found, aborting"))
        .replace("{$FORMAT_REVISION}", FilePanel::tr("Format revision"))
        .replace("{$SIDE}", FilePanel::tr("Side"))
        .replace("{$CUSTOM_DATA}", FilePanel::tr("Custom Data"))
        .replace("{$VTOC_FOUND}", FilePanel::tr("DOS 3.3 VTOC found"))
        .replace("{$VTOC_NOT_FOUND}", FilePanel::tr("DOS 3.3 VTOC not found"))
        .replace("{$VTOC_CATALOG_TRACK}", FilePanel::tr("Root Catalog Track"))
        .replace("{$VTOC_CATALOG_SECTOR}", FilePanel::tr("Root Catalog Sector"))
        .replace("{$VTOC_DOS_RELEASE}", FilePanel::tr("DOS Release"))
        .replace("{$VTOC_VOLUME_ID}", FilePanel::tr("Volume ID"))
        .replace("{$VTOC_VOLUME_NAME}", FilePanel::tr("Volume name"))
        .replace("{$VTOC_PAIRS_ON_SECTOR}", FilePanel::tr("Pairs per T/S list"))
        .replace("{$VTOC_LAST_TRACK}", FilePanel::tr("Last track"))
        .replace("{$VTOC_DIRECTION}", FilePanel::tr("Direction"))
        .replace("{$VTOC_TRACKS_TOTAL}", FilePanel::tr("Tracks total"))
        .replace("{$VTOC_SECTORS_ON_TRACK}", FilePanel::tr("Sectors on track"))
        .replace("{$VTOC_BYTES_PER_SECTOR}", FilePanel::tr("Bytes per sector"))
        .replace("{$ERROR_OPENING}", FilePanel::tr("Error opening the file"))
        .replace("{$ERROR_LOADING}", FilePanel::tr("Error loading, check if the file type is correct"))

        .replace("{$DPB_INFO}", FilePanel::tr("DPB Information"))
        .replace("{$DPB_VOLUME_ID}", FilePanel::tr("Volume ID"))
        .replace("{$DPB_TYPE}", FilePanel::tr("Device type"))
        .replace("{$DPB_DSIDE}", FilePanel::tr("DSIDE"))
        .replace("{$DPB_TSIZE}", FilePanel::tr("Blocks on track"))
        .replace("{$DPB_DSIZE}", FilePanel::tr("Tracks on disk"))
        .replace("{$DPB_MAXBLOK}", FilePanel::tr("Last block"))
        .replace("{$DPB_VTOCADR}", FilePanel::tr("VTOC block"))

        .replace("{$EXTENT}", FilePanel::tr("Extent"))
        .replace("{$FREE_SECTORS}", FilePanel::tr("Free sectors"))
        .replace("{$FREE_BYTES}", FilePanel::tr("Free bytes"))

        .replace("{$META_FILENAME}", FilePanel::tr("File Name"))
        .replace("{$META_PROTECTED}", FilePanel::tr("Protected"))
        .replace("{$META_TYPE}", FilePanel::tr("Type"))
        .replace("{$META_EXTENDED}", FilePanel::tr("Extended"))

        .replace("{$AGAT_VR_FOUND}", FilePanel::tr("Agat image VR block found"))
        .replace("{$AGAT_VR_MODE}", FilePanel::tr("Video mode"))
        .replace("{$AGAT_VR_AGAT_GMODES}", FilePanel::tr("Agat graphic"))
        .replace("{$AGAT_VR_AGAT_TMODES}", FilePanel::tr("Agat text"))
        .replace("{$AGAT_VR_A2_MODES}", FilePanel::tr("Apple II modes"))
        .replace("{$AGAT_VR_GIGA_MODES}", FilePanel::tr("Agat GigaScreen"))
        .replace("{$AGAT_VR_MAIN_PALETTE}", FilePanel::tr("Main palette"))
        .replace("{$AGAT_VR_ATL_PALETTE}", FilePanel::tr("Alternative palette"))
        .replace("{$AGAT_VR_CUSTOM_PALETTE}", FilePanel::tr("Custom palette"))
        .replace("{$AGAT_VR_COMMENT}", FilePanel::tr("Comment block"))
        .replace("{$AGAT_VR_FONT}", FilePanel::tr("Font ID"))
        .replace("{$AGAT_VR_CUSTOM_FONT}", FilePanel::tr("Custom font"))
        ;
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

            dsk_tools::Loader * loader;

            if (format_id == "FILE_RAW_MSB") {
                loader = new dsk_tools::LoaderRAW(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_AIM") {
                loader = new dsk_tools::LoaderAIM(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_MFM_NIC") {
                loader = new dsk_tools::LoaderNIC(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_MFM_NIB") {
                loader = new dsk_tools::LoaderNIB(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_HXC_MFM") {
                loader = new dsk_tools::LoaderHXC_MFM(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_HXC_HFE") {
                loader = new dsk_tools::LoaderHXC_HFE(file_name, format_id, type_id);
            } else {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Not supported yet"));
                return;
            }
            QDialog * file_info = new QDialog(this);

            Ui_FileInfo fileinfoUi;
            fileinfoUi.setupUi(file_info);

            QString info = replace_placeholders(QString::fromStdString(loader->file_info()));
            QFont font("Consolas", 10, 400);
            fileinfoUi.textBox->setFont(font);
            fileinfoUi.textBox->setPlainText(info);
            file_info->exec();
        }
    } else {

    }
}

void FilePanel::onEdit()
{
    emit activated(this);

    if (mode==panelMode::Host) {
        QString path = currentFilePath();
        if (path.isEmpty()) return;  // Skip [..] entry or invalid index

        // Open file with system default application
        QUrl fileUrl = QUrl::fromLocalFile(path);
        if (!QDesktopServices::openUrl(fileUrl)) {
            QMessageBox::warning(this, FilePanel::tr("Error"),
                               FilePanel::tr("Could not open file with system default application"));
        }
    } else {
        // TODO: Implement file metadata editing for image mode
        // This could use FileParamDialog or similar to edit DOS 3.3 file attributes
    }
}

void FilePanel::setSortOrder(HostModel::SortOrder order)
{
    if (mode == panelMode::Host && host_model) {
        host_model->setSortOrder(order);
        emit sortOrderChanged(order);
    }
}

int FilePanel::getSortOrder() const
{
    if (mode == panelMode::Host && host_model) {
        return static_cast<int>(host_model->sortOrder());
    }
    return -1;
}
