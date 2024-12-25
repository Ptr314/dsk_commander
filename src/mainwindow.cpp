#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "dsk_tools/dsk_tools.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , leftFilesModel(this)
    , settings ("dsk_com.ini",  QSettings::IniFormat)
{
    ui->setupUi(this);

    leftFilesModel.setReadOnly(true);
    leftFilesModel.setFilter(QDir::Files | QDir::Hidden | QDir::NoDot);
    ui->leftFiles->setModel(&leftFilesModel);
    ui->leftFiles->header()->hideSection(2);
    ui->leftFiles->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

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
    int filter_index = settings.value("directory/left_file_filter", 1).toInt();
    ui->leftFilterCombo->clear();
    foreach (const QJsonValue & value, file_formats) {
        QJsonObject obj = value.toObject();
        ui->leftFilterCombo->addItem(
            QString("%1 (%2)").arg(obj["name"].toString()).arg(obj["extensions"].toString().replace(";", "; ")),
            obj["extensions"].toString()
        );
    }
    ui->leftFilterCombo->setCurrentIndex(filter_index);

    QString new_dir = settings.value("directory/left", QApplication::applicationDirPath()).toString();
    set_directory(new_dir);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::set_directory(QString new_directory)
{
    directory = new_directory;
    ui->directoryLine->setText(directory);
    ui->leftFiles->setRootIndex(leftFilesModel.setRootPath(directory));
    settings.setValue("directory/left", directory);
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

    settings.setValue("directory/left_file_filter", index);
}


void MainWindow::on_leftFiles_clicked(const QModelIndex &index)
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

void MainWindow::load_file(QString file_name, QString file_format, QString file_type)
{
    qDebug() << "Loading" << file_name << file_format << file_type;
    image = dsk_tools::prepare_image(file_name.toStdString(), file_format.toStdString(), file_type.toStdString());

    auto check_result = image->check();
    if (check_result == FDD_LOAD_OK) {
        auto load_result = image->load();
        if (load_result == FDD_LOAD_OK) {
            // TODO: Do something with the data
        } else {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error loading file"));
        }
    } else {
        QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error checking file parameters"));
    }
}

