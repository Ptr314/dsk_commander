// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Low-level FDD image explorer dialog

#include "explorerdialog.h"
#include "ui_explorerdialog.h"

#include "mainutils.h"

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

void SectorTableModel::setImage(dsk_tools::diskImage *image)
{
    beginResetModel();
    m_image = image;
    if (m_image) {
        m_heads   = m_image->get_heads();
        m_tracks  = m_image->get_tracks();
        m_sectors = m_image->get_sectors();
    } else {
        m_heads = m_tracks = m_sectors = 0;
    }
    endResetModel();
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
    if (m_heads <= 1 || m_sectors == 0) return false;
    if (column < 0) return false;
    const int stride = static_cast<int>(m_sectors) + 1;     // sectors + one spacer
    return (column % stride) == static_cast<int>(m_sectors);
}

bool SectorTableModel::resolve(const QModelIndex &index, unsigned &head, unsigned &track, unsigned &sector) const
{
    if (!m_image || !index.isValid()) return false;
    if (m_heads == 0 || m_tracks == 0 || m_sectors == 0) return false;

    const unsigned row = static_cast<unsigned>(index.row());
    const unsigned col = static_cast<unsigned>(index.column());

    if (m_split_by_heads) {
        if (row >= m_tracks * m_heads) return false;
        if (col >= m_sectors) return false;
        track  = row / m_heads;
        head   = row % m_heads;
        sector = col;
    } else {
        if (row >= m_tracks) return false;
        if (m_heads > 1) {
            const unsigned stride = m_sectors + 1;          // sectors + one spacer
            const unsigned group  = col / stride;
            const unsigned off    = col % stride;
            if (group >= m_heads || off >= m_sectors) return false;   // out of range / spacer
            track  = row;
            head   = group;
            sector = off;
        } else {
            if (col >= m_sectors) return false;
            track  = row;
            head   = 0;
            sector = col;
        }
    }
    return true;
}

int SectorTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    const unsigned heads = m_heads ? m_heads : 1;
    return static_cast<int>(m_split_by_heads ? m_tracks * heads : m_tracks);
}

int SectorTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    const unsigned heads = m_heads ? m_heads : 1;
    if (m_split_by_heads || heads <= 1) {
        return static_cast<int>(m_split_by_heads ? m_sectors : m_sectors * heads);
    }
    // sectors per head × heads + (heads - 1) spacers between groups
    return static_cast<int>(m_sectors * heads + (heads - 1));
}

