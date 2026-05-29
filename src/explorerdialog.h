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

    // Drives the whole model: geometry (heads, cylinders, widest track) and
    // sector data all come from this structure. The pointer is non-owning;
    // the StructDisk must outlive the model (ExplorerDialog owns it).
    void setDisk(const dsk_tools::StructDisk *disk);
    void setOrder(Order order);
    void setSplitByHeads(bool split);
    void setHexNumbers(bool hex);
    void setSectorTypeMap(dsk_tools::SectorTypeMap map);
    Order order() const { return m_order; }
    bool splitByHeads() const { return m_split_by_heads; }
    bool hexNumbers() const { return m_hex; }
    // Formats a track/sector number honouring the current decimal/hex mode.
    QString formatNumber(unsigned value) const;
    unsigned heads()      const { return m_heads; }
    unsigned cylinders()  const { return m_cylinders; }
    unsigned maxSectors() const { return m_max_sectors; }

    // True for the narrow placeholder columns inserted between head groups
    // when heads are concatenated on a single row (split-by-heads off, heads > 1).
    // Spacer columns carry no data and are not selectable.
    bool isSpacerColumn(int column) const;

    // Resolves a cell to (head, cylinder, column) — pure geometry, no data lookup.
    // column is the displayed sector slot (0..maxSectors-1).
    bool resolve(const QModelIndex &index, unsigned &head, unsigned &cylinder, unsigned &column) const;

    // Resolves a cell to its actual sector, honouring the current order:
    //   Physical — column maps straight to the physical sector slot.
    //   Logical  — column c maps to the sector whose 1-based id (sector_map) is c+1.
    // Returns nullptr when the cell carries no sector (spacer, or column beyond
    // this track's sector count). On success fills head/cylinder/sector_id (1-based)
    // and sector_size from the owning track.
    const dsk_tools::StructSector *sectorAt(const QModelIndex &index,
                                            unsigned &head, unsigned &cylinder,
                                            unsigned &sector_id, unsigned &sector_size) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    // Locates the track record for a given (cylinder, head). Assumes the loader
    // stored tracks in cylinder-major / head-interleaved order, verifies the hit,
    // and falls back to a linear search if the layout differs.
    const dsk_tools::StructTrack *trackAt(unsigned cylinder, unsigned head) const;

    // Maps a (head, cylinder, column) triple to a sector honouring m_order.
    const dsk_tools::StructSector *locate(unsigned head, unsigned cylinder, unsigned column,
                                          unsigned &sector_id, unsigned &sector_size) const;

    // Bad first (from StructSector::is_bad), then the optional filesystem map
    // (keyed by 0-based logical sector = sector_id - 1), else Ok by default.
    dsk_tools::SectorType typeOf(const dsk_tools::StructSector *sector,
                                 unsigned head, unsigned cylinder, unsigned sector_id) const;

    const dsk_tools::StructDisk *m_disk = nullptr;   // non-owning; owned by ExplorerDialog
    Order m_order = Order::Physical;
    bool m_split_by_heads = false;
    bool m_hex = false;                      // track/sector numbers shown in hexadecimal
    unsigned m_heads = 0;
    unsigned m_cylinders = 0;
    unsigned m_max_sectors = 0;              // widest track, defines the column count
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
                   std::unique_ptr<dsk_tools::fileSystem> filesystem,
                   std::unique_ptr<dsk_tools::StructDisk> disk_struct);
    ~ExplorerDialog();

    static constexpr int kDefaultSectionSize = 16;
    static constexpr int kSpacerColumnSize   = 3;

private slots:
    void on_closeBtn_clicked();
    void on_infoBtn_clicked();
    void on_fsInfoBtn_clicked();
    void on_orderCombo_currentIndexChanged(int index);
    void on_encodingCombo_currentIndexChanged(int index);
    void on_splitByHeadsCheck_toggled(bool checked);
    void on_hexCheck_toggled(bool checked);
    void onSectorCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

private:
    Ui::ExplorerDialog *ui;
    QSettings *m_settings;
    QString m_file_name;
    std::unique_ptr<dsk_tools::diskImage> m_image;
    std::unique_ptr<dsk_tools::fileSystem> m_filesystem;
    std::unique_ptr<dsk_tools::StructDisk> m_disk_struct;
    SectorTableModel *m_model = nullptr;
    SectorCellDelegate *m_delegate = nullptr;

    void populateEncodings();
    void populateOrders();
    void applyColumnLayout();
    void updateGeometryLabel();
    void updateHexView(const QModelIndex &index);
};