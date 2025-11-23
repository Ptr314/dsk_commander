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
#include <QToolButton>
#include <QDir>
#include <QDateTime>
#include <QSettings>
#include <QStandardItemModel>

#include "FileTable.h"
#include "dsk_tools/dsk_tools.h"

enum class panelMode {Host, Image};

class HostModel : public QStandardItemModel {
    Q_OBJECT
public:
    enum SortOrder {
        ByName,
        BySize,
        NoOrder
    };

    explicit HostModel(QObject *parent = nullptr);

    void setRootPath(const QString& path);
    void setNameFilters(const QStringList& filters);
    void setSortOrder(SortOrder order);
    void refresh();
    void goUp();
    QString currentPath() const { return m_currentPath; }
    QString filePath(const QModelIndex& index) const;
    QFileInfo fileInfo(const QModelIndex& index) const;
    bool isDir(const QModelIndex& index) const;
    SortOrder sortOrder() const { return m_sortOrder; }

private:
    QString m_currentPath;
    QStringList m_nameFilters;
    SortOrder m_sortOrder {ByName};
    bool m_isRoot {false};

    void populateModel();
    QString formatSize(qint64 size) const;
    QString formatDate(const QDateTime& dt) const;
};

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

    void onView();
    void onEdit();
    void onGoUp();
    void chooseDirectory();

    // Sorting methods
    void setSortOrder(HostModel::SortOrder order);
    int getSortOrder() const;

signals:
    void activated(FilePanel* self);
    void switchPanelRequested();
    void sortOrderChanged(int order);

private slots:
    void onFilterChanged(int index);
    void onTypeChanged(int index);
    void onFsChanged(int index);
    void onAutoChanged(Qt::CheckState checked);
    void onPathEntered();
    void onItemDoubleClicked(const QModelIndex& index);

private:
    QToolBar* topToolBar {nullptr};
    QToolBar* filterToolBar {nullptr};
    QToolBar* typeToolBar {nullptr};
    FileTable* tableView {nullptr};
    QComboBox* filterCombo {nullptr};
    QComboBox* typeCombo {nullptr};
    QComboBox* fsCombo {nullptr};
    QCheckBox* autoCheck {nullptr};
    QToolButton* dirButton {nullptr};
    QToolButton* upButton {nullptr};
    QLineEdit* dirEdit {nullptr};
    HostModel * host_model {nullptr};
    QStandardItemModel * image_model {nullptr};

    QString currentPath;
    QString m_lastDirName;  // Track last entered directory for cursor restoration
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
    void setDirectory(const QString& path, bool restoreCursor = false);
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void setComboBoxByItemData(QComboBox* comboBox, const QVariant& value);
    int openImage(QString path);
    void processImage(std::string filesystem_type);
    void updateTable();
    void setMode(panelMode new_mode);
    void dir();
    QString replace_placeholders(const QString & in);

};
