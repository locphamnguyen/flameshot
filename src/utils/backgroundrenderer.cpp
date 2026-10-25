// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundrenderer.h"
#include "confighandler.h"

#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

BackgroundSettings BackgroundSettings::fromConfig()
{
    ConfigHandler config;
    BackgroundSettings s;
    s.type = static_cast<Type>(config.backgroundType());
    s.gradientId = config.backgroundGradient();
    s.color = config.backgroundColor();
    s.imagePath = config.backgroundImage();
    s.blur = config.backgroundBlur();
    s.paddingPercent = config.backgroundPadding();
    s.cornerRadius = config.backgroundCornerRadius();
    s.shadow = config.backgroundShadow();
    s.borderWidth = config.backgroundBorderWidth();
    s.borderColor = config.backgroundBorderColor();
    s.aspectRatio = config.backgroundAspectRatio();
    return s;
}

void BackgroundSettings::saveToConfig() const
{
    ConfigHandler config;
    config.setBackgroundType(static_cast<int>(type));
    config.setBackgroundGradient(gradientId);
    config.setBackgroundColor(color);
    config.setBackgroundImage(imagePath);
    config.setBackgroundBlur(blur);
    config.setBackgroundPadding(paddingPercent);
    config.setBackgroundCornerRadius(cornerRadius);
    config.setBackgroundShadow(shadow);
    config.setBackgroundBorderWidth(borderWidth);
    config.setBackgroundBorderColor(borderColor);
    config.setBackgroundAspectRatio(aspectRatio);
}

