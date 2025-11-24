// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Custom table view for file panel

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QHeaderView>
#include <QApplication>
#include <QItemSelectionModel>
#include <QDebug>
#include <QMessageBox>
#include <QStandardItemModel>

#include "FileTable.h"
#include "definitions.h"

// ============================================================================
// CurrentRowDelegate implementation
// ============================================================================

CurrentRowDelegate::CurrentRowDelegate(QTableView* view, QObject* parent)
    : QStyledItemDelegate(parent), m_tableView(view) {
}

void CurrentRowDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    // Get the current index from the table view
    QModelIndex currentIndex = m_tableView->currentIndex();

    // Check if this cell is in the current row
    bool isCurrentRow = currentIndex.isValid() &&
                        index.isValid() &&
                        (index.row() == currentIndex.row()) &&
                        (index.parent() == currentIndex.parent());

    // Check if this row is selected
    bool isSelected = m_tableView->selectionModel() &&
                      m_tableView->selectionModel()->isRowSelected(index.row(), index.parent());


    // Create a modified copy of the style option
    QStyleOptionViewItem opt = option;

    // Always remove the focus frame
    opt.state &= ~(QStyle::State_HasFocus | QStyle::State_Selected);

    if (isCurrentRow || isSelected) {
        painter->save();

        // Check if the table is active
        FileTable* fileTable = qobject_cast<FileTable*>(m_tableView);
        bool isTableActive = fileTable && fileTable->isActive();

        // Background - only show blue highlight for current row if table is active
        if (isCurrentRow && isTableActive) {
            painter->fillRect(option.rect, QColor(204, 232, 255));
        } else {
            // Restore alternating row colors for even/odd rows
            QColor bgColor;
            if (opt.features & QStyleOptionViewItem::Alternate) {
                bgColor = option.palette.color(QPalette::AlternateBase);
            } else {
                bgColor = option.palette.color(QPalette::Base);
            }
            painter->fillRect(option.rect, bgColor);
        }

        const QString text = index.data(Qt::DisplayRole).toString();

        // Text - selected items always show in red, regardless of active state
        if (isSelected)
            painter->setPen(QColor(255, 0, 0, 255));
        else
            painter->setPen(option.palette.color(QPalette::Text));

        painter->setFont(option.font);

        // Calculate text rectangle with padding
        QRect textRect = option.rect.adjusted(4, 0, -4, 0);

        // Check for and render icon decoration
        QVariant decoration = index.data(Qt::DecorationRole);
        if (decoration.isValid()) {
            QIcon icon = qvariant_cast<QIcon>(decoration);
            if (!icon.isNull()) {
                // Icon size: 16x16 pixels
                QSize iconSize(16, 16);
                // Center icon vertically in the cell
                int iconY = textRect.top() + (textRect.height() - iconSize.height()) / 2;
                QRect iconRect(textRect.left(), iconY, iconSize.width(), iconSize.height());

                // Paint the icon
                icon.paint(painter, iconRect);

                // Adjust text rectangle to start after the icon with 4px spacing
                textRect.setLeft(iconRect.right() + 4);
            }
        }

        // Draw the text with alignment from the model
        Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter;
        QVariant alignData = index.data(Qt::TextAlignmentRole);
        if (alignData.isValid()) {
            alignment = Qt::Alignment(alignData.toInt());
        }

        painter->drawText(textRect, alignment, text);

        painter->restore();
    } else {
        // For all other cases, use the standard delegate painting
        QStyledItemDelegate::paint(painter, opt, index);
    }
}

// ============================================================================
// FileTable implementation
// ============================================================================

FileTable::FileTable(QWidget* parent)
    : QTableView(parent)
    , m_clickTimer(nullptr)
    , m_delegate(nullptr)
{
    // Create timer for single-click detection
    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    connect(m_clickTimer, &QTimer::timeout, this, [this]() {
        // Timer fired - this was a single click, not a double click
        // Just move focus to the pending index without changing selection
        if (m_pendingClickIndex.isValid() && selectionModel()) {
            // Always use column 0 for full-row highlighting
            QModelIndex rowIndex = m_pendingClickIndex.sibling(m_pendingClickIndex.row(), 0);
            selectionModel()->setCurrentIndex(
                rowIndex,
                QItemSelectionModel::NoUpdate
            );
        }
    });

    // Disable the default focus frame completely
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(
        // "QTableView { outline: none; }"
        // "QTableView::item:focus { outline: none; border: none; }"
        "QTableView::item:selected { background: transparent; }"

    );

    // Install custom delegate for full-row cursor highlighting
    m_delegate = new CurrentRowDelegate(this, this);
    setItemDelegate(m_delegate);

    // Install event filters for this view and viewport
    installEventFilter(this);
    viewport()->installEventFilter(this);

    // Disable tab/backtab navigation between cells
    setTabKeyNavigation(false);

    // Force viewport update to trigger delegate painting
    viewport()->update();

    // Connect to current index changes to repaint affected rows
    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &FileTable::onCurrentIndexChanged);
}

