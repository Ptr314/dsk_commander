#include <QComboBox>
#include "mainutils.h"

void adjustComboBoxWidth(QComboBox* comboBox) {
    QFontMetrics fm(comboBox->font());
    int maxWidth = 0;
    for (int i = 0; i < comboBox->count(); ++i) {
        #if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
            int width = fm.width(comboBox->itemText(i));
        #else
            int width = fm.horizontalAdvance(comboBox->itemText(i));
        #endif
        if (width > maxWidth)
            maxWidth = width;
    }
    maxWidth += 30;
    comboBox->setMinimumWidth(maxWidth);
}

std::vector<std::string> get_types_from_map(const std::map<std::string, std::vector<std::string>>& m_subtypes) {
    std::vector<std::string> types;
    for (const auto& pair : m_subtypes) {
        types.push_back(pair.first);
    }
    return types;
}
