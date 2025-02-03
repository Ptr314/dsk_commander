#include <set>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFontDatabase>

#include "mainwindow.h"
#include "viewdialog.h"

#include "./ui_mainwindow.h"

#include "dsk_tools/dsk_tools.h"

#define INI_FILE_NAME "/dsk_com.ini"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , leftFilesModel(this)
    , rightFilesModel(this)
{
    QFontDatabase::addApplicationFont(":/fonts/consolas");
    QFontDatabase::addApplicationFont(":/fonts/dos");

    ui->setupUi(this);

    QString app_path = QApplication::applicationDirPath();
    QString current_path = QDir::currentPath();
    QString ini_path, ini_file;

#if defined(__linux__)
    ini_path = QString(getenv("HOME")) + "/.config";
    ini_file = ini_path + INI_FILE_NAME;
    if (!std::filesystem::exists(ini_file.toStdString())) {
        if (std::filesystem::exists(QString(emulator_root + INI_FILE_NAME).toStdString())) {
            std::filesystem::copy_file(QString(emulator_root + INI_FILE_NAME).toStdString(), ini_file.toStdString());
        } else {
            std::filesystem::copy_file(QString(current_path + INI_FILE_NAME).toStdString(), ini_file.toStdString());
        }
    }
#elif defined(__APPLE__)
    ini_path = QString(getenv("HOME"));
    ini_file = ini_path + INI_FILE_NAME;
    if (!std::filesystem::exists(ini_file.toStdString())) {
        if (std::filesystem::exists(QString(emulator_root + INI_FILE_NAME).toStdString())) {
            std::filesystem::copy_file(QString(emulator_root + INI_FILE_NAME).toStdString(), ini_file.toStdString());
        } else {
            std::filesystem::copy_file(QString(current_path + INI_FILE_NAME).toStdString(), ini_file.toStdString());
        }
    }
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

    leftFilesModel.setReadOnly(true);
    leftFilesModel.setFilter(QDir::Files | QDir::Hidden | QDir::NoDot);
    ui->leftFiles->setModel(&leftFilesModel);
    ui->leftFiles->header()->hideSection(2);
    ui->leftFiles->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    ui->rightFiles->setModel(&rightFilesModel);
    // ui->rightFiles->setFont(QFont("PxPlus IBM VGA9", 12, 400));
    ui->rightFiles->setFont(QFont("Consolas", 10, 400));

    load_config();
    init_controls();
}

void MainWindow::load_config()
{
    QFile file(QApplication::applicationDirPath().append("/config.json"));
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
}

void MainWindow::init_controls()
{
    int filter_index = settings->value("directory/left_file_filter", 1).toInt();
    int type_index = settings->value("directory/left_type_filter", 0).toInt();

    int target_index = settings->value("directory/right_type_filter", 0).toInt();


    ui->leftFilterCombo->clear();
    foreach (const QString & ff_id, file_formats.keys()) {
        QJsonObject obj = file_formats[ff_id].toObject();
        if (obj["source"].toBool()) {
            ui->leftFilterCombo->addItem(
                QString("%1 (%2)").arg(obj["name"].toString(), obj["extensions"].toString().replace(";", "; ")),
                ff_id
            );
        }
    }
    ui->leftFilterCombo->setCurrentIndex(filter_index);
    ui->leftTypeCombo->setCurrentIndex(type_index);
    ui->rightFormatCombo->setCurrentIndex(target_index);
    ui->autoCheckBox->setChecked(settings->value("directory/left_auto", 0).toInt()==1);

    QString new_dir = settings->value("directory/left", QApplication::applicationDirPath()).toString();
    set_directory(new_dir);
}

MainWindow::~MainWindow()
{
    // delete settings;
    delete ui;
}

void MainWindow::set_directory(QString new_directory)
{
    directory = new_directory;
    ui->directoryLine->setText(directory);
    ui->leftFiles->setRootIndex(leftFilesModel.setRootPath(directory));
    settings->setValue("directory/left", directory);
}

void MainWindow::on_actionOpen_directory_triggered()
{
    QUrl dir_object = QFileDialog::getExistingDirectoryUrl(this, MainWindow::tr("Choose a directory"), directory);
    if (!dir_object.isEmpty()) {
        set_directory(dir_object.toLocalFile());
    }
}

void MainWindow::on_openDirectoryBtn_clicked()
{
    on_actionOpen_directory_triggered();
}


void MainWindow::on_leftFilterCombo_currentIndexChanged(int index)
{
    ui->leftTypeCombo->clear();
    QString ff_id = ui->leftFilterCombo->itemData(index).toString();
    QJsonObject filter = file_formats[ff_id].toObject();
    QJsonArray types = filter["types"].toArray();
    foreach (const QJsonValue & value, types) {
        QString type_id = value.toString();
        QJsonObject type = file_types[type_id].toObject();
        QString name = type["name"].toString();
        ui->leftTypeCombo->addItem(name, type_id);
    }

    QStringList filters = filter["extensions"].toString().split(";");
    leftFilesModel.setNameFilters(filters);
    leftFilesModel.setNameFilterDisables(false);

    settings->setValue("directory/left_file_filter", ff_id);
}

