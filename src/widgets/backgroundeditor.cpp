// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Flameshot Contributors

#include "backgroundeditor.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>

namespace {
constexpr int SWATCH_SIZE = 24;
constexpr int GRADIENT_COLUMNS = 8;

QIcon gradientIcon(const GradientPreset& preset)
{
    QPixmap pixmap(SWATCH_SIZE, SWATCH_SIZE);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(pixmap.rect(), 4, 4);
    painter.setClipPath(clip);
    BackgroundRenderer::paintGradient(painter, pixmap.rect(), preset);
    return QIcon(pixmap);
}

QIcon colorIcon(const QColor& color)
{
    QPixmap pixmap(SWATCH_SIZE, SWATCH_SIZE / 2);
    pixmap.fill(color);
    return QIcon(pixmap);
}
} // namespace

BackgroundEditor::BackgroundEditor(QWidget* parent)
  : QWidget(parent)
  , m_layout(new QFormLayout(this))
  , m_type(new QComboBox(this))
  , m_gradients(new QWidget(this))
  , m_gradientGroup(new QButtonGroup(this))
  , m_imageRow(new QWidget(this))
  , m_imagePath(new QLineEdit(this))
  , m_aspectRatio(new QComboBox(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_type->addItem(tr("Gradient"), BackgroundSettings::GRADIENT);
    m_type->addItem(tr("Solid color"), BackgroundSettings::SOLID);
    m_type->addItem(tr("Auto (capture edge color)"), BackgroundSettings::AUTO);
    m_type->addItem(tr("Image"), BackgroundSettings::IMAGE);
    m_type->addItem(tr("Transparent"), BackgroundSettings::TRANSPARENT);
    m_layout->addRow(tr("Background"), m_type);
    connect(m_type, &QComboBox::currentIndexChanged, this, [this]() {
        m_settings.type =
          static_cast<BackgroundSettings::Type>(m_type->currentData().toInt());
        notify();
    });

    auto* grid = new QGridLayout(m_gradients);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);
    // Native styles barely show the checked state of a tool button
    m_gradients->setStyleSheet(QStringLiteral(
      "QToolButton { border: 2px solid transparent; border-radius: 4px; }"
      "QToolButton:checked { border-color: palette(highlight); }"));
    const auto& presets = BackgroundRenderer::gradientPresets();
    for (qsizetype i = 0; i < presets.size(); ++i) {
        auto* button = new QToolButton(m_gradients);
        button->setCheckable(true);
        button->setIcon(gradientIcon(presets[i]));
        button->setIconSize(QSize(SWATCH_SIZE, SWATCH_SIZE));
        button->setToolTip(presets[i].name);
        m_gradientGroup->addButton(button, static_cast<int>(i));
        grid->addWidget(button,
                        static_cast<int>(i) / GRADIENT_COLUMNS,
                        static_cast<int>(i) % GRADIENT_COLUMNS);
    }
    m_layout->addRow(tr("Gradient"), m_gradients);
    connect(m_gradientGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_settings.gradientId = BackgroundRenderer::gradientPresets()[id].id;
        notify();
    });

    m_color = createColorButton(&BackgroundSettings::color);
    m_layout->addRow(tr("Color"), m_color);

    auto* imageLayout = new QHBoxLayout(m_imageRow);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    m_imagePath->setReadOnly(true);
    m_imagePath->setPlaceholderText(tr("No image selected"));
    auto* browse = new QPushButton(tr("Browse..."), m_imageRow);
    imageLayout->addWidget(m_imagePath);
    imageLayout->addWidget(browse);
    m_layout->addRow(tr("Image"), m_imageRow);
    connect(browse, &QPushButton::clicked, this, [this]() {
        QStringList patterns;
        for (const QByteArray& format : QImageReader::supportedImageFormats()) {
            patterns << QStringLiteral("*.") + QString::fromLatin1(format);
        }
        QString path = QFileDialog::getOpenFileName(
          this,
          tr("Choose a background image"),
          m_settings.imagePath,
          tr("Images") + " (" + patterns.join(' ') + ")");
        if (!path.isEmpty()) {
            m_settings.imagePath = path;
            notify();
        }
    });

    m_blurRow = addSlider(
      tr("Blur"), 100, QStringLiteral(" px"), &BackgroundSettings::blur);
    addSlider(tr("Padding"),
              50,
              QStringLiteral(" %"),
              &BackgroundSettings::paddingPercent);
    addSlider(tr("Corner radius"),
              100,
              QStringLiteral(" px"),
              &BackgroundSettings::cornerRadius);
    addSlider(
      tr("Shadow"), 100, QStringLiteral(" px"), &BackgroundSettings::shadow);
    QHBoxLayout* borderLayout = nullptr;
    addSlider(tr("Border"),
              50,
              QStringLiteral(" px"),
              &BackgroundSettings::borderWidth,
              &borderLayout);
    borderLayout->addWidget(
      createColorButton(&BackgroundSettings::borderColor));

    for (const QString& id : BackgroundRenderer::aspectRatioIds()) {
        m_aspectRatio->addItem(
          id == QLatin1String("auto") ? tr("Auto (fit capture)") : id, id);
    }
    m_layout->addRow(tr("Aspect ratio"), m_aspectRatio);
    connect(m_aspectRatio, &QComboBox::currentIndexChanged, this, [this]() {
        m_settings.aspectRatio = m_aspectRatio->currentData().toString();
        notify();
    });

    updateComponents();
}

