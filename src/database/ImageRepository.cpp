#include "database/ImageRepository.h"

#include "database/SqlUtils.h"
#include "utils/RuleUtils.h"

#include <QHash>
#include <QQueue>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {
struct ImageQuery {
    QString sql;
    QVariantList binds;
};

QString globForSql(const ImageFilter& filter)
{
    QString pattern = QString(filter.pattern).replace('\\', '/');
    if (!filter.wholeMatch) {
        if (!pattern.startsWith('*'))
            pattern.prepend('*');
        if (!pattern.endsWith('*'))
            pattern.append('*');
    }
    return filter.caseSensitive ? pattern : pattern.toLower();
}

QVector<int> parseRuleIds(const QString& text)
{
    QVector<int> ids;
    const QStringList parts = text.split(',', Qt::SkipEmptyParts);
    ids.reserve(parts.size());
    for (const QString& part : parts) {
        bool ok = false;
        const int id = part.toInt(&ok);
        if (ok)
            ids << id;
    }
    return ids;
}

QVector<int> descendantRuleIds(QSqlDatabase db, int ruleId)
{
    QVector<int> ids;
    QQueue<int> queue;
    queue.enqueue(ruleId);

    while (!queue.isEmpty()) {
        const int parentId = queue.dequeue();
        QSqlQuery q(db);
        q.prepare("SELECT id FROM rules WHERE parent_id=?");
        q.addBindValue(parentId);
        if (!SqlUtils::exec(&q, nullptr))
            continue;
        while (q.next()) {
            const int childId = q.value(0).toInt();
            ids << childId;
            queue.enqueue(childId);
        }
    }
    return ids;
}

bool statusAllowed(const ImageFilter& filter, ImageStatus status)
{
    const bool allSelected = filter.onlyClassified
        && filter.onlyUnclassified
        && filter.onlyConflict
        && filter.onlyMultiMatch;
    if (allSelected)
        return true;
    if (status == ImageStatus::Classified)
        return filter.onlyClassified;
    if (status == ImageStatus::Unclassified)
        return filter.onlyUnclassified;
    if (status == ImageStatus::Conflict)
        return filter.onlyConflict;
    if (status == ImageStatus::MultiMatch)
        return filter.onlyMultiMatch;
    return false;
}

bool filtersByStatusAfterQuery(const ImageFilter& filter)
{
    return !(filter.onlyClassified
        && filter.onlyUnclassified
        && filter.onlyConflict
        && filter.onlyMultiMatch);
}

bool filtersByRegexAfterQuery(const ImageFilter& filter)
{
    return filter.ruleType == RuleUtils::regexRuleType()
        && !filter.pattern.trimmed().isEmpty();
}

bool filtersAfterQuery(const ImageFilter& filter)
{
    return filtersByStatusAfterQuery(filter) || filtersByRegexAfterQuery(filter);
}

QHash<int, int> loadRuleParents(QSqlDatabase db)
{
    QHash<int, int> parentById;
    QSqlQuery rulesQuery(db);
    if (SqlUtils::exec(&rulesQuery, QStringLiteral("SELECT id, parent_id FROM rules"), nullptr)) {
        while (rulesQuery.next())
            parentById.insert(rulesQuery.value(0).toInt(), rulesQuery.value(1).toInt());
    }
    return parentById;
}

ImageRecord imageRecordFromQuery(const QSqlQuery& q)
{
    ImageRecord r;
    r.id = q.value("id").toInt();
    r.absolutePath = q.value("absolute_path").toString();
    r.relativePath = q.value("relative_path").toString();
    r.fileName = q.value("file_name").toString();
    r.fileStem = q.value("file_stem").toString();
    r.parentDir = q.value("parent_dir").toString();
    r.extension = q.value("extension").toString();
    r.fileSize = q.value("file_size").toLongLong();
    r.modifiedTime = q.value("modified_time").toLongLong();
    r.width = q.value("width").toInt();
    r.height = q.value("height").toInt();
    r.hasAlpha = q.value("has_alpha").toInt() != 0;
    r.imageFormat = q.value("image_format").toString();
    r.thumbnailPath = q.value("thumbnail_path").toString();
    r.thumbnailReady = q.value("thumbnail_ready").toInt() != 0;
    r.matchCount = q.value("match_count").toInt();
    return r;
}

