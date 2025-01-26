#ifndef VIEWDIALOG_H
#define VIEWDIALOG_H

#include <QDialog>

namespace Ui {
class ViewDialog;
}

class ViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ViewDialog(QWidget *parent, std::vector<uint8_t> data);
    ~ViewDialog();

private slots:
    void on_closeBtn_clicked();

    void on_modeCombo_currentIndexChanged(int index);

    void on_encodingCombo_currentTextChanged(const QString &arg1);

private:
    Ui::ViewDialog *ui;

    std::vector<uint8_t> data;
    void print_data();
};

#endif // VIEWDIALOG_H
