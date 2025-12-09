// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: File operations extracted from FilePanel and MainWindow

#include "FileOperations.h"
#include "FilePanel.h"
#include "mainutils.h"
#include "placeholders.h"
#include "fileparamdialog.h"
#include "convertdialog.h"
#include "viewdialog.h"
#include "formatdialog.h"
#include "fs_host.h"
#include "host_helpers.h"
#include "./ui_fileinfodialog.h"

#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QFont>
#include <QFontDatabase>
#include <QDialog>
#include <QDateTime>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QDebug>
#include <memory>
#include <set>

#include "dsk_tools/dsk_tools.h"

void FileOperations::viewFile(FilePanel* panel, QWidget* parent)
{
    if (!panel || !parent) return;

    if (panel->getMode() == panelMode::Host) {
        // Host mode: show information about selected disk image file
        const QString path = panel->currentFilePath();
        if (path.isEmpty()) return;

        const QFileInfo fi(path);
        if (fi.isDir()) {
            const QDir dir(path);
            QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);

            int fileCount = 0;
            int dirCount = 0;
            qint64 totalSize = 0;

            for (const QFileInfo& entry : entries) {
                if (entry.isDir()) {
                    dirCount++;
                } else {
                    fileCount++;
                    totalSize += entry.size();
                }
            }

            QString info = FilePanel::tr("Directory: %1\n\n").arg(fi.fileName());
            info += FilePanel::tr("Path: %1\n").arg(fi.absoluteFilePath());
            info += FilePanel::tr("Subdirectories: %1\n").arg(dirCount);
            info += FilePanel::tr("Files: %1\n").arg(fileCount);
            info += FilePanel::tr("Total size: %1 bytes\n").arg(totalSize);
            info += FilePanel::tr("Last modified: %1").arg(QLocale().toString(fi.lastModified(), QLocale::ShortFormat));

            QMessageBox::information(parent, FilePanel::tr("Directory Information"), info);
        } else {
            const std::string file_name = _toStdString(fi.absoluteFilePath());
            std::string type_id;
            std::string filesystem_id;
            std::string format_id = panel->getSelectedFormat().toStdString();
            // Auto-detect format if necessary
            if (format_id == "FILE_ANY") {
                dsk_tools::Result res = dsk_tools::detect_fdd_type(file_name, format_id, type_id, filesystem_id, true);
            }
            if (type_id.empty()) {
                type_id = panel->getSelectedType().toStdString();
            }
            // Create appropriate loader
            const std::unique_ptr<dsk_tools::Loader> loader = dsk_tools::create_loader(file_name, format_id, type_id);

            if (!loader) {
                QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("Not supported yet"));
                return;
            }
            // Display info
            showInfoDialog(loader->file_info(), parent);
        }
    } else {
        const QModelIndex index = panel->getCurrentIndex();
        if (index.isValid()) {
            openItem(panel, parent, index);
        }

    }
}

void FileOperations::openItem(FilePanel* panel, QWidget* parent, const QModelIndex& index)
{
    if (panel->getMode() == panelMode::Host) {
        HostModel* host_model = panel->getHostModel();
        QString displayName = host_model->data(index, Qt::DisplayRole).toString();

        // Process [..] navigation
        if (displayName == "[..]") {
            panel->onGoUp();
            return;
        }

        const QString path = host_model->filePath(index);
        if (path.isEmpty()) return;

        QFileInfo info(path);

        if (info.isDir()) {
            panel->setDirectory(path);
        } else {
            auto res = panel->openImage(path);
            if (!res) {
                QMessageBox::critical(parent, FilePanel::tr("Error"), decodeError(res));
            }
        }
    } else {
        auto f = panel->getFiles()[index.row()];
        if (f.is_dir){
            panel->getFileSystem()->cd(f);
            panel->dir();
        } else {
            dsk_tools::BYTES data;
            panel->getFileSystem()->get_file(f, "", data);

            if (!data.empty()) {
                QDialog * w = new ViewDialog(
                                    parent,
                                    panel->getSettings(),
                                    QString::fromStdString(f.name),
                                    data,
                                    f.type_preferred,
                                    f.is_deleted,
                                    panel->getImage(),
                                    panel->getFileSystem()
                );
                w->setAttribute(Qt::WA_DeleteOnClose);
                w->setWindowTitle(w->windowTitle() + " (" + QString::fromStdString(f.name) + ")");
                w->show();
            } else {
                QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("File reading error!"));
            }
        }
    }
}

