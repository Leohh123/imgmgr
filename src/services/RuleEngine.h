#pragma once

#include "database/ImageRepository.h"
#include "database/RuleRepository.h"

#include <QObject>
#include <QSet>

class RuleEngine : public QObject {
    Q_OBJECT
public:
    RuleEngine(ImageRepository* images, RuleRepository* rules, QObject* parent = nullptr);

    bool isAncestorRule(int possibleAncestorId, int ruleId) const;
    bool isConflictBetweenRules(int ruleA, int ruleB) const;
    QVector<int> matchedRulesForImage(int imageId) const;

public slots:
    void recalculate();

signals:
    void progress(int current, int total);
    void finished();
    void failed(const QString& error);

private:
    QString targetFor(const ImageRecord& image, const RuleRecord& rule) const;
    bool matches(const QString& target, const RuleRecord& rule) const;

    ImageRepository* m_images = nullptr;
    RuleRepository* m_rules = nullptr;
};
