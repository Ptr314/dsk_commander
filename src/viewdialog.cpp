// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for viewing files

#include <QTimer>
#include <QMessageBox>

#include "viewdialog.h"
#include "mainutils.h"
#include "ui_viewdialog.h"

#include "dsk_tools/dsk_tools.h"

ViewDialog::ViewDialog(QWidget *parent, QSettings *settings, const QString file_name, const dsk_tools::BYTES &data, int preferred_type, bool deleted, dsk_tools::diskImage * disk_image, dsk_tools::fileSystem * filesystem)
    : QDialog(parent)
    , ui(new Ui::ViewDialog)
    , m_settings(settings)
    , m_file_name(file_name)
    , m_disk_image(disk_image)
    , m_filesystem(filesystem)
{
    m_data = data;
    ui->setupUi(this);

    // QFont font("Iosevka Fixed", 10, 400);
    // font.setStretch(QFont::Expanded);
    QFont font("Consolas", 10, 400);
    ui->textBox->setFont(font);

    dsk_tools::register_all_viewers();

    dsk_tools::ViewerManager& manager = dsk_tools::ViewerManager::instance();
    auto all_types = manager.list_types();
    for (const auto& type : all_types) {
        std::vector<std::pair<std::string, std::string>> subtypes = manager.list_subtypes(type);

        for (const std::pair<std::string, std::string>& subtype : subtypes) {
            const std::string& subtype_id = subtype.first;

            std::unique_ptr<dsk_tools::Viewer> viewer = manager.create(type, subtype_id);
            if (viewer && viewer->fits(data)) {
                m_subtypes[type].push_back(subtype_id);
            }
        }
    }

    std::vector<std::string> fit_types = get_types_from_map(m_subtypes);

    ui->modeCombo->blockSignals(true);

    std::map<std::string, int> type_map;

    int c = 0;
    for (const auto& type : fit_types) {
        QString type_str = QString::fromStdString(type)
                            .replace("BINARY", ViewDialog::tr("Binary"))
                            .replace("TEXT", ViewDialog::tr("Text"))
                            .replace("BASIC", ViewDialog::tr("BASIC"))
                            .replace("PICTURE_AGAT", ViewDialog::tr("Agat pictures"))
                            .replace("PICTURE_APPLE", ViewDialog::tr("Apple pictures"))
        ;
        ui->modeCombo->addItem(type_str, QString::fromStdString(type));
        type_map[type] = c++;
    }
    adjustComboBoxWidth(ui->modeCombo);

    QString preferred_subtype = "";
    if (preferred_type == PREFERRED_TEXT) {
        ui->modeCombo->setCurrentIndex(type_map["TEXT"]);
    } else
    if (preferred_type == PREFERRED_AGATBASIC) {
        ui->modeCombo->setCurrentIndex(type_map["BASIC"]);
        preferred_subtype = "AGAT";
    } else
    if (preferred_type == PREFERRED_ABS) {
        ui->modeCombo->setCurrentIndex(type_map["BASIC"]);
        preferred_subtype = "APPLE";
    } else
    if (preferred_type == PREFERRED_MBASIC) {
        ui->modeCombo->setCurrentIndex(type_map["BASIC"]);
        preferred_subtype = "MBASIC";
    } else {
        std::pair<std::string, std::string> suggested = dsk_tools::suggest_file_type(_toStdString(m_file_name), m_data);
        if (suggested.first == "BINARY") {
            auto stored_type = m_settings->value("viewer/type", "BINARY").toString();
            if (stored_type != "BINARY") {
                auto stored_subtype = m_settings->value("viewer/subtype_" + stored_type, "").toString();
                if (stored_subtype != "") {
                    suggested = {stored_type.toStdString(), stored_subtype.toStdString()};
                }
            }
        }
        if (type_map.count(suggested.first)) {
            ui->modeCombo->setCurrentIndex(type_map[suggested.first]);
            preferred_subtype = QString::fromStdString(suggested.second);
        } else {
            ui->modeCombo->setCurrentIndex(type_map["BINARY"]);
            preferred_subtype = "";
        }
    }
    ui->modeCombo->blockSignals(false);

    update_subtypes(preferred_subtype);

    ui->encodingCombo->blockSignals(true);
    ui->encodingCombo->addItem(ViewDialog::tr("Agat"), "agat");
    ui->encodingCombo->addItem(ViewDialog::tr("Apple II"), "apple2");
    ui->encodingCombo->addItem(ViewDialog::tr("Apple //c"), "apple2c");
    ui->encodingCombo->addItem(ViewDialog::tr("ASCII"), "ascii");
    ui->encodingCombo->setCurrentIndex(settings->value("viewer/encoding", 0).toInt());
    adjustComboBoxWidth(ui->encodingCombo);
    ui->encodingCombo->blockSignals(false);

    ui->deletedLabel->setVisible(deleted);


    ui->propsCombo->blockSignals(true);
    ui->propsCombo->addItem(ViewDialog::tr("Square pixels"), "sqp");
    ui->propsCombo->addItem(ViewDialog::tr("Square screen"), "sqs");
    ui->propsCombo->addItem(ViewDialog::tr("4:3"), "43");
    ui->propsCombo->setCurrentIndex(settings->value("viewer/proportions", 0).toInt());
    adjustComboBoxWidth(ui->propsCombo);
    ui->propsCombo->blockSignals(false);

    m_scaleFactor = 1;
    // m_scaleFactor = settings->value("viewer/scale", 1).toInt();

    // ui->scaleSlider->blockSignals(true);
    // ui->scaleSlider->setValue(m_scaleFactor);
    // ui->scaleLabel->setText(QString("%1%").arg(m_scaleFactor*100));
    // ui->scaleSlider->blockSignals(false);


    connect(&m_pic_timer, &QTimer::timeout, this, &ViewDialog::pic_timer_proc);
    // m_pic_timer.setSingleShot(true);
    // m_pic_timer.start(1000);

    fill_options();

    print_data();
}

