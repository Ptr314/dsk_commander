#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>

#include "dsk_tools/disk_image.h"

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

private:
    Ui::MainWindow *ui;

    QString directory;
    QSettings settings;
    QFileSystemModel leftFilesModel;

    QJsonArray file_formats;
    QJsonObject file_types;

    dsk_tools::diskImage * image;

    void set_directory(QString new_directory);
    void load_config();
    void init_controls();
    void load_file(QString file_name, QString file_format, QString file_type);

};
#endif // MAINWINDOW_H