void FileTable::setupForHostMode() {
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setAlternatingRowColors(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    verticalHeader()->hide();

    setTabKeyNavigation(false);

    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);

    QHeaderView* hh = horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::Stretch); // Name - expanded

    verticalHeader()->setDefaultSectionSize(24);

    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &FileTable::onCurrentIndexChanged);
}

void FileTable::setupForImageMode(dsk_tools::FSCaps capabilities) {
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    int const_columns = 2;
    int columns = 0;
    auto image_model = dynamic_cast<QStandardItemModel*>(model());

    image_model->clear();

    if (dsk_tools::hasFlag(capabilities, dsk_tools::FSCaps::Protect)) {
        image_model->setColumnCount(const_columns + columns + 1);
        image_model->setHeaderData(columns, Qt::Horizontal, FileTable::tr("P"));
        image_model->horizontalHeaderItem(columns)->setToolTip(FileTable::tr("Protection"));
        setColumnWidth(columns, 20);
        columns++;
    }
    if (dsk_tools::hasFlag(capabilities, dsk_tools::FSCaps::Types)) {
        image_model->setColumnCount(const_columns + columns + 1);
        image_model->setHeaderData(columns, Qt::Horizontal, FileTable::tr("T"));
        image_model->horizontalHeaderItem(columns)->setToolTip(FileTable::tr("Type"));
        setColumnWidth(columns, 30);
        columns++;
    }

    setColumnWidth(columns, 60);
    image_model->setHeaderData(columns, Qt::Horizontal, FileTable::tr("Size"));
    image_model->horizontalHeaderItem(columns++)->setToolTip(FileTable::tr("Size in bytes"));

    setColumnWidth(columns, 230);
    image_model->setHeaderData(columns, Qt::Horizontal, FileTable::tr("Name"));
    image_model->horizontalHeaderItem(columns++)->setToolTip(FileTable::tr("Name of the file"));

    verticalHeader()->setDefaultSectionSize(8);
    horizontalHeader()->setMinimumSectionSize(20);

    horizontalHeader()->setStretchLastSection(true);

    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &FileTable::onCurrentIndexChanged);
}

