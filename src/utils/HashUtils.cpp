#include "utils/HashUtils.h"

#include <QCryptographicHash>

QString HashUtils::sha1Hex(const QString& text)
{
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1).toHex());
}
