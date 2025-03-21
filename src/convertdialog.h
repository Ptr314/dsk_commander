#ifndef CONVERTDIALOG_H
#define CONVERTDIALOG_H

#include <QDialog>
#include <QSettings>
#include <QJsonObject>

#include "dsk_tools/dsk_tools.h"

namespace Ui {
class ConvertDialog;
}

class ConvertDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConvertDialog(QWidget *parent = nullptr);
    explicit ConvertDialog(QWidget *parent,
                           QSettings *settings,
                           QJsonObject * file_types,
                           QJsonObject * file_formats,
                           QJsonObject * interleavings,
                           dsk_tools::diskImage * image,
                           const QString & type_id,
                           int fs_volume_id
    );

    ~ConvertDialog();
    QString output_file_name;
    QString template_file_name;
    int exec(QString & target_id, QString & output_file, QString & template_file, int & numtracks, uint8_t & volume_id, QString & interleaving_id);

protected:
    void accept() override;

private slots:
    void on_formatCombo_currentIndexChanged(int index);

    void on_actionChoose_Output_triggered();

    void on_useCheck_checkStateChanged(const Qt::CheckState &arg1);

    void on_actionChoose_Template_triggered();

private:
    Ui::ConvertDialog *ui;

    QString m_type_id;
    QSettings * m_settings;
    QJsonObject * m_file_types;
    QJsonObject * m_file_formats;
    QJsonObject * m_interleavings;
    dsk_tools::diskImage * m_image;
    int m_fs_volume_id;

    void set_output();
    void set_controls();
    void save_setup();


};

#endif // CONVERTDIALOG_H
