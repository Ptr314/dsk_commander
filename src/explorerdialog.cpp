// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Low-level FDD image explorer dialog

#include "explorerdialog.h"
#include "ui_explorerdialog.h"

#include "mainutils.h"
#include "FileOperations.h"

#include <QHeaderView>
#include <QPainter>
#include <QFontMetrics>
#include <QFileInfo>
#include <QItemSelectionModel>

#include "charmaps.h"

// ---------------------------------------------------------------------------
// SectorTableModel
// ---------------------------------------------------------------------------

SectorTableModel::SectorTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

void SectorTableModel::setDisk(const dsk_tools::StructDisk *disk)
{
    beginResetModel();
    m_disk = disk;
    if (m_disk && !m_disk->tracks.empty()) {
        m_heads = m_disk->heads ? m_disk->heads : 1;
        m_cylinders = static_cast<unsigned>(m_disk->tracks.size()) / m_heads;
        // Tracks may carry different sector counts; the grid is sized to the widest.
        m_max_sectors = 0;
        for (const auto &t : m_disk->tracks) {
            const unsigned n = static_cast<unsigned>(t.sectors.size());
            if (n > m_max_sectors) m_max_sectors = n;
        }
    } else {
        m_heads = m_cylinders = m_max_sectors = 0;
    }
    endResetModel();
}

const dsk_tools::StructTrack *SectorTableModel::trackAt(unsigned cylinder, unsigned head) const
{
    if (!m_disk) return nullptr;
    // Fast path: cylinder-major, head-interleaved ordering (how loaders emit tracks).
    const unsigned idx = cylinder * m_heads + head;
    if (idx < m_disk->tracks.size()) {
        const auto &t = m_disk->tracks[idx];
        if (t.cylinder == cylinder && t.head == head) return &t;
    }
    // Fallback: layout differs — find the matching record explicitly.
    for (const auto &t : m_disk->tracks) {
        if (t.cylinder == cylinder && t.head == head) return &t;
    }
    return nullptr;
}

const dsk_tools::StructSector *SectorTableModel::locate(unsigned head, unsigned cylinder, unsigned column,
                                                        unsigned &sector_id, unsigned &sector_size) const
{
    const dsk_tools::StructTrack *t = trackAt(cylinder, head);
    if (!t) return nullptr;
    sector_size = t->sector_size;
    const unsigned nsec = static_cast<unsigned>(t->sectors.size());

    if (m_order == Order::Physical) {
        // "As is" order — column maps straight to the physical sector slot.
        if (column >= nsec) return nullptr;
        sector_id = (column < t->sector_map.size()) ? t->sector_map[column] : (column + 1);
        return &t->sectors[column];
    }

    // Logical order — column c stands for the sector whose 1-based id is c+1.
    const unsigned target = column + 1;
    const unsigned map_n = static_cast<unsigned>(t->sector_map.size());
    for (unsigned p = 0; p < map_n && p < nsec; ++p) {
        if (t->sector_map[p] == target) {
            sector_id = target;
            return &t->sectors[p];
        }
    }
    // No sector map (or id not present): fall back to physical position.
    if (map_n == 0 && column < nsec) {
        sector_id = column + 1;
        return &t->sectors[column];
    }
    return nullptr;
}

dsk_tools::SectorType SectorTableModel::typeOf(const dsk_tools::StructSector *sector,
                                               unsigned head, unsigned cylinder, unsigned sector_id) const
{
    if (!sector) return dsk_tools::SectorType::Empty;
    if (sector->is_bad) return dsk_tools::SectorType::Bad;
    // Filesystem map (when present) is keyed by 0-based logical sector.
    if (!m_type_map.empty()) {
        const auto it = m_type_map.find({head, cylinder, sector_id - 1});
        return (it != m_type_map.end()) ? it->second : dsk_tools::SectorType::Empty;
    }
    return dsk_tools::SectorType::Ok;
}

void SectorTableModel::setOrder(Order order)
{
    if (m_order == order) return;
    beginResetModel();
    m_order = order;
    endResetModel();
}

void SectorTableModel::setSplitByHeads(bool split)
{
    if (m_split_by_heads == split) return;
    beginResetModel();
    m_split_by_heads = split;
    endResetModel();
}

void SectorTableModel::setHexNumbers(bool hex)
{
    if (m_hex == hex) return;
    m_hex = hex;
    // Only the displayed labels change, not the structure — refresh headers and
    // tooltips in place so the current selection is preserved.
    if (columnCount() > 0) emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
    if (rowCount() > 0)    emit headerDataChanged(Qt::Vertical, 0, rowCount() - 1);
    if (rowCount() > 0 && columnCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::ToolTipRole});
    }
}

