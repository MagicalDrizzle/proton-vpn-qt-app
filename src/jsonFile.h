#pragma once
// jsonFile.h
// Atomic JSON persistence shared by AppConfig, ConnectionHistory and
// FavoritesManager.
//
// All three used to write straight into the destination file with
// QIODevice::WriteOnly, which truncates it first: a crash, a power loss, or a
// full disk between the truncate and the write left a zero-length or half
// written file, and the user silently lost every setting or every favorite.
// QSaveFile writes to a temporary file in the same directory and renames it
// over the destination only once the content is safely on disk.

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QString>

#include "debug.h"

namespace JsonFile
{

// Writes `doc` to `path`, creating the parent directory if needed.
// Returns false (and logs) when the file could not be written; in that case the
// previous contents of `path` are left untouched.
inline bool write(const QString& path, const QJsonDocument& doc)
{
    const QString dirPath = QFileInfo(path).absolutePath();
    if (QDir().mkpath(dirPath) == false)
    {
        DBG_APP(QStringLiteral("Failed to create directory for ") + path);
        return false;
    }

    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text) == false)
    {
        DBG_APP(QStringLiteral("Failed to open %1 for writing: %2")
                    .arg(path, file.errorString()));
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));

    if (file.commit() == false)
    {
        DBG_APP(QStringLiteral("Failed to write %1: %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

} // namespace JsonFile