void MainWindow::load_file(std::string file_name, std::string file_format, std::string file_type, std::string filesystem_type)
{
    qDebug() << "Loading" << file_name << file_format << file_type;
    if (image != nullptr) delete image;
    image = dsk_tools::prepare_image(file_name, file_format, file_type);

    auto check_result = image->check();
    if (check_result == FDD_LOAD_OK) {
        auto load_result = image->load();
        if (load_result == FDD_LOAD_OK) {
            process_image(filesystem_type);
        } else {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error loading file"));
        }
    } else {
        QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error checking file parameters"));
    }
}

void MainWindow::on_leftFiles_clicked(const QModelIndex &index)
{
    //TODO: Do something
}

void MainWindow::on_leftFiles_doubleClicked(const QModelIndex &index)
{
    std::string format_id;
    std::string type_id;
    std::string filesystem_id;

    QModelIndexList indexes = ui->leftFiles->selectionModel()->selectedIndexes();
    if (indexes.size() == 1) {
        QModelIndex selectedIndex = indexes.at(0);
        QFileInfo fileInfo = leftFilesModel.fileInfo(selectedIndex);
        if (ui->autoCheckBox->isChecked()) {
            int res = dsk_tools::detect_fdd_type(fileInfo.absoluteFilePath().toStdString(), format_id, type_id, filesystem_id);
            if (res != FDD_DETECT_OK ) {
                QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Can't detect type of the file automatically"));
                return;
            } else {
                set_combos(QString::fromStdString(format_id), QString::fromStdString(type_id), QString::fromStdString(filesystem_id));
            }
        } else {
            format_id = ui->leftFilterCombo->itemData(ui->leftFilterCombo->currentIndex()).toString().toStdString();
            type_id = ui->leftTypeCombo->itemData(ui->leftTypeCombo->currentIndex()).toString().toStdString();
            filesystem_id = ui->leftTypeCombo->itemData(ui->filesystemCombo->currentIndex()).toString().toStdString();
        }
        load_file(fileInfo.absoluteFilePath().toStdString(), format_id, type_id, filesystem_id);

    }
}

void MainWindow::process_image(std::string filesystem_type)
{
    qDebug() << "Processing loaded image";

    if (filesystem != nullptr) delete filesystem;
    filesystem = dsk_tools::prepare_filesystem(image, filesystem_type);
    if (filesystem != nullptr) {
        int open_res = filesystem->open();

        if (open_res != FDD_OPEN_OK) {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Unrecognized disk format or disk is damaged!"));
        }

        int dir_res = filesystem->dir(&files);
        if (dir_res != FDD_OP_OK) {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error reading files list!"));
        }

        qDebug() << "Got files: " << files.size();

        // foreach (const dsk_tools::fileData & f, files) {
        //     qDebug() << f.name;
        // }

        init_table();
        update_table();
    } else {
        QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("File system initialization error!"));
    }
}

void MainWindow::init_table()
{
    int const_columns = 2;
    int columns = 0;
    int funcs = filesystem->get_capabilities();

    rightFilesModel.clear();
    ui->rightFiles->horizontalHeader()->setMinimumSectionSize(20);

    if (funcs & FILE_PRORECTION) {
        rightFilesModel.setColumnCount(const_columns + columns + 1);
        rightFilesModel.setHeaderData(columns, Qt::Horizontal, MainWindow::tr("P"));
        rightFilesModel.horizontalHeaderItem(columns)->setToolTip(MainWindow::tr("Protection"));
        ui->rightFiles->setColumnWidth(columns, 20);
        columns++;
    }
    if (funcs & FILE_TYPE) {
        rightFilesModel.setColumnCount(const_columns + columns + 1);
        rightFilesModel.setHeaderData(columns, Qt::Horizontal, MainWindow::tr("F"));
        rightFilesModel.horizontalHeaderItem(columns)->setToolTip(MainWindow::tr("Type"));
        ui->rightFiles->setColumnWidth(columns, 30);
        columns++;
    }

    ui->rightFiles->setColumnWidth(columns, 60);
    rightFilesModel.setHeaderData(columns, Qt::Horizontal, MainWindow::tr("Size"));
    rightFilesModel.horizontalHeaderItem(columns++)->setToolTip(MainWindow::tr("Size in bytes"));

    ui->rightFiles->setColumnWidth(columns, 250);
    rightFilesModel.setHeaderData(columns, Qt::Horizontal, MainWindow::tr("Name"));
    rightFilesModel.horizontalHeaderItem(columns++)->setToolTip(MainWindow::tr("Name of the file"));

    ui->rightFiles->verticalHeader()->setDefaultSectionSize(8);
}