BackgroundSettings BackgroundEditor::settings() const
{
    return m_settings;
}

void BackgroundEditor::setSettings(const BackgroundSettings& settings)
{
    m_settings = settings;
    updateComponents();
}

QWidget* BackgroundEditor::addSlider(const QString& label,
                                     int max,
                                     const QString& suffix,
                                     int BackgroundSettings::* member,
                                     QHBoxLayout** rowLayout)
{
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, max);
    auto* value = new QLabel(QStringLiteral("0") + suffix, row);
    value->setMinimumWidth(
      value->fontMetrics().horizontalAdvance(QString::number(max) + suffix));
    layout->addWidget(slider);
    layout->addWidget(value);
    m_layout->addRow(label, row);
    m_sliders.append({ slider, member });

    connect(slider, &QSlider::valueChanged, this, [=, this](int v) {
        value->setText(QString::number(v) + suffix);
        m_settings.*member = v;
        notify();
    });
    if (rowLayout != nullptr) {
        *rowLayout = layout;
    }
    return row;
}

QPushButton* BackgroundEditor::createColorButton(
  QColor BackgroundSettings::* member)
{
    auto* button = new QPushButton(this);
    button->setIconSize(QSize(SWATCH_SIZE, SWATCH_SIZE / 2));
    m_colorButtons.append({ button, member });
    connect(button, &QPushButton::clicked, this, [=, this]() {
        QColor color = QColorDialog::getColor(
          m_settings.*member, this, tr("Choose a color"));
        if (color.isValid()) {
            m_settings.*member = color;
            notify();
        }
    });
    return button;
}

void BackgroundEditor::setRowVisible(QWidget* field, bool visible)
{
    field->setVisible(visible);
    if (QWidget* label = m_layout->labelForField(field)) {
        label->setVisible(visible);
    }
}

void BackgroundEditor::updateComponents()
{
    m_updating = true;
    m_type->setCurrentIndex(m_type->findData(m_settings.type));
    const auto& presets = BackgroundRenderer::gradientPresets();
    const GradientPreset& gradient =
      BackgroundRenderer::gradientPreset(m_settings.gradientId);
    for (qsizetype i = 0; i < presets.size(); ++i) {
        if (presets[i].id == gradient.id) {
            m_gradientGroup->button(static_cast<int>(i))->setChecked(true);
        }
    }
    m_imagePath->setText(m_settings.imagePath);
    for (const auto& [slider, member] : m_sliders) {
        slider->setValue(m_settings.*member);
    }
    for (const auto& [button, member] : m_colorButtons) {
        button->setIcon(colorIcon(m_settings.*member));
        button->setToolTip((m_settings.*member).name());
    }
    int ratio = m_aspectRatio->findData(m_settings.aspectRatio);
    m_aspectRatio->setCurrentIndex(qMax(0, ratio));

    const auto type = m_settings.type;
    setRowVisible(m_gradients, type == BackgroundSettings::GRADIENT);
    setRowVisible(m_color, type == BackgroundSettings::SOLID);
    setRowVisible(m_imageRow, type == BackgroundSettings::IMAGE);
    setRowVisible(m_blurRow, type == BackgroundSettings::IMAGE);
    m_updating = false;
}

void BackgroundEditor::notify()
{
    if (m_updating) {
        return;
    }
    // Refresh swatches and the visible rows
    updateComponents();
    emit settingsChanged(m_settings);
}
