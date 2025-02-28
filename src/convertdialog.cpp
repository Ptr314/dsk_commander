#include "convertdialog.h"
#include "ui_convertdialog.h"
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>

ConvertDialog::ConvertDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConvertDialog)
{
    ui->setupUi(this);
}

ConvertDialog::ConvertDialog(QWidget *parent, QSettings * settings, QJsonObject * file_types, QJsonObject * file_formats, QJsonObject *interleavings, dsk_tools::diskImage * image, const QString & type_id):
    ConvertDialog(parent)
{
    m_type_id = type_id;
    m_settings = settings;
    m_file_types = file_types;
    m_file_formats = file_formats;
    m_interleavings = interleavings;
    m_image = image;

    ui->volumeIDEdit->setMaximumWidth(ui->volumeIDEdit->fontMetrics().horizontalAdvance("WWW") + 5);

    QString target_def = m_settings->value("export/target_format", "").toString();

    QJsonObject type = (*m_file_types)[type_id].toObject();

    ui->formatCombo->blockSignals(true);
    ui->formatCombo->clear();
    foreach (const QJsonValue & target_val, type["targets"].toArray()) {
        QString target_id = target_val.toString();
        QJsonObject target = (*m_file_formats)[target_id].toObject();
        ui->formatCombo->addItem(target["short_name"].toString(), target_id);
        if (target_id == target_def) ui->formatCombo->setCurrentIndex(ui->formatCombo->count() - 1);
    }
    ui->formatCombo->blockSignals(false);
    on_formatCombo_currentIndexChanged(ui->formatCombo->currentIndex());

    ui->useCheck->setCheckState((m_settings->value("export/use_tracks", 0).toInt() != 0)?Qt::Checked:Qt::Unchecked);
    ui->tracksCounter->setValue(m_settings->value("export/tracks_count", 1).toInt());

    template_file_name = m_settings->value("export/template", "").toString();
    ui->templateText->setText(template_file_name);

    ui->volumeIDEdit->setText(m_settings->value("export/volume_id", "FE").toString());
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
    QString target_dir = m_settings->value("export/target_directory", fi.dir().absolutePath()).toString();

    QString exts = target["extensions"].toString();
    QStringList exts_list = exts.split(";");
    QString ext = exts_list.at(0).right(exts_list.at(0).size()-2);

    output_file_name = QString("%1/%2.%3").arg(target_dir, fi.completeBaseName(), ext);

    ui->outputText->setText(output_file_name);
}


void ConvertDialog::set_controls()
{
    QString target_id = ui->formatCombo->itemData(ui->formatCombo->currentIndex()).toString();

    if (target_id == "FILE_RAW_MSB") {
        ui->volumeIDGroup->setEnabled(false);
        ui->interleavingGroup->setEnabled(false);
    } else
    if (target_id == "FILE_HXC_MFM") {
        ui->volumeIDGroup->setEnabled(true);
        ui->interleavingGroup->setEnabled(true);
    } else
    if (target_id == "FILE_MFM_NIB") {
        ui->volumeIDGroup->setEnabled(true);
        ui->interleavingGroup->setEnabled(true);
    } else
    if (target_id == "FILE_MFM_NIC") {
        ui->volumeIDGroup->setEnabled(true);
        ui->interleavingGroup->setEnabled(true);
    } else
        QMessageBox::critical(0, ConvertDialog::tr("Error"), ConvertDialog::tr("Configuration error!"));

    if (ui->useCheck->checkState()) {
        ui->tracksCounter->setEnabled(true);
        ui->templateBtn->setEnabled(true);
        ui->templateText->setEnabled(true);
        ui->useLabel->setEnabled(true);
    } else {
        ui->tracksCounter->setEnabled(false);
        ui->templateBtn->setEnabled(false);
        ui->templateText->setEnabled(false);
        ui->useLabel->setEnabled(false);
    }

    ui->interleavingCombo->clear();
    foreach (const QJsonValue & int_obj, (*m_file_types)[m_type_id].toObject()["interleaving"].toArray()) {
        QString int_id = int_obj.toString();
        QString int_name = QCoreApplication::translate("config", (*m_interleavings)[int_id].toObject()["name"].toString().toUtf8().constData());
        ui->interleavingCombo->addItem(int_name, int_id);
    }
}


void ConvertDialog::on_formatCombo_currentIndexChanged(int index)
{
    set_output();
    set_controls();
}