void ViewDialog::update_subtypes(const QString & preferred)
{

    auto mode = ui->modeCombo->currentData().toString().toStdString();
    auto all_subtypes = dsk_tools::ViewerManager::instance().list_subtypes(mode);

    std::vector<std::pair<std::string, std::string>> subtypes;
    auto it = m_subtypes.find(mode);
    const auto& allowed_subtypes = it->second;
    for (const auto& pair : all_subtypes) {
        if (std::find(allowed_subtypes.begin(), allowed_subtypes.end(), pair.first) != allowed_subtypes.end()) {
            subtypes.push_back(pair);
        }
    }

    int l = subtypes.size();
    if ( l > 1 || (l==1 && subtypes[0].first.size()!=0)) {
        ui->subtypeCombo->setDisabled(false);
        ui->subtypeCombo->setVisible(true);
        ui->subtypeLabel->setVisible(true);
        ui->sybtypeSpacer->changeSize(10, 20);

        ui->subtypeCombo->blockSignals(true);
        ui->subtypeCombo->clear();
        for (const auto& pair : subtypes) {
            ui->subtypeCombo->addItem(replace_placeholders(QString::fromStdString(pair.second)), QString::fromStdString(pair.first));
            if (pair.first == preferred.toStdString()) {
                int index = ui->subtypeCombo->count()-1;
                last_subtypes[mode] = index;
                ui->subtypeCombo->setCurrentIndex(index);
            }
        }
        if (preferred.isEmpty() && last_subtypes.find(mode) != last_subtypes.end()) {
            ui->subtypeCombo->setCurrentIndex(last_subtypes[mode]);
        }
        adjustComboBoxWidth(ui->subtypeCombo);
        ui->subtypeCombo->blockSignals(false);
        use_subtypes = true;
    } else {
        ui->subtypeCombo->setDisabled(true);
        ui->subtypeCombo->setVisible(false);
        ui->subtypeLabel->setVisible(false);
        ui->sybtypeSpacer->changeSize(0, 20);
        use_subtypes = false;
    }
}

ViewDialog::~ViewDialog()
{
    delete ui;
}

void ViewDialog::on_closeBtn_clicked()
{
    close();
}