void FileOperations::viewFileInfo(FilePanel* panel, QWidget* parent)
{
    const auto file_info = new QDialog(parent);
    file_info->setAttribute(Qt::WA_DeleteOnClose);

    Ui_FileInfo fileinfoUi{};
    fileinfoUi.setupUi(file_info);

    const QModelIndex index = panel->getCurrentIndex();
    if (index.isValid()) {
        auto f = panel->getFiles()[index.row()];

        const auto info = replacePlaceholders(QString::fromStdString(panel->getFileSystem()->file_info(f)));

        QFont font;
#ifdef Q_OS_WIN
        font.setFamily("Consolas");
        font.setPointSize(10);
#else
        font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPointSize(10);
#endif
        fileinfoUi.textBox->setFont(font);
        fileinfoUi.textBox->setPlainText(info);
        file_info->exec();
    }
}

void FileOperations::editFile(FilePanel* panel, QWidget* parent)
{
    QModelIndex index = panel->getCurrentIndex();
    if (index.isValid()) {
        if (panel->getMode()==panelMode::Host) {
            // In host mode just open the image
            openItem(panel, parent, index);
        } else {
            // In image mode edit metadata
            auto filesystem = panel->getFileSystem();
            auto f = panel->getFiles()[index.row()];
            std::vector<dsk_tools::ParameterDescription> params = filesystem->file_get_metadata(f);

            for (auto & param : params) {
                param.name = replacePlaceholders(QString::fromStdString(param.name)).toStdString();
            }

            FileParamDialog dialog(params);
            if (dialog.exec() == QDialog::Accepted) {
                const auto values = dialog.getParameters();
                filesystem->file_set_metadata(f, values);
                panel->dir();
            }
        }
        panel->updateImageStatusIndicator();
    }
}

void FileOperations::viewFilesystemInfo(FilePanel* panel, QWidget* parent)
{
    if (!panel || !parent || panel->getMode() != panelMode::Image || !panel->getImage()) return;

    const std::string info = panel->getFileSystem()->information();
    showInfoDialog(info, parent);
}

void FileOperations::copyFiles(FilePanel* source, FilePanel* target, QWidget* parent)
{
    if (!source || !target || !parent) return;

    dsk_tools::fileSystem* sourceFs = source->getFileSystem();
    dsk_tools::fileSystem* targetFs = target->getFileSystem();

    if (!sourceFs || !targetFs || !target->allowPutFiles()) return;

    if (targetFs->getFS() == dsk_tools::FS::Host && sourceFs->getFS() != dsk_tools::FS::Host) {
        // Extracting files to host. Need to ask for format
        std::vector<std::string> formats = sourceFs->get_save_file_formats();

        QMap<QString, QString> fil_map;

        foreach (const std::string & v, formats) {
            QJsonObject fil = source->getFileFormats()->value(QString::fromStdString(v)).toObject();
            fil_map[QString::fromStdString(v)] = QCoreApplication::translate("config", fil["name"].toString().toUtf8().constData());
        }


        // Restore last used format from settings
        const QString fs_string = QString::number(static_cast<int>(sourceFs->getFS()));
        const QString defaultFormat = source->getSettings()->value("export/extract_format_"+fs_string, "").toString();

        // Show format selection dialog
        auto *dialog = new FormatDialog(parent, fil_map,
                                                defaultFormat,
                                                FilePanel::tr("Selected files: %1").arg(source->selectedCount()),
                                                FilePanel::tr("Choose output file format:"),
                                                FilePanel::tr("Choose the format"));
        dialog->setWindowTitle(FilePanel::tr("Copying files"));

        if (dialog->exec() == QDialog::Accepted) {
            const QString selectedFormat = dialog->getSelectedFormat();

            // Save selected format to settings for next time
            source->getSettings()->setValue("export/extract_format_"+fs_string, selectedFormat);

            const dsk_tools::Files files = source->getSelectedFiles();
            putFiles(source, target, parent, files, selectedFormat);
            target->refresh();

            // qDebug() << "User selected format:" << selectedFormat;
        }

        delete dialog;

    } else {
        // Moving files between FSs in other cases
        const QMessageBox::StandardButton reply = QMessageBox::question(parent,
                                                                  FilePanel::tr("Copying files"),
                                                                  FilePanel::tr("Copy %1 files?").arg(
                                                                      source->selectedCount()),
                                                                  QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            const dsk_tools::Files files = source->getSelectedFiles();
            putFiles(source, target, parent, files, "");
            target->refresh();
        }
    }
}

