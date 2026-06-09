#pragma once

#include <QString>

namespace ProjectPathService {

bool projectWritesWouldTouchResourceDir(const QString& projectDir, const QString& resourceDir);

}
