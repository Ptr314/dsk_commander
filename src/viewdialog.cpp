#include <set>

#include "viewdialog.h"
#include "ui_viewdialog.h"

#include "charmaps.h"
#include "bas_tokens.h"

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
    ui->modeCombo->addItem(ViewDialog::tr("Agat BASIC"), "agatbs");
    ui->modeCombo->addItem(ViewDialog::tr("Applesoft BASIC"), "abs");
    ui->modeCombo->addItem(ViewDialog::tr("MBASIC"), "mbasic");
    if (preferred_type == PREFERRED_TEXT)
        ui->modeCombo->setCurrentIndex(1);
    else
    if (preferred_type == PREFERRED_ABS)
        ui->modeCombo->setCurrentIndex(2);
    else
    if (preferred_type == PREFERRED_MBASIC)
        ui->modeCombo->setCurrentIndex(3);
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
        QString cm_name = ui->encodingCombo->currentData().toString();
        if (cm_name == "agat") {
            charmap = &dsk_tools::agat_charmap;
            crlf = {0x8d, 0x0D};
        } else
        if (cm_name == "apple2") {
            charmap = &dsk_tools::apple2_charmap;
            crlf = {0x8d, 0x0D};
        } else
        if (cm_name == "apple2c") {
            charmap = &dsk_tools::apple2c_charmap;
            crlf = {0x8d, 0x0D};
        } else
        if (cm_name == "ascii") {
            charmap = &dsk_tools::ascii_charmap;
            crlf = {0x0D};
            ignore = {0x0A};
            txt_end = {0x1A};
            tab = 8;
        } else {
            // TODO: Other encodings;
        }

        QString out;

        auto mode = ui->modeCombo->currentData();

        if (mode == "bin") {
            QString text = "    ";
            for (int a=0; a < m_data.size(); a++) {
                if (a % 16 == 0)  {
                    if (a != 0) out += text + "\n";
                    out += QString("%1").arg(a, 4, 16, QChar('0')).toUpper() + " ";
                    text = "    ";
                }
                out += QString(" %1").arg(m_data[a], 2, 16, QChar('0')).toUpper();
                text += QString::fromStdString((*charmap)[m_data[a]]);
            }
            out += text;
            ui->textBox->setWordWrapMode(QTextOption::NoWrap);

        } else
        if (mode == "txt") {
            int last_size = 0;
            for (int a=0; a < m_data.size(); a++) {
                uint8_t c = m_data[a];
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
        if (mode == "abs" || mode== "agatbs") {
            int a=0;
            int declared_size = (int)m_data[a] + (int)m_data[a+1]*256; a +=2;
            int lv_size = (int)m_data[declared_size-2] + (int)m_data[declared_size-1]*256;
            auto lv_start = declared_size-2-lv_size-1;

            // out += QString("Size: $%1\n").arg(declared_size, 4, 16);
            // out += QString("LV Size: $%1\n").arg(lv_size, 4, 16);
            // out += QString("LV Start: $%1\n").arg(lv_start, 4, 16);

            // Unpacking long variables
            int vars_count = 0;
            std::vector<std::string> vars;
            if (lv_size > 0) {
                int aa = lv_start;
                std::string var_name = "";
                while (aa < m_data.size() && m_data[aa] != 0) {
                    uint8_t c = m_data[aa++];
                    var_name += static_cast<unsigned char>(c & 0x7F);
                    if (c & 0x80) {
                        vars.push_back(var_name);
                        var_name = "";
                    }
                }
            }

            do {
                std::string line = "";
                int next_addr = (int)m_data[a] + (int)m_data[a+1]*256; a +=2;
                int line_num =  (int)m_data[a] + (int)m_data[a+1]*256; a +=2;
                line += std::to_string(line_num) + " ";
                uint8_t c = m_data[a++];
                if (c == 0) break;
                while (a < m_data.size() && c != 0) {
                    if (c == 0x01 || c == 0x02) {
                        // Long variable link
                        int var_n = (c-1)*256 + m_data[a++];
                        if (var_n <= vars.size())
                            line += vars[var_n-1];
                        else
                            line += "?VAR"+std::to_string(var_n)+"?";
                    } else
                    if (c >= 0x03 && c <= 0x06) {
                        // Unknown special char
                        line += " ??"+std::to_string(c)+"??";
                    } else
                    if (c & 0x80) {
                        // Token
                        auto tokens = (mode=="abs")?dsk_tools::ABS_tokens:dsk_tools::Agat_tokens;
                        line += ((line[line.size()-1] != ' ')?" ":"") + std::string(tokens[c & 0x7F]) +" ";
                    } else
                        // Ordinal char
                        if (cm_name == "agat")
                            line += (*charmap)[c + 0x80];
                        else
                            line += (*charmap)[c];

                    c = m_data[a++];
                }
                line += "\n";
                out += QString::fromStdString(line);
            } while (a < m_data.size());
        } else
        if (mode == "mbasic") {
            // http://justsolve.archiveteam.org/wiki/MBASIC_tokenized_file
            // https://www.chebucto.ns.ca/~af380/GW-BASIC-tokens.html
            int a=0;
            uint8_t is_protected = m_data[a++]; // FF / FE, just skipping

            do {
                std::string line = "";
                int next_addr = (int)m_data[a] + (int)m_data[a+1]*256; a +=2;
                int line_num =  (int)m_data[a] + (int)m_data[a+1]*256; a +=2;
                line += std::to_string(line_num) + " ";
                uint8_t c = m_data[a++];
                if (c == 0) break;
                while (a < m_data.size() && c != 0) {
                    switch (c) {
                        case 0x0B: {
                            // 0B xx xx // Two byte octal constant
                            int16_t v = static_cast<int16_t>(m_data[a] | (m_data[a+1] << 8)); a+=2;
                            line += "&O" + dsk_tools::int_to_octal(v);
                            break;
                        }
                        case 0x0C: {
                            // 0C xx xx // Two byte hexadecimal constant
                            uint16_t v = static_cast<uint16_t>(m_data[a] | (m_data[a+1] << 8)); a+=2;
                            line += "&H" + dsk_tools::int_to_hex(v);
                            break;
                        }
                        case 0x0D:
                            // 0D xx xx // Two byte line number for internal purposes, must not appear in saved files
                            a += 2;
                            break;
                        case 0x0E:{
                            // 0E xx xx // Two byte line number for GOTO and GOSUB
                            uint16_t v = m_data[a] + (m_data[a+1] << 8); a+=2;
                            line += std::to_string(v);
                            break;
                        }
                        case 0x0F:{
                            // 0F xx // One byte constant
                            uint8_t v = m_data[a++];
                            line += std::to_string(v);
                            break;
                        }
                        case 0x10:{
                            // 10 // Should't appear in files
                            break;
                        }
                        case 0x11:
                        case 0x12:
                        case 0x13:
                        case 0x14:
                        case 0x15:
                        case 0x16:
                        case 0x17:
                        case 0x18:
                        case 0x19:
                        case 0x1A:
                        case 0x1B:
                            // Constants 0 to 10
                            line += std::to_string(c -0x11);
                            break;
                        case 0x1C: {
                            // 1C xx xx // Two byte decimal constant
                            int16_t v = static_cast<int16_t>(m_data[a] | (m_data[a+1] << 8)); a+=2;
                            line += std::to_string(v);
                            break;
                        }
                        case 0x1D: {
                            // 1D xx xx  xx xx // Four byte floating constant
                            // TODO: implement
                            a += 4;
                            line += " ?FLOAT4? ";
                            break;
                        }
                        case 0x1E:{
                            // 1E // Should't appear in files
                            break;
                        }
                        case 0x1F: {
                            // 1F xx xx  xx xx xx xx xx xx // Eight byte floating constant
                            // TODO: implement
                            a += 8;
                            line += " ?FLOAT8? ";
                            break;
                        }
                        default:
                            if (c & 0x80)
                                // Token
                                if (c == 0xFF) {
                                    uint8_t cc = m_data[a++] & 0x7F;
                                    // line += ((line[line.size()-1] != ' ')?" ":"") + std::string(dsk_tools::MBASIC_extended_tokens[cc & 0x7F]) +" ";
                                    if (cc < dsk_tools::MBASIC_extended_tokens.size())
                                        line += std::string(dsk_tools::MBASIC_extended_tokens[cc & 0x7F]);
                                    else
                                        line += "?<" + dsk_tools::int_to_hex(cc) + ">?";
                                } else
                                    // line += ((line[line.size()-1] != ' ')?" ":"") + std::string(dsk_tools::MBASIC_main_tokens[c & 0x7F]) +" ";
                                    line += std::string(dsk_tools::MBASIC_main_tokens[c & 0x7F]);
                            else
                            if ((c == 0x3A) && (a+2 < m_data.size()) && (m_data[a] == 0x8F) && (m_data[a+1] == 0xEA)) {
                                // :REM' at the beginning
                                line += "'"; a+=2;
                            } else
                            if ((c == 0x3A) && (a+1 < m_data.size()) && (m_data[a] == 0x9E)) {
                                // :ELSE - just skipping the colon
                            } else {
                                // Ordinal char
                                line += (*charmap)[c];
                            }
                            break;
                    }
                    c = m_data[a++];
                }
                line += "\n";
                out += QString::fromStdString(line);
            } while (a < m_data.size());
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

