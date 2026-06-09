#include "widgets/ImagePreviewWidget.h"

#include "utils/ImageUtils.h"
#include "utils/PaintUtils.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPaintEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <QVBoxLayout>
#include <QResizeEvent>

namespace {
void addBackgroundRadio(QHBoxLayout* layout, QButtonGroup* group, QWidget* parent, const QString& text, int id, bool checked = false)
{
    auto* button = new QRadioButton(text, parent);
    button->setChecked(checked);
    group->addButton(button, id);
    layout->addWidget(button);
}

void addVerticalSeparator(QHBoxLayout* layout, QWidget* parent)
{
    auto* separator = new QFrame(parent);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addSpacing(6);
    layout->addWidget(separator);
    layout->addSpacing(6);
}
}

class PreviewBackgroundViewport : public QWidget {
public:
    explicit PreviewBackgroundViewport(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
    }

    void setBackgroundPreset(const QColor& color, bool checkerboard)
    {
        m_backgroundColor = color;
        m_checkerboardBackground = checkerboard;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        PaintUtils::paintBackgroundPreset(&painter, event->rect(), m_backgroundColor, m_checkerboardBackground);
    }

private:
    QColor m_backgroundColor = QApplication::palette().color(QPalette::Highlight);
    bool m_checkerboardBackground = true;
};

void PreviewImageLabel::setBackgroundPreset(const QColor& color, bool checkerboard)
{
    m_backgroundColor = color;
    m_checkerboardBackground = checkerboard;
    update();
}

void PreviewImageLabel::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    PaintUtils::paintBackgroundPreset(&painter, event->rect(), m_backgroundColor, m_checkerboardBackground);
    painter.end();
    QLabel::paintEvent(event);
}

void PreviewImageLabel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastGlobalDragPos = event->globalPosition().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void PreviewImageLabel::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        const QPoint globalPos = event->globalPosition().toPoint();
        const QPoint delta = globalPos - m_lastGlobalDragPos;
        m_lastGlobalDragPos = globalPos;
        emit dragDelta(delta);
        event->accept();
        return;
    }
    emit pixelHovered(event->pos());
    QLabel::mouseMoveEvent(event);
}

void PreviewImageLabel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        unsetCursor();
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

ImagePreviewWidget::ImagePreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    createControls();
    buildLayout();
    connectControls();
}

void ImagePreviewWidget::createControls()
{
    m_imageLabel = new PreviewImageLabel(this);
    m_imageLabel->setMouseTracking(true);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setBackgroundRole(QPalette::Base);
    m_imageLabel->setAutoFillBackground(true);
    m_infoLabel = new QLabel(this);
    m_pixelLabel = new QLabel(this);

    m_r = new QCheckBox("R", this);
    m_g = new QCheckBox("G", this);
    m_b = new QCheckBox("B", this);
    m_a = new QCheckBox("A", this);
    m_r->setChecked(true);
    m_g->setChecked(true);
    m_b->setChecked(true);
    m_a->setChecked(true);

    m_fitButton = new QPushButton(QStringLiteral("适应窗口"), this);
    m_actualSizeButton = new QPushButton(QStringLiteral("原始尺寸"), this);
    m_zoomInButton = new QPushButton(QStringLiteral("放大"), this);
    m_zoomOutButton = new QPushButton(QStringLiteral("缩小"), this);
    m_backgroundGroup = new QButtonGroup(this);

    m_scrollArea = new QScrollArea(this);
    m_viewport = new PreviewBackgroundViewport(m_scrollArea);
    m_scrollArea->setViewport(m_viewport);
    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    applyBackgroundColor();
}

void ImagePreviewWidget::buildLayout()
{
    auto* tools = new QHBoxLayout;
    tools->addWidget(m_fitButton);
    tools->addWidget(m_actualSizeButton);
    tools->addWidget(m_zoomInButton);
    tools->addWidget(m_zoomOutButton);
    addVerticalSeparator(tools, this);
    addBackgroundRadio(tools, m_backgroundGroup, this, QStringLiteral("棋盘格"), 0, true);
    addBackgroundRadio(tools, m_backgroundGroup, this, QStringLiteral("系统"), 1);
    addBackgroundRadio(tools, m_backgroundGroup, this, QStringLiteral("黑色"), 2);
    addBackgroundRadio(tools, m_backgroundGroup, this, QStringLiteral("白色"), 3);
    addBackgroundRadio(tools, m_backgroundGroup, this, QStringLiteral("灰色"), 4);
    tools->addStretch();
    addVerticalSeparator(tools, this);
    tools->addWidget(m_r);
    tools->addWidget(m_g);
    tools->addWidget(m_b);
    tools->addWidget(m_a);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(tools);
    layout->addWidget(m_scrollArea, 1);
    layout->addWidget(m_infoLabel);
    layout->addWidget(m_pixelLabel);
}