ImageStatus statusForMatches(int matchCount, bool hasConflict, const QVector<int>& matchedRuleIds, const QHash<int, int>& parentById)
{
    if (matchCount == 0)
        return ImageStatus::Unclassified;
    if (hasConflict)
        return ImageStatus::Conflict;
    for (int i = 0; i < matchedRuleIds.size(); ++i) {
        for (int j = i + 1; j < matchedRuleIds.size(); ++j) {
            const int a = matchedRuleIds.at(i);
            const int b = matchedRuleIds.at(j);
            if (!RuleUtils::isAncestorRule(parentById, a, b) && !RuleUtils::isAncestorRule(parentById, b, a))
                return ImageStatus::MultiMatch;
        }
    }
    return ImageStatus::Classified;
}

ImageStatus imageStatusFromQuery(const QSqlQuery& q, const QHash<int, int>& parentById)
{
    return statusForMatches(
        q.value("match_count").toInt(),
        q.value("has_conflict").toInt() != 0,
        parseRuleIds(q.value("matched_rule_ids").toString()),
        parentById);
}

ImageRecord imageRecordWithStatus(const QSqlQuery& q, const QHash<int, int>& parentById)
{
    ImageRecord record = imageRecordFromQuery(q);
    record.status = imageStatusFromQuery(q, parentById);
    return record;
}

QString imageColumnForTarget(const QString& matchTarget)
{
    if (matchTarget == RuleUtils::relativePathTarget()) return QStringLiteral("i.relative_path");
    if (matchTarget == RuleUtils::absolutePathTarget()) return QStringLiteral("i.absolute_path");
    if (matchTarget == RuleUtils::parentDirTarget()) return QStringLiteral("i.parent_dir");
    if (matchTarget == RuleUtils::fileNameTarget()) return QStringLiteral("i.file_name");
    return QStringLiteral("i.file_stem");
}

void addPatternFilter(const ImageFilter& filter, QStringList* where, QVariantList* binds)
{
    if (filter.pattern.trimmed().isEmpty() || filter.ruleType != RuleUtils::globRuleType())
        return;

    const QString col = imageColumnForTarget(filter.matchTarget);
    const QString sqlValue = filter.caseSensitive
        ? QString("replace(%1, '\\', '/')").arg(col)
        : QString("lower(replace(%1, '\\', '/'))").arg(col);
    *where << QString("%1 GLOB ?").arg(sqlValue);
    *binds << globForSql(filter);
}

void addCurrentRuleFilter(QSqlDatabase db, const ImageFilter& filter, QStringList* where, QVariantList* binds)
{
    if (filter.currentRuleId <= 0)
        return;

    *where << QStringLiteral("i.id IN (SELECT image_id FROM image_rule_matches WHERE rule_id=?)");
    *binds << filter.currentRuleId;

    if (!filter.onlyCurrentRule)
        return;

    const QVector<int> childRuleIds = descendantRuleIds(db, filter.currentRuleId);
    if (childRuleIds.isEmpty())
        return;

    QStringList placeholders;
    placeholders.reserve(childRuleIds.size());
    for (int i = 0; i < childRuleIds.size(); ++i)
        placeholders << QStringLiteral("?");
    *where << QStringLiteral(
        "i.id NOT IN (SELECT image_id FROM image_rule_matches WHERE rule_id IN (%1))")
        .arg(placeholders.join(','));
    for (int childRuleId : childRuleIds)
        *binds << childRuleId;
}

ImageQuery buildFetchImagesQuery(QSqlDatabase db, const ImageFilter& filter, int limit)
{
    ImageQuery query;
    query.sql =
        "SELECT i.*, COUNT(m.rule_id) AS match_count, MAX(COALESCE(m.is_conflict,0)) AS has_conflict, "
        "GROUP_CONCAT(m.rule_id) AS matched_rule_ids "
        "FROM images i LEFT JOIN image_rule_matches m ON m.image_id=i.id ";

    QStringList where;
    addPatternFilter(filter, &where, &query.binds);
    addCurrentRuleFilter(db, filter, &where, &query.binds);

    if (!where.isEmpty())
        query.sql += " WHERE " + where.join(" AND ");

    query.sql += " GROUP BY i.id";
    query.sql += " ORDER BY i.relative_path";
    if (limit > 0 && !filtersAfterQuery(filter)) {
        query.sql += " LIMIT ?";
        query.binds << limit;
    }
    return query;
}

}

ImageRepository::ImageRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

