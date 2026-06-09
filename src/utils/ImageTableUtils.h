#pragma once

#include <QString>

class QTableView;

namespace ImageTableUtils {
QString columnTitle(int column);
bool isDefaultColumnVisible(int column);
void configureColumns(QTableView* table);
}
