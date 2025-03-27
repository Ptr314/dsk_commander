#include "viewdialog.h"
#include "ui_viewdialog.h"

#include "charmaps.h"
#include <set>

ViewDialog::ViewDialog(QWidget *parent, QSettings *settings, const dsk_tools::BYTES &data, int preferred_type, bool deleted)
    : QDialog(parent)
    , ui(new Ui::ViewDialog)
    , m_settings(settings)
{
    m_data = data;
    ui->setupUi(this);

    // QFont font("Iosevka Fixed", 10, 400);
    // font.setStretch(QFont::Expanded);
    QFont font("Consolas", 10, 400);
    ui->textBox->setFont(font);

    ui->modeCombo->blockSignals(true);
    ui->modeCombo->addItem(ViewDialog::tr("Binary"), "bin");
    ui->modeCombo->addItem(ViewDialog::tr("Text"), "txt");
    //ui->modeCombo->addItem(ViewDialog::tr("Applesoft BASIC"), "abs");
    if (preferred_type == PREFERRED_TEXT)
        ui->modeCombo->setCurrentIndex(1);
    ui->modeCombo->blockSignals(false);


    ui->encodingCombo->blockSignals(true);
    ui->encodingCombo->addItem(ViewDialog::tr("Agat"), "agat");
    ui->encodingCombo->addItem(ViewDialog::tr("Apple II"), "apple2");
    ui->encodingCombo->addItem(ViewDialog::tr("Apple //c"), "apple2c");
    ui->encodingCombo->addItem(ViewDialog::tr("ASCII"), "ascii");
    ui->encodingCombo->setCurrentIndex(settings->value("viewer/encoding", 0).toInt());
    ui->encodingCombo->blockSignals(false);

    ui->deletedLabel->setVisible(deleted);

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
    if (m_data.size() != 0) {
        static const std::string (*charmap)[256];

        std::set<uint8_t> crlf;
        std::set<uint8_t> ignore = {};
        std::set<uint8_t> txt_end = {};
        int tab = 0;
        if (ui->encodingCombo->currentData() == "agat") {
            charmap = &dsk_tools::agat_charmap;
            crlf = {0x8d, 0x0D};
        } else
        if (ui->encodingCombo->currentData() == "apple2") {
            charmap = &dsk_tools::apple2_charmap;
            crlf = {0x8d, 0x0D};
        } else
        if (ui->encodingCombo->currentData() == "apple2c") {
            charmap = &dsk_tools::apple2c_charmap;
            crlf = {0x8d, 0x0D};
        } else
        if (ui->encodingCombo->currentData() == "ascii") {
            charmap = &dsk_tools::ascii_charmap;
            crlf = {0x0D};
            ignore = {0x0A};
            txt_end = {0x1A};
            tab = 8;
        } else {
            // TODO: Other encodings;
        }

        QString out;

        if (ui->modeCombo->currentData() == "bin") {
            QString text = "    ";
            for (int a=0; a < m_data.size(); a++) {
                if (a % 16 == 0)  {
                    if (a != 0) out += text + "\n";
                    out += QString("%1").arg(a, 4, 16, QChar('0')).toUpper() + " ";
                    text = "    ";
                }
                out += QString(" %1").arg(m_data.at(a), 2, 16, QChar('0')).toUpper();
                text += QString::fromStdString((*charmap)[m_data.at(a)]);
            }
            out += text;
            ui->textBox->setWordWrapMode(QTextOption::NoWrap);

        } else
        if (ui->modeCombo->currentData() == "txt") {
            int last_size = 0;
            for (int a=0; a < m_data.size(); a++) {
                uint8_t c = m_data.at(a);
                if (ignore.find(c) == ignore.end()) {
                    if (crlf.find(c) != crlf.end()) {
                        out += "\r";
                        last_size = out.size();
                    } else
                    if (tab > 0 && c == 0x09) {
                        int line_size = out.size() - last_size;
                        for (int i=0; i< tab - line_size % tab; i++) out += " ";
                    } else
                    if (txt_end.find(c) != txt_end.end()) {
                        break;
                    } else
                        out += QString::fromStdString((*charmap)[c]);
                }
            }
            ui->textBox->setWordWrapMode(QTextOption::WordWrap);
        } else
        if (ui->modeCombo->currentData() == "abs") {
            // TODO: implement
            int a=0;
            int declared_size = (int)m_data.at(a) + (int)m_data.at(a+1)*256; a +=2;
            out += QString("Size: $%1\n").arg(declared_size, 4, 16);
            int lv_size = (int)m_data.at(declared_size-2) + (int)m_data.at(declared_size-1)*256;
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
    m_settings->setValue("viewer/encoding", ui->encodingCombo->currentIndex());
    print_data();
}

