// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for viewing files

#include <iostream>
#include <fstream>

#include <QTimer>
#include <QMessageBox>
#include <qdir.h>
#include <QClipboard>
#include <QMimeData>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QTextStream>
#include <QDebug>
#include <QHBoxLayout>
#include <QFrame>
#include <QVBoxLayout>

#include "viewdialog.h"

#include "FileOperations.h"
#include "mainutils.h"
#include "placeholders.h"
#include "ui_viewdialog.h"
#include "host_helpers.h"

#include "dsk_tools/dsk_tools.h"

ViewDialog::ViewDialog(QWidget *parent, QSettings *settings, const QString file_name, const dsk_tools::BYTES &data, dsk_tools::PreferredType preferred_type, bool deleted, dsk_tools::diskImage * disk_image, dsk_tools::fileSystem * filesystem, const dsk_tools::UniversalFile& f)
    : QDialog(parent)
    , ui(new Ui::ViewDialog)
    , m_settings(settings)
    , m_file_name(file_name)
    , m_disk_image(disk_image)
    , m_filesystem(filesystem)
    , m_f(f)
{
    m_data = data;
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

    // QFont font("Iosevka Fixed", 10, 400);
    // font.setStretch(QFont::Expanded);
    // QFont font("Consolas", 10, 400);
    // ui->textBox->setFont(font);

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
    if (preferred_type == dsk_tools::PreferredType::Text) {
        ui->modeCombo->setCurrentIndex(type_map["TEXT"]);
    } else
    if (preferred_type == dsk_tools::PreferredType::AgatBASIC) {
        ui->modeCombo->setCurrentIndex(type_map["BASIC"]);
        preferred_subtype = "AGAT";
    } else
    if (preferred_type == dsk_tools::PreferredType::AppleBASIC) {
        ui->modeCombo->setCurrentIndex(type_map["BASIC"]);
        preferred_subtype = "APPLE";
    } else
    if (preferred_type == dsk_tools::PreferredType::MBASIC) {
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
            ui->subtypeCombo->addItem(replacePlaceholders(QString::fromStdString(pair.second)), QString::fromStdString(pair.first));
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
    clearSelectorWidgets();
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

        if (recreate_viewer) {
            m_viewer = dsk_tools::ViewerManager::instance().create(type, subtype);
            auto picViewer = dynamic_cast<dsk_tools::ViewerPic*>(m_viewer.get());
            std::string error_msg;
            int res = picViewer->prepare_data(m_data, *m_disk_image, *m_filesystem, error_msg);
            recreate_viewer = false;
        }

        auto output_type = m_viewer->get_output_type();
        if (output_type == VIEWER_OUTPUT_TEXT) {
            ui->encodingCombo->setVisible(true);
            ui->encodingLabel->setVisible(true);
            ui->encodingSpacer->changeSize(10, 20);

            auto cm_name = ui->encodingCombo->currentData().toString().toStdString();
            auto out = m_viewer->process_as_text(m_data, cm_name);

            QFile css_file(":/files/basic.css");
            if (css_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream stream(&css_file);
                m_saved_css = stream.readAll();
                ui->textEdit->document()->setDefaultStyleSheet(m_saved_css);
                css_file.close();
            } else {
                qDebug() << "Failed to load CSS file";
            }
            m_saved_html = QString::fromStdString(out);
            ui->textEdit->setHtml("<body>" + m_saved_html + "</body>");

            ui->viewArea->setCurrentIndex(0);

            ui->copyButton->setVisible(true);
            ui->saveButton->setVisible(true);
        } else
        if (output_type == VIEWER_OUTPUT_PICTURE) {
            ui->encodingCombo->setVisible(false);
            ui->encodingLabel->setVisible(false);
            ui->encodingSpacer->changeSize(0, 0);

            int sx, sy;
            if (auto picViewer = dynamic_cast<dsk_tools::ViewerPic*>(m_viewer.get())) {
                const dsk_tools::ViewerSelectorValues selectors = collectSelectors();
                picViewer->set_selectors(selectors);
                m_imageData = picViewer->process_picture(m_data, sx, sy, m_pic_frame++);
                m_image = QImage(m_imageData.data(), sx, sy, QImage::Format_RGBA8888);
                update_image();

                int delay = picViewer->get_frame_delay();
                if (delay > 0) {
                    m_pic_timer.setSingleShot(true);
                    m_pic_timer.start(delay);
                }
            }
            ui->viewArea->setCurrentIndex(1);
            ui->copyButton->setVisible(false);
            ui->saveButton->setVisible(false);
        }
    }
}

