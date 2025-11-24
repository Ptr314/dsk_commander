#pragma once

#include <QString>
#include <string>
#include <QComboBox>

// Qt 5.6 compatibility: QOverload was introduced in Qt 5.7
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    template<typename... Args>
    struct QOverload
    {
        template<typename R, typename T>
        static constexpr auto of(R (T::*ptr)(Args...)) -> decltype(ptr) {
            return ptr;
        }
    };
#endif

inline std::string _toStdString(const QString& text) {
    #ifdef _WIN32
        return std::string(text.toLocal8Bit().constData());
    #else
        return std::string(text.toUtf8().constData());
    #endif
}

void adjustComboBoxWidth(QComboBox* comboBox);

std::vector<std::string> get_types_from_map(const std::map<std::string, std::vector<std::string>>& m_subtypes);