void FileOperations::deleteFiles(FilePanel* panel, QWidget* parent)
{
    if (!panel || !parent) return;
    dsk_tools::fileSystem* filesystem = panel->getFileSystem();
    if (!filesystem) return;

    dsk_tools::Files files = panel->getSelectedFiles();
    panel->storeTableState();
    if (!files.empty()) {
        const QMessageBox::StandardButton reply_all = QMessageBox::question(parent,
                            FilePanel::tr("Deleting files"),
                            FilePanel::tr("Delete %1 files?").arg(files.size()),
                            QMessageBox::Yes|QMessageBox::No
        );
        if (reply_all == QMessageBox::Yes) {
            bool recursively = false;
            foreach (const dsk_tools::UniversalFile & f, files) {
                if (f.is_dir) {
                    if (!recursively) {
                        const QMessageBox::StandardButton reply_dir = QMessageBox::question(parent,
                                            FilePanel::tr("Deleting directories"),
                                            FilePanel::tr("'%1' is a directory. Delete it recursively?").arg(QString::fromUtf8(f.name.c_str())),
                                            QMessageBox::Yes|QMessageBox::No
                        );
                        if (reply_dir == QMessageBox::Yes) {
                            recursively = true;
                            deleteRecursively(panel, parent, f);
                        }
                    } else {
                        // User already confirmed recursive deletion for a previous directory
                        deleteRecursively(panel, parent, f);
                    }
                } else {
                    auto result = filesystem->delete_file(f);
                    if (!result) {
                        // Check if trash failed
                        if (result.message == "TRASH_FAILED") {
                            // Prompt user for permanent deletion
                            const QMessageBox::StandardButton reply_perm = QMessageBox::warning(
                                parent,
                                FilePanel::tr("Recycle Bin Failed"),
                                FilePanel::tr("Cannot move '%1' to recycle bin.\n\n"
                                            "Do you want to delete it permanently instead?\n\n"
                                            "Warning: This action cannot be undone!")
                                    .arg(QString::fromStdString(f.name)),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No
                            );

                            if (reply_perm == QMessageBox::Yes) {
                                // Temporarily disable recycle bin and retry
                                bool (*old_callback)() = dsk_tools::fsHost::use_recycle_bin;
                                dsk_tools::fsHost::use_recycle_bin = nullptr;
                                result = filesystem->delete_file(f);
                                dsk_tools::fsHost::use_recycle_bin = old_callback;
                            }
                        }

                        if (!result) {
                            QMessageBox::critical(
                                parent,
                                FilePanel::tr("Error"),
                                FilePanel::tr("Error deleting file '%1'").arg(QString::fromStdString(f.name))
                            );
                        }
                    }
                }
            }
            panel->refresh();
            panel->restoreTableState();
        }
    }
}

