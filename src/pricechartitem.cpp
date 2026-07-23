#include "pricechartitem.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <limits>
#include <QtMath>

PriceChartItem::PriceChartItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::LeftButton);
}

QVariantList PriceChartItem::points() const { return m_points; }
void PriceChartItem::setPoints(const QVariantList &points)
{
    if (m_points == points) return;
    m_points = points;
    if (m_selectedStart != 0) {
        bool selectionStillExists = false;
        for (const QVariant &entry : m_points) {
            if (entry.toMap().value(QStringLiteral("x")).toLongLong() == m_selectedStart) {
                selectionStillExists = true;
                break;
            }
        }
        if (!selectionStillExists)
            m_selectedStart = 0;
    }
    emit pointsChanged();
    emit selectedDetailsChanged();
    update();
}
double PriceChartItem::cheapThreshold() const { return m_cheapThreshold; }
void PriceChartItem::setCheapThreshold(double value)
{
    if (qFuzzyCompare(m_cheapThreshold, value)) return;
    m_cheapThreshold = value;
    emit thresholdsChanged();
    update();
}
double PriceChartItem::expensiveThreshold() const { return m_expensiveThreshold; }
void PriceChartItem::setExpensiveThreshold(double value)
{
    if (qFuzzyCompare(m_expensiveThreshold, value)) return;
    m_expensiveThreshold = value;
    emit thresholdsChanged();
    update();
}
int PriceChartItem::numberFontPixelSize() const { return m_numberFontPixelSize; }
void PriceChartItem::setNumberFontPixelSize(int value)
{
    value = qBound(12, value, 72);
    if (m_numberFontPixelSize == value) return;
    m_numberFontPixelSize = value;
    emit numberFontPixelSizeChanged();
    update();
}
qint64 PriceChartItem::xMinimum() const { return m_xMinimum; }
void PriceChartItem::setXMinimum(qint64 value)
{
    if (m_xMinimum == value) return;
    m_xMinimum = value;
    emit axisRangeChanged();
    update();
}
qint64 PriceChartItem::xMaximum() const { return m_xMaximum; }
void PriceChartItem::setXMaximum(qint64 value)
{
    if (m_xMaximum == value) return;
    m_xMaximum = value;
    emit axisRangeChanged();
    update();
}

void PriceChartItem::clearSelection()
{
    if (m_selectedStart != 0)
        m_selectedStart = 0;
    emit selectedDetailsChanged();
    update();
}

QVariantMap PriceChartItem::selectedDetails() const
{
    qint64 selectedStart = m_selectedStart;
    if (selectedStart == 0) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (const QVariant &entry : m_points) {
            const QVariantMap point = entry.toMap();
            const qint64 start = point.value(QStringLiteral("x")).toLongLong();
            const qint64 end = point.value(QStringLiteral("endX"), start).toLongLong();
            if (start <= now && now < end) {
                selectedStart = start;
                break;
            }
        }
    }

    for (const QVariant &entry : m_points) {
        const QVariantMap point = entry.toMap();
        const qint64 start = point.value(QStringLiteral("x")).toLongLong();
        if (start != selectedStart)
            continue;
        const qint64 end = point.value(QStringLiteral("endX"), start).toLongLong();
        const QDateTime localStart = QDateTime::fromMSecsSinceEpoch(start, Qt::UTC).toLocalTime();
        const QDateTime localEnd = QDateTime::fromMSecsSinceEpoch(end, Qt::UTC).toLocalTime();
        const QString timeRange = localStart.date() == localEnd.date()
                ? localStart.toString(QStringLiteral("HH:mm"))
                  + QStringLiteral(" – ") + localEnd.toString(QStringLiteral("HH:mm"))
                : localStart.toString(QStringLiteral("dd.MM HH:mm"))
                  + QStringLiteral(" – ") + localEnd.toString(QStringLiteral("dd.MM HH:mm"));
        return {
            { QStringLiteral("available"), true },
            { QStringLiteral("start"), start },
            { QStringLiteral("end"), end },
            { QStringLiteral("time"), timeRange },
            { QStringLiteral("transfer"), point.value(QStringLiteral("transfer")).toDouble() },
            { QStringLiteral("electricityTax"), point.value(QStringLiteral("electricityTax")).toDouble() },
            { QStringLiteral("spot"), point.value(QStringLiteral("spot")).toDouble() },
            { QStringLiteral("vat"), point.value(QStringLiteral("vat")).toDouble() },
            { QStringLiteral("total"), point.value(QStringLiteral("avg")).toDouble() }
        };
    }
    return {};
}

