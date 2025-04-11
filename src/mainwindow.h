#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QStandardItemModel>
#include <QSettings>
#include <QJsonArray>
#include <QJsonObject>
#include <QComboBox>
#include <QTranslator>
#include <QItemSelection>

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

    void on_leftFilterCombo_currentIndexChanged(int index);

    void on_leftFiles_clicked(const QModelIndex &index);

    void on_leftTypeCombo_currentIndexChanged(int index);

    void on_actionConvert_triggered();

    void on_toolButton_clicked();

    void on_autoCheckBox_checkStateChanged(const Qt::CheckState &arg1);

    void on_filesystemCombo_currentIndexChanged(int index);

    void on_actionAbout_triggered();

    void on_actionParent_directory_triggered();

    void on_toolButton_2_clicked();

    void on_actionFile_info_triggered();

    void on_actionSave_to_file_triggered();

    void on_actionImage_Info_triggered();

    void on_actionOpen_Image_triggered();

    void on_actionView_triggered();

    void on_imageUpBtn_clicked();

    void on_sortBtn_clicked();

    void on_deletedBtn_clicked();

    void on_actionDelete_triggered();

    void on_tabWidget_currentChanged(int index);

    void on_actionAdd_files_triggered();

    void on_actionAdd_directory_triggered();

    void on_actionRename_triggered();

    void onLeftSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onRightSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);


private:
    Ui::MainWindow *ui;

    QTranslator translator;
    QTranslator qtTranslator;

    QString directory;
    QSettings * settings;
    QFileSystemModel leftFilesModel;
    QStandardItemModel rightFilesModel;

    QJsonObject file_formats;
    QJsonObject file_types;
    QJsonObject file_systems;

    dsk_tools::diskImage * image = nullptr;
    dsk_tools::fileSystem * filesystem = nullptr;

    bool is_loaded = false;

    void setComboBoxByItemData(QComboBox* comboBox, const QVariant& value);
    void set_combos(QString format_id, QString type_id, QString filesystem_id);
    void set_directory(QString new_directory);
    void load_config();
    void init_controls();
    void add_languages();
    void switch_language(const QString &lang, bool init);
    void load_file(std::string file_name, std::string file_format, std::string file_type, std::string filesystem_type);
    void process_image(std::string filesystem_type);                                                       // Open selecteed image file and list its contents
    void dir();
    void update_info();
    void init_table();                                                          // Set columns etc. depending on image type
    void update_table();                                                        // Put file items to the table
    void setup_buttons(bool disabled);
    QString replace_placeholders(const QString & in);
    std::vector<dsk_tools::fileData> files;


};
#endif // MAINWINDOW_H
