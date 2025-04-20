// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Mikhail Revzin <p3.141592653589793238462643@gmail.com>
// Part of the DISK Commander project: https://github.com/Ptr314/dsk_commander
// Description: A QDialog subclass for changing file metadata

#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QCheckBox>
#include <string>
#include <vector>
#include <map>

#include "dsk_tools/dsk_tools.h"

#pragma once

class FileParamDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileParamDialog(const std::vector<dsk_tools::ParameterDescription>& parameters, QWidget* parent = nullptr);
    std::map<std::string, std::string> getParameters() const;

private slots:
    void onHexModeToggled(bool checked);

private:
    void setupUi();
    void setupTable();  // auto-selects between default or collected values
    void setupTable(const std::map<std::string, std::string>& overrideValues);
    QString formatNumericValue(unsigned long value) const;
    unsigned long parseNumericInput(const QString& input) const;
    std::map<std::string, std::string> collectCurrentValues() const;

    std::vector<dsk_tools::ParameterDescription> parameterList;
    QTableWidget* table;
    QCheckBox* hexModeToggle;
    bool hexMode_ = true;
    bool initialized_ = false;
};
