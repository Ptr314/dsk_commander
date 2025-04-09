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

#include "mainwindow.h"
#include "convertdialog.h"
#include "fileparamdialog.h"
#include "viewdialog.h"

#include "./ui_mainwindow.h"
#include "./ui_aboutdlg.h"
#include "./ui_fileinfodialog.h"

#include "dsk_tools/dsk_tools.h"

#include "globals.h"
#include "mainutils.h"

#define INI_FILE_NAME "/dsk_com.ini"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
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
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = QLocale(locale).name().toLower();
            switch_language(baseName, true);
            break;
        }
    } else {
        switch_language(ini_lang, true);
    }

    ui->setupUi(this);

    resize(1000, 600);

    QList<int> sizes;
    int halfWidth = this->width() / 2;
    sizes << halfWidth << halfWidth;
    ui->splitter->setSizes(sizes);

    setWindowTitle(windowTitle() + " " + PROJECT_VERSION);

    leftFilesModel.setReadOnly(true);
    leftFilesModel.setFilter(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot | QDir::AllDirs);
    ui->leftFiles->setModel(&leftFilesModel);
    ui->leftFiles->header()->hideSection(2);
    ui->leftFiles->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    ui->rightFiles->setModel(&rightFilesModel);

    // QFont font("PxPlus IBM VGA9", 12, 400);
    QFont font("Consolas", 10, 400);
    // QFont font("Iosevka Fixed", 10, 400);
    // font.setStretch(QFont::Expanded);

    ui->rightFiles->setFont(font);

    add_languages();
    load_config();
    init_controls();

    ui->tabWidget->setCurrentIndex(0);

    connect(ui->rightFiles->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onSelectionChanged);
}

void MainWindow::switch_language(const QString & lang, bool init)
{
    if (translator.load(":/i18n/" + lang)) {
        qApp->installTranslator(&translator);

        QString t = QString(":/i18n/qtbase_%1.qm").arg(lang.split("_")[0]);
        if (qtTranslator.load(t)) {
            qApp->installTranslator(&qtTranslator);
        }
        if (!init) {
            ui->retranslateUi(this);
            init_controls();
            settings->setValue("interface/language", lang);
        }
    } else {
        QMessageBox::warning(this, MainWindow::tr("Error"), MainWindow::tr("Failed to load language file for: ") + lang);
    }
}


void MainWindow::add_languages()
{
    QAction *langsAction = ui->actionLanguage;

    QMenu *subMenu = new QMenu(MainWindow::tr("Languages"), this);

    QAction *subAction1 = subMenu->addAction(QIcon(":/icons/ru"), MainWindow::tr("Русский"));
    connect(subAction1, &QAction::triggered, this, [this]() { switch_language("ru_ru", false); });

    QAction *subAction2 = subMenu->addAction(QIcon(":/icons/en"), MainWindow::tr("English"));
    connect(subAction2, &QAction::triggered, this, [this]() { switch_language("en_us", false); });

    langsAction->setMenu(subMenu);
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
}


void MainWindow::init_controls()
{
    QString filter_def = settings->value("directory/left_file_filter", "").toString();
    QString type_def = settings->value("directory/left_type_filter", "").toString();
    QString filesystem_def = settings->value("directory/left_filesystem", "").toString();

    ui->leftFilterCombo->clear();
    foreach (const QString & ff_id, file_formats.keys()) {
        QJsonObject obj = file_formats[ff_id].toObject();
        if (obj["source"].toBool()) {
            QString name = QCoreApplication::translate("config", obj["name"].toString().toUtf8().constData());
            ui->leftFilterCombo->addItem(
                QString("%1 (%2)").arg(name, obj["extensions"].toString().replace(";", "; ")),
                ff_id
            );
        }
    }
    setComboBoxByItemData(ui->leftFilterCombo, filter_def);
    setComboBoxByItemData(ui->leftTypeCombo, type_def);
    setComboBoxByItemData(ui->filesystemCombo, filesystem_def);

    ui->autoCheckBox->setChecked(settings->value("directory/left_auto", 0).toInt()==1);

    QString new_dir = settings->value("directory/left", QApplication::applicationDirPath()).toString();
    set_directory(new_dir);

    setup_buttons(true);
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
    QString new_directory = QFileDialog::getExistingDirectory(this, MainWindow::tr("Choose a directory"), directory);
    if (!new_directory.isEmpty()) {
        set_directory(new_directory);
    }
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
        QString name = QCoreApplication::translate("config", type["name"].toString().toUtf8().constData());

        ui->leftTypeCombo->addItem(name, type_id);
    }

    QStringList filters = filter["extensions"].toString().split(";");
    leftFilesModel.setNameFilters(filters);
    leftFilesModel.setNameFilterDisables(false);

    settings->setValue("directory/left_file_filter", ff_id);
}

