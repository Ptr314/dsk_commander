// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for viewing files

#pragma once

#include <QDialog>
#include <QSettings>
#include <QTimer>
#include <QLabel>
#include <QComboBox>
#include <QSpacerItem>
#include <QPushButton>
#include <QToolButton>
#include <QFileDialog>
#include <QFileInfo>

#include "dsk_tools/dsk_tools.h"

namespace Ui {
class ViewDialog;
}

class ViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewDialog(QWidget *parent, QSettings  *settings, const QString file_name, const dsk_tools::BYTES &data, dsk_tools::PreferredType preferred_type, bool deleted, dsk_tools::diskImage * image, dsk_tools::fileSystem * filesystem, const dsk_tools::UniversalFile& f);
    ~ViewDialog();

private slots:
    void on_closeBtn_clicked();

    void on_modeCombo_currentIndexChanged(int index);

    void on_encodingCombo_currentTextChanged(const QString &arg1);

    void on_scaleSlider_valueChanged(int value);

    void on_propsCombo_currentIndexChanged(int index);

    void on_subtypeCombo_currentIndexChanged(int index);

    void pic_timer_proc();

    void on_copyButton_clicked();

    void on_saveButton_clicked();

    void on_infoButton_clicked();

private:
    Ui::ViewDialog *ui;

    QString m_file_name;
    dsk_tools::BYTES m_data;
    dsk_tools::diskImage * m_disk_image;
    dsk_tools::fileSystem * m_filesystem;
    dsk_tools::UniversalFile m_f;
    std::unique_ptr<dsk_tools::Viewer> m_viewer;
    bool recreate_viewer = true;
    QSettings *m_settings;
    int m_scaleFactor;
    QImage m_image;
    int m_pic_frame = 0;
    QTimer m_pic_timer;
    dsk_tools::BYTES m_imageData;
    bool use_subtypes;
    std::map<std::string, int> last_subtypes;
    std::map<std::string, std::vector<std::string>> m_subtypes;
    void print_data();
    void update_subtypes(const QString &preferred = "");
    void update_image();
    void fill_options();

    void store_scale(int value);
    void restore_scale();

    QString m_saved_css;
    QString m_saved_html;

    // Dynamic selector widgets for picture viewers
    struct SelectorWidgetGroup {
        QLabel* iconLabel;           // Icon for the selector
        QComboBox* comboBox;         // Dropdown widget
        QToolButton* customButton;   // Tool button for adding custom files (nullptr if !has_customs())
        QToolButton* clearButton;    // Tool button for clearing custom files (nullptr if !has_customs())
        QSpacerItem* spacerBefore;   // 20px spacer before section
        QSpacerItem* spacerBetween;  // 5px spacer between icon and combo
        QSpacerItem* buttonSpacer;   // 2px spacer between combo and custom button (nullptr if !has_customs())
        QSpacerItem* clearButtonSpacer;  // 2px spacer between custom button and clear button (nullptr if !has_customs())
        std::string selectorId;      // Selector ID (e.g., "agat_font_type")
    };
    std::vector<SelectorWidgetGroup> m_selectorWidgets;

    void clearSelectorWidgets();
    void populateSelectorWidgets(dsk_tools::ViewerSelectorValues suggested_values);
    void onSelectorChanged(const QString& selectorId, const QString& selectedValue);
    dsk_tools::ViewerSelectorValues collectSelectors();

    // Custom file selection methods
    void onCustomFileButtonClicked(const std::string& selectorId);
    void onClearCustomFilesButtonClicked(const std::string& selectorId);
    void loadCustomFilesForSelector(const std::string& selectorId, QComboBox* comboBox);
    void addCustomFileToComboBox(const std::string& selectorId, const QString& filePath, QComboBox* comboBox);
    void clearCustomFilesForSelector(const std::string& selectorId, QComboBox* comboBox);
    QString getCustomFilesSettingsKey(const std::string& selectorId);

};
