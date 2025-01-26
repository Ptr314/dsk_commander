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
    file_formats = jsonRoot["file_formats"].toArray();
    file_types = jsonRoot["file_types"].toObject();
}

void MainWindow::init_controls()
{
    int filter_index = settings->value("directory/left_file_filter", 1).toInt();
    int type_index = settings->value("directory/left_type_filter", 0).toInt();

    int target_index = settings->value("directory/right_type_filter", 0).toInt();


    ui->leftFilterCombo->clear();
    foreach (const QJsonValue & value, file_formats) {
        QJsonObject obj = value.toObject();
        if (obj["source"].toBool()) {
            ui->leftFilterCombo->addItem(
                QString("%1 (%2)").arg(obj["name"].toString()).arg(obj["extensions"].toString().replace(";", "; ")),
                obj["extensions"].toString()
            );
        }
        if (obj["target"].toBool()) {
            ui->rightFormatCombo->addItem(
                obj["short_name"].toString(),
                obj["id"].toString()
                );
        }
    }
    ui->leftFilterCombo->setCurrentIndex(filter_index);
    ui->leftTypeCombo->setCurrentIndex(type_index);
    ui->rightFormatCombo->setCurrentIndex(target_index);

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
    QUrl dir_object = QFileDialog::getExistingDirectoryUrl(this, MainWindow::tr("Choose a directory"));
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
    QJsonObject filter = file_formats[index].toObject();
    QJsonArray types = filter["types"].toArray();
    foreach (const QJsonValue & value, types) {
        QString type_id = value.toString();
        QJsonObject type = file_types[type_id].toObject();
        QString name = type["name"].toString();
        ui->leftTypeCombo->addItem(name, type_id);
    }

    QStringList filters = ui->leftFilterCombo->itemData(index).toString().split(";");
    leftFilesModel.setNameFilters(filters);
    leftFilesModel.setNameFilterDisables(false);

    settings->setValue("directory/left_file_filter", index);
}

void MainWindow::load_file(QString file_name, QString file_format, QString file_type)
{
    qDebug() << "Loading" << file_name << file_format << file_type;
    image = dsk_tools::prepare_image(file_name.toStdString(), file_format.toStdString(), file_type.toStdString());

    auto check_result = image->check();
    if (check_result == FDD_LOAD_OK) {
        auto load_result = image->load();
        if (load_result == FDD_LOAD_OK) {
            process_image();
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
    QModelIndexList indexes = ui->leftFiles->selectionModel()->selectedIndexes();
    if (indexes.size() == 1) {
        QModelIndex selectedIndex = indexes.at(0);
        QFileInfo fileInfo = leftFilesModel.fileInfo(selectedIndex);
        QString format_id = file_formats[ui->leftFilterCombo->currentIndex()].toObject()["id"].toString();
        QString type_id = ui->leftTypeCombo->itemData(ui->leftTypeCombo->currentIndex()).toString();
        load_file(fileInfo.absoluteFilePath(), format_id, type_id);
    }
}

void MainWindow::process_image()
{
    qDebug() << "Processing loaded image";
    int open_res = image->open();

    if (open_res != FDD_OPEN_OK) {
        QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Unrecognized disk format or disk is damaged!"));
    }

    int dir_res = image->dir(&files);
    if (dir_res != FDD_OP_OK) {
        QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error reading files list!"));
    }

    qDebug() << "Got files: " << files.size();

    // foreach (const dsk_tools::fileData & f, files) {
    //     qDebug() << f.name;
    // }

    init_table();
    update_table();
}

void MainWindow::init_table()
{
    int const_columns = 2;
    int columns = 0;
    int funcs = image->get_capabilities();

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
    int funcs = image->get_capabilities();

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
    std::vector<uint8_t> data = image->get_file(f);

    qDebug() << data.size();

    QDialog * w = new ViewDialog(this, data);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}


void MainWindow::on_rightFormatCombo_currentIndexChanged(int index)
{
    settings->setValue("directory/right_type_filter", index);
}


void MainWindow::on_leftTypeCombo_currentIndexChanged(int index)
{
    settings->setValue("directory/left_type_filter", index);
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

    if (format_id == "FILE_HXC_HFE") {
        writer = new dsk_tools::WriterHxCHFE(format_id.toStdString(), image);
    } else
    if (format_id == "FILE_HXC_MFM") {
        writer = new dsk_tools::WriterHxCMFM(format_id.toStdString(), image);
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