void MainWindow::load_file(std::string file_name, std::string file_format, std::string file_type, std::string filesystem_type)
{
    setup_buttons(true);
    rightFilesModel.removeRows(0, rightFilesModel.rowCount());

    if (image != nullptr) delete image;
    image = dsk_tools::prepare_image(file_name, file_format, file_type);

    if (image != nullptr) {
        auto check_result = image->check();
        if (check_result == FDD_LOAD_OK) {
            auto load_result = image->load();
            if (load_result == FDD_LOAD_OK) {
                process_image(filesystem_type);
            } else {
                QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("File loading error. Check your disk type settings or try auto-detection."));
            }
        } else {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error checking file parameters"));
        }
    } else {
        QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error preparing image data"));
    }
}

void MainWindow::on_leftFiles_clicked(const QModelIndex &index)
{
    //TODO: Do something
}

void MainWindow::dir()
{
    bool show_deleted = ui->deletedBtn->isChecked();

    int dir_res = filesystem->dir(&files, show_deleted);
    if (dir_res != FDD_OP_OK) {
        QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error reading files list!"));
    }

    if (ui->sortBtn->isChecked()) {
        std::sort(files.begin(), files.end(), [](const dsk_tools::fileData &a, const dsk_tools::fileData &b) {
            if (a.is_dir != b.is_dir)
                return a.is_dir > b.is_dir;
            return a.name < b.name;
        });
    }

    update_table();
    setup_buttons(false);
}

void MainWindow::update_info()
{
    auto info = replace_placeholders(QString::fromStdString(filesystem->information()));
    ui->informationText->setPlainText(info);
}

