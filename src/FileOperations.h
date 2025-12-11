// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File operations extracted from FilePanel and MainWindow

#pragma once

#include <QWidget>
#include <QSettings>
#include "dsk_tools/dsk_tools.h"

class FilePanel;

class FileOperations {
public:
    static void viewFile(FilePanel* panel, QWidget* parent);
    static void editFile(FilePanel* panel, QWidget* parent);
    static void viewFileInfo(FilePanel* panel, QWidget* parent);
    static void viewFilesystemInfo(FilePanel* panel, QWidget* parent);
    static void copyFiles(FilePanel* source, FilePanel* target, QWidget* parent);
    static void deleteFiles(FilePanel* panel, QWidget* parent);
    static void restoreFiles(FilePanel* panel, QWidget* parent);
    static void renameFile(FilePanel* panel, QWidget* parent);
    static void createDirectory(FilePanel* panel, QWidget* parent);
    static void saveImage(FilePanel* panel, QWidget* parent);
    static void saveImageAs(FilePanel* panel, QWidget* parent);

    static QString decodeError(const dsk_tools::Result& result);
    static void openItem(FilePanel* panel, QWidget* parent, const QModelIndex& index);

private:
    static void showInfoDialog(const std::string& info, QWidget* parent);
    static void deleteRecursively(FilePanel* panel, QWidget* parent, const dsk_tools::UniversalFile & f);
    static void putFiles(FilePanel* source, FilePanel* target, QWidget* parent, const dsk_tools::Files & files, const QString & format, int recursion);
    static void saveImageWithBackup(FilePanel* panel);
};