// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File system model, to change some fields

#include "CustomFileSystemModel.h"

QVariant CustomFileSystemModel::data(const QModelIndex &index, int role) const {
    // Show <DIR> for directories instead of size
    if (index.column() == 1 && role == Qt::DisplayRole) {
        QFileInfo info = fileInfo(index);
        if (info.isDir())
            return QStringLiteral("<DIR>");
    }
    return QFileSystemModel::data(index, role);
}
