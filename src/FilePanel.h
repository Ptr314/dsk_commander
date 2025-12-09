// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File panel UI

#pragma once
#include <QWidget>
#include <QToolBar>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QToolButton>
#include <QMenu>
#include <QDir>
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
    void setSortOrder(SortOrder order, bool ascending);
    void refresh();
    void goUp();
    QString currentPath() const { return m_currentPath; }
    QString filePath(const QModelIndex& index) const;
    QFileInfo fileInfo(const QModelIndex& index) const;
    bool isDir(const QModelIndex& index) const;
    SortOrder sortOrder() const { return m_sortOrder; }
    static QString formatSize(qint64 size) ;
    static QString formatDate(const QDateTime& dt);

private:
    QString m_currentPath;
    QStringList m_nameFilters;
    SortOrder m_sortOrder {ByName};
    bool m_sortAsc {true};
    bool m_isRoot {false};

    void populateModel();
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
    QString currentFilePath() const;
    void refresh();
    void setActive(bool active);
    void focusList();
    void retranslateUi();

    // Sorting methods
    void setSortOrder(HostModel::SortOrder order);
    int getSortOrder() const;

    // Show deleted files methods
    void setShowDeleted(bool show);
    bool getShowDeleted() const { return m_show_deleted; }

    // Mode getter
    panelMode getMode() const { return mode; }

    // Filesystem getter
    dsk_tools::fileSystem* getFileSystem() { return m_filesystem.get(); }
    const dsk_tools::fileSystem* getFileSystem() const { return m_filesystem.get(); }

    // Selection model getter (for MainWindow signal connections)
    QItemSelectionModel* tableSelectionModel() const {
        return tableView ? tableView->selectionModel() : nullptr;
    }

    // File operations
    int selectedCount() const;
    bool isIndexValid() const;
    bool allowPutFiles() const;
    dsk_tools::Files getSelectedFiles() const;
    // void putFiles(dsk_tools::fileSystem* sourceFs, const dsk_tools::Files & files, const QString & format, const bool copy);

    // Panel operations
    void onGoUp();
    void chooseDirectory();

    // Image menu operations
    void saveImage();          // Save (stub for now)
    void saveImageAs();        // Save as...

    QString getSelectedFormat() const;
    QString getSelectedType() const;
    dsk_tools::Files& getFiles() {return m_files;};
    dsk_tools::fileSystem* getFileSytem() {return m_filesystem.get();};
    QSettings* getSettings() {return (m_settings);};
    QModelIndex getCurrentIndex() const {return tableView->currentIndex();};
    dsk_tools::diskImage* getImage() {return m_image.get();};
    HostModel* getHostModel() {return host_model;};
    const QJsonObject* getFileFormats() {return &m_file_formats;};
    const QJsonObject* getFileTypes() {return &m_file_types;};
    const QJsonObject* getFileSystems() {return &m_file_systems;};
    std::string getLoadedFormat() {return m_current_format_id;};


    void dir();
    void setDirectory(const QString& path, bool restoreCursor = false);
    dsk_tools::Result openImage(QString path);
    void updateImageStatusIndicator() const;
    void storeTableState();
    void restoreTableState();


protected:
    void changeEvent(QEvent* event) override;

signals:
    void activated(FilePanel* self);
    void switchPanelRequested();
    void sortOrderChanged(int order);
    void panelModeChanged(panelMode newMode);

private slots:
    void onFilterChanged(int index);
    void onTypeChanged(int index);
    void onFsChanged(int index);
    void onAutoChanged(Qt::CheckState checked);
    void onPathEntered();
    void onItemDoubleClicked(const QModelIndex& index);
    void onHistoryMenuTriggered(QAction* action);

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
    QLabel* imageLabel {nullptr};  // Display image filename in Image mode
    QToolButton* saveButton {nullptr};  // Save button (Image mode only)
    QToolButton* saveAsButton {nullptr};  // Save As button (Image mode only)
    QMenu* historyMenu {nullptr};
    HostModel * host_model {nullptr};
    QStandardItemModel * image_model {nullptr};
    QStringList m_directoryHistory;

    QString currentPath;
    QString m_lastDirName;  // Track last entered directory for cursor restoration
    panelMode mode {panelMode::Host};
    bool m_show_deleted {true};  // Default to showing deleted files

    QSettings * m_settings;
    QString m_ini_label;

    const QJsonObject & m_file_formats;
    const QJsonObject & m_file_types;
    const QJsonObject & m_file_systems;

    std::unique_ptr<dsk_tools::diskImage> m_image {nullptr};
    std::unique_ptr<dsk_tools::fileSystem> m_filesystem {nullptr};

    // Loaded image metadata (Image mode only)
    std::string m_current_format_id;      // Physical format: "FILE_RAW_MSB", "FILE_AIM", "FILE_HXC_HFE", etc.
    std::string m_current_type_id;        // Disk type: "TYPE_AGAT_140", "TYPE_AGAT_840", etc.
    std::string m_current_filesystem_id;  // Filesystem: "FS_DOS33", "FS_SPRITEOS", "FS_CPM", etc.

    std::vector<dsk_tools::UniversalFile> m_files;
    HostModel::SortOrder m_sort_order {HostModel::SortOrder::NoOrder};
    bool m_sort_ascending {true};

    // Table state storage stack (for preserving position during nested updates)
    std::vector<std::pair<int, int>> m_tableStateStack;  // Stack of (row, scroll) pairs

    void setupPanel();
    void setupFilters();
    void populateFilterCombo();
    bool eventFilter(QObject* obj, QEvent* ev) override;

    static void setComboBoxByItemData(QComboBox* comboBox, const QVariant& value);
    void processImage(const std::string &filesystem_type);
    void updateTable();
    void setMode(panelMode new_mode);
    void updateToolbarVisibility() const;

    // Unsaved changes handling
    bool checkUnsavedChanges();

    // Directory history methods
    void loadDirectoryHistory();
    void saveDirectoryHistory();
    void addToDirectoryHistory(const QString& path);
    void updateHistoryMenu();
};
