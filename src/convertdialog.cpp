#include "convertdialog.h"
#include "ui_convertdialog.h"
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>

ConvertDialog::ConvertDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConvertDialog)
{
    ui->setupUi(this);
}

ConvertDialog::ConvertDialog(QWidget *parent, QSettings * settings, QJsonObject * file_types, QJsonObject * file_formats, dsk_tools::diskImage * image, const QString & type_id):
    ConvertDialog(parent)
{
    m_type_id = type_id;
    m_settings = settings;
    m_file_types = file_types;
    m_file_formats = file_formats;
    m_image = image;

    ui->volumeIDEdit->setMaximumWidth(ui->volumeIDEdit->fontMetrics().horizontalAdvance("WWW") + 5);

    QString target_def = m_settings->value("directory/target_format", "").toString();

    QJsonObject type = (*m_file_types)[type_id].toObject();

    ui->formatCombo->clear();
    foreach (const QJsonValue & target_val, type["targets"].toArray()) {
        QString target_id = target_val.toString();
        QJsonObject target = (*m_file_formats)[target_id].toObject();
        ui->formatCombo->addItem(target["short_name"].toString(), target_id);
        if (target_id == target_def) ui->formatCombo->setCurrentIndex(ui->formatCombo->count() - 1);
    }
    set_output();
}

ConvertDialog::~ConvertDialog()
{
    delete ui;
}

void ConvertDialog::set_output()
{
    QString target_id = ui->formatCombo->itemData(ui->formatCombo->currentIndex()).toString();
    QJsonObject target = (*m_file_formats)[target_id].toObject();

    QString source_file = QString::fromStdString(m_image->file_name());
    QFileInfo fi(source_file);
    QString target_dir = m_settings->value("directory/target_directory", fi.dir().absolutePath()).toString();

    QString exts = target["extensions"].toString();
    QStringList exts_list = exts.split(";");
    QString ext = exts_list.at(0).right(exts_list.at(0).size()-2);

    output_file_name = QString("%1/%2.%3").arg(target_dir, fi.completeBaseName(), ext);

    qDebug() << output_file_name;




}

void ConvertDialog::on_formatCombo_currentIndexChanged(int index)
{
    QString target_id = ui->formatCombo->itemData(index).toString();
    m_settings->setValue("directory/target_format", target_id);
    set_output();
}

