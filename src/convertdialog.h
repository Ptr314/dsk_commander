// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for exporting disk images

#pragma once

#include <QDialog>
#include <QSettings>
#include <QJsonObject>

#include "dsk_tools/dsk_tools.h"

namespace Ui {
class ConvertDialog;
}

class ConvertDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConvertDialog(QWidget *parent = nullptr);
    explicit ConvertDialog(QWidget *parent,
                           QSettings *settings,
                           const QJsonObject * file_types,
                           const QJsonObject * file_formats,
                           dsk_tools::diskImage * image,
                           const QString & type_id,
                           int fs_volume_id
    );

    ~ConvertDialog();
    QString output_file_name;
    QString template_file_name;
    int exec(QString & target_id, QString & output_file, QString & template_file, int & numtracks, uint8_t & volume_id);

protected:
    void accept() override;

private slots:
    void on_formatCombo_currentIndexChanged(int index);

    void on_actionChoose_Output_triggered();

    void on_useCheck_checkStateChanged(const Qt::CheckState &arg1);

    void on_actionChoose_Template_triggered();

private:
    Ui::ConvertDialog *ui;

    QString m_type_id;
    QSettings * m_settings;
    const QJsonObject * m_file_types;
    const QJsonObject * m_file_formats;
    dsk_tools::diskImage * m_image;
    int m_fs_volume_id;

    void set_output();
    void set_controls();
    void save_setup();


};
