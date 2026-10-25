// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#pragma once

#include "core/capturerequest.h"
#include "utils/backgroundrenderer.h"

#include <QPixmap>
#include <QTimer>
#include <QWidget>

class BackgroundEditor;
class QLabel;

/**
 * @brief Window to put a capture on a background with a live preview, then
 * copy, save or pin the result.
 */
class BackgroundWindow : public QWidget
{
    Q_OBJECT
public:
    BackgroundWindow(const QPixmap& capture,
                     const QRect& selection,
                     const CaptureRequest& request,
                     QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void render();
    void updatePreview();
    void exportWith(CaptureRequest::ExportTask task);

    QPixmap m_capture;
    QPixmap m_result;
    QRect m_selection;
    CaptureRequest m_request;
    bool m_exported = false;

    BackgroundEditor* m_editor;
    QLabel* m_preview;
    QTimer m_renderTimer;
    QTimer m_saveTimer;
};
