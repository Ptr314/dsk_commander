#include "viewdialog.h"
#include "ui_viewdialog.h"

#include "charmaps.h"
#include <set>

ViewDialog::ViewDialog(QWidget *parent, BYTES data, int preferred_type)
    : QDialog(parent)
    , ui(new Ui::ViewDialog)
{
    ui->setupUi(this);

    ui->textBox->setFont(QFont("Consolas", 10, 400));

    ui->modeCombo->addItem(ViewDialog::tr("Binary"), "bin");
    ui->modeCombo->addItem(ViewDialog::tr("Text"), "txt");
    //ui->modeCombo->addItem(ViewDialog::tr("Applesoft BASIC"), "abs");
    if (preferred_type == PREFERRED_TEXT)
        ui->modeCombo->setCurrentIndex(1);


    ui->encodingCombo->addItem("Agat", "agat");

    this->data = data;

    print_data();
}

ViewDialog::~ViewDialog()
{
    delete ui;
}

void ViewDialog::on_closeBtn_clicked()
{
    close();
}

void ViewDialog::print_data()
{
    if (data.size() != 0) {
        static const std::string (*charmap)[256];

        std::set<uint8_t> crlf;
        if (ui->encodingCombo->currentData() == "agat") {
            charmap = &dsk_tools::agat_charmap;
            crlf = {0x8d, 0x13};
        } else {
            // TODO: Other encodings;
        }

        QString out;

        if (ui->modeCombo->currentData() == "bin") {
            QString text = "    ";
            for (int a=0; a < data.size(); a++) {
                if (a % 16 == 0)  {
                    if (a != 0) out += text + "\n";
                    out += QString("%1").arg(a, 4, 16, QChar('0')).toUpper() + " ";
                    text = "    ";
                }
                out += QString(" %1").arg(data.at(a), 2, 16, QChar('0')).toUpper();
                text += QString::fromStdString((*charmap)[data.at(a)]);
            }
            out += text;
            ui->textBox->setWordWrapMode(QTextOption::NoWrap);

        } else
        if (ui->modeCombo->currentData() == "txt") {
            for (int a=0; a < data.size(); a++) {
                char c = (char)data.at(a);
                if (crlf.find(c) != crlf.end())
                    out += "\r";
                else
                    out += QString::fromStdString((*charmap)[data.at(a)]);
            }
            ui->textBox->setWordWrapMode(QTextOption::WordWrap);
        } else
        if (ui->modeCombo->currentData() == "abs") {
            // TODO: implement
            int a=0;
            int declared_size = (int)data.at(a) + (int)data.at(a+1)*256; a +=2;
            out += QString("Size: $%1\n").arg(declared_size, 4, 16);
            int lv_size = (int)data.at(declared_size-2) + (int)data.at(declared_size-1)*256;
            out += QString("LV Size: $%1\n").arg(lv_size, 4, 16);

            auto lv_start = declared_size-2-lv_size-1;
            out += QString("LV Start: $%1\n").arg(lv_start, 4, 16);

            // std::vector<uint8_t> line;
            // do {
            //     line.clear();
            //     uint8_t c = data.at(a++);
            //     while (c != 0 && a < data.size()) {
            //         line.push_back(c);
            //         c = data.at(a++);
            //     }
            //     for (uint8_t cc : line) {
            //         out += QString("%1").arg(cc, 2, 16, QChar('0')).toUpper() + " ";
            //     }
            //     out += "\r";
            // } while (a < data.size());
        }

        ui->textBox->setPlainText(out);
    }
}

void ViewDialog::on_modeCombo_currentIndexChanged(int index)
{
    print_data();
}


void ViewDialog::on_encodingCombo_currentTextChanged(const QString &arg1)
{
    print_data();
}