QString SectorTableModel::formatNumber(unsigned value) const
{
    return m_hex ? QString::number(value, 16).toUpper() : QString::number(value);
}

void SectorTableModel::setSectorTypeMap(dsk_tools::SectorTypeMap map)
{
    m_type_map = std::move(map);
    // Repaint without disturbing structure
    if (rowCount() > 0 && columnCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1),
                         {StateRole, Qt::ToolTipRole});
    }
}

bool SectorTableModel::isSpacerColumn(int column) const
{
    if (m_split_by_heads) return false;
    if (m_heads <= 1 || m_max_sectors == 0) return false;
    if (column < 0) return false;
    const int stride = static_cast<int>(m_max_sectors) + 1;     // sectors + one spacer
    return (column % stride) == static_cast<int>(m_max_sectors);
}

bool SectorTableModel::resolve(const QModelIndex &index, unsigned &head, unsigned &cylinder, unsigned &column) const
{
    if (!m_disk || !index.isValid()) return false;
    if (m_heads == 0 || m_cylinders == 0 || m_max_sectors == 0) return false;

    const unsigned row = static_cast<unsigned>(index.row());
    const unsigned col = static_cast<unsigned>(index.column());

    if (m_split_by_heads) {
        if (row >= m_cylinders * m_heads) return false;
        if (col >= m_max_sectors) return false;
        cylinder = row / m_heads;
        head     = row % m_heads;
        column   = col;
    } else {
        if (row >= m_cylinders) return false;
        if (m_heads > 1) {
            const unsigned stride = m_max_sectors + 1;          // sectors + one spacer
            const unsigned group  = col / stride;
            const unsigned off    = col % stride;
            if (group >= m_heads || off >= m_max_sectors) return false;   // out of range / spacer
            cylinder = row;
            head     = group;
            column   = off;
        } else {
            if (col >= m_max_sectors) return false;
            cylinder = row;
            head     = 0;
            column   = col;
        }
    }
    return true;
}

const dsk_tools::StructSector *SectorTableModel::sectorAt(const QModelIndex &index,
                                                          unsigned &head, unsigned &cylinder,
                                                          unsigned &sector_id, unsigned &sector_size) const
{
    unsigned column;
    if (!resolve(index, head, cylinder, column)) return nullptr;
    return locate(head, cylinder, column, sector_id, sector_size);
}

int SectorTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    const unsigned heads = m_heads ? m_heads : 1;
    return static_cast<int>(m_split_by_heads ? m_cylinders * heads : m_cylinders);
}

int SectorTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    const unsigned heads = m_heads ? m_heads : 1;
    if (m_split_by_heads || heads <= 1) {
        return static_cast<int>(m_split_by_heads ? m_max_sectors : m_max_sectors * heads);
    }
    // sectors per head × heads + (heads - 1) spacers between groups
    return static_cast<int>(m_max_sectors * heads + (heads - 1));
}

QVariant SectorTableModel::data(const QModelIndex &index, int role) const
{
    if (!m_disk || !index.isValid()) return {};

    unsigned head, cylinder, sector_id, sector_size;
    const dsk_tools::StructSector *sector = sectorAt(index, head, cylinder, sector_id, sector_size);
    // No sector behind this cell (spacer, or column past this track's count):
    // return an invalid variant so the delegate leaves the cell blank.
    if (!sector) return {};

    const dsk_tools::SectorType type = typeOf(sector, head, cylinder, sector_id);

    switch (role) {
        case StateRole:  return QVariant::fromValue(static_cast<int>(type));
        case TrackRole:  return cylinder;
        case HeadRole:   return head;
        case SectorRole: return sector_id;
        case Qt::ToolTipRole: {
            // Logical track number = cylinder * heads + head, counting both sides.
            const unsigned heads = m_heads ? m_heads : 1;
            const unsigned track = cylinder * heads + head;
            QString tip = ExplorerDialog::tr("Track %1, Sector %2")
                              .arg(formatNumber(track), formatNumber(sector_id));

            QString type_str;
            switch (type) {
                case dsk_tools::SectorType::Empty:       type_str = ExplorerDialog::tr("free");       break;
                case dsk_tools::SectorType::Ok:          type_str = ExplorerDialog::tr("data");       break;
                case dsk_tools::SectorType::Bad:         type_str = ExplorerDialog::tr("bad");        break;
                case dsk_tools::SectorType::System:      type_str = ExplorerDialog::tr("system");     break;
                case dsk_tools::SectorType::Catalog:     type_str = ExplorerDialog::tr("catalog");    break;
                case dsk_tools::SectorType::File:        type_str = ExplorerDialog::tr("file");       break;
                case dsk_tools::SectorType::DeletedFile: type_str = ExplorerDialog::tr("deleted");    break;
            }
            if (!type_str.isEmpty()) tip += " — " + type_str;
            return tip;
        }
        default:
            return {};
    }
}