void FileOperations::deleteRecursively(FilePanel* panel, QWidget* parent, const dsk_tools::UniversalFile & f)
{
   if (panel->getMode() == panelMode::Host) {
        // Extract full directory path from metadata
        QString path_to_delete = QString::fromStdString(dsk_tools::bytesToString(f.metadata));

        // Create QDir object and validate
        QDir dir(path_to_delete);
        if (!dir.exists()) {
            QMessageBox::critical(
                parent,
                FilePanel::tr("Error"),
                FilePanel::tr("Directory '%1' not found").arg(path_to_delete)
            );
            return;
        }

        // Perform recursive deletion
        bool success = false;

        // Try trash first if enabled
        if (dsk_tools::fsHost::use_recycle_bin && dsk_tools::fsHost::use_recycle_bin()) {
            std::string path_std = dsk_tools::bytesToString(f.metadata);
            if (utf8_trash(path_std) == 0) {
                success = true;
            } else {
                // Trash failed - ask user
                const QMessageBox::StandardButton reply_perm = QMessageBox::warning(
                    parent,
                    FilePanel::tr("Recycle Bin Failed"),
                    FilePanel::tr("Cannot move directory '%1' to recycle bin.\n\n"
                                "Do you want to delete it permanently instead?\n\n"
                                "Warning: This action cannot be undone!")
                        .arg(QString::fromStdString(f.name)),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No
                );

                if (reply_perm == QMessageBox::Yes) {
                    success = dir.removeRecursively();
                }
            }
        } else {
            // Recycle bin disabled - permanent deletion
            success = dir.removeRecursively();
        }

        if (!success) {
            QMessageBox::critical(
                parent,
                FilePanel::tr("Error"),
                FilePanel::tr("Error deleting directory '%1'").arg(QString::fromStdString(f.name))
            );
        }
    } else {
        // Image mode: not implemented yet
        QMessageBox::information(
            parent,
            FilePanel::tr("Not Implemented"),
            FilePanel::tr("Recursive directory deletion in image mode is not yet implemented")
        );
    }
}

void FileOperations::renameFile(FilePanel* panel, QWidget* parent)
{
    if (!panel || !parent) return;
    dsk_tools::fileSystem* filesystem = panel->getFileSystem();
    if (!filesystem) return;

    const dsk_tools::Files files = panel->getSelectedFiles();

    // Rename only works on single files
    if (files.size() != 1) {
        QMessageBox::information(parent,
                                FilePanel::tr("Rename"),
                                FilePanel::tr("Please select exactly one file to rename."));
        return;
    }

    const dsk_tools::UniversalFile & file = files[0];

    // Don't allow renaming parent directory entry
    if (file.name == "..") {
        return;
    }

    QString old_name = QString::fromStdString(file.name);
    bool ok{};
    QString new_name = QInputDialog::getText(
        parent,
        FilePanel::tr("Rename"),
        FilePanel::tr("New name:"),
        QLineEdit::Normal,
        old_name,
        &ok
    );

    if (ok && !new_name.isEmpty()) {
        // Don't rename if the name hasn't changed
        if (new_name == old_name) {
            return;
        }

        const auto result = filesystem->rename_file(file, new_name.toStdString());
        if (!result) {
            QMessageBox::critical(
                parent,
                FilePanel::tr("Error"),
                FilePanel::tr("Error renaming file '%1' to '%2': %3").arg(
                    old_name,
                    new_name,
                    QString::fromStdString(decodeError(result).toStdString())
                )
            );
        } else {
            panel->refresh();
        }
    }
}

void FileOperations::createDirectory(FilePanel* panel, QWidget* parent)
{
    if (!panel || !parent) return;
    dsk_tools::fileSystem* filesystem = panel->getFileSystem();
    if (!filesystem) return;

    const dsk_tools::FSCaps funcs = filesystem->getCaps();

    if (dsk_tools::hasFlag(funcs, dsk_tools::FSCaps::MkDir)) {
        bool ok{};
        const QString text = QInputDialog::getText(parent, FilePanel::tr("Add a directory"),
                                             FilePanel::tr("Directory name:"),
                                             QLineEdit::Normal,
                                             "New",
                                             &ok);
        if (ok && !text.isEmpty()) {
            dsk_tools::UniversalFile new_dir;
            const dsk_tools::Result res = filesystem->mkdir(text.toStdString(), new_dir);
            if (!res) {
                QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("Error creating directory: ") + decodeError(res));
            }
            panel->refresh();
            panel->highlight(text);
        }
    }
    panel->updateImageStatusIndicator();
}