dsk_tools::ViewerSelectorValues ViewDialog::collectSelectors()
{
    dsk_tools::ViewerSelectorValues result;

    // Iterate through all selector widgets and collect their current values
    for (const auto& group : m_selectorWidgets) {
        if (group.comboBox) {
            // Get the currently selected value (option id) from the combo box
            QString selectedValue = group.comboBox->itemData(group.comboBox->currentIndex()).toString();
            result[group.selectorId] = _toStdString(selectedValue);
        }
    }

    return result;
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
                QString msg = replacePlaceholders(QString::fromStdString(error_msg));
                QMessageBox::critical(this, ViewDialog::tr("Error"), msg);
            }

            populateSelectorWidgets(picViewer->suggest_selectors(_toStdString(m_file_name), m_data));
        }
    }
}

void ViewDialog::clearSelectorWidgets()
{
    // Iterate in reverse to safely remove from layout (use int loop for Qt 5.6 compatibility)
    for (int i = (int)m_selectorWidgets.size() - 1; i >= 0; --i) {
        SelectorWidgetGroup& group = m_selectorWidgets[i];

        // Remove widgets from layout
        if (group.iconLabel) ui->pic_toolbar->removeWidget(group.iconLabel);
        if (group.comboBox) ui->pic_toolbar->removeWidget(group.comboBox);
        if (group.infoButton) ui->pic_toolbar->removeWidget(group.infoButton);
        if (group.customButton) ui->pic_toolbar->removeWidget(group.customButton);
        if (group.clearButton) ui->pic_toolbar->removeWidget(group.clearButton);
        if (group.spacerBefore) ui->pic_toolbar->removeWidget(group.spacerBefore);
        if (group.spacerBetween) ui->pic_toolbar->removeItem(group.spacerBetween);
        if (group.buttonSpacer) ui->pic_toolbar->removeItem(group.buttonSpacer);
        if (group.clearButtonSpacer) ui->pic_toolbar->removeItem(group.clearButtonSpacer);

        // Delete widgets
        delete group.iconLabel;
        delete group.comboBox;
        delete group.infoButton;
        delete group.customButton;
        delete group.clearButton;
        delete group.spacerBefore;
        delete group.spacerBetween;
        delete group.buttonSpacer;
        delete group.clearButtonSpacer;
    }

    // Clear the vector
    m_selectorWidgets.clear();
}

