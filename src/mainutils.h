#ifndef MAINUTILS_H
#define MAINUTILS_H

#include <QString>
#include <string>

inline std::string _toStdString(const QString& text) {
    #ifdef _WIN32
        return std::string(text.toLocal8Bit().constData());
    #else
        return std::string(text.toUtf8().constData());
    #endif
}


#endif // MAINUTILS_H