void FileOperations::saveImage(FilePanel* panel, QWidget* parent)
{
    if (!panel || !parent || panel->getMode() != panelMode::Image) return;

    dsk_tools::fileSystem* filesystem = panel->getFileSystem();
    if (!filesystem || !filesystem->get_changed()) return;

    if (panel->getLoadedFormat()=="FILE_RAW_MSB")
    {
        saveImageWithBackup(panel);
    } else {
        QMessageBox::critical(
            parent,
            FilePanel::tr("Error"),
        FilePanel::tr("Saving is not available or the uploaded image has not yet been modified.")
        );
    }
}

void FileOperations::saveImageWithBackup(FilePanel* panel)
{
    if (panel->getMode() != panelMode::Image) return;
    const auto image = panel->getImage();
    if (!image) return;
    const bool use_backups = panel->getSettings()->value("files/make_backups_on_save", true).toBool();

    const auto current_format_id = panel->getLoadedFormat();
    if (current_format_id == "FILE_RAW_MSB") {
        const std::string file_name = image->file_name();
        if (use_backups) {
            const QString qfile_name = QString::fromStdString(file_name);
            if (QFile::exists(qfile_name)) {
                const QFileInfo fileInfo(qfile_name);
                const QString baseName = fileInfo.completeBaseName();
                const QString suffix = fileInfo.suffix();
                const QString dirPath = fileInfo.absolutePath();

                // Find first available backup number
                int backupNum = 1;
                QString backupName;
                while (true) {
                    backupName = dirPath + "/" + baseName + "." + QString::number(backupNum);
                    if (!suffix.isEmpty()) {
                        backupName += "." + suffix;
                    }

                    if (!QFile::exists(backupName)) {
                        break;
                    }
                    backupNum++;
                }

                // Rename old file to backup
                QFile::rename(qfile_name, backupName);
            }
        }
        auto writer = dsk_tools::make_unique<dsk_tools::WriterRAW>(current_format_id, image);
        dsk_tools::BYTES buffer;
        dsk_tools::Result result = writer->write(buffer);
        if (result) {
            UTF8_ofstream file(file_name, std::ios::binary);
            if (file.good()) {
                file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
                panel->getFileSystem()->reset_changed();
                panel->updateImageStatusIndicator();
            }
        }
    }

}

void FileOperations::saveImageAs(FilePanel* panel, QWidget* parent)
{
    if (panel->getMode() != panelMode::Image) return;
    const auto image = panel->getImage();
    dsk_tools::fileSystem* filesystem = panel->getFileSystem();
    if (!image || !filesystem) return;

    QString type_id = panel->getSelectedType();
    QString target_id;
    QString output_file;
    QString template_file;
    int numtracks;
    uint8_t volume_id;

    ConvertDialog dialog(
                        parent,
                        panel->getSettings(),
                        panel->getFileTypes(),
                        panel->getFileFormats(),
                        image,
                        type_id,
                        filesystem->get_volume_id(),
                        panel->currentDir()
    );
    if (dialog.exec(target_id, output_file, template_file, numtracks, volume_id) == QDialog::Accepted) {
        std::unique_ptr<dsk_tools::Writer> writer;

        std::set<QString> mfm_formats = {"FILE_HXC_MFM", "FILE_MFM_NIB", "FILE_MFM_NIC"};

        if (mfm_formats.find(target_id) != mfm_formats.end()) {
            writer = dsk_tools::make_unique<dsk_tools::WriterHxCMFM>(target_id.toStdString(), image, volume_id);
        } else if (target_id == "FILE_HXC_HFE") {
            writer = dsk_tools::make_unique<dsk_tools::WriterHxCHFE>(target_id.toStdString(), image, volume_id);
        } else if (target_id == "FILE_RAW_MSB") {
            writer = dsk_tools::make_unique<dsk_tools::WriterRAW>(target_id.toStdString(), image);
        } else {
            QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("Not implemented!"));
            return;
        }

        dsk_tools::BYTES buffer;
        dsk_tools::Result result = writer->write(buffer);

        if (!result) {
            QMessageBox::critical(parent, FilePanel::tr("Error"), FileOperations::decodeError(result));
            return;
        }

        if (numtracks > 0) {
            dsk_tools::BYTES tmplt;

            UTF8_ifstream tf(template_file.toStdString(), std::ios::binary);
            if (!tf.good()) {
                QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("Error opening template file"));
                return;
            }
            tf.seekg(0, std::ios::end);
            auto tfsize = tf.tellg();
            tf.seekg(0, std::ios::beg);
            tmplt.resize(tfsize);
            tf.read(reinterpret_cast<char*>(tmplt.data()), tfsize);
            if (!tf.good()) {
                QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("Error reading template file"));
                return;
            }

            result = writer->substitute_tracks(buffer, tmplt, numtracks);
            if (!result) {
                if (result.code == dsk_tools::ErrorCode::WriteIncorrectTemplate)
                    QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("The selected template cannot be used - it must be the same type and size as the target."));
                else if (result.code == dsk_tools::ErrorCode::WriteIncorrectSource)
                    QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("Incorrect source data for tracks replacement."));
                else
                    QMessageBox::critical(parent, FilePanel::tr("Error"), FileOperations::decodeError(result));
                return;
            }
        }

        UTF8_ofstream file(output_file.toStdString(), std::ios::binary);

        if (file.good()) {
            file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());

            // Reset the changed flag on successful save
            if (filesystem) {
                filesystem->reset_changed();
                panel->updateImageStatusIndicator();
            }

            QMessageBox::information(parent, FilePanel::tr("Success"),
                FilePanel::tr("File saved successfully"));
        } else {
            QMessageBox::critical(parent, FilePanel::tr("Error"), FilePanel::tr("Error writing file to disk"));
        }
    }
}

