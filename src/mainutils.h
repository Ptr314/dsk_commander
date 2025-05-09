#pragma once

#include <QString>
#include <string>
#include <QComboBox>

inline std::string _toStdString(const QString& text) {
    #ifdef _WIN32
        return std::string(text.toLocal8Bit().constData());
    #else
        return std::string(text.toUtf8().constData());
    #endif
}

void adjustComboBoxWidth(QComboBox* comboBox);

std::vector<std::string> get_types_from_map(const std::map<std::string, std::vector<std::string>>& m_subtypes);
