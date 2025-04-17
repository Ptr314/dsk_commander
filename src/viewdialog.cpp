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
                            .replace("PICTURE", ViewDialog::tr("Image"));
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
        ui->modeCombo->setCurrentIndex(type_map["BINARY"]);
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
        for (const auto& subtype : subtypes) {
            QString subtype_str = QString::fromStdString(subtype)
                                   .replace("AGAT", ViewDialog::tr("Agat BASIC"))
                                   .replace("APPLE", ViewDialog::tr("Apple BASIC"))
                                   .replace("MBASIC", ViewDialog::tr("CP/M MBASIC"));
            ui->subtypeCombo->addItem(subtype_str, QString::fromStdString(subtype));

            if (subtype == preferred)
                ui->subtypeCombo->setCurrentIndex(ui->subtypeCombo->count()-1);
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

void ViewDialog::print_data()
{
    if (m_data.size() != 0) {
        auto type = ui->modeCombo->currentData().toString().toStdString();
        auto subtype = (use_subtypes)?ui->subtypeCombo->currentData().toString().toStdString():"";

        auto viewer = dsk_tools::ViewerManager::instance().create(type, subtype);

        auto output_type = viewer->get_output_type();
        if (output_type == VIEWER_OUTPUT_TEXT) {
            auto cm_name = ui->encodingCombo->currentData().toString().toStdString();
            auto out = viewer->process_as_text(m_data, cm_name);

            // ui->textBox->setWordWrapMode(QTextOption::WordWrap);
            ui->textBox->setWordWrapMode(QTextOption::NoWrap);

            ui->textBox->setPlainText(QString::fromStdString(out));
        } else
        if (output_type == VIEWER_OUTPUT_PICTURE) {
            // TODO: Implement
        }
    }
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


void ViewDialog::on_subtypeCombo_currentTextChanged(const QString &arg1)
{
    print_data();
}