QString FileOperations::decodeError(const dsk_tools::Result& result)
{
    QString error;
    switch (result.code) {
        case dsk_tools::ErrorCode::Ok:
            error = QCoreApplication::translate("FilePanel", "No error");
            break;
        case dsk_tools::ErrorCode::NotImplementedYet:
            error = QCoreApplication::translate("FilePanel", "Not implemented yet");
            break;
        case dsk_tools::ErrorCode::FileAddErrorSpace:
            error = QCoreApplication::translate("FilePanel", "No enough free space");
            break;
        case dsk_tools::ErrorCode::FileAddErrorAllocateDirEntry:
            error = QCoreApplication::translate("FilePanel", "Can't allocate a directory entry");
            break;
        case dsk_tools::ErrorCode::FileAddErrorAllocateSector:
            error = QCoreApplication::translate("FilePanel", "Can't allocate a sector");
            break;
        case dsk_tools::ErrorCode::DirNotEmpty:
            error = QCoreApplication::translate("FilePanel", "Directory is not empty");
            break;
        case dsk_tools::ErrorCode::DirErrorSpace:
            error = QCoreApplication::translate("FilePanel", "No enough free space");
            break;
        case dsk_tools::ErrorCode::DirErrorAllocateDirEntry:
            error = QCoreApplication::translate("FilePanel", "Can't allocate a directory entry");
            break;
        case dsk_tools::ErrorCode::DirErrorAllocateSector:
            error = QCoreApplication::translate("FilePanel", "Can't allocate a sector");
            break;
        case dsk_tools::ErrorCode::FileAlreadyExists:
            error = QCoreApplication::translate("FilePanel", "File already exists");
            break;
        case dsk_tools::ErrorCode::DirAlreadyExists:
            error = QCoreApplication::translate("FilePanel", "Directory already exists");
            break;
        case dsk_tools::ErrorCode::DirError:
            error = QCoreApplication::translate("FilePanel", "Error creating a directory");
            break;
        case dsk_tools::ErrorCode::OpenNotLoaded:
            error = QCoreApplication::translate("FilePanel", "Image file is not loaded");
            break;
        case dsk_tools::ErrorCode::OpenBadFormat:
            error = QCoreApplication::translate("FilePanel", "Unrecognized disk format or disk is damaged");
            break;
        case dsk_tools::ErrorCode::LoadError:
            error = QCoreApplication::translate("FilePanel", "Error loading disk image file");
            break;
        default:
            error = QCoreApplication::translate("FilePanel", "Unknown error");
            break;
    }

    if (!result.message.empty()) {
        error += ": " + QString::fromStdString(result.message);
    }

    return error;
}

