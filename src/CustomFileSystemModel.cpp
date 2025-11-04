// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File system model, to change some fields

#include "CustomFileSystemModel.h"
// #include <QLocale>

QVariant CustomFileSystemModel::data(const QModelIndex &index, int role) const {
    // Show <DIR> for directories, bytes for files with space separator
    if (index.column() == 1 && role == Qt::DisplayRole) {
        QFileInfo info = fileInfo(index);
        if (info.isDir())
            return QStringLiteral("<DIR>");
        else {
            QString numStr = QString::number(info.size());
            QString result;
            int count = 0;
            for (int i = numStr.length() - 1; i >= 0; --i) {
                if (count == 3) {
                    result.prepend(' ');
                    count = 0;
                }
                result.prepend(numStr[i]);
                count++;
            }
            return result;
        }
    }

    // Right-align date column
    if (index.column() == 3 && role == Qt::TextAlignmentRole) {
        return QVariant(Qt::AlignRight | Qt::AlignVCenter);
    }

    return QFileSystemModel::data(index, role);
}
