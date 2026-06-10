#include "services/ProjectStatsService.h"

#include "database/ImageRepository.h"
#include "database/SqlUtils.h"

#include <QSqlDatabase>
#include <QSqlQuery>

namespace {
int scalarInt(const QSqlDatabase& db, const QString& sql)
{
    QSqlQuery query(db);
    if (!SqlUtils::exec(&query, sql, nullptr))
        return 0;
    return query.next() ? query.value(0).toInt() : 0;
}

struct ImageStatusTotals {
    int total = 0;
    int classified = 0;
    int unclassified = 0;
    int conflicts = 0;
    int multiMatch = 0;
    int transparent = 0;
};

ImageStatusTotals summarizeImages(const QVector<ImageRecord>& records)
{
    ImageStatusTotals totals;
    totals.total = records.size();

    for (const ImageRecord& image : records) {
        if (image.status == ImageStatus::Classified)
            ++totals.classified;
        else if (image.status == ImageStatus::Unclassified)
            ++totals.unclassified;
        else if (image.status == ImageStatus::Conflict)
            ++totals.conflicts;
        else if (image.status == ImageStatus::MultiMatch)
            ++totals.multiMatch;

        if (image.hasAlpha)
            ++totals.transparent;
    }

    return totals;
}
}

namespace ProjectStatsService {

QString buildStatsText(const QSqlDatabase& db, const ImageRepository& images)
{
    const QVector<ImageRecord> records = images.fetchAllImages();
    const ImageStatusTotals imageTotals = summarizeImages(records);

    const int rules = scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM rules"));
    const int enabled = scalarInt(db, QStringLiteral("SELECT COUNT(*) FROM rules WHERE enabled=1"));
    const int opaque = imageTotals.total - imageTotals.transparent;

    return QStringLiteral(
        "总图片数：%1\n已分类图片数：%2\n未分类图片数：%3\n冲突图片数：%4\n多重命中图片数：%5\n规则数量：%6\n启用规则数量：%7\n禁用规则数量：%8\n透明图片数量：%9\n不透明图片数量：%10")
        .arg(imageTotals.total).arg(imageTotals.classified).arg(imageTotals.unclassified)
        .arg(imageTotals.conflicts).arg(imageTotals.multiMatch)
        .arg(rules).arg(enabled).arg(rules - enabled).arg(imageTotals.transparent).arg(opaque);
}

}
