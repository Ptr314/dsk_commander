#ifndef VIEWDIALOG_H
#define VIEWDIALOG_H

#include <QDialog>
#include <QSettings>

#include "dsk_tools/dsk_tools.h"

namespace Ui {
class ViewDialog;
}

class ViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewDialog(QWidget *parent, QSettings  *settings, const dsk_tools::BYTES &data, int preferred_type);
    ~ViewDialog();

private slots:
    void on_closeBtn_clicked();

    void on_modeCombo_currentIndexChanged(int index);

    void on_encodingCombo_currentTextChanged(const QString &arg1);

private:
    Ui::ViewDialog *ui;

    dsk_tools::BYTES m_data;
    QSettings *m_settings;
    void print_data();
};

#endif // VIEWDIALOG_H