QVariant SectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole) return {};

    if (orientation == Qt::Vertical) {
        // Logical track number = cylinder * heads + head, counting both sides.
        // Split on:  one row per (cylinder, head) → section already is that number (0,1,2…).
        // Split off: one row per cylinder (head 0)  → section * heads (0,2,4… for two heads).
        const unsigned heads = m_heads ? m_heads : 1;
        const unsigned track = m_split_by_heads
                                   ? static_cast<unsigned>(section)
                                   : static_cast<unsigned>(section) * heads;
        return formatNumber(track);
    }

    // Horizontal: spacer columns have no label; data columns show the 1-based
    // sector slot (this table counts sectors from 1, matching sector_map ids).
    if (m_max_sectors == 0) return {};
    if (isSpacerColumn(section)) return QString();
    if (m_split_by_heads || m_heads <= 1) {
        return formatNumber(static_cast<unsigned>(section) % m_max_sectors + 1);
    }
    const unsigned stride = m_max_sectors + 1;
    return formatNumber(static_cast<unsigned>(section) % stride + 1);
}

Qt::ItemFlags SectorTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    if (isSpacerColumn(index.column())) return Qt::NoItemFlags;     // not selectable, not enabled
    return QAbstractTableModel::flags(index);
}

// ---------------------------------------------------------------------------
// SectorCellDelegate
// ---------------------------------------------------------------------------

SectorCellDelegate::SectorCellDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

QSize SectorCellDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(24, 20);
}

void SectorCellDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // Cell background — use the view's palette base so it blends with the grid
    painter->fillRect(opt.rect, opt.palette.base());

    // Spacer columns between head groups: leave them empty
    if (const auto *model = qobject_cast<const SectorTableModel*>(index.model())) {
        if (model->isSpacerColumn(index.column())) {
            painter->restore();
            return;
        }
    }

    const QVariant stateVar = index.data(SectorTableModel::StateRole);
    // Invalid state → the cell has no sector (track narrower than the grid):
    // leave it blank rather than painting an Empty square.
    if (!stateVar.isValid()) {
        painter->restore();
        return;
    }
    const dsk_tools::SectorType state = static_cast<dsk_tools::SectorType>(stateVar.toInt());

    // Centered square glyph, sized to the smaller of the available width/height
    const QRect inner = opt.rect.adjusted(1, 1, -1, -1);
    const int side = qMin(inner.width(), inner.height());
    QRect square(0, 0, side, side);
    square.moveCenter(inner.center());

    QColor fill;
    switch (state) {
        case dsk_tools::SectorType::Ok:          fill = QColor(0x2e, 0x8b, 0x57); break;  // sea green
        case dsk_tools::SectorType::Bad:         fill = QColor(0xb0, 0x30, 0x30); break;  // muted red
        case dsk_tools::SectorType::System:      fill = QColor(0x4a, 0x6f, 0xc8); break;  // steel blue
        case dsk_tools::SectorType::Catalog:     fill = QColor(0xc0, 0x6a, 0xd0); break;  // orchid
        case dsk_tools::SectorType::File:        fill = QColor(0x6c, 0xa6, 0x4c); break;  // apple green (lighter than Ok)
        case dsk_tools::SectorType::DeletedFile: fill = QColor(0xa7, 0xb8, 0x9d); break;  // gray-leaning sage (~⅔ Empty, ⅓ File)
        case dsk_tools::SectorType::Empty:       fill = QColor(0xc0, 0xc0, 0xc0); break;  // light gray
    }
    painter->fillRect(square, fill);

    // Selection: outline the square in the highlight color
    if (opt.state & QStyle::State_Selected) {
        QPen pen(opt.palette.highlight().color(), 2);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(square.adjusted(0, 0, -1, -1));
    }

    painter->restore();
}