void PriceChartItem::mousePressEvent(QMouseEvent *event)
{
    for (const BarHitTarget &target : m_barHitTargets) {
        if (target.rect.contains(event->localPos())) {
            m_selectedStart = target.start;
            event->accept();
            emit selectedDetailsChanged();
            update();
            return;
        }
    }
    QQuickPaintedItem::mousePressEvent(event);
}

void PriceChartItem::paint(QPainter *painter)
{
    m_barHitTargets.clear();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF panel = boundingRect().adjusted(8.0, 4.0, -8.0, -4.0);
    painter->setPen(QPen(QColor(255, 255, 255, 55), 2));
    painter->setBrush(QColor(0, 0, 0, 180));
    painter->drawRoundedRect(panel, 18.0, 18.0);

    const QList<QPair<QString, QColor>> segmentStyles {
        qMakePair(QStringLiteral("Transfer"), QColor(115, 198, 142, 220)),
        qMakePair(QStringLiteral("Electricity tax"), QColor(184, 124, 220, 220)),
        qMakePair(QStringLiteral("Spot"), QColor(70, 155, 235, 210)),
        qMakePair(QStringLiteral("VAT"), QColor(245, 178, 70, 215))
    };
    const QStringList segmentKeys {
        QStringLiteral("transfer"), QStringLiteral("electricityTax"),
        QStringLiteral("spot"), QStringLiteral("vat")
    };
    QVector<bool> visibleSegments(segmentKeys.size(), false);
    for (const QVariant &entry : m_points) {
        const QVariantMap point = entry.toMap();
        for (int index = 0; index < segmentKeys.size(); ++index) {
            if (qAbs(point.value(segmentKeys.at(index)).toDouble()) > 0.00001)
                visibleSegments[index] = true;
        }
    }
    const bool hasBreakdown = std::any_of(visibleSegments.cbegin(), visibleSegments.cend(),
                                          [](bool visible) { return visible; });
    const QFont numberFont(painter->font().family(), m_numberFontPixelSize, QFont::DemiBold);
    const QFontMetrics numberMetrics(numberFont);
    const QFont legendFont(painter->font().family(), qMax(12, m_numberFontPixelSize / 2));
    const QFontMetrics legendMetrics(legendFont);
    const qreal leftMargin = qMax(qreal(110.0), qreal(numberMetrics.width(QStringLiteral("-99.99")) + 24));
    const qreal rightMargin = 18.0;
    const qreal topMargin = numberMetrics.height() * (hasBreakdown ? 1.75 : 0.75);
    const qreal bottomMargin = numberMetrics.height() * 2.5;
    const QRectF area(leftMargin, topMargin,
                      qMax(qreal(1.0), width() - leftMargin - rightMargin),
                      qMax(qreal(1.0), height() - bottomMargin - topMargin));
    painter->setPen(QPen(QColor(255, 255, 255, 70), 1));
    painter->drawRect(area);
    if (m_points.isEmpty()) {
        painter->setPen(QColor(255, 255, 255, 160));
        painter->drawText(area, Qt::AlignCenter, QStringLiteral("No price data"));
        return;
    }

    qint64 minX = std::numeric_limits<qint64>::max();
    qint64 maxX = std::numeric_limits<qint64>::min();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    for (const QVariant &entry : m_points) {
        const QVariantMap point = entry.toMap();
        const qint64 x = point.value(QStringLiteral("x")).toLongLong();
        const qint64 endX = point.value(QStringLiteral("endX"), x).toLongLong();
        minX = qMin(minX, x);
        maxX = qMax(maxX, endX);
        double cumulative = 0.0;
        minY = qMin(minY, cumulative);
        maxY = qMax(maxY, cumulative);
        for (const QString &key : segmentKeys) {
            cumulative += point.value(key).toDouble();
            minY = qMin(minY, cumulative);
            maxY = qMax(maxY, cumulative);
        }
    }
    const double dataRange = maxY - minY;
    const double padding = dataRange > 0.0001 ? dataRange * 0.08 : qMax(0.5, qAbs(maxY) * 0.1);
    minY -= padding;
    maxY += padding;
    if (qFuzzyCompare(maxY, minY)) maxY = minY + 1.0;
    if (m_xMinimum > 0)
        minX = m_xMinimum;
    if (m_xMaximum > minX)
        maxX = m_xMaximum;
    if (maxX <= minX) maxX = minX + 1;
    const auto mapX = [&](qint64 value) { return area.left() + (value - minX) * area.width() / double(maxX - minX); };
    const auto mapY = [&](double value) { return area.bottom() - (value - minY) * area.height() / (maxY - minY); };

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now >= minX && now <= maxX) {
        const qreal futureX = mapX(now);
        painter->fillRect(QRectF(futureX, area.top(), area.right() - futureX, area.height()),
                          QColor(100, 180, 255, 18));
    }

    painter->setFont(numberFont);
    painter->setPen(QColor(255, 255, 255, 210));
    painter->drawText(QRectF(0, area.top() - numberMetrics.height() / 2.0,
                             leftMargin - 10, numberMetrics.height() * 1.5), Qt::AlignRight,
                      QLocale().toString(maxY, 'f', 2));
    // Zero is the meaningful lower reference for a stacked price build-up.
    // Negative spot values still have room to extend below this baseline.
    const qreal zeroY = mapY(0.0);
    painter->drawText(QRectF(0, zeroY - numberMetrics.height() / 2.0,
                             leftMargin - 10, numberMetrics.height() * 1.5), Qt::AlignRight,
                      QLocale().toString(0.0, 'f', 2));
    painter->setPen(QPen(QColor(255, 255, 255, 105), 1));
    painter->drawLine(QPointF(area.left(), zeroY), QPointF(area.right(), zeroY));

    const QDateTime localMin = QDateTime::fromMSecsSinceEpoch(minX, Qt::UTC).toLocalTime();
    const QDateTime localMax = QDateTime::fromMSecsSinceEpoch(maxX, Qt::UTC).toLocalTime();
    const QString minimumLabel = localMin.toString(QStringLiteral("dd.MM\nHH:mm"));
    const QString maximumLabel = localMax.toString(QStringLiteral("dd.MM\nHH:mm"));
    painter->drawText(QRectF(area.left(), area.bottom() + 8, area.width() / 2.0,
                             numberMetrics.height() * 2.2),
                      Qt::AlignLeft | Qt::AlignTop, minimumLabel);
    painter->drawText(QRectF(area.center().x(), area.bottom() + 8, area.width() / 2.0,
                             numberMetrics.height() * 2.2),
                      Qt::AlignRight | Qt::AlignTop, maximumLabel);

    if (hasBreakdown) {
        painter->setFont(legendFont);
        qreal legendX = area.left();
        const qreal legendY = panel.top() + 4.0;
        for (int index = 0; index < segmentStyles.size(); ++index) {
            if (!visibleSegments.at(index))
                continue;
            const QString text = segmentStyles.at(index).first;
            const qreal textWidth = legendMetrics.width(text);
            painter->setPen(Qt::NoPen);
            painter->setBrush(segmentStyles.at(index).second);
            painter->drawRect(QRectF(legendX, legendY + 3, 10, 10));
            painter->setPen(QColor(255, 255, 255, 210));
            painter->drawText(QRectF(legendX + 14, legendY, textWidth + 2, legendMetrics.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, text);
            legendX += textWidth + 28;
        }
    }

    for (const QPair<double, QColor> &threshold : {
             qMakePair(m_cheapThreshold, QColor(100, 180, 255, 130)),
             qMakePair(m_expensiveThreshold, QColor(255, 130, 90, 150)) }) {
        if (threshold.first >= minY && threshold.first <= maxY) {
            painter->setPen(QPen(threshold.second, 1, Qt::DashLine));
            painter->drawLine(QPointF(area.left(), mapY(threshold.first)), QPointF(area.right(), mapY(threshold.first)));
        }
    }

    for (const QVariant &entry : m_points) {
        const QVariantMap point = entry.toMap();
        const qint64 pointStart = point.value(QStringLiteral("x")).toLongLong();
        const qint64 pointEnd = point.value(QStringLiteral("endX"), pointStart).toLongLong();
        const qreal left = mapX(pointStart);
        const qreal right = mapX(pointEnd);
        m_barHitTargets.append({ QRectF(left, area.top(), qMax(qreal(1.0), right - left),
                                       area.height()), pointStart });
        const bool currentHour = pointEnd - pointStart <= 2 * 60 * 60 * 1000
                && pointStart <= now && now < pointEnd;
        double cumulative = 0.0;
        for (int index = 0; index < segmentKeys.size(); ++index) {
            const double next = cumulative + point.value(segmentKeys.at(index)).toDouble();
            if (qAbs(next - cumulative) > 0.00001) {
                const qreal startY = mapY(qBound(minY, cumulative, maxY));
                const qreal endY = mapY(qBound(minY, next, maxY));
                const QRectF bar(left + 2.0, qMin(startY, endY),
                                 qMax(qreal(1.0), right - left - qreal(4.0)),
                                 qMax(qreal(2.0), qAbs(endY - startY)));
                QColor color = segmentStyles.at(index).second;
                if (currentHour)
                    color = color.darker(145);
                painter->setPen(QPen(QColor(255, 255, 255, 150), 1));
                painter->setBrush(color);
                painter->drawRect(bar);
            }
            cumulative = next;
        }
    }

    if (now >= minX && now <= maxX) {
        painter->setPen(QPen(QColor(255, 255, 255, 180), 1));
        painter->drawLine(QPointF(mapX(now), area.top()), QPointF(mapX(now), area.bottom()));
    }

    double labelledValue = std::numeric_limits<double>::quiet_NaN();
    qreal labelledX = 0.0;
    qint64 labelledStart = 0;
    qint64 labelledEnd = 0;
    if (m_selectedStart != 0) {
        for (const QVariant &entry : m_points) {
            const QVariantMap point = entry.toMap();
            const qint64 pointStart = point.value(QStringLiteral("x")).toLongLong();
            const qint64 pointEnd = point.value(QStringLiteral("endX"), pointStart).toLongLong();
            if (pointStart == m_selectedStart) {
                labelledValue = point.value(QStringLiteral("avg")).toDouble();
                labelledX = (mapX(pointStart) + mapX(pointEnd)) / 2.0;
                labelledStart = pointStart;
                labelledEnd = pointEnd;
                break;
            }
        }
    } else if (now >= minX && now <= maxX) {
        for (const QVariant &entry : m_points) {
            const QVariantMap point = entry.toMap();
            const qint64 pointStart = point.value(QStringLiteral("x")).toLongLong();
            const qint64 pointEnd = point.value(QStringLiteral("endX"), pointStart).toLongLong();
            if (pointStart <= now && now < pointEnd) {
                labelledValue = point.value(QStringLiteral("avg")).toDouble();
                labelledX = mapX(now);
                labelledStart = pointStart;
                labelledEnd = pointEnd;
                break;
            }
        }
    }

    if (!qIsNaN(labelledValue)) {
        const QPointF marker(labelledX, mapY(labelledValue));
        painter->setPen(QPen(QColor(255, 255, 255, 240), 3));
        painter->setBrush(QColor(70, 155, 235, 245));
        painter->drawEllipse(marker, 7, 7);

        painter->setFont(numberFont);
        const QString priceLabel = QLocale().toString(labelledValue, 'f', 2)
                + QStringLiteral(" c/kWh");
        const QRect textBounds = painter->fontMetrics().boundingRect(priceLabel);
        const QSizeF bubbleSize(textBounds.width() + 24, textBounds.height() + 14);
        qreal bubbleX = marker.x() + 12;
        if (bubbleX + bubbleSize.width() > area.right())
            bubbleX = marker.x() - bubbleSize.width() - 12;
        qreal bubbleY = marker.y() - bubbleSize.height() - 14;
        if (bubbleY < area.top())
            bubbleY = marker.y() + 14;
        const QRectF bubble(bubbleX, bubbleY, bubbleSize.width(), bubbleSize.height());
        painter->setPen(QPen(QColor(255, 255, 255, 220), 2));
        painter->setBrush(QColor(35, 95, 155, 235));
        painter->drawRoundedRect(bubble, 8, 8);
        painter->setPen(QColor(255, 255, 255, 255));
        painter->drawText(bubble, Qt::AlignCenter, priceLabel);

        const bool hourlyBar = labelledEnd - labelledStart <= 2 * 60 * 60 * 1000;
        const QDateTime labelledLocal = QDateTime::fromMSecsSinceEpoch(
                    labelledStart, Qt::UTC).toLocalTime();
        const QString pointedTime = labelledLocal.toString(
                    hourlyBar ? QStringLiteral("HH:mm") : QStringLiteral("dd.MM"));
        painter->drawText(QRectF(area.left() + area.width() / 3.0, area.bottom() + 8,
                                 area.width() / 3.0, numberMetrics.height() * 1.5),
                          Qt::AlignHCenter | Qt::AlignTop, pointedTime);
    }
}
