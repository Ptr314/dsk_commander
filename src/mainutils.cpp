// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Qt compatibility utilities and helper functions

#include <QComboBox>
#include "mainutils.h"

void adjustComboBoxWidth(QComboBox* comboBox) {
    QFontMetrics fm(comboBox->font());
    int maxWidth = 0;
    for (int i = 0; i < comboBox->count(); ++i) {
        #if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
            int width = fm.width(comboBox->itemText(i));
        #else
            int width = fm.horizontalAdvance(comboBox->itemText(i));
        #endif
        if (width > maxWidth)
            maxWidth = width;
    }
    maxWidth += 30;
    comboBox->setMinimumWidth(maxWidth);
}

std::vector<std::string> get_types_from_map(const std::map<std::string, std::vector<std::string>>& m_subtypes) {
    std::vector<std::string> types;
    for (const auto& pair : m_subtypes) {
        types.push_back(pair.first);
    }
    return types;
}