QVariant SectorTableModel::data(const QModelIndex &index, int role) const
{
    if (!m_image || !index.isValid()) return {};

    unsigned head, track, sector;
    if (!resolve(index, head, track, sector)) return {};

    switch (role) {
        case StateRole:
            return QVariant::fromValue(static_cast<int>(sectorTypeAt(head, track, sector)));
        case TrackRole:  return track;
        case HeadRole:   return head;
        case SectorRole: return sector;
        case Qt::ToolTipRole: {
            QString tip = ExplorerDialog::tr("Track %1, Sector %2").arg(track).arg(sector);
            if (m_heads > 1) tip += ExplorerDialog::tr(", Head %1").arg(head);

            QString type_str;
            switch (sectorTypeAt(head, track, sector)) {
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

dsk_tools::SectorType SectorTableModel::sectorTypeAt(unsigned head, unsigned track, unsigned sector) const
{
    if (!m_image) return dsk_tools::SectorType::Empty;
    // Logical mode: image API handles translation + bounds + bad-sector lookup.
    // Physical mode: we still ask the image, knowing the bad-sector hit may be
    // approximate when a translation table is present (acceptable for v1).
    if (m_image->is_bad_sector(head, track, sector)) {
        return dsk_tools::SectorType::Bad;
    }
    // If a filesystem provided a sector-type map, defer to it:
    // - found entry → that type
    // - no entry    → Empty
    // If no map was supplied (filesystem null or returned an empty map),
    // fall back to the optimistic Ok default.
    if (!m_type_map.empty()) {
        const auto it = m_type_map.find({head, track, sector});
        return (it != m_type_map.end()) ? it->second : dsk_tools::SectorType::Empty;
    }
    return dsk_tools::SectorType::Ok;
}

QVariant SectorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole) return {};

    if (orientation == Qt::Vertical) {
        if (m_split_by_heads && m_heads > 0) {
            const unsigned track = static_cast<unsigned>(section) / m_heads;
            const unsigned head  = static_cast<unsigned>(section) % m_heads;
            return QString("%1:%2").arg(track).arg(head);
        }
        return QString::number(section);
    }

    // Horizontal: spacer columns have no label; data columns show the sector number.
    if (m_sectors == 0) return {};
    if (isSpacerColumn(section)) return QString();
    if (m_split_by_heads || m_heads <= 1) {
        return QString::number(static_cast<unsigned>(section) % m_sectors);
    }
    const unsigned stride = m_sectors + 1;
    return QString::number(static_cast<unsigned>(section) % stride);
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
    const dsk_tools::SectorType state = stateVar.isValid()
        ? static_cast<dsk_tools::SectorType>(stateVar.toInt())
        : dsk_tools::SectorType::Empty;

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
                               std::unique_ptr<dsk_tools::fileSystem> filesystem)
    : QDialog(parent)
    , ui(new Ui::ExplorerDialog)
    , m_settings(settings)
    , m_file_name(file_name)
    , m_image(std::move(image))
    , m_filesystem(std::move(filesystem))
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
    ui->sectorTable->setShowGrid(false);
    ui->sectorTable->setFocusPolicy(Qt::StrongFocus);

    const bool split = m_settings ? m_settings->value("explorer/split_by_heads", false).toBool() : false;
    ui->splitByHeadsCheck->blockSignals(true);
    ui->splitByHeadsCheck->setChecked(split);
    ui->splitByHeadsCheck->blockSignals(false);

    m_model->setImage(m_image.get());
    m_model->setSplitByHeads(split);
    m_model->setOrder(ui->orderCombo->currentData().toInt() == 1
                          ? SectorTableModel::Order::Logical
                          : SectorTableModel::Order::Physical);
    if (m_filesystem) {
        m_model->setSectorTypeMap(m_filesystem->get_sector_type_map());
    }
    applyColumnLayout();

    ui->hexEdit->setFont(getMonospaceFont(10));

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
    if (!m_image) {
        ui->geometryLabel->setText("");
        return;
    }
    QString txt = tr("%1 tracks × %2 sectors × %3 bytes")
                      .arg(m_image->get_tracks())
                      .arg(m_image->get_sectors())
                      .arg(m_image->get_sector_size());
    if (m_image->get_heads() > 1) {
        txt += tr(", %1 heads").arg(m_image->get_heads());
    }
    ui->geometryLabel->setText(txt);
}

void ExplorerDialog::on_closeBtn_clicked()
{
    accept();
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
    if (!m_image || !m_model || !index.isValid()) {
        ui->hexEdit->clear();
        ui->sectorInfoLabel->setText("");
        return;
    }

    unsigned head, track, sector;
    if (!m_model->resolve(index, head, track, sector)) {
        ui->hexEdit->clear();
        ui->sectorInfoLabel->setText("");
        return;
    }

    const auto &fmt   = m_image->get_format();
    const unsigned sector_size = fmt.sector_size;

    const uint8_t *data = nullptr;
    if (m_model->order() == SectorTableModel::Order::Logical) {
        data = m_image->get_sector_data(head, track, sector);
    } else {
        // Physical: bypass sector translation by computing the raw offset ourselves.
        unsigned track_index = track * fmt.heads + head;
        if (fmt.heads == 2 && !fmt.sides_interleaved) {
            track_index = dsk_tools::diskImage::transform_index(track_index, fmt.heads * fmt.tracks - 1);
        }
        const unsigned sector_index = track_index * fmt.sectors + sector;
        const unsigned offset = sector_index * sector_size;
        const dsk_tools::BYTES *buf = m_image->get_buffer();
        if (buf && offset + sector_size <= buf->size()) {
            data = buf->data() + offset;
        }
    }

    // Sector info label
    QString info = tr("Track %1, Sector %2").arg(track).arg(sector);
    if (m_image->get_heads() > 1) info += tr(", Head %1").arg(head);
    info += tr(" — %1 bytes").arg(sector_size);
    ui->sectorInfoLabel->setText(info);

    if (!data) {
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