void ImagePreviewWidget::connectControls()
{
    auto refreshFn = [this] { refresh(); };
    connect(m_r, &QCheckBox::toggled, this, refreshFn);
    connect(m_g, &QCheckBox::toggled, this, refreshFn);
    connect(m_b, &QCheckBox::toggled, this, refreshFn);
    connect(m_a, &QCheckBox::toggled, this, refreshFn);
    connect(m_fitButton, &QPushButton::clicked, this, [this] { m_fitToWindow = true; refresh(); });
    connect(m_actualSizeButton, &QPushButton::clicked, this, [this] { m_fitToWindow = false; m_scale = 1.0; refresh(); });
    connect(m_zoomInButton, &QPushButton::clicked, this, [this] {
        const double base = m_fitToWindow ? fitScale() : m_scale;
        m_fitToWindow = false;
        m_scale = base * 1.25;
        refresh();
    });
    connect(m_zoomOutButton, &QPushButton::clicked, this, [this] {
        const double base = m_fitToWindow ? fitScale() : m_scale;
        m_fitToWindow = false;
        m_scale = base / 1.25;
        refresh();
    });
    connect(m_backgroundGroup, &QButtonGroup::idClicked, this, [this](int id) {
        switch (id) {
        case 0: setBackgroundPreset(QApplication::palette().color(QPalette::Highlight), true); break;
        case 1: setBackgroundPreset(QApplication::palette().color(QPalette::Highlight), false); break;
        case 2: setBackgroundPreset(QColor(Qt::black), false); break;
        case 3: setBackgroundPreset(QColor(Qt::white), false); break;
        case 4: setBackgroundPreset(QColor(Qt::gray), false); break;
        default: break;
        }
    });
    connect(m_imageLabel, &PreviewImageLabel::pixelHovered, this, &ImagePreviewWidget::updatePixelInfo);
    connect(m_imageLabel, &PreviewImageLabel::dragDelta, this, [this](const QPoint& delta) {
        m_scrollArea->horizontalScrollBar()->setValue(m_scrollArea->horizontalScrollBar()->value() - delta.x());
        m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->value() - delta.y());
    });
}

void ImagePreviewWidget::setBackgroundColor(const QColor& color)
{
    if (!color.isValid())
        return;
    setBackgroundPreset(color, false);
}

void ImagePreviewWidget::setBackgroundPreset(const QColor& color, bool checkerboard)
{
    if (!color.isValid() && !checkerboard)
        return;
    m_backgroundColor = color.isValid() ? color : QApplication::palette().color(QPalette::Highlight);
    m_checkerboardBackground = checkerboard;
    applyBackgroundColor();
    refresh();
}

void ImagePreviewWidget::setImage(const ImageRecord& image)
{
    m_record = image;
    QImageReader reader(image.absolutePath);
    reader.setAutoTransform(true);
    m_source = reader.read();
    m_infoLabel->setText(QStringLiteral("%1 | %2 x %3 | %4 KB | %5")
        .arg(image.relativePath)
        .arg(m_source.width())
        .arg(m_source.height())
        .arg(image.fileSize / 1024.0, 0, 'f', 1)
        .arg(image.imageFormat));
    refresh();
}

void ImagePreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_fitToWindow)
        refresh();
}

void ImagePreviewWidget::refresh()
{
    if (m_source.isNull()) {
        m_imageLabel->clear();
        return;
    }
    m_view = ImageUtils::buildChannelView(m_source, m_r->isChecked(), m_g->isChecked(), m_b->isChecked(), m_a->isChecked());
    QPixmap pix = QPixmap::fromImage(m_view);
    const double scale = m_fitToWindow ? fitScale() : m_scale;
    const QSize scaledSize(qMax(1, int(m_view.width() * scale)), qMax(1, int(m_view.height() * scale)));
    pix = pix.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_renderedSize = pix.size();
    m_imageLabel->setPixmap(pix);
    m_imageLabel->resize(pix.size());
    if (m_fitToWindow) {
        m_scrollArea->horizontalScrollBar()->setValue(0);
        m_scrollArea->verticalScrollBar()->setValue(0);
    }
}

void ImagePreviewWidget::updatePixelInfo(const QPoint& widgetPos)
{
    if (m_source.isNull() || m_renderedSize.isEmpty())
        return;
    const QRect pixmapRect(QPoint(0, 0), m_renderedSize);
    if (!pixmapRect.contains(widgetPos)) {
        m_pixelLabel->clear();
        return;
    }
    const double sx = double(m_source.width()) / qMax(1, pixmapRect.width());
    const double sy = double(m_source.height()) / qMax(1, pixmapRect.height());
    const QPoint imagePos = widgetPos - pixmapRect.topLeft();
    const int x = qBound(0, int(imagePos.x() * sx), m_source.width() - 1);
    const int y = qBound(0, int(imagePos.y() * sy), m_source.height() - 1);
    const QColor c = QColor::fromRgba(m_source.pixel(x, y));
    m_pixelLabel->setText(QStringLiteral("x=%1 y=%2 | R=%3 G=%4 B=%5 A=%6")
        .arg(x).arg(y).arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
}

double ImagePreviewWidget::fitScale() const
{
    if (m_source.isNull() || !m_scrollArea)
        return 1.0;
    const int frame = m_scrollArea->frameWidth() * 2;
    QSize available = m_scrollArea->size() - QSize(frame + 2, frame + 2);
    if (available.width() <= 0 || available.height() <= 0)
        available = m_scrollArea->viewport()->size();
    if (available.width() <= 0 || available.height() <= 0)
        return 1.0;
    const double sx = double(available.width()) / qMax(1, m_source.width());
    const double sy = double(available.height()) / qMax(1, m_source.height());
    return qMax(0.01, qMin(sx, sy));
}

void ImagePreviewWidget::applyBackgroundColor()
{
    auto apply = [this](QWidget* widget) {
        if (!widget)
            return;
        QPalette pal = widget->palette();
        pal.setColor(QPalette::Window, m_backgroundColor);
        pal.setColor(QPalette::Base, m_backgroundColor);
        widget->setPalette(pal);
        widget->setAutoFillBackground(!m_checkerboardBackground);
    };
    apply(m_imageLabel);
    if (m_imageLabel)
        m_imageLabel->setBackgroundPreset(m_backgroundColor, m_checkerboardBackground);
    if (m_viewport)
        m_viewport->setBackgroundPreset(m_backgroundColor, m_checkerboardBackground);
}