// ---------------------------------------------------------------------------
// ExplorerDialog
// ---------------------------------------------------------------------------

ExplorerDialog::ExplorerDialog(QWidget *parent,
                               QSettings *settings,
                               const QString &file_name,
                               std::unique_ptr<dsk_tools::diskImage> image,
                               std::unique_ptr<dsk_tools::fileSystem> filesystem,
                               std::unique_ptr<dsk_tools::StructDisk> disk_struct)
    : QDialog(parent)
    , ui(new Ui::ExplorerDialog)
    , m_settings(settings)
    , m_file_name(file_name)
    , m_image(std::move(image))
    , m_filesystem(std::move(filesystem))
    , m_disk_struct(std::move(disk_struct))
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    setWindowTitle(tr("Image Explorer") + " — " + QFileInfo(m_file_name).fileName());

    populateOrders();
    populateEncodings();

    m_model = new SectorTableModel(this);
    m_delegate = new SectorCellDelegate(this);

    ui->sectorTable->setModel(m_model);
    ui->sectorTable->setItemDelegate(m_delegate);
    ui->sectorTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->sectorTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->sectorTable->horizontalHeader()->setDefaultSectionSize(kDefaultSectionSize);
    ui->sectorTable->verticalHeader()->setDefaultSectionSize(kDefaultSectionSize);
    ui->sectorTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->sectorTable->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->sectorTable->setShowGrid(false);
    ui->sectorTable->setFocusPolicy(Qt::StrongFocus);

    // Smaller font on the row/column headers so the track and sector numbers
    // stay readable in the narrow header cells. A global QApplication stylesheet
    // is active (see main.cpp), which makes QWidget::setFont() on the headers be
    // ignored — so the size has to go through a widget-level stylesheet instead.
    {
        const QFont base = ui->sectorTable->font();
        const int pt = base.pointSize();
        // padding/margin/border are zeroed: once a QSS rule touches the section,
        // Qt applies the CSS box model on top of the (tiny) section size, which
        // on Qt 5.6 crops the digits. Reclaiming that space keeps them readable.
        const QString hss = (pt > 0)
            ? QString("QHeaderView::section { font-size: %1pt; padding: 0px; margin: 0px; border: 0px; }").arg(qMax(1, pt - 2))
            : QString("QHeaderView::section { font-size: %1px; padding: 0px; margin: 0px; border: 0px; }").arg(qMax(1, base.pixelSize() - 2));
        ui->sectorTable->horizontalHeader()->setStyleSheet(hss);
        ui->sectorTable->verticalHeader()->setStyleSheet(hss);
    }

    // Filesystem info is only available when a filesystem was mounted.
    ui->fsInfoBtn->setVisible(static_cast<bool>(m_filesystem));

    // "Split by heads" is only meaningful for two-head images — hide it otherwise.
    const unsigned heads = (m_disk_struct && m_disk_struct->heads) ? m_disk_struct->heads : 1;
    const bool two_heads = heads > 1;
    ui->splitByHeadsCheck->setVisible(two_heads);

    const bool split = two_heads && m_settings
                           ? m_settings->value("explorer/split_by_heads", false).toBool() : false;
    ui->splitByHeadsCheck->blockSignals(true);
    ui->splitByHeadsCheck->setChecked(split);
    ui->splitByHeadsCheck->blockSignals(false);

    const bool hex = m_settings && m_settings->value("explorer/hex_numbers", false).toBool();
    ui->hexCheck->blockSignals(true);
    ui->hexCheck->setChecked(hex);
    ui->hexCheck->blockSignals(false);

    m_model->setDisk(m_disk_struct.get());
    m_model->setSplitByHeads(split);
    m_model->setHexNumbers(hex);
    m_model->setOrder(ui->orderCombo->currentData().toInt() == 1
                          ? SectorTableModel::Order::Logical
                          : SectorTableModel::Order::Physical);
    if (m_filesystem) {
        m_model->setSectorTypeMap(m_filesystem->get_sector_type_map());
    }
    applyColumnLayout();

    ui->hexEdit->setFont(getViewerFont(m_settings));

    updateGeometryLabel();

    connect(ui->sectorTable->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &ExplorerDialog::onSectorCurrentChanged);

    // Select the first sector by default so the right pane is not blank
    if (m_model->rowCount() > 0 && m_model->columnCount() > 0) {
        const QModelIndex first = m_model->index(0, 0);
        ui->sectorTable->setCurrentIndex(first);
    }

    if (m_settings) {
        const QByteArray geom = m_settings->value("explorer/geometry").toByteArray();
        if (!geom.isEmpty()) restoreGeometry(geom);
        const QByteArray splitterState = m_settings->value("explorer/splitter").toByteArray();
        if (!splitterState.isEmpty()) ui->splitter->restoreState(splitterState);
    }
}

