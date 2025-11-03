// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File panel UI

#include <QFileDialog>
#include <QHeaderView>
#include <QEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QJsonArray>
#include <QMessageBox>

#include "FilePanel.h"
#include "mainutils.h"

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
    upButton = new QPushButton("⬆️ Вверх", this);
    dirEdit = new QLineEdit(this);
    dirEdit->setPlaceholderText("Введите путь и нажмите Enter…");
    dirButton = new QPushButton("Выбрать...", this);

    topToolBar->addWidget(upButton);
    topToolBar->addWidget(dirEdit);
    topToolBar->addWidget(dirButton);

    connect(upButton,  &QPushButton::clicked, this, &FilePanel::onGoUp);
    connect(dirButton, &QPushButton::clicked, this, &FilePanel::chooseDirectory);
    connect(dirEdit,   &QLineEdit::returnPressed, this, &FilePanel::onPathEntered);

    // Main table with files
    tableView = new QTableView(this);
    tableView->setFont(font);
    tableView->setModel(host_proxy);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setAlternatingRowColors(true);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setShowGrid(false);
    tableView->verticalHeader()->hide();

    tableView->setSortingEnabled(true);
    tableView->sortByColumn(0, Qt::AscendingOrder);

    // Columns
    tableView->setColumnHidden(2, true);  // Hide "Type"
    QHeaderView* hh = tableView->horizontalHeader();
    hh->setStretchLastSection(false);             // Manual mode
    hh->setSectionResizeMode(0, QHeaderView::Stretch); // Name - expanded
    hh->setSectionResizeMode(1, QHeaderView::Fixed);   // Size — fixed
    hh->setSectionResizeMode(3, QHeaderView::Fixed);   // Date — fixed

    const int colSizeWidth = 120;
    const int colDateWidth = 180;
    tableView->setColumnWidth(1, colSizeWidth);
    tableView->setColumnWidth(3, colDateWidth);

    // Hiding horizontal scrollbar
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableView->setTextElideMode(Qt::ElideRight);

    connect(tableView, &QTableView::doubleClicked, this, &FilePanel::onItemDoubleClicked);

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
    connect(autoCheck, QOverload<Qt::CheckState>::of(&QCheckBox::checkStateChanged),
            this, &FilePanel::onAutoChanged);

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

    tableView->setColumnWidth(1, 120);
    tableView->setColumnWidth(3, 180);

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
    QDir dir(currentPath);
    if (dir.cdUp()) setDirectory(dir.absolutePath());
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

    }
}

int FilePanel::openImage(QString path)
{
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
    loadFile(file_name, format_id, type_id, filesystem_id);
    return FDD_LOAD_OK;
}

int FilePanel::loadFile(std::string file_name, std::string file_format, std::string file_type, std::string filesystem_type)
{
    // setup_buttons(true);
    image_model->removeRows(0, image_model->rowCount());

    if (m_image != nullptr) delete m_image;
    m_image = dsk_tools::prepare_image(file_name, file_format, file_type);

    if (m_image != nullptr) {
        auto check_result = m_image->check();
        if (check_result == FDD_LOAD_OK) {
            auto load_result = m_image->load();
            if (load_result == FDD_LOAD_OK) {
                processImage(filesystem_type);
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

        initTable();

        if (open_res != FDD_OPEN_OK) {
            QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("Unrecognized disk format or disk is damaged!"));
            return;
        }

        dir();
        // update_info();
        // setup_buttons(false);
        // ui->tabWidget->setCurrentIndex(0);
        // is_loaded = true;
        tableView->setModel(image_model);
        mode = panelMode::Image;
    } else {
        QMessageBox::critical(this, FilePanel::tr("Error"), FilePanel::tr("File system initialization error!"));
    }
}

void FilePanel::initTable()
{
    int const_columns = 2;
    int columns = 0;
    int funcs = m_filesystem->get_capabilities();

    image_model->clear();
    tableView->horizontalHeader()->setMinimumSectionSize(20);

    if (funcs & FILE_PROTECTION) {
        image_model->setColumnCount(const_columns + columns + 1);
        image_model->setHeaderData(columns, Qt::Horizontal, FilePanel::tr("P"));
        image_model->horizontalHeaderItem(columns)->setToolTip(FilePanel::tr("Protection"));
        tableView->setColumnWidth(columns, 20);
        columns++;
    }
    if (funcs & FILE_TYPE) {
        image_model->setColumnCount(const_columns + columns + 1);
        image_model->setHeaderData(columns, Qt::Horizontal, FilePanel::tr("T"));
        image_model->horizontalHeaderItem(columns)->setToolTip(FilePanel::tr("Type"));
        tableView->setColumnWidth(columns, 30);
        columns++;
    }

    tableView->setColumnWidth(columns, 60);
    image_model->setHeaderData(columns, Qt::Horizontal, FilePanel::tr("Size"));
    image_model->horizontalHeaderItem(columns++)->setToolTip(FilePanel::tr("Size in bytes"));

    tableView->setColumnWidth(columns, 230);
    image_model->setHeaderData(columns, Qt::Horizontal, FilePanel::tr("Name"));
    image_model->horizontalHeaderItem(columns++)->setToolTip(FilePanel::tr("Name of the file"));

    tableView->verticalHeader()->setDefaultSectionSize(8);

    tableView->horizontalHeader()->setStretchLastSection(true);
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
    for (int row = 0; row < ui->rightFiles->model()->rowCount(); ++row) {
        ui->rightFiles->setRowHeight(row, 24);
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
    // TODO: check why is not working
    setStyleSheet(active ? "FilePanel { border: 2px solid red; }" : "");
}

void FilePanel::focusList() {
    if (tableView) tableView->setFocus();
}

bool FilePanel::eventFilter(QObject* obj, QEvent* ev) {
    if ((ev->type() == QEvent::FocusIn || ev->type() == QEvent::MouseButtonPress))
        emit activated(this);
    return QWidget::eventFilter(obj, ev);
}
