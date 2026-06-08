#include "services/RuleEngine.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QtConcurrent>

RuleEngine::RuleEngine(ImageRepository* images, RuleRepository* rules, QObject* parent)
    : QObject(parent)
    , m_images(images)
    , m_rules(rules)
{
}

bool RuleEngine::isAncestorRule(int possibleAncestorId, int ruleId) const
{
    if (!m_rules || possibleAncestorId == ruleId)
        return false;
    const auto rules = m_rules->fetchRules(false);
    QHash<int, int> parentById;
    for (const auto& rule : rules)
        parentById.insert(rule.id, rule.parentId);
    int current = parentById.value(ruleId, 0);
    while (current != 0) {
        if (current == possibleAncestorId)
            return true;
        current = parentById.value(current, 0);
    }
    return false;
}

bool RuleEngine::isConflictBetweenRules(int ruleA, int ruleB) const
{
    if (!m_rules || ruleA == ruleB)
        return false;
    const auto rules = m_rules->fetchRules(false);
    QHash<int, RuleRecord> byId;
    for (const auto& rule : rules)
        byId.insert(rule.id, rule);
    if (byId.value(ruleA).allowConflict || byId.value(ruleB).allowConflict)
        return false;
    return !isAncestorRule(ruleA, ruleB) && !isAncestorRule(ruleB, ruleA);
}

QVector<int> RuleEngine::matchedRulesForImage(int imageId) const
{
    QVector<int> ids;
    if (!m_images)
        return ids;
    QSqlQuery q(m_images->database());
    q.prepare("SELECT rule_id FROM image_rule_matches WHERE image_id=? ORDER BY rule_id");
    q.addBindValue(imageId);
    q.exec();
    while (q.next())
        ids << q.value(0).toInt();
    return ids;
}

void RuleEngine::recalculate()
{
    if (!m_images || !m_rules) {
        emit failed(QStringLiteral("RuleEngine 未初始化"));
        return;
    }

    QSqlDatabase db = m_images->database();
    const auto images = m_images->fetchImages({});
    const auto rules = m_rules->fetchRules(true);
    QHash<int, RuleRecord> ruleById;
    QHash<int, int> parentById;
    for (const auto& r : rules) {
        ruleById.insert(r.id, r);
        parentById.insert(r.id, r.parentId);
    }

    auto isAncestor = [parentById](int ancestor, int id) {
        int current = parentById.value(id, 0);
        while (current != 0) {
            if (current == ancestor)
                return true;
            current = parentById.value(current, 0);
        }
        return false;
    };

    if (!db.transaction()) {
        emit failed(db.lastError().text());
        return;
    }
    QSqlQuery clear(db);
    if (!clear.exec("DELETE FROM image_rule_matches")) {
        db.rollback();
        emit failed(clear.lastError().text());
        return;
    }

    QSqlQuery insert(db);
    insert.prepare("INSERT INTO image_rule_matches (image_id, rule_id, is_conflict, created_at) VALUES (?,?,?,?)");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int i = 0; i < images.size(); ++i) {
        QVector<int> matched;
        for (const auto& rule : rules) {
            if (matches(targetFor(images.at(i), rule), rule))
                matched << rule.id;
        }
        QSet<int> matchedSet;
        for (int id : matched)
            matchedSet.insert(id);

        QSet<int> conflictRules;
        for (int id : matched) {
            int parent = parentById.value(id, 0);
            while (parent != 0) {
                if (ruleById.contains(parent) && !matchedSet.contains(parent)) {
                    conflictRules.insert(id);
                    break;
                }
                parent = parentById.value(parent, 0);
            }
        }
        for (int a = 0; a < matched.size(); ++a) {
            for (int b = a + 1; b < matched.size(); ++b) {
                const auto ra = ruleById.value(matched[a]);
                const auto rb = ruleById.value(matched[b]);
                const bool allowed = ra.allowConflict || rb.allowConflict
                    || isAncestor(ra.id, rb.id) || isAncestor(rb.id, ra.id);
                if (!allowed) {
                    conflictRules.insert(ra.id);
                    conflictRules.insert(rb.id);
                }
            }
        }
        for (int id : matched) {
            insert.addBindValue(images.at(i).id);
            insert.addBindValue(id);
            insert.addBindValue(conflictRules.contains(id) ? 1 : 0);
            insert.addBindValue(now);
            if (!insert.exec()) {
                db.rollback();
                emit failed(insert.lastError().text());
                return;
            }
        }
        if (i % 100 == 0)
            emit progress(i + 1, images.size());
    }
    if (!db.commit()) {
        emit failed(db.lastError().text());
        return;
    }
    emit progress(images.size(), images.size());
    emit finished();
}

QString RuleEngine::targetFor(const ImageRecord& image, const RuleRecord& rule) const
{
    if (rule.matchTarget == "relative_path") return image.relativePath;
    if (rule.matchTarget == "absolute_path") return image.absolutePath;
    if (rule.matchTarget == "parent_dir") return image.parentDir;
    if (rule.matchTarget == "filename") return image.fileName;
    if (rule.matchTarget == "filename_stem") return image.fileStem;
    return image.fileName;
}

bool RuleEngine::matches(const QString& target, const RuleRecord& rule) const
{
    QString expression;
    if (rule.ruleType == "glob") {
        auto options = rule.wholeMatch
            ? QRegularExpression::DefaultWildcardConversion
            : QRegularExpression::UnanchoredWildcardConversion;
        expression = QRegularExpression::wildcardToRegularExpression(rule.pattern, options);
    } else {
        expression = rule.wholeMatch ? QStringLiteral("\\A(?:%1)\\z").arg(rule.pattern) : rule.pattern;
    }
    QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
    if (!rule.caseSensitive)
        patternOptions |= QRegularExpression::CaseInsensitiveOption;
    QRegularExpression re(expression, patternOptions);
    if (!re.isValid())
        return false;
    if (re.match(target).hasMatch())
        return true;
    const QString normalized = QString(target).replace('\\', '/');
    return normalized != target && re.match(normalized).hasMatch();
}