QString ViewDialog::replace_placeholders(const QString & in)
{
    return QString(in)
        .replace("{$PALETTE}", ViewDialog::tr("Palette"))
        .replace("{$CUSTOM_PALETTE}", ViewDialog::tr("Custom palette"))
        .replace("{$BW}", ViewDialog::tr("b/w"))

        .replace("{$FONT_LOADING_ERROR}", ViewDialog::tr("Custom font loading error"))

        .replace("{$NTSC_AGAT_IMPROVED}", ViewDialog::tr("Agat Improved"))
        .replace("{$NTSC_APPLE_IMPROVED}", ViewDialog::tr("Apple Improved"))
        .replace("{$NTSC_APPLE_ORIGINAL}", ViewDialog::tr("Apple NTSC Original"))

        .replace("{$AGAT_FONT_A7_CLASSIC}", ViewDialog::tr("Agat-7 classic font"))
        .replace("{$AGAT_FONT_A7_ENCHANCED}", ViewDialog::tr("Agat-7 enchanced font"))
        .replace("{$AGAT_FONT_A9_CLASSIC}", ViewDialog::tr("Agat-9 classic font"))
        .replace("{$AGAT_FONT_CUSTOM_GARNIZON}", ViewDialog::tr("GARNIZON custom font"))
        .replace("{$AGAT_FONT_CUSTOM_LOADED}", ViewDialog::tr("Loaded custom font"))
        .replace("{$FONT_A9}", ViewDialog::tr("Agat-9 Font"))
        .replace("{$FONT_A7}", ViewDialog::tr("Agat-7 Font"))
        .replace("{$FONT_FILE}", ViewDialog::tr("Font file"))
        ;
}


void ViewDialog::print_data()
{
    if (m_data.size() != 0) {
        auto type = ui->modeCombo->currentData().toString().toStdString();
        auto subtype = (use_subtypes)?ui->subtypeCombo->currentData().toString().toStdString():"";

        if (recreate_viewer) {
            m_viewer = dsk_tools::ViewerManager::instance().create(type, subtype);
            recreate_viewer = false;
        }

        auto output_type = m_viewer->get_output_type();
        if (output_type == VIEWER_OUTPUT_TEXT) {
            ui->encodingCombo->setVisible(true);
            ui->encodingLabel->setVisible(true);

            auto cm_name = ui->encodingCombo->currentData().toString().toStdString();
            auto out = m_viewer->process_as_text(m_data, cm_name);

            // ui->textBox->setWordWrapMode(QTextOption::WordWrap);
            ui->textBox->setWordWrapMode(QTextOption::NoWrap);

            ui->textBox->setPlainText(QString::fromStdString(out));
            ui->viewArea->setCurrentIndex(0);
        } else
        if (output_type == VIEWER_OUTPUT_PICTURE) {
            ui->encodingCombo->setVisible(false);
            ui->encodingLabel->setVisible(false);
            int sx, sy;
            if (auto picViewer = dynamic_cast<dsk_tools::ViewerPic*>(m_viewer.get())) {
                m_imageData = picViewer->process_picture(m_data, sx, sy, m_opt, m_pic_frame++);
                m_image = QImage(m_imageData.data(), sx, sy, QImage::Format_RGBA8888);
                update_image();

                int delay = picViewer->get_frame_delay();
                if (delay > 0) {
                    m_pic_timer.setSingleShot(true);
                    m_pic_timer.start(delay);
                }
            }
            ui->viewArea->setCurrentIndex(1);
        }
    }
}


void ViewDialog::update_image()
{
    // QSize labelSize = ui->picLabel->size();
    QString scr_mode = ui->propsCombo->itemData(ui->propsCombo->currentIndex()).toString();
    QImage scaledImage;
    if (scr_mode == "sqp" || (scr_mode == "sqs" && m_image.width()==m_image.height())) {
        scaledImage = m_image.scaled(
            m_image.width() * m_scaleFactor,
            m_image.height() * m_scaleFactor,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );
    } else
    if (scr_mode == "sqs") {
        double ratio_w, ratio_h;
        if (m_image.width() > m_image.height()) {
            ratio_w = 1;
            ratio_h = (double)m_image.width() / m_image.height();
        } else {
            ratio_w = (double)m_image.height() / m_image.width();
            ratio_h = 1;

        }
        scaledImage = m_image.scaled(
            m_image.width() * m_scaleFactor * ratio_w,
            m_image.height() * m_scaleFactor * ratio_h,
            Qt::IgnoreAspectRatio,
            Qt::FastTransformation
        );
    } else {
        // 43
        double ratio_w, ratio_h;
        if (m_image.width() > m_image.height()) {
            ratio_w = 1;
            ratio_h = (double)m_image.width() / m_image.height() * 3 / 4;
        } else {
            ratio_w = (double)m_image.height() / m_image.width() * 4 / 3;
            ratio_h = 1;

        }
        scaledImage = m_image.scaled(
            m_image.width() * m_scaleFactor * ratio_w,
            m_image.height() * m_scaleFactor * ratio_h,
            Qt::IgnoreAspectRatio,
            Qt::FastTransformation
            );

    };

    QPixmap pixmap = QPixmap::fromImage(scaledImage);
    ui->picLabel->setPixmap(pixmap);
}


