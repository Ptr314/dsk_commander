// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for viewing files

#include "viewdialog.h"
#include "ui_viewdialog.h"

#include "dsk_tools/dsk_tools.h"

ViewDialog::ViewDialog(QWidget *parent, QSettings *settings, const dsk_tools::BYTES &data, int preferred_type, bool deleted)
    : QDialog(parent)
    , ui(new Ui::ViewDialog)
    , m_settings(settings)
{
    m_data = data;
    ui->setupUi(this);

    // QFont font("Iosevka Fixed", 10, 400);
    // font.setStretch(QFont::Expanded);
    QFont font("Consolas", 10, 400);
    ui->textBox->setFont(font);

    dsk_tools::register_all_viewers();

    auto all_types = dsk_tools::ViewerManager::instance().list_types();

    ui->modeCombo->blockSignals(true);

    std::map<std::string, int> type_map;

    int c = 0;
    for (const auto& type : all_types) {
        QString type_str = QString::fromStdString(type)
                            .replace("BINARY", ViewDialog::tr("Binary"))
                            .replace("TEXT", ViewDialog::tr("Text"))
                            .replace("BASIC", ViewDialog::tr("BASIC"))
                            .replace("PICTURE_AGAT", ViewDialog::tr("Agat pictures"));
        ui->modeCombo->addItem(type_str, QString::fromStdString(type));
        type_map[type] = c++;
    }

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
        std::pair<std::string, std::string> suggested = dsk_tools::suggest_file_type(m_data);
        ui->modeCombo->setCurrentIndex(type_map[suggested.first]);
        preferred_subtype = QString::fromStdString(suggested.second);
    }
    ui->modeCombo->blockSignals(false);

    update_subtypes(preferred_subtype);

    ui->encodingCombo->blockSignals(true);
    ui->encodingCombo->addItem(ViewDialog::tr("Agat"), "agat");
    ui->encodingCombo->addItem(ViewDialog::tr("Apple II"), "apple2");
    ui->encodingCombo->addItem(ViewDialog::tr("Apple //c"), "apple2c");
    ui->encodingCombo->addItem(ViewDialog::tr("ASCII"), "ascii");
    ui->encodingCombo->setCurrentIndex(settings->value("viewer/encoding", 0).toInt());
    ui->encodingCombo->blockSignals(false);

    ui->deletedLabel->setVisible(deleted);


    ui->propsCombo->blockSignals(true);
    ui->propsCombo->addItem(ViewDialog::tr("Square pixels"), "sqp");
    ui->propsCombo->addItem(ViewDialog::tr("Square screen"), "sqs");
    ui->propsCombo->addItem(ViewDialog::tr("4:3"), "43");
    ui->propsCombo->setCurrentIndex(settings->value("viewer/proportions", 0).toInt());
    ui->propsCombo->blockSignals(false);

    m_scaleFactor = 1;

    fill_options();

    print_data();
}

void ViewDialog::update_subtypes(const QString & preferred)
{

    auto mode = ui->modeCombo->currentData().toString().toStdString();
    auto subtypes = dsk_tools::ViewerManager::instance().list_subtypes(mode);

    if (subtypes.size() > 1) {
        ui->subtypeCombo->setDisabled(false);
        ui->subtypeCombo->setVisible(true);
        ui->subtypeLabel->setVisible(true);
        ui->sybtypeSpacer->changeSize(10, 20);

        ui->subtypeCombo->blockSignals(true);
        ui->subtypeCombo->clear();
        for (const auto& pair : subtypes) {
            ui->subtypeCombo->addItem(QString::fromStdString(pair.second), QString::fromStdString(pair.first));
            if (pair.first == preferred.toStdString()) {
                int index = ui->subtypeCombo->count()-1;
                last_subtypes[mode] = index;
                ui->subtypeCombo->setCurrentIndex(index);
            }
        }
        if (preferred.isEmpty() && last_subtypes.find(mode) != last_subtypes.end()) {
            ui->subtypeCombo->setCurrentIndex(last_subtypes[mode]);
        }
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
        .replace("{$MAIN_PALETTE}", ViewDialog::tr("Main palette"))
        .replace("{$ALT_PALETTE}", ViewDialog::tr("Alt palette"))
        ;
}


void ViewDialog::print_data()
{
    if (m_data.size() != 0) {
        auto type = ui->modeCombo->currentData().toString().toStdString();
        auto subtype = (use_subtypes)?ui->subtypeCombo->currentData().toString().toStdString():"";

        auto viewer = dsk_tools::ViewerManager::instance().create(type, subtype);

        auto output_type = viewer->get_output_type();
        if (output_type == VIEWER_OUTPUT_TEXT) {
            ui->encodingCombo->setVisible(true);
            ui->encodingLabel->setVisible(true);

            auto cm_name = ui->encodingCombo->currentData().toString().toStdString();
            auto out = viewer->process_as_text(m_data, cm_name);

            // ui->textBox->setWordWrapMode(QTextOption::WordWrap);
            ui->textBox->setWordWrapMode(QTextOption::NoWrap);

            ui->textBox->setPlainText(QString::fromStdString(out));
            ui->viewArea->setCurrentIndex(0);
        } else
        if (output_type == VIEWER_OUTPUT_PICTURE) {
            ui->encodingCombo->setVisible(false);
            ui->encodingLabel->setVisible(false);
            int sx, sy;
            if (auto picViewer = dynamic_cast<dsk_tools::ViewerPic*>(viewer.get())) {
                m_imageData = picViewer->process_picture(m_data, sx, sy, m_opt);
                m_image = QImage(m_imageData.data(), sx, sy, QImage::Format_RGBA8888);
                update_image();
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
        double ratio = (double)m_image.width() / m_image.height();
        scaledImage = m_image.scaled(
            m_image.width() * m_scaleFactor / ratio,
            m_image.height() * m_scaleFactor
        );
    } else {
        // 43
        double ratio = ((double)m_image.width() / m_image.height()) * 3 / 4;
        scaledImage = m_image.scaled(
            m_image.width() * m_scaleFactor / ratio,
            m_image.height() * m_scaleFactor
        );
    };

    QPixmap pixmap = QPixmap::fromImage(scaledImage);
    ui->picLabel->setPixmap(pixmap);
}


void ViewDialog::on_modeCombo_currentIndexChanged(int index)
{
    update_subtypes();
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
    auto viewer = dsk_tools::ViewerManager::instance().create(type, subtype);
    auto output_type = viewer->get_output_type();
    if (output_type == VIEWER_OUTPUT_PICTURE) {
        if (auto picViewer = dynamic_cast<dsk_tools::ViewerPic*>(viewer.get())) {
            auto options = picViewer->get_options();
            if (options.size() > 0) {
                ui->optionsCombo->blockSignals(true);
                ui->optionsCombo->clear();
                for (const auto& opt : options) {
                    ui->optionsCombo->addItem(replace_placeholders(QString::fromStdString(opt.second)), opt.first);
                }
                ui->optionsCombo->blockSignals(false);
                ui->optionsCombo->setVisible(true);
            } else {
                ui->optionsCombo->setVisible(false);
                m_opt = 0;
            }
        }
    }
}


void ViewDialog::on_scaleSlider_valueChanged(int value)
{
    ui->scaleLabel->setText(QString("%1%").arg(value*100));
    m_scaleFactor = value;
    update_image();
}


void ViewDialog::on_optionsCombo_currentIndexChanged(int index)
{
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
    auto mode = ui->modeCombo->currentData().toString().toStdString();
    last_subtypes[mode] = index;

    fill_options();
    print_data();
}