void MainWindow::update_table()
{
    int funcs = filesystem->get_capabilities();

    rightFilesModel.removeRows(0, rightFilesModel.rowCount());

    foreach (const dsk_tools::fileData & f, files) {
        QList<QStandardItem*> items;

        if (funcs & FILE_PRORECTION) {
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

        QStandardItem * size_item = new QStandardItem();
        size_item->setText(QString::number(f.size));
        size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        items.append(size_item);

        items.append(new QStandardItem(( QString::fromStdString(f.name) )));
        rightFilesModel.appendRow( items );
    }
}

void MainWindow::on_rightFiles_doubleClicked(const QModelIndex &index)
{
    // QModelIndexList indexes = ui->rightFiles->selectionModel()->selectedIndexes();

    dsk_tools::fileData f = files[index.row()];
    BYTES data = filesystem->get_file(f);

    qDebug() << data.size();

    QDialog * w = new ViewDialog(this, data);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}


void MainWindow::on_rightFormatCombo_currentIndexChanged(int index)
{
    QString target_id = ui->rightFormatCombo->itemData(index).toString();
    settings->setValue("directory/right_type_filter", target_id);
}


void MainWindow::on_leftTypeCombo_currentIndexChanged(int index)
{
    QString type_id = ui->leftTypeCombo->itemData(index).toString();
    settings->setValue("directory/left_type_filter", type_id);

    ui->filesystemCombo->clear();
    QJsonObject type = file_types[type_id].toObject();
    foreach (const QJsonValue & fs_val, type["filesystems"].toArray()) {
        QString fs_id = fs_val.toString();
        QJsonObject fs = file_systems[fs_id].toObject();
        ui->filesystemCombo->addItem(fs["name"].toString(), fs_id);
    }

    ui->rightFormatCombo->clear();
    foreach (const QJsonValue & target_val, type["targets"].toArray()) {
        QString target_id = target_val.toString();
        QJsonObject target = file_formats[target_id].toObject();
        ui->rightFormatCombo->addItem(target["short_name"].toString(), target_id);
    }
}


void MainWindow::on_toolButton_clicked()
{
    on_actionConvert_triggered();
}


void MainWindow::on_actionConvert_triggered()
{

    int index = ui->rightFormatCombo->currentIndex();
    QString format_id = ui->rightFormatCombo->itemData(index).toString();
    QString source_file = QString::fromStdString(image->file_name());

    QFileInfo fi(source_file);

    dsk_tools::Writer * writer;

    std::set<QString> mfm_formats = {"FILE_HXC_MFM", "FILE_MFM_NIB", "FILE_MFM_NIC"};

    if ( mfm_formats.find(format_id) != mfm_formats.end()) {
        writer = new dsk_tools::WriterHxCMFM(format_id.toStdString(), image);
    } else
        if (format_id == "FILE_HXC_HFE") {
            writer = new dsk_tools::WriterHxCHFE(format_id.toStdString(), image);
    } else
    if (format_id == "FILE_RAW_MSB") {
        writer = new dsk_tools::WriterRAW(format_id.toStdString(), image);
    } else {
        QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Not implemented!"));
        return;
    }

    QString new_ext = QString::fromStdString(writer->get_default_ext());

    QString filter = "";

    foreach (const QJsonValue & value, file_formats) {
        QJsonObject obj = value.toObject();
        if (obj["id"].toString() == format_id) {
            filter = QString("%1 (%2)").arg(obj["name"].toString(), obj["extensions"].toString().replace(";", " "));
        }
    }

    QString file_name = QString("%1/%2.%3").arg(fi.dir().absolutePath(), fi.completeBaseName(), new_ext);

    file_name = QFileDialog::getSaveFileName(this, MainWindow::tr("Export as"), file_name, filter);

    if (!file_name.isEmpty()) {

        int result = writer->write(file_name.toStdString());

        if (result != FDD_WRITE_OK) {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error writing file"));
        }
    }

    delete writer;
}

void MainWindow::on_filesystemCombo_currentIndexChanged(int index)
{
    QString fs_id = ui->filesystemCombo->itemData(index).toString();
    settings->setValue("directory/left_filesystem", fs_id);
}

void MainWindow::on_autoCheckBox_checkStateChanged(const Qt::CheckState &arg1)
{
    settings->setValue("directory/left_auto", ui->autoCheckBox->isChecked()?1:0);
}


void MainWindow::setComboBoxByItemData(QComboBox* comboBox, const QVariant& value) {
    if (!comboBox) return;  // Защита от nullptr

    for (int i = 0; i < comboBox->count(); ++i) {
        if (comboBox->itemData(i) == value) {
            comboBox->setCurrentIndex(i);
            return;
        }
    }
}

void MainWindow::set_combos(QString format_id, QString type_id, QString filesystem_id)
{
    setComboBoxByItemData(ui->leftFilterCombo, format_id);
    setComboBoxByItemData(ui->leftTypeCombo, type_id);
    setComboBoxByItemData(ui->filesystemCombo, filesystem_id);
}