void ViewDialog::on_modeCombo_currentIndexChanged(int index)
{
    recreate_viewer = true;

    auto type = ui->modeCombo->currentData().toString();
    m_settings->setValue("viewer/type", type);

    update_subtypes();
    fill_options();
    print_data();
}


void ViewDialog::on_encodingCombo_currentTextChanged(const QString &arg1)
{
    m_settings->setValue("viewer/encoding", ui->encodingCombo->currentIndex());
    print_data();
}


void ViewDialog::fill_options()
{
    auto type = ui->modeCombo->currentData().toString().toStdString();
    auto subtype = (use_subtypes)?ui->subtypeCombo->currentData().toString().toStdString():"";
    if (recreate_viewer) {
        m_viewer = dsk_tools::ViewerManager::instance().create(type, subtype);
        recreate_viewer = false;
    }
    auto output_type = m_viewer->get_output_type();
    if (output_type == VIEWER_OUTPUT_PICTURE) {
        restore_scale();
        if (auto picViewer = dynamic_cast<dsk_tools::ViewerPic*>(m_viewer.get())) {
            std::string error_msg;
            int res = picViewer->prepare_data(m_data, *m_disk_image, *m_filesystem, error_msg);
            if (res != PREPARE_PIC_OK) {
                QString msg = replace_placeholders(QString::fromStdString(error_msg));
                QMessageBox::critical(this, ViewDialog::tr("Error"), msg);
            }

            auto options = picViewer->get_options();
            if (options.size() > 0) {
                int suggested = picViewer->suggest_option(_toStdString(m_file_name), m_data);
                ui->optionsCombo->blockSignals(true);
                ui->optionsCombo->clear();
                for (const auto& opt : options) {
                    ui->optionsCombo->addItem(replace_placeholders(QString::fromStdString(opt.second)), opt.first);
                    if (opt.first == suggested) {
                        ui->optionsCombo->setCurrentIndex(ui->optionsCombo->count()-1);
                        m_opt = suggested;
                    }
                }
                adjustComboBoxWidth(ui->optionsCombo);
                ui->optionsCombo->blockSignals(false);
                ui->optionsLabel->setVisible(true);
                ui->optionsCombo->setVisible(true);
            } else {
                ui->optionsLabel->setVisible(false);
                ui->optionsCombo->setVisible(false);
                m_opt = 0;
            }
        }
    }
}


void ViewDialog::on_scaleSlider_valueChanged(int value)
{
    store_scale(value);

    ui->scaleLabel->setText(QString("%1%").arg(value*100));
    m_scaleFactor = value;
    update_image();
}


void ViewDialog::on_optionsCombo_currentIndexChanged(int index)
{
    recreate_viewer = true;
    m_opt = ui->optionsCombo->itemData(index).toInt();
    print_data();
}


void ViewDialog::on_propsCombo_currentIndexChanged(int index)
{
    m_settings->setValue("viewer/proportions", ui->propsCombo->currentIndex());
    update_image();
}


void ViewDialog::on_subtypeCombo_currentIndexChanged(int index)
{
    recreate_viewer = true;
    auto type = ui->modeCombo->currentData().toString();
    if (use_subtypes) {
        auto subtype = ui->subtypeCombo->currentData().toString();
        m_settings->setValue("viewer/subtype_" + type, subtype);
    }
    last_subtypes[type.toStdString()] = index;

    fill_options();
    print_data();
}


void ViewDialog::pic_timer_proc()
{
    print_data();
}


void ViewDialog::store_scale(int value)
{
    auto type = ui->modeCombo->currentData().toString();
    auto subtype = (use_subtypes)?ui->subtypeCombo->currentData().toString():"";

    m_settings->setValue("viewer/scale_" + type + "_" + subtype, value);
}


void ViewDialog::restore_scale()
{
    auto type = ui->modeCombo->currentData().toString();
    auto subtype = (use_subtypes)?ui->subtypeCombo->currentData().toString():"";
    m_scaleFactor = m_settings->value("viewer/scale_" + type + "_" + subtype, 1).toInt();
    ui->scaleSlider->blockSignals(true);
    ui->scaleSlider->setValue(m_scaleFactor);
    ui->scaleLabel->setText(QString("%1%").arg(m_scaleFactor*100));
    ui->scaleSlider->blockSignals(false);
}
