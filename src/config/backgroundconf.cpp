// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundconf.h"
#include "utils/backgroundrenderer.h"
#include "utils/confighandler.h"
#include "widgets/backgroundeditor.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

namespace {
constexpr int SAVE_DELAY_MS = 300;
constexpr QSize PREVIEW_SIZE(360, 220);

// A fake application window used to preview the background settings
QPixmap sampleCapture()
{
    QPixmap pixmap(480, 300);
    pixmap.fill(QColor(250, 250, 250));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(0, 0, pixmap.width(), 36, QColor(225, 225, 230));
    const QColor dots[] = { "#ff5f57", "#febc2e", "#28c840" };
    for (int i = 0; i < 3; ++i) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(dots[i]);
        painter.drawEllipse(QPoint(20 + i * 20, 18), 6, 6);
    }
    painter.setBrush(QColor(210, 210, 215));
    for (int i = 0; i < 6; ++i) {
        painter.drawRoundedRect(30, 70 + i * 34, i % 2 ? 300 : 420, 14, 7, 7);
    }
    return pixmap;
}
} // namespace

BackgroundConf::BackgroundConf(QWidget* parent)
  : QWidget(parent)
  , m_autoApply(new QCheckBox(
      tr("Put every capture on this background when it is exported"),
      this))
  , m_editor(new BackgroundEditor(this))
  , m_preview(new QLabel(this))
{
    auto* layout = new QVBoxLayout(this);
    m_autoApply->setToolTip(
      tr("Otherwise use the Background button of the capture toolbar or the "
         "--background command line option"));
    layout->addWidget(m_autoApply);

    auto* settingsBox = new QGroupBox(tr("Background settings"), this);
    auto* settingsLayout = new QVBoxLayout(settingsBox);
    settingsLayout->addWidget(m_editor);
    layout->addWidget(settingsBox);

    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(PREVIEW_SIZE);
    layout->addWidget(m_preview);
    layout->addStretch();

    connect(m_autoApply, &QCheckBox::clicked, this, [](bool checked) {
        ConfigHandler().setBackgroundAutoApply(checked);
    });
    // Sliders emit many changes, only write the config once they settle
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SAVE_DELAY_MS);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() {
        m_editor->settings().saveToConfig();
    });
    connect(m_editor, &BackgroundEditor::settingsChanged, this, [this]() {
        m_saveTimer.start();
        updatePreview();
    });

    updateComponents();
}

void BackgroundConf::updateComponents()
{
    // Don't overwrite changes that are still waiting to be saved
    if (m_saveTimer.isActive()) {
        return;
    }
    m_autoApply->setChecked(ConfigHandler().backgroundAutoApply());
    m_editor->setSettings(BackgroundSettings::fromConfig());
    updatePreview();
}

void BackgroundConf::updatePreview()
{
    static const QPixmap sample = sampleCapture();
    QPixmap preview = BackgroundRenderer::render(sample, m_editor->settings());
    m_preview->setPixmap(preview.scaled(
      PREVIEW_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
