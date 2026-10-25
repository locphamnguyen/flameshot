// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include "utils/backgroundrenderer.h"

#include <QList>
#include <QPair>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QFormLayout;
class QHBoxLayout;
class QListWidget;
class QPushButton;
class QSlider;

/**
 * @brief Controls to edit BackgroundSettings. Emits settingsChanged on every
 * modification, it doesn't write the config itself.
 */
class BackgroundEditor : public QWidget
{
    Q_OBJECT
public:
    explicit BackgroundEditor(QWidget* parent = nullptr);

    BackgroundSettings settings() const;
    void setSettings(const BackgroundSettings& settings);

signals:
    void settingsChanged(const BackgroundSettings& settings);

private:
    QWidget* addSlider(const QString& label,
                       int max,
                       const QString& suffix,
                       int BackgroundSettings::* member,
                       QHBoxLayout** rowLayout = nullptr);
    QPushButton* createColorButton(QColor BackgroundSettings::* member);
    void setRowVisible(QWidget* field, bool visible);
    void updateComponents();
    void refreshGallery();
    void selectDefaultImage();
    void notify();

    BackgroundSettings m_settings;
    bool m_updating = false;

    QFormLayout* m_layout;
    QComboBox* m_type;
    QWidget* m_gradients;
    QButtonGroup* m_gradientGroup;
    QPushButton* m_color;
    QWidget* m_imageRow;
    QListWidget* m_gallery;
    QPushButton* m_removeImage;
    bool m_galleryLoaded = false;
    QWidget* m_blurRow;
    QComboBox* m_aspectRatio;
    QList<QPair<QSlider*, int BackgroundSettings::*>> m_sliders;
    QList<QPair<QPushButton*, QColor BackgroundSettings::*>> m_colorButtons;
};
