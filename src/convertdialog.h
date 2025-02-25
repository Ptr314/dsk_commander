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
                           const QString & type_id
    );

    ~ConvertDialog();

private slots:
    void on_formatCombo_currentIndexChanged(int index);

private:
    Ui::ConvertDialog *ui;

    QString m_type_id;
    QSettings * m_settings;
    QJsonObject * m_file_types;
    QJsonObject * m_file_formats;
    QJsonObject * m_interleavings;
    dsk_tools::diskImage * m_image;

    QString output_file_name;

    void set_output();
    void set_controls();


};

#endif // CONVERTDIALOG_H