void MainWindow::process_image(std::string filesystem_type)
{
    if (filesystem != nullptr) delete filesystem;
    filesystem = dsk_tools::prepare_filesystem(image, filesystem_type);
    if (filesystem != nullptr) {
        int open_res = filesystem->open();

        init_table();

        if (open_res != FDD_OPEN_OK) {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Unrecognized disk format or disk is damaged!"));
            return;
        }

        dir();
        // update_info();
        setup_buttons(false);
        ui->tabWidget->setCurrentIndex(0);
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

    if (funcs & FILE_PROTECTION) {
        rightFilesModel.setColumnCount(const_columns + columns + 1);
        rightFilesModel.setHeaderData(columns, Qt::Horizontal, MainWindow::tr("P"));
        rightFilesModel.horizontalHeaderItem(columns)->setToolTip(MainWindow::tr("Protection"));
        ui->rightFiles->setColumnWidth(columns, 20);
        columns++;
    }
    if (funcs & FILE_TYPE) {
        rightFilesModel.setColumnCount(const_columns + columns + 1);
        rightFilesModel.setHeaderData(columns, Qt::Horizontal, MainWindow::tr("T"));
        rightFilesModel.horizontalHeaderItem(columns)->setToolTip(MainWindow::tr("Type"));
        ui->rightFiles->setColumnWidth(columns, 30);
        columns++;
    }

    ui->rightFiles->setColumnWidth(columns, 60);
    rightFilesModel.setHeaderData(columns, Qt::Horizontal, MainWindow::tr("Size"));
    rightFilesModel.horizontalHeaderItem(columns++)->setToolTip(MainWindow::tr("Size in bytes"));

    ui->rightFiles->setColumnWidth(columns, 230);
    rightFilesModel.setHeaderData(columns, Qt::Horizontal, MainWindow::tr("Name"));
    rightFilesModel.horizontalHeaderItem(columns++)->setToolTip(MainWindow::tr("Name of the file"));

    ui->rightFiles->verticalHeader()->setDefaultSectionSize(8);

    ui->rightFiles->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::update_table()
{
    int funcs = filesystem->get_capabilities();

    rightFilesModel.removeRows(0, rightFilesModel.rowCount());

    foreach (const dsk_tools::fileData & f, files) {
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
        rightFilesModel.appendRow( items );
    }
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
        QString name = QCoreApplication::translate("config", fs["name"].toString().toUtf8().constData());
        ui->filesystemCombo->addItem(name, fs_id);
    }
}


void MainWindow::on_toolButton_clicked()
{
    on_actionConvert_triggered();
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



void MainWindow::on_actionAbout_triggered()
{
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


void MainWindow::on_toolButton_2_clicked()
{
    on_actionParent_directory_triggered();
}


void MainWindow::on_actionParent_directory_triggered()
{
    QDir dir = leftFilesModel.rootDirectory();
    dir.cdUp();
    set_directory(dir.absolutePath());
}


QString MainWindow::replace_placeholders(const QString & in)
{
    return QString(in)
        .replace("{$DIRECTORY_ENTRY}", MainWindow::tr("Directory Entry"))
        .replace("{$FILE_NAME}", MainWindow::tr("File Name"))
        .replace("{$SIZE}", MainWindow::tr("File Size"))
        .replace("{$BYTES}", MainWindow::tr("byte(s)"))
        .replace("{$SIDES}", MainWindow::tr("Sides"))
        .replace("{$TRACKS}", MainWindow::tr("Tracks"))
        .replace("{$SECTORS}", MainWindow::tr("sector(s)"))
        .replace("{$ATTRIBUTES}", MainWindow::tr("Attributes"))
        .replace("{$DATE}", MainWindow::tr("Date"))
        .replace("{$TYPE}", MainWindow::tr("Type"))
        .replace("{$PROTECTED}", MainWindow::tr("Protected"))
        .replace("{$YES}", MainWindow::tr("Yes"))
        .replace("{$NO}", MainWindow::tr("No"))
        .replace("{$TS_LIST_LOCATION}", MainWindow::tr("T/S List Location"))
        .replace("{$INCORRECT_TS_DATA}", MainWindow::tr("Incorrect T/S data, stopping iterations"))
        .replace("{$NEXT_TS}", MainWindow::tr("Next T/S List Location"))
        .replace("{$FILE_END_REACHED}", MainWindow::tr("File End Reached"))
        .replace("{$FILE_DELETED}", MainWindow::tr("The file is marked as deleted, the data below may be incorrect"))
        .replace("{$ERROR_PARSING}", MainWindow::tr("File parsing error"))
        .replace("{$TRACK}", MainWindow::tr("Track"))
        .replace("{$TRACK_SHORT}", MainWindow::tr("T"))
        .replace("{$SIDE_SHORT}", MainWindow::tr("H"))
        .replace("{$PHYSICAL_SECTOR}", MainWindow::tr("S"))
        .replace("{$LOGICAL_SECTOR}", MainWindow::tr("S"))
        .replace("{$PARSING_FINISHED}", MainWindow::tr("Parsing finished"))
        .replace("{$VOLUME_ID}", MainWindow::tr("V"))
        .replace("{$SECTOR_INDEX}", MainWindow::tr("Index"))
        .replace("{$INDEX_MARK}", MainWindow::tr("Index Mark"))
        .replace("{$DATA_MARK}", MainWindow::tr("Data Mark"))
        .replace("{$DATA_FIELD}", MainWindow::tr("Sector Data"))
        .replace("{$SECTOR_INDEX_END_OK}", MainWindow::tr("End Mark: OK"))
        .replace("{$SECTOR_INDEX_END_ERROR}", MainWindow::tr("End Mark: Not Detected"))
        .replace("{$SECTOR_CRC_OK}", MainWindow::tr("CRC: OK"))
        .replace("{$SECTOR_CRC_ERROR}", MainWindow::tr("CRC: Error"))
        .replace("{$CRC_EXPECTED}", MainWindow::tr("Expected"))
        .replace("{$CRC_FOUND}", MainWindow::tr("Found"))
        .replace("{$SECTOR_ERROR}", MainWindow::tr("Data Error"))
        .replace("{$INDEX_CRC_OK}", MainWindow::tr("CRC: OK"))
        .replace("{$INDEX_CRC_ERROR}", MainWindow::tr("CRC: Error"))
        .replace("{$INDEX_EPILOGUE_OK}", MainWindow::tr("Epilogue: OK"))
        .replace("{$INDEX_EPILOGUE_ERROR}", MainWindow::tr("Epilogue: Error"))
        .replace("{$DATA_EPILOGUE_OK}", MainWindow::tr("Epilogue: OK"))
        .replace("{$DATA_EPILOGUE_ERROR}", MainWindow::tr("Epilogue: Error"))
        .replace("{$TRACKLIST_OFFSET}", MainWindow::tr("Track List Offset"))
        .replace("{$TRACK_OFFSET}", MainWindow::tr("Track Offset"))
        .replace("{$TRACK_SIZE}", MainWindow::tr("Track Size"))
        .replace("{$HEADER}", MainWindow::tr("Header"))
        .replace("{$SIGNATURE}", MainWindow::tr("Signature"))
        .replace("{$NO_SIGNATURE}", MainWindow::tr("No known signature found, aborting"))
        .replace("{$FORMAT_REVISION}", MainWindow::tr("Format revision"))
        .replace("{$SIDE}", MainWindow::tr("Side"))
        .replace("{$CUSTOM_DATA}", MainWindow::tr("Custom Data"))
        .replace("{$VTOC_FOUND}", MainWindow::tr("DOS 3.3 VTOC found"))
        .replace("{$VTOC_NOT_FOUND}", MainWindow::tr("DOS 3.3 VTOC not found"))
        .replace("{$VTOC_CATALOG_TRACK}", MainWindow::tr("Root Catalog Track"))
        .replace("{$VTOC_CATALOG_SECTOR}", MainWindow::tr("Root Catalog Sector"))
        .replace("{$VTOC_DOS_RELEASE}", MainWindow::tr("DOS Release"))
        .replace("{$VTOC_VOLUME_ID}", MainWindow::tr("Volume ID"))
        .replace("{$VTOC_VOLUME_NAME}", MainWindow::tr("Volume name"))
        .replace("{$VTOC_PAIRS_ON_SECTOR}", MainWindow::tr("Pairs per T/S list"))
        .replace("{$VTOC_LAST_TRACK}", MainWindow::tr("Last track"))
        .replace("{$VTOC_DIRECTION}", MainWindow::tr("Direction"))
        .replace("{$VTOC_TRACKS_TOTAL}", MainWindow::tr("Tracks total"))
        .replace("{$VTOC_SECTORS_ON_TRACK}", MainWindow::tr("Sectors on track"))
        .replace("{$VTOC_BYTES_PER_SECTOR}", MainWindow::tr("Bytes per sector"))
        .replace("{$ERROR_OPENING}", MainWindow::tr("Error opening the file"))
        .replace("{$ERROR_LOADING}", MainWindow::tr("Error loading, check if the file type is correct"))

        .replace("{$DPB_INFO}", MainWindow::tr("DPB Information"))
        .replace("{$DPB_VOLUME_ID}", MainWindow::tr("Volume ID"))
        .replace("{$DPB_TYPE}", MainWindow::tr("Device type"))
        .replace("{$DPB_DSIDE}", MainWindow::tr("DSIDE"))
        .replace("{$DPB_TSIZE}", MainWindow::tr("Blocks on track"))
        .replace("{$DPB_DSIZE}", MainWindow::tr("Tracks on disk"))
        .replace("{$DPB_MAXBLOK}", MainWindow::tr("Last block"))
        .replace("{$DPB_VTOCADR}", MainWindow::tr("VTOC block"))

        .replace("{$EXTENT}", MainWindow::tr("Extent"))
        .replace("{$FREE_SECTORS}", MainWindow::tr("Free sectors"))
        .replace("{$FREE_BYTES}", MainWindow::tr("Free bytes"))

        .replace("{$META_FILENAME}", MainWindow::tr("File Name"))
        .replace("{$META_PROTECTED}", MainWindow::tr("Protected"))
        .replace("{$META_TYPE}", MainWindow::tr("Type"))
        .replace("{$META_EXTENDED}", MainWindow::tr("Extended"))
    ;
}


void MainWindow::on_actionFile_info_triggered()
{
    QDialog * file_info = new QDialog(this);

    Ui_FileInfo fileinfoUi;
    fileinfoUi.setupUi(file_info);

    QItemSelectionModel * selection = ui->rightFiles->selectionModel();
    if (selection->hasSelection()) {
        QModelIndexList rows = selection->selectedRows();
        if (rows.size() == 1) {
            QModelIndex selectedIndex = rows.at(0);

            dsk_tools::fileData f = files[selectedIndex.row()];

            QString info = replace_placeholders(QString::fromStdString(filesystem->file_info(f)));

            fileinfoUi.textBox->setPlainText(info);
            file_info->exec();

        }
    }
}


void MainWindow::on_actionSave_to_file_triggered()
{
    QString file_name;
    QItemSelectionModel * selection = ui->rightFiles->selectionModel();
    if (selection->hasSelection()) {
        QModelIndexList rows = selection->selectedRows();
        if (rows.size() == 1) {
            QModelIndex selectedIndex = rows.at(0);
            dsk_tools::fileData f = files[selectedIndex.row()];
            file_name = QString::fromStdString(f.name);

            std::vector<std::string> formats = filesystem->get_save_file_formats();

            QString filters = "";

            std::map<QString, QString> fil_map;

            foreach (const std::string & v, formats) {
                QJsonObject fil = file_formats[QString::fromStdString(v)].toObject();
                if (filters.length() != 0) filters += ";;";
                QString name = QCoreApplication::translate("config", fil["name"].toString().toUtf8().constData());
                QString filter_name = QString("%1 (%2)").arg(name, fil["extensions"].toString().replace(";", " "));
                filters += filter_name;
                fil_map[filter_name] = QString::fromStdString(v);
            }
            QString selected_filter;
            file_name = QFileDialog::getSaveFileName(this, MainWindow::tr("Export as"), file_name, filters, &selected_filter);

            // qDebug() << file_name << fil_map[selected_filter];
            filesystem->save_file(fil_map[selected_filter].toStdString(), file_name.toStdString(), f);

        }
    }
}

void MainWindow::setup_buttons(bool disabled)
{
    ui->sortBtn->setDisabled(disabled);
    ui->deletedBtn->setDisabled(disabled);
    ui->convertButton->setDisabled(disabled);
    if (disabled) {
        ui->infoButton->setDisabled(disabled);
        ui->viewButton->setDisabled(disabled);
        ui->saveFileButton->setDisabled(disabled);
        ui->renameFileBtn->setDisabled(disabled);
        ui->addFileBtn->setDisabled(disabled);
        ui->deleteFileBtn->setDisabled(disabled);
        ui->addDirBtn->setDisabled(disabled);
        ui->imageUpBtn->setDisabled(disabled);
    } else {
        int funcs = filesystem->get_capabilities();
        ui->addDirBtn->setDisabled((funcs & FILE_DIRECTORIES) == 0);
        ui->addFileBtn->setDisabled((funcs & FILE_ADD) == 0);
        ui->imageUpBtn->setDisabled(filesystem->is_root());

        QItemSelectionModel * selection = ui->rightFiles->selectionModel();
        int mode = 0; // None selected
        if (selection->hasSelection()) {
            QModelIndexList rows = selection->selectedRows();
            mode = (rows.size()==1)?1:2;
        }
        ui->infoButton->setDisabled(mode != 1);
        ui->viewButton->setDisabled(mode != 1);
        ui->saveFileButton->setDisabled(mode != 1);
        ui->renameFileBtn->setDisabled(mode != 1 || (funcs & FILE_RENAME) == 0);
        ui->deleteFileBtn->setDisabled(mode == 0 || (funcs & FILE_DELETE) == 0);
    }
}

void MainWindow::on_actionConvert_triggered()
{
    QString type_id = ui->leftTypeCombo->itemData(ui->leftTypeCombo->currentIndex()).toString();
    QString target_id;
    QString output_file;
    QString template_file;
    int numtracks;
    uint8_t volume_id;

    ConvertDialog dialog(this, settings, &file_types, &file_formats, image, type_id, filesystem->get_volume_id());
    if (dialog.exec(target_id, output_file, template_file, numtracks, volume_id) == QDialog::Accepted){
        dsk_tools::Writer * writer;

        std::set<QString> mfm_formats = {"FILE_HXC_MFM", "FILE_MFM_NIB", "FILE_MFM_NIC"};

        if ( mfm_formats.find(target_id) != mfm_formats.end()) {
            writer = new dsk_tools::WriterHxCMFM(target_id.toStdString(), image, volume_id);
        } else
            if (target_id == "FILE_HXC_HFE") {
                writer = new dsk_tools::WriterHxCHFE(target_id.toStdString(), image, volume_id);
        } else
        if (target_id == "FILE_RAW_MSB") {
            writer = new dsk_tools::WriterRAW(target_id.toStdString(), image);
        } else {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Not implemented!"));
            return;
        }

        dsk_tools::BYTES buffer;
        int result = writer->write(buffer);

        if (result != FDD_WRITE_OK) {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error generating file"));
        }

        if (numtracks > 0) {

            dsk_tools::BYTES tmplt;

            std::ifstream tf(template_file.toStdString(), std::ios::binary);
            if (!tf.good()) {
                QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error opening template file"));
                delete writer;
                return;
            }
            tf.seekg (0, tf.end);
            auto tfsize = tf.tellg();
            tf.seekg (0, tf.beg);
            tmplt.resize(tfsize);
            tf.read(reinterpret_cast<char*>(tmplt.data()), tfsize);
            if (!tf.good()) {
                QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error reading template file"));
                delete writer;
                return;
            }

            result = writer->substitute_tracks(buffer, tmplt, numtracks);
            if (result != FDD_WRITE_OK) {
                if (result == FDD_WRITE_INCORECT_TEMPLATE)
                    QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("The selected template cannot be used - it must be the same type and size as the target."));
                else
                if (result == FDD_WRITE_INCORECT_SOURCE)
                    QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Incorrect source data for tracks replacement."));
                else
                    QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error inserting tracks from template"));
                delete writer;
                return;
            }
        }

        std::ofstream file(output_file.toStdString(), std::ios::binary);

        if (file.good()) {
            file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
        } else {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error writing file to disk"));
        }

        delete writer;
    }

}

void MainWindow::on_actionImage_Info_triggered()
{
    QModelIndexList indexes = ui->leftFiles->selectionModel()->selectedIndexes();
    if (indexes.size() == 1) {
        QModelIndex selectedIndex = indexes.at(0);
        QFileInfo fi = leftFilesModel.fileInfo(selectedIndex);
        if (!fi.isDir()) {
            std::string format_id = ui->leftFilterCombo->itemData(ui->leftFilterCombo->currentIndex()).toString().toStdString();
            std::string type_id = ui->leftTypeCombo->itemData(ui->leftTypeCombo->currentIndex()).toString().toStdString();
            dsk_tools::Loader * loader;

            std::string file_name = _toStdString(fi.absoluteFilePath())            ;

            if (format_id == "FILE_RAW_MSB") {
                loader = new dsk_tools::LoaderRAW(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_AIM") {
                loader = new dsk_tools::LoaderAIM(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_MFM_NIC") {
                loader = new dsk_tools::LoaderGCR_NIC(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_MFM_NIB") {
                loader = new dsk_tools::LoaderGCR_NIB(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_HXC_MFM") {
                loader = new dsk_tools::LoaderGCR_MFM(file_name, format_id, type_id);
            } else
            if (format_id == "FILE_HXC_HFE") {
                loader = new dsk_tools::LoaderHXC_HFE(file_name, format_id, type_id);
            } else {
                QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Not supported yet"));
                return;
            }
            QDialog * file_info = new QDialog(this);

            Ui_FileInfo fileinfoUi;
            fileinfoUi.setupUi(file_info);

            QString info = replace_placeholders(QString::fromStdString(loader->file_info()));
            fileinfoUi.textBox->setPlainText(info);
            file_info->exec();
        }
    }
}


void MainWindow::on_actionOpen_Image_triggered()
{
    std::string format_id;
    std::string type_id;
    std::string filesystem_id;

    QModelIndexList indexes = ui->leftFiles->selectionModel()->selectedIndexes();
    if (indexes.size() == 1) {
        QModelIndex selectedIndex = indexes.at(0);
        QFileInfo fileInfo = leftFilesModel.fileInfo(selectedIndex);
        if (fileInfo.isDir()) {
            set_directory(fileInfo.absoluteFilePath());
        } else {
            std::string file_name = _toStdString(fileInfo.absoluteFilePath());
            if (ui->autoCheckBox->isChecked()) {
                int res = dsk_tools::detect_fdd_type(file_name, format_id, type_id, filesystem_id);
                if (res != FDD_DETECT_OK ) {
                    QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Can't detect type of the file automatically"));
                    return;
                } else {
                    set_combos(QString::fromStdString(format_id), QString::fromStdString(type_id), QString::fromStdString(filesystem_id));
                }
            } else {
                format_id = ui->leftFilterCombo->itemData(ui->leftFilterCombo->currentIndex()).toString().toStdString();
                type_id = ui->leftTypeCombo->itemData(ui->leftTypeCombo->currentIndex()).toString().toStdString();
                filesystem_id = ui->filesystemCombo->itemData(ui->filesystemCombo->currentIndex()).toString().toStdString();
            }

            load_file(file_name, format_id, type_id, filesystem_id);
        }
    }

}

void MainWindow::on_actionView_triggered()
{
    QModelIndex index = ui->rightFiles->currentIndex();
    dsk_tools::fileData f = files[index.row()];

    if (f.is_dir){
        filesystem->cd(f);
        dir();
    } else {
        dsk_tools::BYTES data = filesystem->get_file(f);

        if (data.size() > 0) {
            QDialog * w = new ViewDialog(this, settings, data, f.preferred_type, f.is_deleted);
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->show();
        } else {
            QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("File reading error!"));
        }
    }

}


void MainWindow::on_imageUpBtn_clicked()
{
    filesystem->cd_up();
    dir();
}


void MainWindow::on_sortBtn_clicked()
{
    dir();
}


void MainWindow::on_deletedBtn_clicked()
{
    dir();
}


void MainWindow::on_actionDelete_triggered()
{
    QItemSelectionModel * selection = ui->rightFiles->selectionModel();
    if (selection->hasSelection()) {
        QModelIndexList rows = selection->selectedRows();
        for (int i=0; i<rows.size(); i++) {
            QModelIndex selectedIndex = rows.at(i);
            dsk_tools::fileData f = files[selectedIndex.row()];
            int res = filesystem->file_delete(f);
            if (res != FILE_DELETE_OK) {
                if (res == FDD_DIR_NOT_EMPTY)
                    QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error deleting directory. It must be empty to be deleted"));
                else
                    QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error deleting file"));
                break;
            }
        }
        dir();
    }
}

void MainWindow::on_tabWidget_currentChanged(int index)
{
    if (index == 1) update_info();
}


void MainWindow::on_actionAdd_files_triggered()
{
    dir();
    QString filters = "";

    std::vector<std::string> formats = filesystem->get_add_file_formats();
    std::map<QString, QString> fil_map;

    foreach (const std::string & v, formats) {
        QJsonObject fil = file_formats[QString::fromStdString(v)].toObject();
        if (filters.length() != 0) filters += ";;";
        QString name = QCoreApplication::translate("config", fil["name"].toString().toUtf8().constData());
        QString filter_name = QString("%1 (%2)").arg(name, fil["extensions"].toString().replace(";", " "));
        filters += filter_name;
        fil_map[filter_name] = QString::fromStdString(v);
    }
    QString selected_filter;
    QString open_dir = settings->value("directory/add", directory).toString();
    QStringList files = QFileDialog::getOpenFileNames(this, MainWindow::tr("Add files"), open_dir, filters, &selected_filter);

    if (files.size() > 0) {
        QFileInfo fi(files[0]);
        settings->setValue("directory/add", fi.absolutePath());
        int res;
        foreach (const QString & fn, files) {
            std::string file_name = _toStdString(fn);
            res = filesystem->file_add(file_name, fil_map[selected_filter].toStdString());
            if (res != FILE_ADD_OK) {
                if (res == FILE_ADD_ERROR_SPACE)
                    QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error adding file. Not enough free disk space"));
                else
                    QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error adding file"));
                break;
            }
        };
        if (res == FILE_ADD_OK)
            QMessageBox::information(this, MainWindow::tr("Success"), MainWindow::tr("Successfully added %1 file(s)").arg(files.size()));
        dir();
    }
}


void MainWindow::on_actionAdd_directory_triggered()
{
    int funcs = filesystem->get_capabilities();

    if ((funcs & FILE_DIRECTORIES) != 0) {
        bool ok{};
        QString text = QInputDialog::getText(this, MainWindow::tr("Add a directory"),
                                             MainWindow::tr("Directory name:"),
                                             QLineEdit::Normal,
                                             "New",
                                             &ok);
        if (ok && !text.isEmpty()) {
            int res = filesystem->mkdir(text.toStdString());
            if (res != FDD_DIR_OK) {
                QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error adding new catalog"));
            }
            dir();
        }
    }
}

void MainWindow::onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    setup_buttons(false);
}

void MainWindow::on_actionRename_triggered()
{
    // QItemSelectionModel * selection = ui->rightFiles->selectionModel();
    // if (selection->hasSelection()) {
    //     QModelIndexList rows = selection->selectedRows();
    //     if (rows.size() == 1) {
    //         QModelIndex selectedIndex = rows.at(0);
    //         dsk_tools::fileData f = files[selectedIndex.row()];
    //         QString old_name = QString::fromStdString(f.name);
    //         bool ok{};
    //         QString new_name = QInputDialog::getText(this, MainWindow::tr("Rename"),
    //                                              MainWindow::tr("New name:"),
    //                                              QLineEdit::Normal,
    //                                              old_name,
    //                                              &ok);
    //         if (ok && !new_name.isEmpty()) {
    //             int res = filesystem->file_rename(f, new_name.toStdString());
    //             if (res != FILE_RENAME_OK) {
    //                 QMessageBox::critical(this, MainWindow::tr("Error"), MainWindow::tr("Error renaming the file"));
    //             }
    //             dir();
    //         }
    //     }
    // }

    QItemSelectionModel * selection = ui->rightFiles->selectionModel();
    if (selection->hasSelection()) {
        QModelIndexList rows = selection->selectedRows();
        if (rows.size() == 1) {
            QModelIndex selectedIndex = rows.at(0);
            dsk_tools::fileData fd = files[selectedIndex.row()];
            std::vector<dsk_tools::ParameterDescription> params = filesystem->file_get_metadata(fd);

            for (int i = 0; i < static_cast<int>(params.size()); ++i) {
                params[i].name = replace_placeholders(QString::fromStdString(params[i].name)).toStdString();
            }

            FileParamDialog dialog(params);
            if (dialog.exec() == QDialog::Accepted) {
                auto values = dialog.getParameters();
                filesystem->file_set_metadata(fd, values);
                dir();
            }
        }
    }
}