void ViewDialog::populateSelectorWidgets(const dsk_tools::ViewerSelectorValues suggested_values)
{
    // First, cleanup any existing selector widgets
    clearSelectorWidgets();

    // Only pic viewers have selectors
    auto picViewer = dynamic_cast<dsk_tools::ViewerPic*>(m_viewer.get());
    if (!picViewer) {
        return;
    }

    auto selectors = picViewer->get_selectors();
    if (selectors.empty()) {
        return;
    }

    // Find the right_spacer widget to insert before it
    QHBoxLayout* toolbar = qobject_cast<QHBoxLayout*>(ui->pic_toolbar);
    if (!toolbar) {
        return;
    }

    // Find the last spacer item (right_spacer)
    int insertIndex = toolbar->count();
    for (int i = toolbar->count() - 1; i >= 0; --i) {
        QLayoutItem* item = toolbar->itemAt(i);
        if (item && item->spacerItem()) {
            insertIndex = i;
            break;
        }
    }

    // Process each selector
    for (const auto& selector : selectors) {
        const std::string selectorType = selector->get_type();
        if (selectorType != "dropdown" && selectorType != "info") {
            continue; // Only support dropdown and info types
        }

        SelectorWidgetGroup group;
        group.selectorId = selector->get_id();
        group.infoButton = nullptr;

        // Get and translate selector title
        QString selectorTitle = replacePlaceholders(QString::fromStdString(selector->get_title()));

        // Handle info selector type (no separate icon label, inline button with text)
        if (selectorType == "info") {
            // 1. Create vertical line separator before section
            group.spacerBefore = new QFrame(this);
            group.spacerBefore->setFrameShape(QFrame::VLine);
            // group.spacerBefore->setFrameShadow(QFrame::Sunken);
            group.spacerBefore->setFixedSize(20, 24);
            toolbar->insertWidget(insertIndex++, group.spacerBefore);

            // 2. Create tool button with icon and text inline
            group.infoButton = new QToolButton(this);
            group.infoButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
            group.infoButton->setText(selectorTitle);
            group.infoButton->setToolTip(selectorTitle);
            group.infoButton->setIconSize(QSize(24, 24));
            group.infoButton->setObjectName(QString::fromStdString("infoBtn_" + selector->get_id()));

            // Set icon on the button
            QString iconPath = QString(":/icons/%1").arg(QString::fromStdString(selector->get_icon()));
            QPixmap iconPixmap(iconPath);
            if (!iconPixmap.isNull()) {
                group.infoButton->setIcon(QIcon(iconPixmap));
            } else {
                group.infoButton->setText("? " + selectorTitle);
            }

            // Connect button click to show info popup
            connect(group.infoButton, &QToolButton::clicked, this, [this, group]() {
                onInfoButtonClicked(group.infoButton, group.selectorId);
            });

            toolbar->insertWidget(insertIndex++, group.infoButton);

            // 3. Create 5px spacer after button
            group.spacerBetween = new QSpacerItem(5, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
            toolbar->insertItem(insertIndex++, group.spacerBetween);

            // Initialize other pointers to nullptr for info type
            group.iconLabel = nullptr;
            group.comboBox = nullptr;
            group.customButton = nullptr;
            group.clearButton = nullptr;
            group.buttonSpacer = nullptr;
            group.clearButtonSpacer = nullptr;

            // Store the widget group
            m_selectorWidgets.push_back(group);
            continue;  // Skip the dropdown-specific code below
        }

        // For dropdown type: create icon label and vertical line separator
        // 1. Create vertical line separator before section
        group.spacerBefore = new QFrame(this);
        group.spacerBefore->setFrameShape(QFrame::VLine);
        // group.spacerBefore->setFrameShadow(QFrame::Sunken);
        group.spacerBefore->setFixedSize(20, 24);
        toolbar->insertWidget(insertIndex++, group.spacerBefore);

        // 2. Create icon label
        group.iconLabel = new QLabel(this);
        group.iconLabel->setMaximumSize(24, 24);
        group.iconLabel->setScaledContents(true);
        group.iconLabel->setToolTip(selectorTitle);

        // Map icon name to resource path
        QString iconPath = QString(":/icons/%1").arg(QString::fromStdString(selector->get_icon()));
        QPixmap iconPixmap(iconPath);
        if (!iconPixmap.isNull()) {
            group.iconLabel->setPixmap(iconPixmap);
        } else {
            qDebug() << "Warning: Icon not found:" << iconPath;
        }

        toolbar->insertWidget(insertIndex++, group.iconLabel);

        // 3. Create 5px spacer between icon and combo
        group.spacerBetween = new QSpacerItem(5, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
        toolbar->insertItem(insertIndex++, group.spacerBetween);

        // 4. Create and populate combo box
        group.comboBox = new QComboBox(this);
        group.comboBox->blockSignals(true);
        group.comboBox->setToolTip(selectorTitle);

        auto options = selector->get_options();
        int selectedIndex = 0;

        // Populate combo box with all options
        for (size_t i = 0; i < options.size(); ++i) {
            const auto& opt = options[i];
            QString displayText = replacePlaceholders(QString::fromStdString(opt.title));
            QString optionId = QString::fromStdString(opt.id);

            group.comboBox->addItem(displayText, optionId);
        }

        // Load custom files if selector supports them (BEFORE setting selected index)
        group.customButton = nullptr;
        group.clearButton = nullptr;
        group.buttonSpacer = nullptr;
        group.clearButtonSpacer = nullptr;
        if (selector->has_customs()) {
            loadCustomFilesForSelector(selector->get_id(), group.comboBox);
        }

        // Check if there's a suggested value for this selector
        auto suggested_it = suggested_values.find(group.selectorId);
        if (suggested_it != suggested_values.end()) {
            const QString suggestedValue = QString::fromStdString(suggested_it->second);
            // Find the combo item with this value and set it as current
            for (int i = 0; i < group.comboBox->count(); ++i) {
                if (group.comboBox->itemData(i).toString() == suggestedValue) {
                    selectedIndex = i;
                    break;
                }
            }
        }

        group.comboBox->setCurrentIndex(selectedIndex);
        adjustComboBoxWidth(group.comboBox);

        // Connect signal BEFORE unblocking signals
        QString selectorId = QString::fromStdString(group.selectorId);
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
        connect(group.comboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, [this, selectorId](int index) {
                    QComboBox* combo = qobject_cast<QComboBox*>(sender());
                    if (combo) {
                        QString selectedValue = combo->itemData(index).toString();
                        onSelectorChanged(selectorId, selectedValue);
                    }
                });
#else
        connect(group.comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, selectorId](int index) {
                    QComboBox* combo = qobject_cast<QComboBox*>(sender());
                    if (combo) {
                        QString selectedValue = combo->itemData(index).toString();
                        onSelectorChanged(selectorId, selectedValue);
                    }
                });
#endif

        group.comboBox->blockSignals(false);

        toolbar->insertWidget(insertIndex++, group.comboBox);

        // Create custom file selection button if selector supports it
        if (selector->has_customs()) {
            group.customButton = new QToolButton(this);

            // Load and set icon for file selection
            QPixmap iconPixmap(":/icons/add_from_file");
            if (!iconPixmap.isNull()) {
                group.customButton->setIcon(QIcon(iconPixmap));
            } else {
                // Fallback if icon not found
                group.customButton->setText("+");
            }

            group.customButton->setIconSize(QSize(24, 24));
            // group.customButton->setMinimumSize(32, 32);
            group.customButton->setToolTip(tr("Add custom file"));
            group.customButton->setObjectName(QString::fromStdString("customBtn_" + selector->get_id()));

            // Connect button click
            std::string selectorId = selector->get_id();
            connect(group.customButton, &QToolButton::clicked, this, [this, selectorId]() {
                onCustomFileButtonClicked(selectorId);
            });

            // Add spacer before button
            group.buttonSpacer = new QSpacerItem(8, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
            toolbar->insertItem(insertIndex++, group.buttonSpacer);

            // Add button to layout
            toolbar->insertWidget(insertIndex++, group.customButton);

            // Create clear custom files button
            group.clearButton = new QToolButton(this);

            // Load and set delete icon
            QPixmap deleteIconPixmap(":/icons/deleted");
            if (!deleteIconPixmap.isNull()) {
                group.clearButton->setIcon(QIcon(deleteIconPixmap));
            } else {
                // Fallback if icon not found
                group.clearButton->setText("X");
            }

            group.clearButton->setIconSize(QSize(24, 24));
            // group.clearButton->setMinimumSize(32, 32);
            group.clearButton->setToolTip(tr("Clear custom files"));
            group.clearButton->setObjectName(QString::fromStdString("clearBtn_" + selector->get_id()));

            // Connect button click
            connect(group.clearButton, &QToolButton::clicked, this, [this, selectorId]() {
                onClearCustomFilesButtonClicked(selectorId);
            });

            // Add spacer before clear button
            // group.clearButtonSpacer = new QSpacerItem(2, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
            // toolbar->insertItem(insertIndex++, group.clearButtonSpacer);

            // Add clear button to layout
            toolbar->insertWidget(insertIndex++, group.clearButton);
        } else {
            group.clearButton = nullptr;
            group.clearButtonSpacer = nullptr;
        }

        // Store the widget group
        m_selectorWidgets.push_back(group);
    }
}

void ViewDialog::onSelectorChanged(const QString& selectorId, const QString& selectedValue)
{
    recreate_viewer = true;
    print_data();
}


QString ViewDialog::getCustomFilesSettingsKey(const std::string& selectorId) {
    return QString("viewer/custom_files_%1").arg(QString::fromStdString(selectorId));
}


void ViewDialog::loadCustomFilesForSelector(const std::string& selectorId, QComboBox* comboBox) {
    QString settingsKey = getCustomFilesSettingsKey(selectorId);
    QStringList customFiles = m_settings->value(settingsKey).toStringList();

    if (customFiles.isEmpty()) return;

    // First pass: check if any valid files exist
    bool hasValidFiles = false;
    for (const QString& filePath : customFiles) {
        if (QFile::exists(filePath)) {
            hasValidFiles = true;
            break;
        }
    }

    if (!hasValidFiles) return;  // Don't add separator if no valid files exist

    // Add separator before custom files
    comboBox->insertSeparator(comboBox->count());

    // Add each custom file
    for (const QString& filePath : customFiles) {
        if (QFile::exists(filePath)) {  // Only add if file still exists
            QString fileName = QFileInfo(filePath).fileName();
            QString itemId = QString("custom:%1").arg(filePath);
            comboBox->addItem(tr("From file: %1").arg(fileName), itemId);
        }
    }
}


void ViewDialog::addCustomFileToComboBox(const std::string& selectorId, const QString& filePath, QComboBox* comboBox) {
    // Load existing custom files from settings
    QString settingsKey = getCustomFilesSettingsKey(selectorId);
    QStringList customFiles = m_settings->value(settingsKey).toStringList();

    // Check if file already exists in list
    if (customFiles.contains(filePath)) {
        // Already added, just select it
        for (int i = 0; i < comboBox->count(); ++i) {
            QString itemId = comboBox->itemData(i).toString();
            if (itemId == QString("custom:%1").arg(filePath)) {
                comboBox->setCurrentIndex(i);
                return;
            }
        }
    }

    // Add to settings list
    customFiles.append(filePath);
    m_settings->setValue(settingsKey, customFiles);

    // Find separator or add one
    int separatorIndex = -1;
    for (int i = comboBox->count() - 1; i >= 0; --i) {
        if (comboBox->itemData(i).isNull()) {  // Separator has null data
            separatorIndex = i;
            break;
        }
    }

    if (separatorIndex == -1) {
        // No separator yet, add one
        comboBox->insertSeparator(comboBox->count());
        separatorIndex = comboBox->count() - 1;
    }

    // Add new custom file after separator
    QString fileName = QFileInfo(filePath).fileName();
    QString itemId = QString("custom:%1").arg(filePath);
    comboBox->addItem(tr("From file: %1").arg(fileName), itemId);

    // Select the newly added item
    comboBox->setCurrentIndex(comboBox->count() - 1);
}


void ViewDialog::onCustomFileButtonClicked(const std::string& selectorId) {
    // Get last used directory from settings, or initialize from m_disk_image on first use
    QString customFilesDir = m_settings->value("viewer/custom_files_dir", "").toString();

    if (customFilesDir.isEmpty()) {
        // First time: use directory from m_disk_image (UTF-8 safe for Windows)
        customFilesDir = QFileInfo(QString::fromUtf8(m_disk_image->file_name().c_str())).dir().absolutePath();
    }

    // Determine file filter based on selector type
    QString filter;
    QString dialogTitle;

    if (selectorId == "agat_palette") {
        filter = tr("FIL files (*.fil);;All files (*.*)");
        dialogTitle = tr("Select custom palette file");
    } else if (selectorId == "agat_font") {
        filter = tr("FIL files (*.fil);;All files (*.*)");
        dialogTitle = tr("Select custom font file");
    } else {
        // Generic filter
        filter = tr("All files (*.*)");
        dialogTitle = tr("Select custom file");
    }

    // Open file dialog with stored directory
    QString fileName = QFileDialog::getOpenFileName(
        this,
        dialogTitle,
        customFilesDir,  // Use directory from settings or initial value
        filter
    );

    if (fileName.isEmpty()) return;  // User cancelled

    // Store the directory of the chosen file in settings for next time
    QString chosenDir = QFileInfo(fileName).dir().absolutePath();
    m_settings->setValue("viewer/custom_files_dir", chosenDir);

    // Find the corresponding combobox
    for (const auto& group : m_selectorWidgets) {
        if (group.selectorId == selectorId) {
            addCustomFileToComboBox(selectorId, fileName, group.comboBox);
            break;
        }
    }
}


void ViewDialog::onClearCustomFilesButtonClicked(const std::string& selectorId) {
    // Ask for confirmation
    int ret = QMessageBox::question(
        this,
        tr("Clear custom files"),
        tr("Are you sure you want to clear all custom files for this selector?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ret != QMessageBox::Yes) return;

    // Find the corresponding combobox and clear
    for (const auto& group : m_selectorWidgets) {
        if (group.selectorId == selectorId) {
            clearCustomFilesForSelector(selectorId, group.comboBox);
            break;
        }
    }
}


void ViewDialog::clearCustomFilesForSelector(const std::string& selectorId, QComboBox* comboBox) {
    // Clear from settings
    QString settingsKey = getCustomFilesSettingsKey(selectorId);
    m_settings->remove(settingsKey);

    // Remove custom items and separator from combobox
    // Find the separator
    int separatorIndex = -1;
    for (int i = 0; i < comboBox->count(); ++i) {
        if (comboBox->itemData(i).isNull()) {  // Separator has null data
            separatorIndex = i;
            break;
        }
    }

    // Remove items starting from separator onwards
    if (separatorIndex != -1) {
        for (int i = comboBox->count() - 1; i >= separatorIndex; --i) {
            comboBox->removeItem(i);
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

void ViewDialog::on_copyButton_clicked()
{
    QString html = ui->textEdit->toHtml();
    QMimeData *mimeData = new QMimeData();
    mimeData->setHtml(html);
    mimeData->setText(ui->textEdit->toPlainText());
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(mimeData);
}


void ViewDialog::on_saveButton_clicked()
{
    QString file_name = m_file_name;

    QStringList filters = {"HTML (*.html)", "TXT (*.txt)"};
    QString filters_str = filters.join(";;");
    QString selected_filter = m_settings->value("viewer/txt_filter", filters[0]).toString();

    QString dir = m_settings->value("viewer/txt_save_dir", m_settings->value("directory/save_to_file").toString()).toString() + "/";


    #ifdef __linux__
        file_name = QFileDialog::getSaveFileName(this, ViewDialog::tr("Save as"), dir + file_name, filters_str, &selected_filter, QFileDialog::DontConfirmOverwrite);
    #else
        file_name = QFileDialog::getSaveFileName(this, ViewDialog::tr("Save as"), dir + file_name, filters_str, &selected_filter);
    #endif

    if (!file_name.isEmpty()) {
        #ifdef __linux__
            QString ext = selected_filter.split("*")[1].split(")")[0];
            if (ext != ".") file_name += ext;
        #endif
        QFileInfo fi(file_name);
        #ifdef __linux__
            // In Linux FileSaveDialog works with extensions incorrectly, so we have to check the existence manually
            if (fi.exists()) {
                QMessageBox::StandardButton res = QMessageBox::question(this, ViewDialog::tr("File exists"), ViewDialog::tr("File already exists. Overwrite?"));
                if (res != QMessageBox::Yes) return;
            }
        #endif
        UTF8_ofstream file(file_name.toStdString(), std::ios::binary);
        if (file.good()) {
            std::string buffer;
            if (selected_filter.startsWith("HTML")) {
                QString html =  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\r\n"
                                "<html><head><meta charset=\"utf-8\" /><style type=\"text/css\">" +
                                m_saved_css +
                                "</style></head><body><div style=\"display: flex; flex-direction: column; gap: 0\">" +
                                m_saved_html +
                                "</div></body></html>";
                buffer = html.toUtf8().toStdString();
            }
            else
                buffer = ui->textEdit->toPlainText().toUtf8().toStdString();
            file.write(buffer.data(), buffer.size());
        }
        QString new_dir = fi.absolutePath();
        m_settings->setValue("directory/save_to_file", new_dir);
        m_settings->setValue("viewer/txt_filter", selected_filter);
    }

}


void ViewDialog::on_infoButton_clicked()
{
    FileOperations::infoDialog(this, QString::fromStdString(m_filesystem->file_info(m_f)));
}


void ViewDialog::onInfoButtonClicked(QToolButton* button, const std::string& selectorId)
{
    if (selectorId == AGAT_INFO_SELECTOR_ID) {
        const QString infoText = QString::fromStdString(dsk_tools::agat_vr_info(m_data, true));

        // Create a small popup frame window
        QFrame* popupFrame = new QFrame();
        popupFrame->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        popupFrame->setStyleSheet("QFrame { background-color: #f0f0f0; border: 1px solid #999999; border-radius: 4px; }");

        // Create layout with a label
        QVBoxLayout* layout = new QVBoxLayout(popupFrame);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(0);

        QFont font = getMonospaceFont(10);

        QLabel* infoLabel = new QLabel(infoText);
        infoLabel->setFont(font);
        infoLabel->setWordWrap(true);
        infoLabel->setMaximumWidth(300);
        infoLabel->setStyleSheet("QLabel { color: #000000; font-size: 14px; }");
        // infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        layout->addWidget(infoLabel);

        // Set a reasonable size and adjust before positioning
        popupFrame->adjustSize();

        // Position the popup right-aligned with the button and below it
        int xOffset = button->width() - popupFrame->width();
        QPoint buttonPos = button->mapToGlobal(QPoint(xOffset, button->height()));
        popupFrame->move(buttonPos);

        // Show the popup
        popupFrame->show();

        // Auto-delete the popup after a timeout (3 seconds)
        QTimer::singleShot(5000, popupFrame, &QWidget::deleteLater);
    }
}

