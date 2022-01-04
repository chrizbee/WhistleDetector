#include "visualizer.h"
#include <QPainter>

#ifdef VISUALIZE

Visualizer::Visualizer(QWidget *parent) :
    QWidget(parent),
    cutoff_(1.0)
{
}

void Visualizer::setCutoff(double cutoff)
{
    cutoff_ = cutoff ? cutoff : 1.0;
    repaint();
}

void Visualizer::setValues(const std::vector<double> &values)
{
    values_ = values;
    repaint();
}

void Visualizer::setX(const std::vector<double> &x)
{
    x_ = x;
    repaint();
}

void Visualizer::paintEvent(QPaintEvent *event)
{
    // Paint parent and initialize painter
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::gray);
    painter.setPen(Qt::black);

    // Get sizes
    double w = double(width()) / values_.size();
    double h = height();
    double c = h / 2.0;
    double x = 0.0;

    // Paint cutoff line
    painter.drawLine(0, c, width(), c);

    // Paint a bar for each value
    for (const double &value : values_) {
        int y = c * value / cutoff_;
        painter.drawRect(QRectF(x, h - y, w, y));
        x += w;
    }
}

#endif // VISUALIZE
