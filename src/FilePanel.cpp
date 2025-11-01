// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File panel UI

#include "FilePanel.h"
#include <QFileDialog>
#include <QHeaderView>
#include <QEvent>
#include <QDesktopServices>
#include <QUrl>

FilePanel::FilePanel(QWidget *parent) : QWidget(parent) {
    setupPanel();
}

void FilePanel::setupPanel() {
    QFont font("Consolas", 10, 400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(3);

    // Model for the file system
    model = new CustomFileSystemModel(this);
    model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    model->setReadOnly(true);

    // A proxy for custom sorting
    proxy = new CustomSortProxyModel(this);
    proxy->setSourceModel(model);
    proxy->setDynamicSortFilter(true);

    // Upper tooldar
    topToolBar = new QToolBar(this);
    upButton = new QPushButton("⬆️ Вверх", this);
    dirEdit = new QLineEdit(this);
    dirEdit->setPlaceholderText("Введите путь и нажмите Enter…");
    dirButton = new QPushButton("Выбрать...", this);

    topToolBar->addWidget(upButton);
    topToolBar->addWidget(dirEdit);
    topToolBar->addWidget(dirButton);

    connect(upButton,  &QPushButton::clicked, this, &FilePanel::onGoUp);
    connect(dirButton, &QPushButton::clicked, this, &FilePanel::chooseDirectory);
    connect(dirEdit,   &QLineEdit::returnPressed, this, &FilePanel::onPathEntered);

    // Main table with files
    tableView = new QTableView(this);
    tableView->setFont(font);
    tableView->setModel(proxy);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setAlternatingRowColors(true);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setShowGrid(false);
    tableView->verticalHeader()->hide();

    tableView->setSortingEnabled(true);
    tableView->sortByColumn(0, Qt::AscendingOrder);

    // Columns
    tableView->setColumnHidden(2, true);  // Hide "Type"
    QHeaderView* hh = tableView->horizontalHeader();
    hh->setStretchLastSection(false);             // Manual mode
    hh->setSectionResizeMode(0, QHeaderView::Stretch); // Name - expanded
    hh->setSectionResizeMode(1, QHeaderView::Fixed);   // Size — fixed
    hh->setSectionResizeMode(3, QHeaderView::Fixed);   // Date — fixed

    const int colSizeWidth = 120;
    const int colDateWidth = 180;
    tableView->setColumnWidth(1, colSizeWidth);
    tableView->setColumnWidth(3, colDateWidth);

    // Hiding horizontal scrollbar
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tableView->setTextElideMode(Qt::ElideRight);

    connect(tableView, &QTableView::doubleClicked, this, &FilePanel::onItemDoubleClicked);

    // Bottom toolbar
    bottomToolBar = new QToolBar(this);
    filterCombo = new QComboBox(this);
    filterCombo->addItems({
        "Все файлы (*.*)",
        "Изображения (*.png *.jpg *.jpeg *.bmp *.gif)",
        "Текстовые (*.txt *.md *.cpp *.h *.hpp *.json *.xml)",
        "Документы (*.pdf *.doc *.docx *.xls *.xlsx)"
    });
    bottomToolBar->addWidget(filterCombo);
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilePanel::onFilterChanged);

    // Install event filters for controls
    for (QObject* obj : {
                         static_cast<QObject*>(tableView),
                         static_cast<QObject*>(tableView->viewport()),
                         static_cast<QObject*>(filterCombo),
                         static_cast<QObject*>(dirButton),
                         static_cast<QObject*>(upButton),
                         static_cast<QObject*>(dirEdit)})
    {
        obj->installEventFilter(this);
    }

    // Adding elements
    layout->addWidget(topToolBar, 0);
    layout->addWidget(tableView, 1);
    layout->addWidget(bottomToolBar, 0);

    layout->setStretch(1, 1); // Table consumes maximim
    setLayout(layout);

    // Starting path
    setDirectory(QDir::homePath());
}

void FilePanel::setDirectory(const QString& path) {
    if (path.isEmpty()) return;
    QDir dir(path);
    if (!dir.exists()) return;

    currentPath = dir.absolutePath();
    dirEdit->setText(currentPath);
    QModelIndex sourceRoot = model->setRootPath(currentPath);
    QModelIndex proxyRoot  = proxy->mapFromSource(sourceRoot);
    tableView->setRootIndex(proxyRoot);

    tableView->setColumnWidth(1, 120);
    tableView->setColumnWidth(3, 180);
}

void FilePanel::chooseDirectory() {
    emit activated(this);
    QString dir = QFileDialog::getExistingDirectory(this, FilePanel::tr("Choose a path"), currentPath);
    if (!dir.isEmpty()) setDirectory(dir);
}

void FilePanel::onFilterChanged(int) {
    QString text = filterCombo->currentText();
    QStringList filters;
    int l = text.indexOf('(');
    int r = text.indexOf(')');
    if (l >= 0 && r > l)
        filters = text.mid(l + 1, r - l - 1).split(' ', Qt::SkipEmptyParts);
    model->setNameFilters(filters);
    model->setNameFilterDisables(false);
}

void FilePanel::onGoUp() {
    emit activated(this);
    QDir dir(currentPath);
    if (dir.cdUp()) setDirectory(dir.absolutePath());
}

void FilePanel::onPathEntered() {
    emit activated(this);
    QString entered = dirEdit->text().trimmed();
    QDir dir(entered);
    if (dir.exists()) setDirectory(dir.absolutePath());
}

void FilePanel::onItemDoubleClicked(const QModelIndex& index) {
    emit activated(this);  // broadcast the activation

    if (!index.isValid())
        return;

    // Getting a selected item
    QString displayName = proxy->data(index, Qt::DisplayRole).toString();

    // Process upper directory
    if (displayName == "..") {
        QDir dir(currentPath);
        if (dir.cdUp())
            setDirectory(dir.absolutePath());
        return;
    }

    // Getting an original path
    QModelIndex srcIndex = proxy->mapToSource(index);
    QString path = model->filePath(srcIndex);
    QFileInfo info(path);

    // Process a dir or a file
    if (info.isDir()) {
        setDirectory(path);  // Direcory: Set as a new
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));  // File: Open it
    }
}

QStringList FilePanel::selectedPaths() const {
    QStringList paths;
    if (!tableView->selectionModel()) return paths;
    const auto rows = tableView->selectionModel()->selectedRows(0);
    for (const auto& idx : rows) {
        QModelIndex srcIdx = proxy->mapToSource(idx);
        paths << model->filePath(srcIdx);
    }
    return paths;
}

void FilePanel::refresh() {
    setDirectory(currentPath);
}

void FilePanel::setActive(bool active) {
    // TODO: check why is not working
    setStyleSheet(active ? "FilePanel { border: 2px solid red; }" : "");
}

void FilePanel::focusList() {
    if (tableView) tableView->setFocus();
}

bool FilePanel::eventFilter(QObject* obj, QEvent* ev) {
    if ((ev->type() == QEvent::FocusIn || ev->type() == QEvent::MouseButtonPress))
        emit activated(this);
    return QWidget::eventFilter(obj, ev);
}
