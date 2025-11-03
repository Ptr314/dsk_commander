// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File panel UI

#pragma once
#include <QWidget>
#include <QToolBar>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTableView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDir>
#include <QSettings>
#include <QJsonObject>
#include <QStandardItemModel>

#include "CustomFileSystemModel.h"
#include "CustomSortProxyModel.h"
#include "dsk_tools/dsk_tools.h"

enum class panelMode {Host, Image};

class FilePanel : public QWidget {
    Q_OBJECT
public:
    explicit FilePanel(
        QWidget *parent,
        QSettings *settings,
        const QString ini_label,
        const QJsonObject & file_formats,
        const QJsonObject & file_types,
        const QJsonObject & file_systems
        );

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
    void onTypeChanged(int index);
    void onFsChanged(int index);
    void onAutoChanged(Qt::CheckState checked);
    void onGoUp();
    void onPathEntered();
    void onItemDoubleClicked(const QModelIndex& index);

private:
    QToolBar* topToolBar {nullptr};
    QToolBar* filterToolBar {nullptr};
    QToolBar* typeToolBar {nullptr};
    QTableView* tableView {nullptr};
    QComboBox* filterCombo {nullptr};
    QComboBox* typeCombo {nullptr};
    QComboBox* fsCombo {nullptr};
    QCheckBox* autoCheck {nullptr};
    QPushButton* dirButton {nullptr};
    QPushButton* upButton {nullptr};
    QLineEdit* dirEdit {nullptr};
    CustomFileSystemModel * host_model {nullptr};
    CustomSortProxyModel * host_proxy {nullptr};
    QStandardItemModel * image_model {nullptr};

    QString currentPath;
    panelMode mode {panelMode::Host};

    QSettings * m_settings;
    QString m_ini_label;

    const QJsonObject & m_file_formats;
    const QJsonObject & m_file_types;
    const QJsonObject & m_file_systems;

    dsk_tools::diskImage * m_image {nullptr};
    dsk_tools::fileSystem * m_filesystem {nullptr};
    std::vector<dsk_tools::fileData> m_files;


    void setupPanel();
    void setupFilters();
    void setDirectory(const QString& path);
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void setComboBoxByItemData(QComboBox* comboBox, const QVariant& value);
    int openImage(QString path);
    int loadFile(std::string file_name, std::string file_format, std::string file_type, std::string filesystem_type);
    void processImage(std::string filesystem_type);
    void initTable();
    void updateTable();
    void dir();

};
