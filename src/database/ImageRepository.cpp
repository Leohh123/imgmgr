#include "database/ImageRepository.h"

#include <QHash>
#include <QQueue>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {
QString imageTargetForFilter(const ImageRecord& image, const ImageFilter& filter)
{
    if (filter.matchTarget == "relative_path") return image.relativePath;
    if (filter.matchTarget == "absolute_path") return image.absolutePath;
    if (filter.matchTarget == "parent_dir") return image.parentDir;
    if (filter.matchTarget == "filename_stem") return image.fileStem;
    return image.fileName;
}

QRegularExpression buildRegex(const QString& pattern, const QString& ruleType, bool caseSensitive, bool wholeMatch)
{
    QString expression;
    if (ruleType == "glob") {
        auto options = wholeMatch
            ? QRegularExpression::DefaultWildcardConversion
            : QRegularExpression::UnanchoredWildcardConversion;
        expression = QRegularExpression::wildcardToRegularExpression(pattern, options);
    } else {
        expression = wholeMatch ? QStringLiteral("\\A(?:%1)\\z").arg(pattern) : pattern;
    }
    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (!caseSensitive)
        options |= QRegularExpression::CaseInsensitiveOption;
    return QRegularExpression(expression, options);
}

bool targetMatchesPattern(const QString& target, const ImageFilter& filter)
{
    QRegularExpression re = buildRegex(filter.pattern.trimmed(), filter.ruleType, filter.caseSensitive, filter.wholeMatch);
    if (!re.isValid())
        return false;
    if (re.match(target).hasMatch())
        return true;
    const QString normalized = QString(target).replace('\\', '/');
    return normalized != target && re.match(normalized).hasMatch();
}

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

bool isAncestor(const QHash<int, int>& parentById, int possibleAncestorId, int ruleId)
{
    int current = parentById.value(ruleId, 0);
    while (current != 0) {
        if (current == possibleAncestorId)
            return true;
        current = parentById.value(current, 0);
    }
    return false;
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
        if (!q.exec())
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
        if (!q.exec()) {
            m_lastError = q.lastError().text();
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
    QHash<int, int> parentById;
    QSqlQuery rulesQuery(m_db);
    if (rulesQuery.exec("SELECT id, parent_id FROM rules")) {
        while (rulesQuery.next())
            parentById.insert(rulesQuery.value(0).toInt(), rulesQuery.value(1).toInt());
    }

    QString sql =
        "SELECT i.*, COUNT(m.rule_id) AS match_count, MAX(COALESCE(m.is_conflict,0)) AS has_conflict, "
        "GROUP_CONCAT(m.rule_id) AS matched_rule_ids "
        "FROM images i LEFT JOIN image_rule_matches m ON m.image_id=i.id ";
    QStringList where;
    QVariantList binds;

    if (!filter.pattern.trimmed().isEmpty()) {
        const QString col = filter.matchTarget == "relative_path" ? "i.relative_path"
            : filter.matchTarget == "absolute_path" ? "i.absolute_path"
            : filter.matchTarget == "parent_dir" ? "i.parent_dir"
            : filter.matchTarget == "filename" ? "i.file_name" : "i.file_stem";
        if (filter.ruleType == "glob") {
            const QString sqlValue = filter.caseSensitive
                ? QString("replace(%1, '\\', '/')").arg(col)
                : QString("lower(replace(%1, '\\', '/'))").arg(col);
            where << QString("%1 GLOB ?").arg(sqlValue);
            binds << globForSql(filter);
        }
    }

    if (filter.currentRuleId > 0) {
        where << "i.id IN (SELECT image_id FROM image_rule_matches WHERE rule_id=?)";
        binds << filter.currentRuleId;
        if (filter.onlyCurrentRule) {
            const QVector<int> childRuleIds = descendantRuleIds(m_db, filter.currentRuleId);
            if (!childRuleIds.isEmpty()) {
                QStringList placeholders;
                placeholders.reserve(childRuleIds.size());
                for (int i = 0; i < childRuleIds.size(); ++i)
                    placeholders << "?";
                where << QStringLiteral(
                    "i.id NOT IN (SELECT image_id FROM image_rule_matches WHERE rule_id IN (%1))")
                    .arg(placeholders.join(','));
                for (int childRuleId : childRuleIds)
                    binds << childRuleId;
            }
        }
    }

    if (!where.isEmpty())
        sql += " WHERE " + where.join(" AND ");

    sql += " GROUP BY i.id";
    sql += " ORDER BY i.relative_path LIMIT ?";
    binds << limit;

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const QVariant& bind : binds)
        q.addBindValue(bind);

    QVector<ImageRecord> out;
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return out;
    }
    while (q.next()) {
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
        r.status = statusForMatches(r.matchCount,
            q.value("has_conflict").toInt() != 0,
            parseRuleIds(q.value("matched_rule_ids").toString()),
            parentById);
        if (!statusAllowed(filter, r.status))
            continue;
        out << r;
    }

    if (filter.ruleType == "regex" && !filter.pattern.trimmed().isEmpty()) {
        QVector<ImageRecord> filtered;
        for (const auto& r : out) {
            if (targetMatchesPattern(imageTargetForFilter(r, filter), filter))
                filtered << r;
        }
        return filtered;
    }
    return out;
}

ImageRecord ImageRepository::fetchImage(int imageId) const
{
    ImageFilter f;
    auto images = fetchImages(f);
    for (const auto& image : images) {
        if (image.id == imageId)
            return image;
    }
    return {};
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
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

int ImageRepository::imageCount() const
{
    QSqlQuery q("SELECT COUNT(*) FROM images", m_db);
    return q.next() ? q.value(0).toInt() : 0;
}

ImageStatus ImageRepository::statusForMatches(int matchCount, bool hasConflict, const QVector<int>& matchedRuleIds, const QHash<int, int>& parentById) const
{
    if (matchCount == 0) return ImageStatus::Unclassified;
    if (hasConflict) return ImageStatus::Conflict;
    for (int i = 0; i < matchedRuleIds.size(); ++i) {
        for (int j = i + 1; j < matchedRuleIds.size(); ++j) {
            const int a = matchedRuleIds.at(i);
            const int b = matchedRuleIds.at(j);
            if (!isAncestor(parentById, a, b) && !isAncestor(parentById, b, a))
                return ImageStatus::MultiMatch;
        }
    }
    return ImageStatus::Classified;
}