bool FileTable::eventFilter(QObject* obj, QEvent* ev) {
    if ((ev->type() == QEvent::FocusIn || ev->type() == QEvent::MouseButtonPress))
        emit focusReceived();

    // Handle double-click events - cancel timer since this is a double-click
    if (ev->type() == QEvent::MouseButtonDblClick) {
        if (obj == viewport()) {
            handleMouseDoubleClick();
        }
        // Let Qt handle the double-click normally
        return QTableView::eventFilter(obj, ev);
    }

    // Handle mouse clicks in table view
    if (ev->type() == QEvent::MouseButtonPress) {
        if (obj == viewport()) {
            handleMousePress(static_cast<QMouseEvent*>(ev));
            return true;  // Block Qt's default handler to prevent selection changes
        }
    }

    // Block mouse drag selection - prevent selection changes when dragging with left button
    if (ev->type() == QEvent::MouseMove) {
        if (obj == viewport()) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(ev);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                // Left button is pressed during move - block to prevent drag-select
                return true;
            }
        }
    }

    // Handle keyboard navigation and selection in table view
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);

        if (obj == this || obj == viewport()) {
            // Handle arrow keys
            if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
                if (handleArrowKeys(keyEvent)) return true;
            }

            // Handle Insert key
            if (keyEvent->key() == Qt::Key_Insert) {
                QModelIndex current = currentIndex();
                if (current.isValid()) {
                    // Toggle selection of current row
                    selectionModel()->select(
                        current,
                        QItemSelectionModel::Toggle | QItemSelectionModel::Rows
                    );

                    // Move down one line WITHOUT changing selection
                    int nextRow = current.row() + 1;
                    int maxRow = model()->rowCount(rootIndex()) - 1;
                    if (nextRow <= maxRow) {
                        // Always use column 0 for full-row highlighting
                        QModelIndex nextIndex = model()->index(nextRow, 0, rootIndex());
                        selectionModel()->setCurrentIndex(
                            nextIndex,
                            QItemSelectionModel::NoUpdate
                        );
                        scrollTo(nextIndex);
                    }
                }
                return true; // Event handled
            }

            // Handle Home key
            if (keyEvent->key() == Qt::Key_Home) {
                QModelIndex current = currentIndex();
                if (current.isValid()) {
                    // Move to first row WITHOUT changing selection (column 0 for full-row highlighting)
                    QModelIndex firstIndex = model()->index(0, 0, rootIndex());
                    if (firstIndex.isValid()) {
                        selectionModel()->setCurrentIndex(
                            firstIndex,
                            QItemSelectionModel::NoUpdate
                        );
                        scrollTo(firstIndex);
                    }
                }
                return true; // Event handled
            }

            // Handle End key
            if (keyEvent->key() == Qt::Key_End) {
                QModelIndex current = currentIndex();
                if (current.isValid()) {
                    // Move to last row WITHOUT changing selection (column 0 for full-row highlighting)
                    int lastRow = model()->rowCount(rootIndex()) - 1;
                    if (lastRow >= 0) {
                        QModelIndex lastIndex = model()->index(lastRow, 0, rootIndex());
                        if (lastIndex.isValid()) {
                            selectionModel()->setCurrentIndex(
                                lastIndex,
                                QItemSelectionModel::NoUpdate
                            );
                            scrollTo(lastIndex);
                        }
                    }
                }
                return true; // Event handled
            }

            // Handle PageUp key
            if (keyEvent->key() == Qt::Key_PageUp) {
                QModelIndex current = currentIndex();
                if (current.isValid()) {
                    // Calculate how many rows fit in viewport
                    int rowHeight = this->rowHeight(0);
                    if (rowHeight <= 0) rowHeight = 24; // Default if not set
                    int viewportHeight = viewport()->height();
                    int pageSize = viewportHeight / rowHeight;
                    if (pageSize < 1) pageSize = 1;

                    // Move up by one page
                    int newRow = current.row() - pageSize;
                    if (newRow < 0) newRow = 0;

                    // Always use column 0 for full-row highlighting
                    QModelIndex newIndex = model()->index(newRow, 0, rootIndex());
                    if (newIndex.isValid()) {
                        selectionModel()->setCurrentIndex(
                            newIndex,
                            QItemSelectionModel::NoUpdate
                        );
                        scrollTo(newIndex);
                    }
                }
                return true; // Event handled
            }

            // Handle PageDown key
            if (keyEvent->key() == Qt::Key_PageDown) {
                QModelIndex current = currentIndex();
                if (current.isValid()) {
                    // Calculate how many rows fit in viewport
                    int rowHeight = this->rowHeight(0);
                    if (rowHeight <= 0) rowHeight = 24; // Default if not set
                    int viewportHeight = viewport()->height();
                    int pageSize = viewportHeight / rowHeight;
                    if (pageSize < 1) pageSize = 1;

                    // Move down by one page
                    int maxRow = model()->rowCount(rootIndex()) - 1;
                    int newRow = current.row() + pageSize;
                    if (newRow > maxRow) newRow = maxRow;

                    // Always use column 0 for full-row highlighting
                    QModelIndex newIndex = model()->index(newRow, 0, rootIndex());
                    if (newIndex.isValid()) {
                        selectionModel()->setCurrentIndex(
                            newIndex,
                            QItemSelectionModel::NoUpdate
                        );
                        scrollTo(newIndex);
                    }
                }
                return true; // Event handled
            }

            // Handle selection shortcuts: +, -, *
            if (handleSelectionKeys(keyEvent)) return true;

            // Handle Tab key - switch to the other panel
            if (keyEvent->key() == Qt::Key_Tab) {
                emit switchPanelRequested();
                return true; // Event handled
            }

            // Handle Enter/Return key - emit doubleClicked signal for current row
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                QModelIndex current = currentIndex();
                if (current.isValid()) {
                    // Emit doubleClicked signal, same as mouse double-click
                    emit doubleClicked(current);
                }
                return true; // Event handled
            }

            // Block Left and Right arrow keys - they have no effect in row-based navigation
            if (keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right) {
                return true; // Block the event, do nothing
            }
        }
    }

    return QTableView::eventFilter(obj, ev);
}

void FileTable::handleMousePress(QMouseEvent* mouseEvent) {
    QModelIndex clickedIndex = indexAt(mouseEvent->pos());

    if (clickedIndex.isValid()) {
        if (mouseEvent->button() == Qt::LeftButton) {
            // Store the clicked index for potential double-click
            m_pendingClickIndex = clickedIndex;

            // Immediately move current index to clicked row without changing selection
            // This provides instant visual feedback (no delay)
            QModelIndex rowIndex = clickedIndex.sibling(clickedIndex.row(), 0);
            selectionModel()->setCurrentIndex(
                rowIndex,
                QItemSelectionModel::NoUpdate
            );

        } else if (mouseEvent->button() == Qt::RightButton) {
            // Right click: toggle selection of the clicked row
            selectionModel()->select(
                clickedIndex,
                QItemSelectionModel::Toggle | QItemSelectionModel::Rows
            );
            // Also move current index to right-clicked item (column 0 for full-row highlighting)
            QModelIndex rowIndex = clickedIndex.sibling(clickedIndex.row(), 0);
            selectionModel()->setCurrentIndex(
                rowIndex,
                QItemSelectionModel::NoUpdate
            );
        }
    }
}