void FileOperations::showInfoDialog(const std::string& info, QWidget* parent)
{
    auto* file_info = new QDialog(parent);

    Ui_FileInfo fileinfoUi{};
    fileinfoUi.setupUi(file_info);

    const QString text = replacePlaceholders(QString::fromStdString(info));
    QFont font;
#ifdef Q_OS_WIN
    font.setFamily("Consolas");
    font.setPointSize(10);
#else
    font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(10);
#endif
    fileinfoUi.textBox->setFont(font);
    fileinfoUi.textBox->setPlainText(text);

    file_info->exec();
    delete file_info;
}

void FileOperations::putFiles(FilePanel* source, FilePanel* target, QWidget* parent, const dsk_tools::Files & files, const QString & format)
{
    dsk_tools::fileSystem* sourceFs = source->getFileSystem();
    dsk_tools::fileSystem* targetFs = target->getFileSystem();

    if (!sourceFs || !targetFs || !target->allowPutFiles()) return;
   const std::string std_format = format.toStdString();
    foreach (const dsk_tools::UniversalFile & f, files) {
        if (f.is_dir) {
            if (f.name != "..") {
                dsk_tools::UniversalFile new_dir;
                const auto mkdir_result = targetFs->mkdir(f, new_dir);
                if (mkdir_result) {
                    // Getting files
                    dsk_tools::Files dir_files;
                    sourceFs->cd(f);
                    sourceFs->dir(dir_files, false);
                    sourceFs->cd_up();

                    // Putting files
                    targetFs->cd(new_dir);
                    putFiles(source, target, parent, dir_files, format);
                    targetFs->cd_up();
                } else {
                    const QMessageBox::StandardButton res = QMessageBox::critical(
                        parent,
                        FilePanel::tr("Error"),
                        FilePanel::tr("Error creating directory '%1': %2. Continue?").arg(QString::fromUtf8(f.name.c_str()), FileOperations::decodeError(mkdir_result)),
                        QMessageBox::Yes | QMessageBox::No
                    );
                    if (res != QMessageBox::Yes) break;
                }
            }
        } else {
            dsk_tools::BYTES data;
            auto get_result = sourceFs->get_file(f, std_format, data);
            if (get_result) {
                auto put_result = targetFs->put_file(f, std_format, data, false);
                if (!put_result) {
                    // Check if file already exists
                    if (put_result.code == dsk_tools::ErrorCode::FileAlreadyExists) {
                        const QMessageBox::StandardButton res = QMessageBox::question(
                            parent,
                            FilePanel::tr("File exists"),
                            FilePanel::tr("File '%1' already exists. Overwrite?").arg(QString::fromStdString(f.name)),
                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                        );

                        if (res == QMessageBox::Yes) {
                            // Retry with force_replace flag
                            put_result = targetFs->put_file(f, std_format, data, true);
                            if (!put_result) {
                                QString error_message = FileOperations::decodeError(put_result);
                                QMessageBox::critical(parent,
                                    FilePanel::tr("Error"),
                                    FilePanel::tr("Error writing file '%1': %2")
                                            .arg(QString::fromStdString(f.name), error_message)
                                );
                            }
                        } else if (res == QMessageBox::Cancel) {
                            // Stop all operations
                            break;
                        }
                        // If No: continue to next file (implicit - loop continues)
                    } else if (put_result.code == dsk_tools::ErrorCode::NotImplementedYet) {
                        QMessageBox::critical(
                            parent,
                            FilePanel::tr("Error"),
                            FilePanel::tr("Writing for this type of file system is not implemented yet")
                        );
                        break;
                    } else {
                        QString error_message = FileOperations::decodeError(put_result);
                        QMessageBox::critical(parent,
                            FilePanel::tr("Error"),
                            FilePanel::tr("Error writing file '%1': %2")
                                    .arg(QString::fromStdString(f.name), error_message)
                        );
                    }
                }
            } else {
                QMessageBox::critical(
                    parent,
                    FilePanel::tr("Error"),
                    FilePanel::tr("Error reading file '%1'").arg(QString::fromStdString(f.name))
                );
            }
        }
    }
}