ExplorerDialog::~ExplorerDialog()
{
    if (m_settings) {
        m_settings->setValue("explorer/geometry", saveGeometry());
        m_settings->setValue("explorer/splitter", ui->splitter->saveState());
    }
    delete ui;
}

void ExplorerDialog::populateOrders()
{
    ui->orderCombo->blockSignals(true);
    ui->orderCombo->clear();
    ui->orderCombo->addItem(tr("Physical order"), 0);
    ui->orderCombo->addItem(tr("Logical order"),  1);
    int idx = 0;
    if (m_settings) idx = m_settings->value("explorer/order", 0).toInt();
    if (idx < 0 || idx >= ui->orderCombo->count()) idx = 0;
    ui->orderCombo->setCurrentIndex(idx);
    adjustComboBoxWidth(ui->orderCombo);
    ui->orderCombo->blockSignals(false);
}

void ExplorerDialog::populateEncodings()
{
    ui->encodingCombo->blockSignals(true);
    ui->encodingCombo->clear();
    ui->encodingCombo->addItem(tr("Agat"),             "agat");
    ui->encodingCombo->addItem(tr("Apple II"),         "apple2");
    ui->encodingCombo->addItem(tr("Apple //c"),        "apple2c");
    ui->encodingCombo->addItem(tr("ASCII"),            "ascii");
    ui->encodingCombo->addItem(tr("КОИ-7 Н0/Н1"),      "koi7_n0_n1");
    ui->encodingCombo->addItem(tr("КОИ-7 Н2"),         "koi7_n2");
    ui->encodingCombo->addItem(tr("КОИ8-R"),           "koi8_r");
    ui->encodingCombo->addItem(tr("КОИ8-M"),           "koi8_m");
    ui->encodingCombo->addItem(tr("CP866 (OEM)"),      "cp866");
    ui->encodingCombo->addItem(tr("CP1251 (Windows)"), "cp1251");
    ui->encodingCombo->addItem(tr("ISO 8859-5"),       "iso8859_5");
    int idx = 0;
    if (m_settings) idx = m_settings->value("explorer/encoding", 0).toInt();
    if (idx < 0 || idx >= ui->encodingCombo->count()) idx = 0;
    ui->encodingCombo->setCurrentIndex(idx);
    adjustComboBoxWidth(ui->encodingCombo);
    ui->encodingCombo->blockSignals(false);
}

void ExplorerDialog::updateGeometryLabel()
{
    if (!m_model || m_model->cylinders() == 0) {
        ui->geometryLabel->setText("");
        return;
    }
    // Sector size is taken from the first track (the structure has no global value);
    // sector count is the widest track, since tracks may differ.
    const unsigned sector_size = (m_disk_struct && !m_disk_struct->tracks.empty())
                                     ? m_disk_struct->tracks.front().sector_size : 0;
    QString txt = tr("%1 tracks × %2 sectors × %3 bytes")
                      .arg(m_model->cylinders())
                      .arg(m_model->maxSectors())
                      .arg(sector_size);
    if (m_model->heads() > 1) {
        txt += tr(", %1 heads").arg(m_model->heads());
    }
    ui->geometryLabel->setText(txt);
}

void ExplorerDialog::on_closeBtn_clicked()
{
    accept();
}

void ExplorerDialog::on_infoBtn_clicked()
{
    if (!m_image) return;
    // The loader's image-level info text — the same description the main window
    // used to show on F3 before this explorer replaced it. Placeholders are
    // expanded inside infoDialog().
    FileOperations::infoDialog(this, QString::fromStdString(m_image->file_info()));
}

void ExplorerDialog::on_fsInfoBtn_clicked()
{
    if (!m_filesystem) return;
    // Same content the main window's "Filesystem Info..." menu item (Ctrl+Alt+F3)
    // shows — placeholders are expanded inside infoDialog().
    FileOperations::infoDialog(this, QString::fromStdString(m_filesystem->information()));
}

