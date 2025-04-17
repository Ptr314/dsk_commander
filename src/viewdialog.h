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

    void on_subtypeCombo_currentTextChanged(const QString &arg1);

private:
    Ui::ViewDialog *ui;

    dsk_tools::BYTES m_data;
    QSettings *m_settings;
    bool use_subtypes;
    void print_data();
    void update_subtypes(const QString &preferred = "");
};

#endif // VIEWDIALOG_H
