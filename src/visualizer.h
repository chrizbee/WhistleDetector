#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <QWidget>
#include <vector>

class Visualizer : public QWidget
{
    Q_OBJECT

public:
    Visualizer(QWidget *parent = nullptr);
    void setCutoff(double cutoff);
    void setX(const std::vector<double> &x);
    void setValues(const std::vector<double> &values);

protected:
    void paintEvent(QPaintEvent *event);

private:
    double cutoff_;
    std::vector<double> x_;
    std::vector<double> values_;
};

#endif // VISUALIZER_H
