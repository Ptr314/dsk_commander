// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File panel UI

#pragma once
#include <QWidget>
#include <QToolBar>
#include <QLineEdit>
#include <QComboBox>
#include <QTableView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDir>

#include "CustomFileSystemModel.h"
#include "CustomSortProxyModel.h"

class FilePanel : public QWidget {
    Q_OBJECT
public:
    explicit FilePanel(QWidget *parent = nullptr);

    QString currentDir() const { return currentPath; }
    QStringList selectedPaths() const;
    void refresh();
    void setActive(bool active);
    void focusList();

    QItemSelectionModel* tableSelectionModel() const {
        return tableView ? tableView->selectionModel() : nullptr;
    }

signals:
    void activated(FilePanel* self);

private slots:
    void chooseDirectory();
    void onFilterChanged(int index);
    void onGoUp();
    void onPathEntered();
    void onItemDoubleClicked(const QModelIndex& index);

private:
    QToolBar* topToolBar {nullptr};
    QToolBar* bottomToolBar {nullptr};
    QTableView* tableView {nullptr};
    QComboBox* filterCombo {nullptr};
    QPushButton* dirButton {nullptr};
    QPushButton* upButton {nullptr};
    QLineEdit* dirEdit {nullptr};
    CustomFileSystemModel* model {nullptr};
    CustomSortProxyModel* proxy {nullptr};
    QString currentPath;

    void setupPanel();
    void setDirectory(const QString& path);
    bool eventFilter(QObject* obj, QEvent* ev) override;
};
