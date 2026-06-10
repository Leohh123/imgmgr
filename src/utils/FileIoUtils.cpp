#include "utils/FileIoUtils.h"

#include <QFile>
#include <QIODevice>

namespace FileIoUtils {

bool readAll(const QString& path, QByteArray* data, QString* error)
{
    if (data)
        data->clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    const QByteArray content = file.readAll();
    if (file.error() != QFile::NoError) {
        if (error)
            *error = file.errorString();
        return false;
    }

    if (data)
        *data = content;
    return true;
}

bool writeAll(const QString& path, const QByteArray& data, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    if (file.write(data) != data.size()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (!file.flush()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

}
