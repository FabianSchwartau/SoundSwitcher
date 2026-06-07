#include "app_icon.h"

#include <QApplication>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QStyle>

namespace {

QIcon renderedSoundSwitcherIcon()
{
    QIcon icon;
    const QList<int> sizes = {16, 22, 24, 32, 48, 64, 128};
    const QColor foreground = qApp != nullptr
        ? qApp->palette().color(QPalette::WindowText)
        : QColor(Qt::black);

    for (const int size : sizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal scale = size / 64.0;
        painter.scale(scale, scale);

        const QPolygonF speaker({
            QPointF(9, 25),
            QPointF(19, 25),
            QPointF(31, 15),
            QPointF(31, 49),
            QPointF(19, 39),
            QPointF(9, 39),
        });

        painter.setPen(Qt::NoPen);
        painter.setBrush(foreground);
        painter.drawPolygon(speaker);

        painter.setPen(QPen(foreground, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        QPainterPath topArrow(QPointF(38, 23));
        topArrow.lineTo(QPointF(55, 23));
        topArrow.moveTo(QPointF(48, 16));
        topArrow.lineTo(QPointF(55, 23));
        topArrow.lineTo(QPointF(48, 30));
        painter.drawPath(topArrow);

        QPainterPath bottomArrow(QPointF(55, 41));
        bottomArrow.lineTo(QPointF(38, 41));
        bottomArrow.moveTo(QPointF(45, 34));
        bottomArrow.lineTo(QPointF(38, 41));
        bottomArrow.lineTo(QPointF(45, 48));
        painter.drawPath(bottomArrow);

        icon.addPixmap(pixmap);
    }

    return icon;
}

} // namespace

QIcon soundSwitcherIcon()
{
    auto icon = renderedSoundSwitcherIcon();
    if (icon.isNull()) {
        icon = QIcon(QStringLiteral(":/icons/soundswitcher.svg"));
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral(SOUNDSWITCHER_DESKTOP_ID));
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("audio-card"));
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("audio-volume-high"));
    }
    if (icon.isNull() && qApp != nullptr) {
        icon = QApplication::style()->standardIcon(QStyle::SP_MediaVolume);
    }
    return icon;
}