void ExplorerDialog::on_orderCombo_currentIndexChanged(int index)
{
    if (!m_model) return;
    if (m_settings) m_settings->setValue("explorer/order", index);
    m_model->setOrder(ui->orderCombo->currentData().toInt() == 1
                          ? SectorTableModel::Order::Logical
                          : SectorTableModel::Order::Physical);
    // Refresh the hex view in case the underlying sector changed
    updateHexView(ui->sectorTable->currentIndex());
}

void ExplorerDialog::on_encodingCombo_currentIndexChanged(int index)
{
    if (m_settings) m_settings->setValue("explorer/encoding", index);
    updateHexView(ui->sectorTable->currentIndex());
}

void ExplorerDialog::on_splitByHeadsCheck_toggled(bool checked)
{
    if (!m_model) return;
    if (m_settings) m_settings->setValue("explorer/split_by_heads", checked);
    m_model->setSplitByHeads(checked);
    applyColumnLayout();
    updateHexView(ui->sectorTable->currentIndex());
}

void ExplorerDialog::on_hexCheck_toggled(bool checked)
{
    if (!m_model) return;
    if (m_settings) m_settings->setValue("explorer/hex_numbers", checked);
    m_model->setHexNumbers(checked);
    // Refresh the sector info label so it switches between decimal and hex too.
    updateHexView(ui->sectorTable->currentIndex());
}

void ExplorerDialog::applyColumnLayout()
{
    if (!m_model) return;
    auto *header = ui->sectorTable->horizontalHeader();
    const int total = m_model->columnCount();
    for (int c = 0; c < total; ++c) {
        const int w = m_model->isSpacerColumn(c) ? kSpacerColumnSize : kDefaultSectionSize;
        header->resizeSection(c, w);
    }
}

void ExplorerDialog::onSectorCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    updateHexView(current);
}

void ExplorerDialog::updateHexView(const QModelIndex &index)
{
    if (!m_model || !index.isValid()) {
        ui->hexEdit->clear();
        ui->sectorInfoLabel->setText("");
        return;
    }

    // The sector and its bytes both come from the structure, honouring the
    // current physical/logical order (resolved inside sectorAt → locate).
    unsigned head, cylinder, sector_id, sector_size;
    const dsk_tools::StructSector *sector =
        m_model->sectorAt(index, head, cylinder, sector_id, sector_size);
    if (!sector) {
        ui->hexEdit->clear();
        ui->sectorInfoLabel->setText("");
        return;
    }

    // Sector info label — logical track number = cylinder * heads + head, counting both sides.
    const unsigned heads = m_model->heads() ? m_model->heads() : 1;
    const unsigned track = cylinder * heads + head;
    QString info = tr("Track %1, Sector %2")
                       .arg(m_model->formatNumber(track), m_model->formatNumber(sector_id));
    info += tr(" — %1 bytes").arg(sector_size);
    ui->sectorInfoLabel->setText(info);

    const uint8_t *data = sector->data.data();
    // The structure always allocates sector_size bytes; clamp the dump to what is there.
    if (sector->data.size() < sector_size) sector_size = static_cast<unsigned>(sector->data.size());
    if (!data || sector_size == 0) {
        ui->hexEdit->setPlainText(tr("<no data>"));
        return;
    }

    // Resolve charmap once for this render
    const QString enc_id = ui->encodingCombo->currentData().toString();
    const dsk_tools::CharmapInfo cm = dsk_tools::init_charmap(enc_id.toStdString());

    // Build dump: 16 bytes per row,   "OOOO | hh hh hh hh hh hh hh hh  hh hh hh hh hh hh hh hh | ASCII"
    QString out;
    out.reserve(sector_size * 6);
    constexpr unsigned BPL = 16;
    for (unsigned i = 0; i < sector_size; i += BPL) {
        QString hex;
        hex.reserve(BPL * 3 + 1);
        QString ascii;
        ascii.reserve(BPL);

        for (unsigned j = 0; j < BPL; ++j) {
            if (i + j < sector_size) {
                const uint8_t b = data[i + j];
                hex += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
                if (cm.charmap) {
                    ascii += QString::fromStdString((*cm.charmap)[b]);
                } else {
                    // ASCII fallback when encoding is unknown
                    ascii += (b >= 0x20 && b < 0x7F) ? QChar(b) : QChar('.');
                }
            } else {
                hex += "   ";
                ascii += ' ';
            }
            if (j == 7) hex += ' ';
        }

        const QString offset = QString("%1").arg(i, 4, 16, QChar('0')).toUpper();
        out += offset + " | " + hex + "| " + ascii + '\n';
    }

    ui->hexEdit->setPlainText(out);
}