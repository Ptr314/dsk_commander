#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QStandardItemModel>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>

#include "dsk_tools/disk_image.h"
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

    void on_rightFormatCombo_currentIndexChanged(int index);

    void on_leftTypeCombo_currentIndexChanged(int index);

    void on_actionConvert_triggered();

    void on_toolButton_clicked();

private:
    Ui::MainWindow *ui;

    QString directory;
    QSettings * settings;
    QFileSystemModel leftFilesModel;
    QStandardItemModel rightFilesModel;

    QJsonObject file_formats;
    QJsonObject file_types;
    QJsonObject file_systems;

    dsk_tools::diskImage * image;

    void set_directory(QString new_directory);
    void load_config();
    void init_controls();
    void load_file(QString file_name, QString file_format, QString file_type);
    void process_image();                                                       // Open selecteed image file and list its contents
    void init_table();                                                          // Set columns etc. depending on image type
    void update_table();                                                        // Put file items to the table

    std::vector<dsk_tools::fileData> files;

};
#endif // MAINWINDOW_H
