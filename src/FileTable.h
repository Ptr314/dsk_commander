// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: Custom table view for file panel

#ifndef FILETABLE_H
#define FILETABLE_H

#include <QTableView>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTimer>
#include <QModelIndex>

#include "libs/dsk_tools/src/definitions.h"

// Forward declarations
class QEvent;
class QMouseEvent;
class QKeyEvent;

// Custom delegate for highlighting current row
class CurrentRowDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit CurrentRowDelegate(QTableView* view, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    QTableView* m_tableView;
};

// Custom table view with Norton Commander-style selection behavior
class FileTable : public QTableView {
    Q_OBJECT

public:
    explicit FileTable(QWidget* parent = nullptr);
    ~FileTable() override = default;

    // Setup methods for different display modes
    void setupForHostMode();
    void setupForImageMode(dsk_tools::FSCaps capabilities);

    // Active state management
    void setActive(bool active);
    bool isActive() const { return m_isActive; }

signals:
    // Emitted when the view receives focus or is clicked
    void focusReceived();

    // Emitted when Tab key is pressed to switch panels
    void switchPanelRequested();

protected:
    // Override event filter to implement custom selection behavior
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    // Handle current index changes to repaint affected rows
    void onCurrentIndexChanged(const QModelIndex& current, const QModelIndex& previous);

private:
    // Timer for distinguishing single-click from double-click
    QTimer* m_clickTimer;

    // Index of the last clicked item (pending single-click processing)
    QModelIndex m_pendingClickIndex;

    // Custom delegate for row highlighting
    CurrentRowDelegate* m_delegate;

    // Active state (whether this panel is active)
    bool m_isActive = false;

    // Helper methods
    void handleMousePress(QMouseEvent* mouseEvent);
    void handleMouseDoubleClick();

    // Specific key handlers
    bool handleArrowKeys(QKeyEvent* keyEvent);
    bool handleSelectionKeys(QKeyEvent* keyEvent);
};

#endif // FILETABLE_H
