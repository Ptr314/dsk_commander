// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File sorting class

#include "CustomSortProxyModel.h"

bool CustomSortProxyModel::lessThan(const QModelIndex &source_left,
                                   const QModelIndex &source_right) const
{
    const QFileSystemModel* model =
        qobject_cast<const QFileSystemModel*>(sourceModel());
    if (!model)
        return QSortFilterProxyModel::lessThan(source_left, source_right);

    QFileInfo leftInfo  = model->fileInfo(source_left);
    QFileInfo rightInfo = model->fileInfo(source_right);

    const bool leftIsDir  = leftInfo.isDir();
    const bool rightIsDir = rightInfo.isDir();

    // Dirs have more precedence
    if (leftIsDir != rightIsDir)
        return leftIsDir;

    // Sorting
    int column = sortColumn();
    switch (column) {
    case 0: // Name
        return QString::localeAwareCompare(leftInfo.fileName(), rightInfo.fileName()) < 0;
    case 1: // Size
        return leftInfo.size() < rightInfo.size();
    case 3: // Date
        return leftInfo.lastModified() < rightInfo.lastModified();
    default:
        return QSortFilterProxyModel::lessThan(source_left, source_right);
    }
}
