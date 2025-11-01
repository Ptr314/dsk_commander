// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File system model, to change some fields

#pragma once
#include <QFileSystemModel>
#include <QFileInfo>

class CustomFileSystemModel : public QFileSystemModel {
    Q_OBJECT
public:
    using QFileSystemModel::QFileSystemModel;

    QVariant data(const QModelIndex &index, int role) const override;
};
