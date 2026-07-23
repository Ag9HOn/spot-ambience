#pragma once

#include <QQuickPaintedItem>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class QMouseEvent;

class PriceChartItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList points READ points WRITE setPoints NOTIFY pointsChanged)
    Q_PROPERTY(double cheapThreshold READ cheapThreshold WRITE setCheapThreshold NOTIFY thresholdsChanged)
    Q_PROPERTY(double expensiveThreshold READ expensiveThreshold WRITE setExpensiveThreshold NOTIFY thresholdsChanged)
    Q_PROPERTY(int numberFontPixelSize READ numberFontPixelSize WRITE setNumberFontPixelSize NOTIFY numberFontPixelSizeChanged)
    Q_PROPERTY(qint64 xMinimum READ xMinimum WRITE setXMinimum NOTIFY axisRangeChanged)
    Q_PROPERTY(qint64 xMaximum READ xMaximum WRITE setXMaximum NOTIFY axisRangeChanged)
    Q_PROPERTY(QVariantMap selectedDetails READ selectedDetails NOTIFY selectedDetailsChanged)
public:
    explicit PriceChartItem(QQuickItem *parent = nullptr);
    QVariantList points() const;
    void setPoints(const QVariantList &points);
    double cheapThreshold() const;
    void setCheapThreshold(double value);
    double expensiveThreshold() const;
    void setExpensiveThreshold(double value);
    int numberFontPixelSize() const;
    void setNumberFontPixelSize(int value);
    qint64 xMinimum() const;
    void setXMinimum(qint64 value);
    qint64 xMaximum() const;
    void setXMaximum(qint64 value);
    QVariantMap selectedDetails() const;
    Q_INVOKABLE void clearSelection();
    void paint(QPainter *painter) override;

signals:
    void pointsChanged();
    void thresholdsChanged();
    void numberFontPixelSizeChanged();
    void axisRangeChanged();
    void selectedDetailsChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    struct BarHitTarget {
        QRectF rect;
        qint64 start = 0;
    };

    QVariantList m_points;
    QVector<BarHitTarget> m_barHitTargets;
    double m_cheapThreshold = 5.0;
    double m_expensiveThreshold = 15.0;
    int m_numberFontPixelSize = 36;
    qint64 m_xMinimum = 0;
    qint64 m_xMaximum = 0;
    qint64 m_selectedStart = 0;
};
