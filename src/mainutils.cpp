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
