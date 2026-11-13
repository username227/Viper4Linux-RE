// Shared file-copy policy for DDC and IRS libraries; UI validates inputs
// and creates the destination directory before calling this helper.
#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace viper_library_import {

// Return the selected library path, or an empty string if the copy failed.
// Selecting a file already in the library must not create an extra copy.
inline QString copyOrSelect(const QString& selected, const QString& directory) {
    const QFileInfo source(selected);
    const QDir library(directory);
    QString destination = library.filePath(source.fileName());
    const QString canonicalSource = source.canonicalFilePath();
    if (!canonicalSource.isEmpty() &&
        QFileInfo(destination).canonicalFilePath() == canonicalSource)
        return destination;

    const QString basename = source.completeBaseName();
    const QString suffix = source.suffix();
    const QString extension = suffix.isEmpty() ? QString() : "." + suffix;
    unsigned serial = 2;
    while (QFile::exists(destination))
        destination = library.filePath(basename + " (" + QString::number(serial++) + ")" + extension);

    return QFile::copy(selected, destination) ? destination : QString();
}

} // namespace viper_library_import
