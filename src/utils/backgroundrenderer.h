// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include <QColor>
#include <QList>
#include <QPixmap>
#include <QString>

/**
 * @brief Settings used to put a capture on top of a decorative background
 * (padding, gradient/solid/image backdrop, rounded corners, shadow, border).
 */
struct BackgroundSettings
{
    enum Type
    {
        GRADIENT = 0,
        SOLID = 1,
        AUTO = 2, // average color of the capture edges
        IMAGE = 3,
        TRANSPARENT = 4,
    };

    Type type = GRADIENT;
    QString gradientId = QStringLiteral("ocean");
    QColor color = Qt::white;
    QString imagePath;
    int blur = 0;           // px, image backgrounds only
    int paddingPercent = 5; // % of the shorter side of the capture
    int cornerRadius = 12;  // px
    int shadow = 30;        // px of shadow blur
    int borderWidth = 0;    // px
    QColor borderColor = Qt::black;
    QString aspectRatio = QStringLiteral("auto");

    static BackgroundSettings fromConfig();
    void saveToConfig() const;
};

struct GradientPreset
{
    QString id;
    QString name;
    QList<QColor> colors;
    bool radial;
    int angle; // CSS-like angle in degrees, linear gradients only
};

namespace BackgroundRenderer {

const QList<GradientPreset>& gradientPresets();
const GradientPreset& gradientPreset(const QString& id);

// Aspect ratio ids accepted by BackgroundSettings::aspectRatio
const QStringList& aspectRatioIds();
// Returns width / height of the ratio id, or 0 for "auto"
qreal aspectRatioValue(const QString& id);

// Fills `rect` of the painter with the gradient preset
void paintGradient(QPainter& painter,
                   const QRectF& rect,
                   const GradientPreset& preset);

QColor dominantEdgeColor(const QImage& image);

QPixmap render(const QPixmap& capture, const BackgroundSettings& settings);

} // namespace BackgroundRenderer
