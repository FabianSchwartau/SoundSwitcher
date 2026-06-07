#include "app_icon.h"

#include <QApplication>
#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

namespace {

QIcon renderedSoundSwitcherIcon()
{
    QIcon icon;
    const QList<int> sizes = {16, 22, 24, 32, 48, 64, 128};

    for (const int size : sizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        const qreal scale = size / 64.0;
        painter.scale(scale, scale);

        QLinearGradient gradient(QPointF(10, 8), QPointF(56, 58));
        gradient.setColorAt(0.0, QColor(QStringLiteral("#38d5c8")));
        gradient.setColorAt(0.55, QColor(QStringLiteral("#246bfe")));
        gradient.setColorAt(1.0, QColor(QStringLiteral("#7a4dff")));

        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(QRectF(7, 7, 50, 50), 14, 14);

        painter.setBrush(Qt::white);
        painter.drawPolygon(QPolygonF({
            QPointF(18, 24),
            QPointF(26, 24),
            QPointF(35, 16),
            QPointF(35, 48),
            QPointF(26, 40),
            QPointF(18, 40),
        }));

        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(QRectF(34, 22, 18, 20), -45 * 16, 90 * 16);

        painter.setPen(QPen(QColor(QStringLiteral("#e0f7ff")), 3.5, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(QRectF(38, 15, 22, 34), -48 * 16, 96 * 16);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#0b1736")));
        painter.drawEllipse(QPointF(44, 32), 5, 5);

        painter.setPen(QPen(Qt::white, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(42, 32), QPointF(50, 32));
        painter.drawLine(QPointF(47, 29), QPointF(50, 32));
        painter.drawLine(QPointF(47, 35), QPointF(50, 32));

        icon.addPixmap(pixmap);
    }

    return icon;
}

} // namespace

QIcon soundSwitcherIcon()
{
    auto icon = QIcon::fromTheme(QStringLiteral(SOUNDSWITCHER_DESKTOP_ID));
    if (icon.isNull()) {
        icon = QIcon(QStringLiteral(":/icons/io.github.fabian.SoundSwitcher.svg"));
    }
    const QIcon fallbackIcon = renderedSoundSwitcherIcon();
    for (const QSize &size : fallbackIcon.availableSizes()) {
        icon.addPixmap(fallbackIcon.pixmap(size));
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
