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
// Debug logging configuration
// ============================================================================
#define FILETABLE_DEBUG_LOGGING 0

#if FILETABLE_DEBUG_LOGGING
    #define LOG_EVENT(msg) qDebug() << "[FileTable] EVENT:" << msg
    #define LOG_STATE(msg) qDebug() << "[FileTable] STATE:" << msg
    #define LOG_ACTION(msg) qDebug() << "[FileTable] ACTION:" << msg
    #define LOG_SIGNAL(msg) qDebug() << "[FileTable] SIGNAL:" << msg
#else
    #define LOG_EVENT(msg)
    #define LOG_STATE(msg)
    #define LOG_ACTION(msg)
    #define LOG_SIGNAL(msg)
#endif

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

        // Use custom font if set on the item, otherwise use default font from style option
        QVariant fontData = index.data(Qt::FontRole);
        if (fontData.isValid()) {
            painter->setFont(qvariant_cast<QFont>(fontData));
        } else {
            painter->setFont(option.font);
        }

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
    hh->setSectionsClickable(false);  // Disable click-to-sort on headers
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
    horizontalHeader()->setSectionsClickable(false);  // Disable click-to-sort on headers

    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, &FileTable::onCurrentIndexChanged);
}

bool FileTable::eventFilter(QObject* obj, QEvent* ev) {
    // Log Focus events
    if (ev->type() == QEvent::FocusIn) {
        LOG_EVENT("FocusIn");
        logSelectionState("FocusIn");
        emit focusReceived();
    }

    // Handle double-click events
    if (ev->type() == QEvent::MouseButtonDblClick) {
        if (obj == viewport()) {
            LOG_EVENT("MouseButtonDblClick");
            logSelectionState("Before handleMouseDoubleClick");
            handleMouseDoubleClick();
            logSelectionState("After handleMouseDoubleClick");
            return true;  // Block event - we've handled it completely, don't let Qt's default handler select the row
        }
    }

    // Handle mouse clicks in table view
    if (ev->type() == QEvent::MouseButtonPress) {
        if (obj == viewport()) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(ev);
            QString buttonStr = mouseEvent->button() == Qt::LeftButton ? "Left" :
                               mouseEvent->button() == Qt::RightButton ? "Right" : "Other";
            QModelIndex idx = indexAt(mouseEvent->pos());
            int row = idx.isValid() ? idx.row() : -1;
            LOG_EVENT(QString("MouseButtonPress | Button=%1 | Row=%2 | Pos=(%3,%4)")
                .arg(buttonStr).arg(row).arg(mouseEvent->pos().x()).arg(mouseEvent->pos().y()));
            logSelectionState("Before handleMousePress");
            handleMousePress(mouseEvent);
            logSelectionState("After handleMousePress");
            emit focusReceived();
            return true;  // Block Qt's default handler to prevent selection changes
        }
    }

    // Block mouse drag selection - prevent selection changes when dragging with left button
    if (ev->type() == QEvent::MouseMove) {
        if (obj == viewport()) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(ev);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                // Left button is pressed during move - block to prevent drag-select
                LOG_ACTION("MouseMove with LeftButton held - blocking drag select");
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
                QString direction = keyEvent->key() == Qt::Key_Up ? "Up" : "Down";
                LOG_EVENT(QString("KeyPress | Key=%1").arg(direction));
                logSelectionState(QString("Before Arrow-%1").arg(direction));
                if (handleArrowKeys(keyEvent)) {
                    logSelectionState(QString("After Arrow-%1").arg(direction));
                    return true;
                }
            }

            // Handle Insert key
            if (keyEvent->key() == Qt::Key_Insert) {
                LOG_EVENT("KeyPress | Key=Insert");
                logSelectionState("Before Insert");
                QModelIndex current = currentIndex();
                if (current.isValid()) {
                    // Toggle selection of current row
                    selectionModel()->select(
                        current,
                        QItemSelectionModel::Toggle | QItemSelectionModel::Rows
                    );
                    logSelectionState("After Toggle selection");

                    // Move down one line WITHOUT changing selection
                    int nextRow = current.row() + 1;
                    int maxRow = model()->rowCount(rootIndex()) - 1;
                    if (nextRow <= maxRow) {
                        // Always use column 0 for full-row highlighting
                        QModelIndex nextIndex = model()->index(nextRow, 0, rootIndex());
                        LOG_ACTION(QString("Moving from row %1 to %2").arg(current.row()).arg(nextRow));
                        selectionModel()->setCurrentIndex(
                            nextIndex,
                            QItemSelectionModel::NoUpdate
                        );
                        scrollTo(nextIndex);
                    } else {
                        LOG_ACTION("At last row - not moving down");
                    }
                }
                logSelectionState("After Insert");
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

            // Handle Backspace key - navigate to parent directory
            if (keyEvent->key() == Qt::Key_Backspace) {
                emit goUpRequested();
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
            LOG_ACTION(QString("Left-click on row %1 - setting current index (NoUpdate)").arg(clickedIndex.row()));

            // Start timer to detect if this is a single or double click
            // Timer will fire if no second click occurs within the system's double-click interval
            m_clickTimer->start(QApplication::doubleClickInterval());

            // Immediately move current index to clicked row without changing selection
            // This provides instant visual feedback (no delay)
            QModelIndex rowIndex = clickedIndex.sibling(clickedIndex.row(), 0);
            selectionModel()->setCurrentIndex(
                rowIndex,
                QItemSelectionModel::NoUpdate
            );

        } else if (mouseEvent->button() == Qt::RightButton) {
            LOG_ACTION(QString("Right-click on row %1 - toggling selection").arg(clickedIndex.row()));
            // Right click: toggle selection of the clicked row
            selectionModel()->select(
                clickedIndex,
                QItemSelectionModel::Toggle | QItemSelectionModel::Rows
            );
            logSelectionState(QString("After toggle selection on row %1").arg(clickedIndex.row()));
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
    // Stop the timer to prevent both single and double-click handlers from firing
    m_clickTimer->stop();

    // Manually emit doubleClicked signal since we blocked the mouse press events
    // Qt's normal double-click handling can't work when we block press events
    if (m_pendingClickIndex.isValid()) {
        emit doubleClicked(m_pendingClickIndex);
    }
}

bool FileTable::handleArrowKeys(QKeyEvent* keyEvent) {
    if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down) {
        QModelIndex current = currentIndex();

        // Handle case where there's no current index (invalid)
        if (!current.isValid()) {
            // If there's at least one row, set current index to first row
            if (model()->rowCount(rootIndex()) > 0) {
                QModelIndex firstIndex = model()->index(0, 0, rootIndex());
                LOG_ACTION("No current index - setting to first row (0) on arrow press");
                selectionModel()->setCurrentIndex(
                    firstIndex,
                    QItemSelectionModel::NoUpdate
                );
                scrollTo(firstIndex);
                return true; // Event handled, prevent Qt's default
            }
            return false; // No rows, let Qt handle it
        }

        // Normal case: current index is valid
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
    return false;
}

bool FileTable::isParentDirEntry(int row) const {
    if (!model()) return false;

    QModelIndex idx = model()->index(row, 0, rootIndex());
    if (!idx.isValid()) return false;

    QString displayText = idx.data(Qt::DisplayRole).toString();
    return (displayText == "[..]" || displayText == "<..>");
}

bool FileTable::handleSelectionKeys(QKeyEvent* keyEvent) {
    QString keyText = keyEvent->text();
    if (!keyText.isEmpty()) {
        QChar keyChar = keyText[0];

        // Plus key - select all (except parent directory)
        if (keyChar == '+' || keyEvent->key() == Qt::Key_Plus) {
            LOG_EVENT("KeyPress | Key=Plus (Select All)");
            logSelectionState("Before Select All");
            if (selectionModel()) {
                int rowCount = model()->rowCount(rootIndex());
                int startRow = 0;

                // Skip parent directory entry if present at row 0
                if (rowCount > 0 && isParentDirEntry(0)) {
                    startRow = 1;
                }

                // Only proceed if there are rows to select after skipping parent
                if (startRow < rowCount) {
                    LOG_ACTION(QString("Selecting rows %1-%2").arg(startRow).arg(rowCount - 1));
                    selectionModel()->select(
                        QItemSelection(
                            model()->index(startRow, 0, rootIndex()),
                            model()->index(
                                rowCount - 1,
                                model()->columnCount(rootIndex()) - 1,
                                rootIndex()
                            )
                        ),
                        QItemSelectionModel::Select | QItemSelectionModel::Rows
                    );
                }
            }
            logSelectionState("After Select All");
            return true;
        }

        // Minus key - clear selection
        if (keyChar == '-' || keyEvent->key() == Qt::Key_Minus) {
            LOG_EVENT("KeyPress | Key=Minus (Clear Selection)");
            logSelectionState("Before Clear Selection");
            if (selectionModel()) {
                selectionModel()->clearSelection();
            }
            logSelectionState("After Clear Selection");
            return true;
        }

        // Asterisk key - invert selection (except parent directory)
        if (keyChar == '*' || keyEvent->key() == Qt::Key_Asterisk) {
            LOG_EVENT("KeyPress | Key=Asterisk (Invert Selection)");
            logSelectionState("Before Invert Selection");
            if (selectionModel()) {
                int rowCount = model()->rowCount(rootIndex());
                int toggleCount = 0;
                for (int row = 0; row < rowCount; ++row) {
                    // Skip parent directory entry
                    if (isParentDirEntry(row)) {
                        continue;
                    }

                    QModelIndex idx = model()->index(row, 0, rootIndex());
                    selectionModel()->select(
                        idx,
                        QItemSelectionModel::Toggle | QItemSelectionModel::Rows
                    );
                    toggleCount++;
                }
                LOG_ACTION(QString("Toggled %1 rows").arg(toggleCount));
            }
            logSelectionState("After Invert Selection");
            return true;
        }
    }
    return false;
}

void FileTable::setActive(bool active) {
    m_isActive = active;
    viewport()->update();  // Repaint to show/hide current row bar
}

void FileTable::logSelectionState(const QString& context) {
#if FILETABLE_DEBUG_LOGGING
    if (!model() || !selectionModel()) return;

    QModelIndex current = currentIndex();
    QModelIndexList selectedRows = selectionModel()->selectedRows();

    QString selectedStr;
    for (int i = 0; i < selectedRows.size(); ++i) {
        if (i > 0) selectedStr += ",";
        selectedStr += QString::number(selectedRows[i].row());
    }

    int currentRow = current.isValid() ? current.row() : -1;
    int totalRows = model()->rowCount();
    int selectedCount = selectedRows.size();

    LOG_STATE(QString("Context=%1 | CurrentRow=%2 | Selected=[%3] | Count=%4/%5 | Active=%6")
        .arg(context)
        .arg(currentRow)
        .arg(selectedStr.isEmpty() ? "none" : selectedStr)
        .arg(selectedCount)
        .arg(totalRows)
        .arg(m_isActive ? "yes" : "no"));
#endif
}

void FileTable::onCurrentIndexChanged(const QModelIndex& current, const QModelIndex& previous)
{
    // Log the current index change with context
    int prevRow = previous.isValid() ? previous.row() : -1;
    int currRow = current.isValid() ? current.row() : -1;

    LOG_ACTION(QString("CurrentIndex changed: %1 -> %2").arg(prevRow).arg(currRow));
    logSelectionState(QString("After currentIndex change"));

    // When the current row changes, we need to repaint both the previous and new current rows
    // to ensure the delegate updates the highlight correctly across all columns

    // Repaint the entire previous current row
    if (previous.isValid() && model()) {
        QRect previousRowRect = visualRect(model()->index(previous.row(), 0, previous.parent()));
        int lastCol = model()->columnCount(previous.parent()) - 1;
        if (lastCol >= 0) {
            QRect lastCellRect = visualRect(model()->index(previous.row(), lastCol, previous.parent()));
            previousRowRect = previousRowRect.united(lastCellRect);
            LOG_ACTION(QString("Repainting previous row %1: rect=(%2,%3) %4x%5")
                .arg(previous.row()).arg(previousRowRect.x()).arg(previousRowRect.y())
                .arg(previousRowRect.width()).arg(previousRowRect.height()));
            viewport()->update(previousRowRect);
        }
    }

    // Repaint the entire new current row
    if (current.isValid() && model()) {
        QRect currentRowRect = visualRect(model()->index(current.row(), 0, current.parent()));
        int lastCol = model()->columnCount(current.parent()) - 1;
        if (lastCol >= 0) {
            QRect lastCellRect = visualRect(model()->index(current.row(), lastCol, current.parent()));
            currentRowRect = currentRowRect.united(lastCellRect);
            LOG_ACTION(QString("Repainting current row %1: rect=(%2,%3) %4x%5")
                .arg(current.row()).arg(currentRowRect.x()).arg(currentRowRect.y())
                .arg(currentRowRect.width()).arg(currentRowRect.height()));
            viewport()->update(currentRowRect);
        }
    }
}
