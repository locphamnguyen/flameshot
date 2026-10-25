// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include <QTimer>
#include <QWidget>

class BackgroundEditor;
class QCheckBox;
class QLabel;

class BackgroundConf : public QWidget
{
    Q_OBJECT
public:
    explicit BackgroundConf(QWidget* parent = nullptr);

public slots:
    void updateComponents();

private:
    void updatePreview();

    QCheckBox* m_autoApply;
    BackgroundEditor* m_editor;
    QLabel* m_preview;
    QTimer m_saveTimer;
};
