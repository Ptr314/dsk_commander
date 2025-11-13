// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File panel UI

#include <QFileDialog>
#include <QHeaderView>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QApplication>
#include <QJsonArray>
#include <QMessageBox>
#include <QIcon>
#include <QDebug>

#include "./ui_fileinfodialog.h"

#include "FilePanel.h"
#include "mainutils.h"
#include "viewdialog.h"

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
    host_model = new CustomFileSystemModel(this);
    host_model->setFilter(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::AllDirs);
    host_model->setReadOnly(true);

    // A proxy for custom sorting
    host_proxy = new CustomSortProxyModel(this);
    host_proxy->setSourceModel(host_model);
    host_proxy->setDynamicSortFilter(true);

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

void FilePanel::setDirectory(const QString& path) {
    if (path.isEmpty()) return;
    QDir dir(path);
    if (!dir.exists()) return;

    currentPath = dir.absolutePath();
    dirEdit->setText(currentPath);
    QModelIndex sourceRoot = host_model->setRootPath(currentPath);
    QModelIndex proxyRoot  = host_proxy->mapFromSource(sourceRoot);
    tableView->setRootIndex(proxyRoot);

    // tableView->setColumnWidth(1, 120);
    // tableView->setColumnWidth(3, 180);

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
    host_model->setNameFilterDisables(false);

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
        if (dir.cdUp()) setDirectory(dir.absolutePath());
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
        QString displayName = host_proxy->data(index, Qt::DisplayRole).toString();

        // Process upper directory
        // if (displayName == "..") {
        //     QDir dir(currentPath);
        //     if (dir.cdUp())
        //         setDirectory(dir.absolutePath());
        //     return;
        // }

        QModelIndex srcIndex = host_proxy->mapToSource(index);
        QString path = host_model->filePath(srcIndex);
        QFileInfo info(path);

        if (info.isDir()) {
            setDirectory(path);
        } else {
            // QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            int res = openImage(path);
            if (res != 0 ) {
                QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Can't detect type of the file automatically"));
            }
        }
    } else {
        QModelIndex index = tableView->currentIndex();
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
        tableView->setModel(host_proxy);
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
        QModelIndex srcIdx = host_proxy->mapToSource(idx);
        paths << host_model->filePath(srcIdx);
    }
    return paths;
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
    if (mode==panelMode::Host) {
        const QStringList paths = selectedPaths();
        if (paths.size() == 1) {
            QFileInfo fi(paths.at(0));
            if (!fi.isDir()) {
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
        }
    } else {

    }
}

void FilePanel::onEdit()
{
    if (mode==panelMode::Host) {

    } else {

    }
}
