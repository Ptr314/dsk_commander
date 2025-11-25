// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for selecting file format

#pragma once

#include <QDialog>
#include <QMap>
#include <QLabel>
#include <QComboBox>
#include <QDialogButtonBox>

class FormatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FormatDialog(QWidget *parent,
                          const QMap<QString, QString> &formatMap,
                          const QString &defaultFormat = "",
                          const QString &infoText = "",
                          const QString &labelText = "",
                          const QString &comboToolTip = "");

    ~FormatDialog();

    // Public method to get selected format short_name
    QString getSelectedFormat() const;

protected:
    void accept() override;

private:
    void setupUi();

    QLabel *info;
    QLabel *label;
    QComboBox *formatCombo;
    QDialogButtonBox *buttonBox;
    QString m_selectedFormat;  // Stores the chosen short_name
};