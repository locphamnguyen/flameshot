// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include <QIcon>
#include <QList>
#include <QString>

/**
 * @brief Background images offered by the background editor: the ones bundled
 * in the application resources and the ones the user added from the computer.
 */
namespace BackgroundLibrary {

struct Image
{
    QString path;
    QString name;
    bool builtIn;
};

QList<Image> images();

// Folder where images added by the user are kept, next to the config file
QString userImageDir();

// Copies `file` into the user folder, returns the path of the copy or an empty
// string on failure
QString importImage(const QString& file);

bool isBuiltIn(const QString& path);
bool removeImage(const QString& path);

QIcon thumbnail(const QString& path, const QSize& size);

} // namespace BackgroundLibrary
