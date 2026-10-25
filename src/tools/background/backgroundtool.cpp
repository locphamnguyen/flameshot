// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundtool.h"

BackgroundTool::BackgroundTool(QObject* parent)
  : AbstractActionTool(parent)
{}

bool BackgroundTool::closeOnButtonPressed() const
{
    return true;
}

QIcon BackgroundTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "background.svg");
}

QString BackgroundTool::name() const
{
    return tr("Background");
}

CaptureTool::Type BackgroundTool::type() const
{
    return CaptureTool::TYPE_BACKGROUND;
}

QString BackgroundTool::description() const
{
    return tr("Put the capture on a background");
}

CaptureTool* BackgroundTool::copy(QObject* parent)
{
    return new BackgroundTool(parent);
}

void BackgroundTool::pressed(CaptureContext& context)
{
    emit requestAction(REQ_CAPTURE_DONE_OK);
    context.request.setOpenBackgroundEditor(true);
    emit requestAction(REQ_CLOSE_GUI);
}
