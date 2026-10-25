// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundlibrary.h"
#include "confighandler.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QPainter>

namespace {
const QString BUILT_IN_DIR = QStringLiteral(":/backgrounds");
// Unsplash photo ids are appended to the photographer name: name-ID.jpg
constexpr int UNSPLASH_ID_LENGTH = 11;

QString displayName(const QFileInfo& info, bool builtIn)
{
    QString base = info.completeBaseName();
    if (!builtIn || base.size() <= UNSPLASH_ID_LENGTH + 1) {
        return base;
    }
    QStringList words =
      base.left(base.size() - UNSPLASH_ID_LENGTH - 1).split('-');
    for (QString& word : words) {
        if (!word.isEmpty()) {
            word[0] = word[0].toUpper();
        }
    }
    return QCoreApplication::translate("BackgroundLibrary", "Photo by %1")
      .arg(words.join(' '));
}

QStringList imageFilters()
{
    QStringList filters;
    for (const QByteArray& format : QImageReader::supportedImageFormats()) {
        filters << QStringLiteral("*.") + QString::fromLatin1(format);
    }
    return filters;
}

void appendImages(QList<BackgroundLibrary::Image>& list,
                  const QString& dirPath,
                  bool builtIn)
{
    QDir dir(dirPath);
    const auto entries =
      dir.entryInfoList(imageFilters(), QDir::Files, QDir::Name);
    for (const QFileInfo& info : entries) {
        list.append(
          { info.absoluteFilePath(), displayName(info, builtIn), builtIn });
    }
}
} // namespace

namespace BackgroundLibrary {

QList<Image> images()
{
    QList<Image> list;
    appendImages(list, BUILT_IN_DIR, true);
    appendImages(list, userImageDir(), false);
    return list;
}

QString userImageDir()
{
    QFileInfo config(ConfigHandler().configFilePath());
    return config.absolutePath() + QStringLiteral("/backgrounds");
}

QString importImage(const QString& file)
{
    QFileInfo source(file);
    QDir dir(userImageDir());
    if (!source.isFile() || !dir.mkpath(QStringLiteral("."))) {
        return {};
    }
    // Don't overwrite an image with the same name
    QString target = dir.filePath(source.fileName());
    for (int i = 2; QFileInfo::exists(target); ++i) {
        target = dir.filePath(QStringLiteral("%1-%2.%3")
                                .arg(source.completeBaseName())
                                .arg(i)
                                .arg(source.suffix()));
    }
    return QFile::copy(file, target) ? target : QString();
}

bool isBuiltIn(const QString& path)
{
    return path.startsWith(QLatin1String(":/"));
}

bool removeImage(const QString& path)
{
    return !isBuiltIn(path) && QFile::remove(path);
}

QIcon thumbnail(const QString& path, const QSize& size)
{
    static QHash<QString, QIcon> cache;
    const QString key = QStringLiteral("%1@%2x%3")
                          .arg(path)
                          .arg(size.width())
                          .arg(size.height());
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return *it;
    }
    // Decode a downscaled image (fast for JPEG) that covers the thumbnail
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize scaled = reader.size().scaled(size, Qt::KeepAspectRatioByExpanding);
    if (scaled.isValid()) {
        reader.setScaledSize(scaled);
    }
    QImage image = reader.read();
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    if (!image.isNull()) {
        QPainter painter(&pixmap);
        painter.drawImage(QPoint((size.width() - image.width()) / 2,
                                 (size.height() - image.height()) / 2),
                          image);
    }
    QIcon icon(pixmap);
    cache.insert(key, icon);
    return icon;
}

} // namespace BackgroundLibrary
