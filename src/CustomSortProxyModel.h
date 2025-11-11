// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File sorting class

#pragma once
#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QFileInfo>
#include <QDateTime>

class CustomSortProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;
};
