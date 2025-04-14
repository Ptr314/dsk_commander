// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for changing file metadata

#include "fileparamdialog.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

using namespace dsk_tools;

FileParamDialog::FileParamDialog(const std::vector<ParameterDescription>& parameters, QWidget* parent)
    : QDialog(parent), parameterList(parameters) {
    setupUi();
    setupTable();
    resize(600, 400);
}

void FileParamDialog::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);

    hexModeToggle = new QCheckBox(FileParamDialog::tr("Hex ($XX)"));
    hexModeToggle->setChecked(true);
    connect(hexModeToggle, &QCheckBox::toggled, this, &FileParamDialog::onHexModeToggled);
    layout->addWidget(hexModeToggle);

    table = new QTableWidget(this);
    layout->addWidget(table);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &FileParamDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &FileParamDialog::reject);
    layout->addWidget(buttons);

    setWindowTitle(FileParamDialog::tr("Edit metadata"));
}

void FileParamDialog::onHexModeToggled(bool checked) {
    std::map<std::string, std::string> values = collectCurrentValues();
    hexMode_ = checked;
    setupTable(values);
}

QString FileParamDialog::formatNumericValue(unsigned long value) const {
    return hexMode_
               ? QString("$%1").arg(value, 0, 16).toUpper()
               : QString::number(value);
}

unsigned long FileParamDialog::parseNumericInput(const QString& input) const {
    QString clean = input.trimmed();
    bool ok = false;

    if (hexMode_ && clean.startsWith("$")) {
        return clean.mid(1).toULong(&ok, 16);
    } else {
        return clean.toULong(&ok, 10);
    }
}

void FileParamDialog::setupTable() {
    std::map<std::string, std::string> values;

    if (initialized_) {
        // Сначала собираем значения ДО очистки таблицы!
        values = collectCurrentValues();
    } else {
        for (const auto& param : parameterList) {
            values[param.id] = param.initialValue;
        }
        initialized_ = true;
    }

    // Теперь безопасно пересоздаём таблицу
    setupTable(values);
}

void FileParamDialog::setupTable(const std::map<std::string, std::string>& overrideValues) {
    table->clear();
    table->setColumnCount(2);
    table->setRowCount(static_cast<int>(parameterList.size()));
    table->setHorizontalHeaderLabels(QStringList() << FileParamDialog::tr("Parameter") << FileParamDialog::tr("Value"));
    table->horizontalHeader()->setStretchLastSection(true);

    table->setColumnWidth(0, 120);

    for (int i = 0; i < static_cast<int>(parameterList.size()); ++i) {
        const auto& param = parameterList[i];
        std::string value = param.initialValue;

        auto it = overrideValues.find(param.id);
        if (it != overrideValues.end()) value = it->second;

        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(param.name)));

        QWidget* editor = nullptr;

        switch (param.type) {
        case ParamType::String: {
            auto* edit = new QLineEdit(this);
            edit->setText(QString::fromStdString(value));
            editor = edit;
            break;
        }

        case ParamType::Byte:
        case ParamType::Word:
        case ParamType::DWord: {
            auto* edit = new QLineEdit(this);
            QRegularExpression regex(hexMode_ ? "^\\$[0-9A-Fa-f]+$" : "^\\d+$");
            edit->setValidator(new QRegularExpressionValidator(regex, edit));

            try {
                unsigned long val = std::stoul(value);
                edit->setText(formatNumericValue(val));
            } catch (...) {
                edit->setText(hexMode_ ? "$0" : "0");
            }

            editor = edit;
            break;
        }

        case ParamType::Enum: {
            auto* combo = new QComboBox(this);
            int selectedIndex = 0;
            for (size_t j = 0; j < param.enumOptions.size(); ++j) {
                const std::pair<std::string, std::string>& entry = param.enumOptions[j];
                const std::string& display = entry.first;
                const std::string& code = entry.second;
                combo->addItem(QString::fromStdString(display), QString::fromStdString(code));
                if (code == value) selectedIndex = static_cast<int>(j);
            }
            combo->setCurrentIndex(selectedIndex);
            editor = combo;
            break;
        }

        case ParamType::Checkbox: {
            auto* checkbox = new QCheckBox(this);
            checkbox->setChecked(value == "true" || value == "1");
            editor = checkbox;
            break;
        }
        }

        table->setCellWidget(i, 1, editor);
    }
}

std::map<std::string, std::string> FileParamDialog::collectCurrentValues() const {
    std::map<std::string, std::string> result;

    for (int i = 0; i < static_cast<int>(parameterList.size()); ++i) {
        const auto& param = parameterList[i];
        QWidget* widget = table->cellWidget(i, 1);
        std::string value;

        if (param.type == ParamType::Byte || param.type == ParamType::Word || param.type == ParamType::DWord) {
            if (auto* hexEdit = qobject_cast<QLineEdit*>(widget)) {
                value = std::to_string(parseNumericInput(hexEdit->text()));
            }
        } else if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
            value = edit->text().toStdString();
        } else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
            value = combo->currentData().toString().toStdString();
        } else if (auto* check = qobject_cast<QCheckBox*>(widget)) {
            value = check->isChecked() ? "true" : "false";
        }

        result[param.id] = value;
    }

    return result;
}

std::map<std::string, std::string> FileParamDialog::getParameters() const {
    return collectCurrentValues();
}