bool ImageRepository::upsertImages(const QVector<ImageRecord>& records)
{
    if (!m_db.isOpen()) return false;
    QSqlQuery q(m_db);
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    q.prepare(QStringLiteral(
        "INSERT INTO images (absolute_path,relative_path,file_name,file_stem,parent_dir,extension,file_size,modified_time,width,height,has_alpha,image_format,created_at,updated_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(absolute_path) DO UPDATE SET "
        "relative_path=excluded.relative_path,file_name=excluded.file_name,file_stem=excluded.file_stem,parent_dir=excluded.parent_dir,extension=excluded.extension,"
        "file_size=excluded.file_size,modified_time=excluded.modified_time,width=excluded.width,height=excluded.height,"
        "has_alpha=excluded.has_alpha,image_format=excluded.image_format,updated_at=excluded.updated_at,"
        "thumbnail_ready=CASE WHEN images.file_size=excluded.file_size AND images.modified_time=excluded.modified_time THEN images.thumbnail_ready ELSE 0 END"));

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const auto& r : records) {
        q.addBindValue(r.absolutePath);
        q.addBindValue(r.relativePath);
        q.addBindValue(r.fileName);
        q.addBindValue(r.fileStem);
        q.addBindValue(r.parentDir);
        q.addBindValue(r.extension);
        q.addBindValue(r.fileSize);
        q.addBindValue(r.modifiedTime);
        q.addBindValue(r.width);
        q.addBindValue(r.height);
        q.addBindValue(r.hasAlpha ? 1 : 0);
        q.addBindValue(r.imageFormat);
        q.addBindValue(now);
        q.addBindValue(now);
        if (!SqlUtils::exec(&q, &m_lastError)) {
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    return true;
}

QVector<ImageRecord> ImageRepository::fetchImages(const ImageFilter& filter, int limit) const
{
    const QHash<int, int> parentById = loadRuleParents(m_db);
    const ImageQuery imageQuery = buildFetchImagesQuery(m_db, filter, limit);

    QSqlQuery q(m_db);
    q.prepare(imageQuery.sql);
    for (const QVariant& bind : imageQuery.binds)
        q.addBindValue(bind);

    QVector<ImageRecord> out;
    if (!SqlUtils::exec(&q, &m_lastError)) {
        return out;
    }
    while (q.next()) {
        ImageRecord r = imageRecordWithStatus(q, parentById);
        if (!statusAllowed(filter, r.status))
            continue;
        if (filtersByRegexAfterQuery(filter) && !RuleUtils::imageMatchesFilter(r, filter))
            continue;
        out << r;
        if (limit > 0 && out.size() >= limit)
            break;
    }
    return out;
}

QVector<ImageRecord> ImageRepository::fetchAllImages(const ImageFilter& filter) const
{
    return fetchImages(filter, 0);
}

QVector<ImageRecord> ImageRepository::fetchImagesForRuleEvaluation() const
{
    QVector<ImageRecord> out;
    QSqlQuery q(m_db);
    if (!SqlUtils::exec(&q, QStringLiteral(
            "SELECT i.*, 0 AS match_count "
            "FROM images i "
            "ORDER BY i.relative_path"),
            &m_lastError)) {
        return out;
    }

    while (q.next())
        out << imageRecordFromQuery(q);
    return out;
}

ImageRecord ImageRepository::fetchImage(int imageId) const
{
    const QHash<int, int> parentById = loadRuleParents(m_db);
    QSqlQuery q(m_db);
    q.prepare(
        "SELECT i.*, COUNT(m.rule_id) AS match_count, MAX(COALESCE(m.is_conflict,0)) AS has_conflict, "
        "GROUP_CONCAT(m.rule_id) AS matched_rule_ids "
        "FROM images i LEFT JOIN image_rule_matches m ON m.image_id=i.id "
        "WHERE i.id=? GROUP BY i.id");
    q.addBindValue(imageId);
    if (!SqlUtils::exec(&q, &m_lastError)) {
        return {};
    }
    if (!q.next())
        return {};

    return imageRecordWithStatus(q, parentById);
}

bool ImageRepository::updateThumbnail(int imageId, const QString& thumbnailPath, int width, int height)
{
    Q_UNUSED(width)
    Q_UNUSED(height)
    QSqlQuery q(m_db);
    q.prepare("UPDATE images SET thumbnail_path=?, thumbnail_ready=1, updated_at=? WHERE id=?");
    q.addBindValue(thumbnailPath);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(imageId);
    if (!SqlUtils::exec(&q, &m_lastError)) {
        return false;
    }
    return true;
}

int ImageRepository::imageCount() const
{
    QSqlQuery q(m_db);
    if (!SqlUtils::exec(&q, QStringLiteral("SELECT COUNT(*) FROM images"), &m_lastError))
        return 0;
    if (!q.next()) {
        m_lastError = q.lastError().text();
        return 0;
    }
    return q.value(0).toInt();
}