void FileTable::handleMouseDoubleClick() {
    // Manually emit doubleClicked signal since we blocked the mouse press events
    // Qt's normal double-click handling can't work when we block press events
    if (m_pendingClickIndex.isValid()) {
        emit doubleClicked(m_pendingClickIndex);
    }
}

bool FileTable::handleArrowKeys(QKeyEvent* keyEvent) {
    if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
        QModelIndex current = currentIndex();
        if (current.isValid()) {
            int nextRow;
            if (keyEvent->key() == Qt::Key_Up) {
                nextRow = current.row() - 1;
                if (nextRow < 0) return true; // At top, block event
            } else { // Down
                nextRow = current.row() + 1;
                int maxRow = model()->rowCount(rootIndex()) - 1;
                if (nextRow > maxRow) return true; // At bottom, block event
            }

            // Always use column 0 for full-row highlighting
            QModelIndex nextIndex = model()->index(nextRow, 0, rootIndex());

            // Move current index WITHOUT changing selection
            selectionModel()->setCurrentIndex(
                nextIndex,
                QItemSelectionModel::NoUpdate  // Don't modify selection!
            );

            // Ensure visibility
            scrollTo(nextIndex);

            return true; // Event handled, block default behavior
        }
    }
    return false;
}

bool FileTable::handleSelectionKeys(QKeyEvent* keyEvent) {
    QString keyText = keyEvent->text();
    if (!keyText.isEmpty()) {
        QChar keyChar = keyText[0];

        // Plus key - select all
        if (keyChar == '+' || keyEvent->key() == Qt::Key_Plus) {
            if (selectionModel()) {
                selectionModel()->select(
                    QItemSelection(
                        model()->index(0, 0, rootIndex()),
                        model()->index(
                            model()->rowCount(rootIndex()) - 1,
                            model()->columnCount(rootIndex()) - 1,
                            rootIndex()
                        )
                    ),
                    QItemSelectionModel::Select | QItemSelectionModel::Rows
                );
            }
            return true;
        }

        // Minus key - clear selection
        if (keyChar == '-' || keyEvent->key() == Qt::Key_Minus) {
            if (selectionModel()) {
                selectionModel()->clearSelection();
            }
            return true;
        }

        // Asterisk key - invert selection
        if (keyChar == '*' || keyEvent->key() == Qt::Key_Asterisk) {
            if (selectionModel()) {
                int rowCount = model()->rowCount(rootIndex());
                for (int row = 0; row < rowCount; ++row) {
                    QModelIndex idx = model()->index(row, 0, rootIndex());
                    selectionModel()->select(
                        idx,
                        QItemSelectionModel::Toggle | QItemSelectionModel::Rows
                    );
                }
            }
            return true;
        }
    }
    return false;
}

void FileTable::setActive(bool active) {
    m_isActive = active;
    viewport()->update();  // Repaint to show/hide current row bar
}

void FileTable::onCurrentIndexChanged(const QModelIndex& current, const QModelIndex& previous)
{
        qDebug() << "FileTable::onCurrentIndexChanged()";
    // When the current row changes, we need to repaint both the previous and new current rows
    // to ensure the delegate updates the highlight correctly across all columns

    // Repaint the entire previous current row
    if (previous.isValid() && model()) {
        QRect previousRowRect = visualRect(model()->index(previous.row(), 0, previous.parent()));
        int lastCol = model()->columnCount(previous.parent()) - 1;
        if (lastCol >= 0) {
            QRect lastCellRect = visualRect(model()->index(previous.row(), lastCol, previous.parent()));
            previousRowRect = previousRowRect.united(lastCellRect);
            viewport()->update(previousRowRect);
        }
    }

    // Repaint the entire new current row
    if (current.isValid() && model()) {
        QRect currentRowRect = visualRect(model()->index(current.row(), 0, current.parent()));
        qDebug() << "currentRowRect: " << currentRowRect;
        int lastCol = model()->columnCount(current.parent()) - 1;
        if (lastCol >= 0) {
            QRect lastCellRect = visualRect(model()->index(current.row(), lastCol, current.parent()));
            currentRowRect = currentRowRect.united(lastCellRect);
            viewport()->update(currentRowRect);
        }
    }
}
