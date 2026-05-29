// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Low-level FDD image explorer dialog

#pragma once

#include <QDialog>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QSettings>
#include <memory>

#include "dsk_tools/dsk_tools.h"

namespace Ui {
class ExplorerDialog;
}

class SectorTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum class Order { Physical, Logical };

    // Custom roles
    enum CellRoles {
        StateRole = Qt::UserRole + 1,
        TrackRole,
        HeadRole,
        SectorRole
    };

    explicit SectorTableModel(QObject *parent = nullptr);

    void setImage(dsk_tools::diskImage *image);
    void setOrder(Order order);
    void setSplitByHeads(bool split);
    void setSectorTypeMap(dsk_tools::SectorTypeMap map);
    Order order() const { return m_order; }
    bool splitByHeads() const { return m_split_by_heads; }
    unsigned heads()   const { return m_heads; }
    unsigned sectors() const { return m_sectors; }
    dsk_tools::diskImage *image() const { return m_image; }

    // True for the narrow placeholder columns inserted between head groups
    // when heads are concatenated on a single row (split-by-heads off, heads > 1).
    // Spacer columns carry no data and are not selectable.
    bool isSpacerColumn(int column) const;

    // Resolves a cell to (head, track, sector_within_track).
    // For Order::Physical, returned sector is the physical index (no translation).
    // For Order::Logical,  returned sector is the logical  index (translation applied at read time).
    bool resolve(const QModelIndex &index, unsigned &head, unsigned &track, unsigned &sector) const;

    // Same logic used by StateRole: bad first, then the optional filesystem map,
    // else Ok by default. Centralised here so tooltips, paint and any future
    // consumers stay in sync.
    dsk_tools::SectorType sectorTypeAt(unsigned head, unsigned track, unsigned sector) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    dsk_tools::diskImage *m_image = nullptr;
    Order m_order = Order::Physical;
    bool m_split_by_heads = false;
    unsigned m_heads = 0;
    unsigned m_tracks = 0;
    unsigned m_sectors = 0;
    dsk_tools::SectorTypeMap m_type_map;     // empty unless a filesystem supplied one
};

class SectorCellDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SectorCellDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class ExplorerDialog : public QDialog
{
    Q_OBJECT
public:
    ExplorerDialog(QWidget *parent,
                   QSettings *settings,
                   const QString &file_name,
                   std::unique_ptr<dsk_tools::diskImage> image,
                   std::unique_ptr<dsk_tools::fileSystem> filesystem);
    ~ExplorerDialog();

    static constexpr int kDefaultSectionSize = 18;
    static constexpr int kSpacerColumnSize   = 4;

private slots:
    void on_closeBtn_clicked();
    void on_orderCombo_currentIndexChanged(int index);
    void on_encodingCombo_currentIndexChanged(int index);
    void on_splitByHeadsCheck_toggled(bool checked);
    void onSectorCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

private:
    Ui::ExplorerDialog *ui;
    QSettings *m_settings;
    QString m_file_name;
    std::unique_ptr<dsk_tools::diskImage> m_image;
    std::unique_ptr<dsk_tools::fileSystem> m_filesystem;
    SectorTableModel *m_model = nullptr;
    SectorCellDelegate *m_delegate = nullptr;

    void populateEncodings();
    void populateOrders();
    void applyColumnLayout();
    void updateGeometryLabel();
    void updateHexView(const QModelIndex &index);
};