void ConvertDialog::on_actionChoose_Output_triggered()
{
    QString target_id = ui->formatCombo->itemData(ui->formatCombo->currentIndex()).toString();

    QJsonObject target = (*m_file_formats)[target_id].toObject();
    QString exts = target["extensions"].toString();
    QStringList exts_list = exts.split(";");
    QString ext = exts_list.at(0); //
    QString new_ext = ext.right(exts_list.at(0).size()-2);

    QString filter = QString("%1 (%2)").arg(target["name"].toString(), ext);

    QString source_file = QString::fromStdString(m_image->file_name());
    QFileInfo fi(source_file);

    QString file_name = QString("%1/%2.%3").arg(fi.dir().absolutePath(), fi.completeBaseName(), new_ext);

    file_name = QFileDialog::getSaveFileName(this, ConvertDialog::tr("Choose file"), file_name, filter, nullptr, QFileDialog::DontConfirmOverwrite);

    if (file_name.size() > 0) {
        output_file_name = file_name;
        ui->outputText->setText(output_file_name);
    }
}


void ConvertDialog::on_useCheck_checkStateChanged(const Qt::CheckState &arg1)
{
    set_controls();
}


void ConvertDialog::accept()
{
    if (ui->useCheck->isChecked()) {
        // Check is template is expected but not selected
        if (template_file_name.isEmpty()) {
            QMessageBox::critical(this, ConvertDialog::tr("Error"), ConvertDialog::tr("No template file selected."));
            return;
        }

        // Check template type
        QFileInfo tf(template_file_name);
        QString target_id = ui->formatCombo->itemData(ui->formatCombo->currentIndex()).toString();
        QJsonObject target = (*m_file_formats)[target_id].toObject();
        QString exts = target["extensions"].toString();
        QStringList exts_list = exts.split(";");

        bool found = false;
        foreach (const QString & ext, exts_list) {
            if (tf.suffix() == ext.right(ext.size()-2)) {
                found = true;
                break;
            }
        }
        if (!found) {
            QMessageBox::critical(this, ConvertDialog::tr("Error"), ConvertDialog::tr("The template file type must be the same as the selected export format."));
            return;
        }

    }
    QFileInfo fi(output_file_name);
    if (fi.exists()) {
        QMessageBox::StandardButton res = QMessageBox::question(this, ConvertDialog::tr("File exists"), ConvertDialog::tr("File already exists. Overwrite?"));
        if (res != QMessageBox::Yes) return;
    }
    save_setup();
    QDialog::accept();
}


void ConvertDialog::save_setup()
{
    QString target_id = ui->formatCombo->itemData(ui->formatCombo->currentIndex()).toString();
    m_settings->setValue("export/target_format", target_id);

    QFileInfo fi(output_file_name);
    m_settings->setValue("export/target_directory", fi.dir().absolutePath());

    m_settings->setValue("export/use_tracks", (ui->useCheck->checkState() == Qt::Checked)?1:0);
    m_settings->setValue("export/tracks_count", ui->tracksCounter->value());

    m_settings->setValue("export/template", template_file_name);

    m_settings->setValue("export/volume_id", ui->volumeIDEdit->text());
}


void ConvertDialog::on_actionChoose_Template_triggered()
{
    QString dir;
    if (template_file_name.size() > 0) {
        QFileInfo fi(template_file_name);
        dir = fi.dir().absolutePath();
    } else {
        QFileInfo fi(output_file_name);
        dir = fi.dir().absolutePath();
    }

    QString target_id = ui->formatCombo->itemData(ui->formatCombo->currentIndex()).toString();

    QJsonObject target = (*m_file_formats)[target_id].toObject();
    QString exts = target["extensions"].toString().replace(";", " ");
    QString filter = QString("%1 (%2)").arg(target["name"].toString(), exts);

    QString file_name = QFileDialog::getOpenFileName(this, ConvertDialog::tr("Choose template"), dir, filter);

    if (file_name.size() > 0) {
        template_file_name = file_name;
        ui->templateText->setText(template_file_name);
    }
}


int ConvertDialog::exec(QString &target_id, QString & output_file, QString & template_file, int &numtracks, uint8_t &volume_id, QString &interleaving_id)
{
    int res = QDialog::exec();
    if (res == QDialog::Accepted) {
        target_id = ui->formatCombo->itemData(ui->formatCombo->currentIndex()).toString();
        output_file = output_file_name;
        template_file = template_file_name;
        if (ui->useCheck->isChecked())
            numtracks = ui->tracksCounter->value();
        else
            numtracks = 0;
        QString volume_is_str = ui->volumeIDEdit->text();
        if (volume_is_str.size() > 0)
            volume_id = static_cast<uint8_t>(std::stoi(volume_is_str.toStdString(), nullptr, 16));
        else
            volume_id = 0;

        interleaving_id = ui->interleavingCombo->itemData(ui->interleavingCombo->currentIndex()).toString();
    }
    return res;
}
