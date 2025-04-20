// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for viewing files

#ifndef VIEWDIALOG_H
#define VIEWDIALOG_H

#include <QDialog>
#include <QSettings>

#include "dsk_tools/dsk_tools.h"

namespace Ui {
class ViewDialog;
}

class ViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewDialog(QWidget *parent, QSettings  *settings, const dsk_tools::BYTES &data, int preferred_type, bool deleted);
    ~ViewDialog();

private slots:
    void on_closeBtn_clicked();

    void on_modeCombo_currentIndexChanged(int index);

    void on_encodingCombo_currentTextChanged(const QString &arg1);

    void on_scaleSlider_valueChanged(int value);

    void on_optionsCombo_currentIndexChanged(int index);

    void on_propsCombo_currentIndexChanged(int index);

    void on_subtypeCombo_currentIndexChanged(int index);

private:
    Ui::ViewDialog *ui;

    dsk_tools::BYTES m_data;
    QSettings *m_settings;
    int m_scaleFactor;
    QImage m_image;
    int m_opt = 0;
    dsk_tools::BYTES m_imageData;
    bool use_subtypes;
    std::map<std::string, int> last_subtypes;
    std::map<std::string, std::vector<std::string>> m_subtypes;
    void print_data();
    void update_subtypes(const QString &preferred = "");
    void update_image();
    void fill_options();
    QString replace_placeholders(const QString & in);
};

#endif // VIEWDIALOG_H
