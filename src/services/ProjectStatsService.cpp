#include "services/ProjectStatsService.h"

#include "database/ImageRepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>

namespace {
int scalarInt(QSqlQuery& query, const QString& sql)
{
    query.exec(sql);
    return query.next() ? query.value(0).toInt() : 0;
}
}

namespace ProjectStatsService {

QString buildStatsText(const QSqlDatabase& db, const ImageRepository& images)
{
    QSqlQuery query(db);
    const QVector<ImageRecord> records = images.fetchAllImages();

    int classified = 0;
    int unclassified = 0;
    int conflicts = 0;
    int multiMatch = 0;
    for (const ImageRecord& image : records) {
        if (image.status == ImageStatus::Classified)
            ++classified;
        else if (image.status == ImageStatus::Unclassified)
            ++unclassified;
        else if (image.status == ImageStatus::Conflict)
            ++conflicts;
        else if (image.status == ImageStatus::MultiMatch)
            ++multiMatch;
    }

    const int total = records.size();
    const int rules = scalarInt(query, QStringLiteral("SELECT COUNT(*) FROM rules"));
    const int enabled = scalarInt(query, QStringLiteral("SELECT COUNT(*) FROM rules WHERE enabled=1"));
    const int transparent = scalarInt(query, QStringLiteral("SELECT COUNT(*) FROM images WHERE has_alpha=1"));

    return QStringLiteral(
        "总图片数：%1\n已分类图片数：%2\n未分类图片数：%3\n冲突图片数：%4\n多重命中图片数：%5\n规则数量：%6\n启用规则数量：%7\n禁用规则数量：%8\n透明图片数量：%9\n不透明图片数量：%10")
        .arg(total).arg(classified).arg(unclassified).arg(conflicts).arg(multiMatch)
        .arg(rules).arg(enabled).arg(rules - enabled).arg(transparent).arg(total - transparent);
}

}
