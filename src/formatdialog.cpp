// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for selecting file format

#include "formatdialog.h"
#include <QVBoxLayout>

FormatDialog::FormatDialog(QWidget *parent,
                           const QMap<QString, QString> &formatMap,
                           const QString &defaultFormat,
                           const QString &infoText,
                           const QString &labelText,
                           const QString &comboToolTip)
    : QDialog(parent)
{
    setupUi();

    // Set info text if provided
    if (!infoText.isEmpty()) {
        info->setText(infoText);
    }

    // Set label text if provided
    if (!labelText.isEmpty()) {
        label->setText(labelText);
    }

    // Set combo tooltip if provided
    if (!comboToolTip.isEmpty()) {
        formatCombo->setToolTip(comboToolTip);
    }

    // Populate combo box
    formatCombo->blockSignals(true);
    formatCombo->clear();

    // Iterate through formatMap (short_name -> full name)
    QMapIterator<QString, QString> i(formatMap);
    int defaultIndex = -1;
    int currentIndex = 0;
    while (i.hasNext()) {
        i.next();
        QString shortName = i.key();
        QString fullName = i.value();

        // Display full name, store short_name as user data
        formatCombo->addItem(fullName, shortName);

        // Track index of default format
        if (shortName == defaultFormat) {
            defaultIndex = currentIndex;
        }
        currentIndex++;
    }

    formatCombo->blockSignals(false);

    // Disable combo if empty or only one element
    if (formatMap.isEmpty() || formatMap.size() == 1) {
        formatCombo->setEnabled(false);
    }

    // Set selection to default format if found, otherwise first item
    if (defaultIndex >= 0) {
        formatCombo->setCurrentIndex(defaultIndex);
    } else if (formatCombo->count() > 0) {
        formatCombo->setCurrentIndex(0);
    }

    // Set initial dialog size
    resize(400, 150);
}

void FormatDialog::setupUi()
{
    // Create main layout
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Create info
    info = new QLabel(this);
    layout->addWidget(info);

    // Create label
    label = new QLabel(this);
    layout->addWidget(label);

    // Create combo box
    formatCombo = new QComboBox(this);
    layout->addWidget(formatCombo);

    // Create button box
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    // Connect signals
    connect(buttonBox, &QDialogButtonBox::accepted, this, &FormatDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &FormatDialog::reject);
}

FormatDialog::~FormatDialog()
{
    // Widgets are automatically deleted by Qt parent-child relationship
}

void FormatDialog::accept()
{
    // Capture selected short_name from combo box user data
    int currentIndex = formatCombo->currentIndex();
    if (currentIndex >= 0) {
        m_selectedFormat = formatCombo->itemData(currentIndex).toString();
    }

    QDialog::accept();
}

QString FormatDialog::getSelectedFormat() const
{
    return m_selectedFormat;
}