#pragma once

#include <QString>

class ImageRepository;
class QSqlDatabase;

namespace ProjectStatsService {
QString buildStatsText(const QSqlDatabase& db, const ImageRepository& images);
}
