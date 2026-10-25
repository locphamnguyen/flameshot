// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundwindow.h"
#include "core/flameshot.h"
#include "utils/globalvalues.h"
#include "widgets/backgroundeditor.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QVBoxLayout>

namespace {
constexpr int RENDER_DELAY_MS = 40;
constexpr int SAVE_DELAY_MS = 300;
constexpr QSize MIN_PREVIEW_SIZE(480, 320);
constexpr QSize WINDOW_SIZE(1100, 700);
} // namespace

BackgroundWindow::BackgroundWindow(const QPixmap& capture,
                                   const QRect& selection,
                                   const CaptureRequest& request,
                                   QWidget* parent)
  : QWidget(parent)
  , m_capture(capture)
  , m_selection(selection)
  , m_request(request)
  , m_editor(new BackgroundEditor(this))
  , m_preview(new QLabel(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Window);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Background"));
    resize(WINDOW_SIZE);

    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(MIN_PREVIEW_SIZE);
    // Keep the label from growing with the pixmap it shows
    m_preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(m_editor);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumWidth(m_editor->sizeHint().width() + 24);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* buttons = new QHBoxLayout();
    auto addButton = [&](const QString& text, auto slot) {
        auto* button = new QPushButton(text, this);
        connect(button, &QPushButton::clicked, this, slot);
        buttons->addWidget(button);
        return button;
    };
    using CR = CaptureRequest;
    const auto presetTasks =
      CR::COPY | CR::SAVE | CR::PIN | CR::UPLOAD | CR::PRINT_RAW;
    if (m_request.tasks() & presetTasks) {
        // Tasks were requested on the command line, just run them
        addButton(tr("Accept"), [this]() {
            exportWith(CR::NO_TASK);
        })->setDefault(true);
    } else {
        addButton(tr("Copy"), [this]() {
            exportWith(CR::COPY);
        })->setDefault(true);
        addButton(tr("Save"), [this]() { exportWith(CR::SAVE); });
        addButton(tr("Pin"), [this]() { exportWith(CR::PIN); });
        new QShortcut(
          QKeySequence::Copy, this, [this]() { exportWith(CR::COPY); });
        new QShortcut(
          QKeySequence::Save, this, [this]() { exportWith(CR::SAVE); });
    }
    addButton(tr("Close"), [this]() { close(); });
    new QShortcut(Qt::Key_Escape, this, [this]() { close(); });

    auto* side = new QVBoxLayout();
    side->addWidget(scroll);
    side->addLayout(buttons);

    auto* layout = new QHBoxLayout(this);
    layout->addWidget(m_preview, 1);
    layout->addLayout(side);

    m_renderTimer.setSingleShot(true);
    m_renderTimer.setInterval(RENDER_DELAY_MS);
    connect(&m_renderTimer, &QTimer::timeout, this, &BackgroundWindow::render);
    // Remember the last used settings for the next captures
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SAVE_DELAY_MS);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() {
        m_editor->settings().saveToConfig();
    });
    connect(m_editor, &BackgroundEditor::settingsChanged, this, [this]() {
        m_renderTimer.start();
        m_saveTimer.start();
    });

    m_editor->setSettings(BackgroundSettings::fromConfig());
    render();
}

void BackgroundWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updatePreview();
}

void BackgroundWindow::closeEvent(QCloseEvent* event)
{
    if (m_saveTimer.isActive()) {
        m_saveTimer.stop();
        m_editor->settings().saveToConfig();
    }
    if (!m_exported) {
        Flameshot::instance()->setExternalWidget(false);
        emit Flameshot::instance() -> captureFailed();
    }
    QWidget::closeEvent(event);
}

void BackgroundWindow::render()
{
    m_result = BackgroundRenderer::render(m_capture, m_editor->settings());
    updatePreview();
}

void BackgroundWindow::updatePreview()
{
    if (m_result.isNull()) {
        return;
    }
    const qreal dpr = devicePixelRatioF();
    QPixmap preview = m_result.scaled(
      m_preview->size() * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    preview.setDevicePixelRatio(dpr);
    m_preview->setPixmap(preview);
}

void BackgroundWindow::exportWith(CaptureRequest::ExportTask task)
{
    if (m_renderTimer.isActive()) {
        m_renderTimer.stop();
        render();
    }
    CaptureRequest request = m_request;
    request.setOpenBackgroundEditor(false);
    request.setBackgroundMode(CaptureRequest::BACKGROUND_NEVER);
    // The selection geometry was already printed before opening the editor
    request.removeTask(CaptureRequest::PRINT_GEOMETRY);
    if (task != CaptureRequest::NO_TASK) {
        request.addTask(task);
    }
    // Keep a pinned capture centered on the original selection
    QRect geometry = m_selection;
    geometry.setSize(m_result.deviceIndependentSize().toSize());
    geometry.moveCenter(m_selection.center());

    m_exported = true;
    Flameshot::instance()->setExternalWidget(false);
    Flameshot::instance()->exportCapture(m_result, geometry, request);
    close();
}
