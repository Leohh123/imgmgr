#pragma once

#include <QByteArray>
#include <QString>

namespace FileIoUtils {

bool readAll(const QString& path, QByteArray* data, QString* error);
bool writeAll(const QString& path, const QByteArray& data, QString* error);

}