namespace {

// Gradient presets adapted from beautiFULLshot
// (https://github.com/itsddvn/beautiFULLshot, MIT License,
// Copyright (c) 2025 BeautyFullShot Contributors)
const QList<GradientPreset> GRADIENTS = {
    { "ocean", "Ocean", { "#667eea", "#764ba2" }, false, 135 },
    { "royal", "Royal", { "#141E30", "#243B55" }, false, 180 },
    { "azure", "Azure", { "#0099F7", "#F11712" }, false, 135 },
    { "velvet", "Velvet", { "#DA22FF", "#9733EE" }, false, 135 },
    { "midnight", "Midnight", { "#232526", "#414345" }, false, 180 },
    { "cosmic", "Cosmic", { "#ff00cc", "#333399" }, false, 135 },
    { "sunset", "Sunset", { "#f12711", "#f5af19" }, false, 135 },
    { "sunrise", "Sunrise", { "#FF512F", "#F09819" }, false, 90 },
    { "peach", "Peach", { "#ed4264", "#ffedbc" }, false, 135 },
    { "forest", "Forest", { "#134E5E", "#71B280" }, false, 135 },
    { "mint", "Mint", { "#00b09b", "#96c93d" }, false, 135 },
    { "emerald", "Emerald", { "#348F50", "#56B4D3" }, false, 135 },
    { "slate", "Slate", { "#2C3E50", "#4CA1AF" }, false, 135 },
    { "charcoal", "Charcoal", { "#373B44", "#4286f4" }, false, 135 },
    { "silver", "Silver", { "#bdc3c7", "#2c3e50" }, false, 180 },
    { "rainbow", "Rainbow", { "#f12711", "#f5af19", "#56B4D3" }, false, 90 },
    { "neon", "Neon", { "#12c2e9", "#c471ed", "#f64f59" }, false, 90 },
    { "electric", "Electric", { "#4776E6", "#8E54E9" }, false, 135 },
    { "blush", "Blush", { "#ffecd2", "#fcb69f" }, false, 135 },
    { "lavender", "Lavender", { "#e0c3fc", "#8ec5fc" }, false, 135 },
    { "cream", "Cream", { "#fdfbfb", "#ebedee" }, false, 180 },
    { "obsidian", "Obsidian", { "#000000", "#434343" }, false, 180 },
    { "void", "Void", { "#0f0c29", "#302b63", "#24243e" }, false, 135 },
    { "carbon", "Carbon", { "#1c1c1c", "#383838" }, false, 180 },
};

const QStringList ASPECT_RATIOS = { "auto", "1:1",  "4:3", "3:2",
                                    "16:9", "21:9", "2:1", "1.91:1",
                                    "4:5",  "3:4",  "9:16" };

// Shadow color and offset, relative to the shadow blur radius
constexpr int SHADOW_ALPHA = 140;
constexpr qreal SHADOW_OFFSET_X = 0.15;
constexpr qreal SHADOW_OFFSET_Y = 0.25;
// Blurs are computed on a downscaled image so large radii stay fast
constexpr int BLUR_DOWNSCALE_STEP = 6;
constexpr int BLUR_PASSES = 3;

// One pass of a box blur with edge clamping, on premultiplied ARGB32 pixels
void boxBlurPass(QImage& img, int radius, bool horizontal)
{
    const int w = img.width();
    const int h = img.height();
    const int lines = horizontal ? h : w;
    const int length = horizontal ? w : h;
    const int window = 2 * radius + 1;
    const qsizetype stride = img.bytesPerLine() / 4;
    auto* bits = reinterpret_cast<QRgb*>(img.bits());
    QVector<QRgb> line(length);

    for (int l = 0; l < lines; ++l) {
        QRgb* first = horizontal ? bits + l * stride : bits + l;
        const qsizetype step = horizontal ? 1 : stride;
        for (int i = 0; i < length; ++i) {
            line[i] = first[i * step];
        }
        int a = 0, r = 0, g = 0, b = 0;
        for (int i = -radius; i <= radius; ++i) {
            QRgb c = line[qBound(0, i, length - 1)];
            a += qAlpha(c);
            r += qRed(c);
            g += qGreen(c);
            b += qBlue(c);
        }
        for (int i = 0; i < length; ++i) {
            first[i * step] =
              qRgba(r / window, g / window, b / window, a / window);
            QRgb in = line[qMin(i + radius + 1, length - 1)];
            QRgb out = line[qMax(i - radius, 0)];
            a += qAlpha(in) - qAlpha(out);
            r += qRed(in) - qRed(out);
            g += qGreen(in) - qGreen(out);
            b += qBlue(in) - qBlue(out);
        }
    }
}

// Approximates a gaussian blur of the given radius with stacked box blurs
QImage blurred(const QImage& source, int radius)
{
    if (radius <= 0 || source.isNull()) {
        return source;
    }
    const int scale = qMax(1, radius / BLUR_DOWNSCALE_STEP);
    QImage img = source
                   .scaled(qMax(1, source.width() / scale),
                           qMax(1, source.height() / scale),
                           Qt::IgnoreAspectRatio,
                           Qt::SmoothTransformation)
                   .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int passRadius = qMax(1, radius / scale / 2);
    for (int i = 0; i < BLUR_PASSES; ++i) {
        boxBlurPass(img, passRadius, true);
        boxBlurPass(img, passRadius, false);
    }
    return img.scaled(
      source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

// Scales and crops `image` so it covers `size` while keeping its aspect ratio
QImage coverImage(const QImage& image, const QSize& size)
{
    QSize scaled = image.size().scaled(size, Qt::KeepAspectRatioByExpanding);
    QImage result =
      image.scaled(scaled, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return result.copy((result.width() - size.width()) / 2,
                       (result.height() - size.height()) / 2,
                       size.width(),
                       size.height());
}

// Keeps the last loaded background image so live previews stay fast
QImage loadBackgroundImage(const QString& path)
{
    static QString cachedPath;
    static QDateTime cachedModified;
    static QImage cachedImage;

    QFileInfo info(path);
    if (path.isEmpty() || !info.isFile()) {
        return {};
    }
    if (path != cachedPath || info.lastModified() != cachedModified) {
        cachedPath = path;
        cachedModified = info.lastModified();
        cachedImage = QImage(path);
    }
    return cachedImage;
}

void paintBackdrop(QPainter& painter,
                   const QRect& rect,
                   const BackgroundSettings& s,
                   const QImage& capture,
                   qreal dpr)
{
    switch (s.type) {
        case BackgroundSettings::SOLID:
            painter.fillRect(rect, s.color);
            return;
        case BackgroundSettings::AUTO:
            painter.fillRect(rect,
                             BackgroundRenderer::dominantEdgeColor(capture));
            return;
        case BackgroundSettings::TRANSPARENT:
            return;
        case BackgroundSettings::IMAGE: {
            QImage image = loadBackgroundImage(s.imagePath);
            if (!image.isNull()) {
                image =
                  blurred(coverImage(image, rect.size()), qRound(s.blur * dpr));
                painter.drawImage(rect.topLeft(), image);
                return;
            }
            // Missing image: fall back to the selected gradient
            [[fallthrough]];
        }
        case BackgroundSettings::GRADIENT:
        default:
            BackgroundRenderer::paintGradient(
              painter, rect, BackgroundRenderer::gradientPreset(s.gradientId));
            return;
    }
}

} // namespace

namespace BackgroundRenderer {

const QList<GradientPreset>& gradientPresets()
{
    return GRADIENTS;
}

const GradientPreset& gradientPreset(const QString& id)
{
    for (const GradientPreset& preset : GRADIENTS) {
        if (preset.id == id) {
            return preset;
        }
    }
    return GRADIENTS.first();
}

const QStringList& aspectRatioIds()
{
    return ASPECT_RATIOS;
}

qreal aspectRatioValue(const QString& id)
{
    const QStringList parts = id.split(':');
    if (parts.size() != 2) {
        return 0;
    }
    bool okW = false, okH = false;
    qreal w = parts[0].toDouble(&okW);
    qreal h = parts[1].toDouble(&okH);
    return (okW && okH && w > 0 && h > 0) ? w / h : 0;
}

void paintGradient(QPainter& painter,
                   const QRectF& rect,
                   const GradientPreset& preset)
{
    QGradient* gradient = nullptr;
    QLinearGradient linear;
    QRadialGradient radial;
    if (preset.radial) {
        radial =
          QRadialGradient(rect.center(), qMax(rect.width(), rect.height()) / 2);
        gradient = &radial;
    } else {
        // Same geometry as a CSS linear-gradient(<angle>deg, ...)
        const qreal angle = qDegreesToRadians(static_cast<qreal>(preset.angle));
        const QPointF direction(qSin(angle), -qCos(angle));
        const qreal halfLength = (qAbs(rect.width() * direction.x()) +
                                  qAbs(rect.height() * direction.y())) /
                                 2;
        linear = QLinearGradient(rect.center() - direction * halfLength,
                                 rect.center() + direction * halfLength);
        gradient = &linear;
    }
    const qsizetype last = qMax<qsizetype>(1, preset.colors.size() - 1);
    for (qsizetype i = 0; i < preset.colors.size(); ++i) {
        gradient->setColorAt(static_cast<qreal>(i) / last, preset.colors[i]);
    }
    painter.fillRect(rect, *gradient);
}

QColor dominantEdgeColor(const QImage& image)
{
    if (image.isNull()) {
        return Qt::gray;
    }
    constexpr int sampleSize = 100;
    constexpr int edge = sampleSize / 10;
    QImage sample = image
                      .scaled(sampleSize,
                              sampleSize,
                              Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation)
                      .convertToFormat(QImage::Format_ARGB32);
    qint64 r = 0, g = 0, b = 0, count = 0;
    for (int y = 0; y < sampleSize; ++y) {
        const auto* row =
          reinterpret_cast<const QRgb*>(sample.constScanLine(y));
        const bool edgeRow = y < edge || y >= sampleSize - edge;
        for (int x = 0; x < sampleSize; ++x) {
            if (edgeRow || x < edge || x >= sampleSize - edge) {
                r += qRed(row[x]);
                g += qGreen(row[x]);
                b += qBlue(row[x]);
                ++count;
            }
        }
    }
    return QColor(static_cast<int>(r / count),
                  static_cast<int>(g / count),
                  static_cast<int>(b / count));
}

QPixmap render(const QPixmap& capture, const BackgroundSettings& s)
{
    if (capture.isNull()) {
        return capture;
    }
    // Sizes in the settings are logical pixels, the capture has physical ones
    const qreal dpr = capture.devicePixelRatio();
    const QImage shot =
      capture.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = shot.width();
    const int h = shot.height();
    const int padding = qRound(s.paddingPercent / 100.0 * qMin(w, h));
    const int border = qRound(s.borderWidth * dpr);
    const int shadow = qRound(s.shadow * dpr);
    const qreal radius = qMin<qreal>(s.cornerRadius * dpr, qMin(w, h) / 2.0);

    int outW = w + 2 * padding;
    int outH = h + 2 * padding;
    const qreal ratio = aspectRatioValue(s.aspectRatio);
    if (ratio > 0) {
        if (static_cast<qreal>(outW) / outH < ratio) {
            outW = qRound(outH * ratio);
        } else {
            outH = qRound(outW / ratio);
        }
    }
    const QPoint origin((outW - w) / 2, (outH - h) / 2);
    const QRectF shotRect(origin, QSizeF(w, h));

    QImage out(outW, outH, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    paintBackdrop(painter, out.rect(), s, shot, dpr);

    if (shadow > 0) {
        // Blurred silhouette of the capture, drawn below and slightly offset
        const int margin = 2 * shadow;
        QImage silhouette(
          w + 2 * margin, h + 2 * margin, QImage::Format_ARGB32_Premultiplied);
        silhouette.fill(Qt::transparent);
        QPainter sp(&silhouette);
        sp.setRenderHint(QPainter::Antialiasing);
        sp.setPen(Qt::NoPen);
        sp.setBrush(QColor(0, 0, 0, SHADOW_ALPHA));
        sp.drawRoundedRect(QRectF(margin, margin, w, h), radius, radius);
        sp.end();
        painter.drawImage(
          QPointF(origin.x() - margin + shadow * SHADOW_OFFSET_X,
                  origin.y() - margin + shadow * SHADOW_OFFSET_Y),
          blurred(silhouette, shadow));
    }

    QPainterPath shotPath;
    shotPath.addRoundedRect(shotRect, radius, radius);
    QBrush shotBrush(shot);
    shotBrush.setTransform(QTransform::fromTranslate(origin.x(), origin.y()));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shotBrush);
    painter.drawPath(shotPath);

    if (border > 0) {
        const qreal half = border / 2.0;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(s.borderColor, border));
        painter.drawRoundedRect(shotRect.adjusted(-half, -half, half, half),
                                radius + half,
                                radius + half);
    }
    painter.end();

    QPixmap result = QPixmap::fromImage(out);
    result.setDevicePixelRatio(dpr);
    return result;
}

} // namespace BackgroundRenderer
