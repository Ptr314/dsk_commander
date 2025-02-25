#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QStandardItemModel>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include <QComboBox>
#include <qtranslator.h>

#include "dsk_tools/dsk_tools.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionOpen_directory_triggered();

    void on_openDirectoryBtn_clicked();

    void on_leftFilterCombo_currentIndexChanged(int index);

    void on_leftFiles_clicked(const QModelIndex &index);

    void on_leftFiles_doubleClicked(const QModelIndex &index);

    void on_rightFiles_doubleClicked(const QModelIndex &index);

    // void on_rightFormatCombo_currentIndexChanged(int index);

    void on_leftTypeCombo_currentIndexChanged(int index);

    void on_actionConvert_triggered();

    void on_toolButton_clicked();

    void on_autoCheckBox_checkStateChanged(const Qt::CheckState &arg1);

    void on_filesystemCombo_currentIndexChanged(int index);

    void on_actionAbout_triggered();

    void on_actionParent_directory_triggered();

    void on_toolButton_2_clicked();

    void on_actionFile_info_triggered();

    void on_viewButton_clicked();

    void on_actionSave_to_file_triggered();

private:
    Ui::MainWindow *ui;

    QTranslator translator;

    QString directory;
    QSettings * settings;
    QFileSystemModel leftFilesModel;
    QStandardItemModel rightFilesModel;

    QJsonObject file_formats;
    QJsonObject file_types;
    QJsonObject file_systems;
    QJsonObject interleavings;

    dsk_tools::diskImage * image = nullptr;
    dsk_tools::fileSystem * filesystem = nullptr;

    void setComboBoxByItemData(QComboBox* comboBox, const QVariant& value);
    void set_combos(QString format_id, QString type_id, QString filesystem_id);
    void set_directory(QString new_directory);
    void load_config();
    void init_controls();
    void add_languages();
    void switch_language(const QString &lang);
    void load_file(std::string file_name, std::string file_format, std::string file_type, std::string filesystem_type);
    void process_image(std::string filesystem_type);                                                       // Open selecteed image file and list its contents
    void dir();
    void init_table();                                                          // Set columns etc. depending on image type
    void update_table();                                                        // Put file items to the table
    void setup_buttons(bool disabled);

    std::vector<dsk_tools::fileData> files;

};
#endif // MAINWINDOW_H
