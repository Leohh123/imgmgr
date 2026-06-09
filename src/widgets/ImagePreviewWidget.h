#pragma once

#include "types.h"

#include <QColor>
#include <QImage>
#include <QLabel>
#include <QPoint>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QButtonGroup;
class QPaintEvent;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class PreviewBackgroundViewport;

class PreviewImageLabel : public QLabel {
    Q_OBJECT
public:
    using QLabel::QLabel;
    void setBackgroundPreset(const QColor& color, bool checkerboard);
signals:
    void pixelHovered(const QPoint& pos);
    void dragDelta(const QPoint& delta);
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
private:
    bool m_dragging = false;
    QPoint m_lastGlobalDragPos;
    QColor m_backgroundColor;
    bool m_checkerboardBackground = true;
};

class ImagePreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImagePreviewWidget(QWidget* parent = nullptr);
    void setImage(const ImageRecord& image);
    void setBackgroundColor(const QColor& color);
    void setBackgroundPreset(const QColor& color, bool checkerboard);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void createControls();
    void buildLayout();
    void connectControls();
    void refresh();
    void updatePixelInfo(const QPoint& widgetPos);
    double fitScale() const;
    void applyBackgroundColor();

    ImageRecord m_record;
    QImage m_source;
    QImage m_view;
    QSize m_renderedSize;
    QScrollArea* m_scrollArea = nullptr;
    PreviewBackgroundViewport* m_viewport = nullptr;
    PreviewImageLabel* m_imageLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    QLabel* m_pixelLabel = nullptr;
    QCheckBox* m_r = nullptr;
    QCheckBox* m_g = nullptr;
    QCheckBox* m_b = nullptr;
    QCheckBox* m_a = nullptr;
    QPushButton* m_fitButton = nullptr;
    QPushButton* m_actualSizeButton = nullptr;
    QPushButton* m_zoomInButton = nullptr;
    QPushButton* m_zoomOutButton = nullptr;
    QButtonGroup* m_backgroundGroup = nullptr;
    double m_scale = 1.0;
    bool m_fitToWindow = false;
    QColor m_backgroundColor;
    bool m_checkerboardBackground = true;
};
