#include "UiIcons.h"
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <utility>
namespace {
class IconEngine final : public QIconEngine {
public:
    explicit IconEngine(QString name) : name_(std::move(name)) {}
    QIconEngine* clone() const override { return new IconEngine(name_); }
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        QPixmap result(size); result.fill(Qt::transparent);
        QPainter painter(&result); paint(&painter, QRect(QPoint(), size), mode, state); return result;
    }
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State) override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const double side = qMin(rect.width(), rect.height());
        painter->translate(rect.x() + (rect.width()-side)/2, rect.y() + (rect.height()-side)/2);
        painter->scale(side / 24.0, side / 24.0);
        const QColor ink(mode == QIcon::Disabled ? "#6d6978" : "#eae5f7");
        painter->setPen(QPen(ink, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        auto line = [painter](double x1,double y1,double x2,double y2) { painter->drawLine(QPointF(x1,y1),QPointF(x2,y2)); };
        auto path = [painter](std::initializer_list<QPointF> points, bool close=false) {
            QPainterPath shape; bool first=true;
            for (const auto& point : points) { if (first) shape.moveTo(point); else shape.lineTo(point); first=false; }
            if (close) shape.closeSubpath(); painter->drawPath(shape);
        };
        if (name_ == "play") {
            painter->setPen(Qt::NoPen); painter->setBrush(ink);
            QPainterPath shape; shape.moveTo(8,5); shape.quadTo(7,4.4,7,5.8); shape.lineTo(7,18.2);
            shape.quadTo(7,19.6,8,19); shape.lineTo(19,12.8); shape.quadTo(20.2,12,19,11.2); shape.closeSubpath(); painter->drawPath(shape);
        } else if (name_ == "pause") {
            painter->setPen(Qt::NoPen); painter->setBrush(ink);
            painter->drawRoundedRect(QRectF(6.5,5,3.5,14),1,1); painter->drawRoundedRect(QRectF(14,5,3.5,14),1,1);
        } else if (name_ == "open") {
            path({{3,9},{3,6},{9,6},{11,8},{20,8},{20,10}});
            path({{3,10},{21,10},{18.5,19},{4.5,19},{3,10}},true);
        } else if (name_ == "camera") {
            path({{4,7},{8,7},{9.5,4.5},{14.5,4.5},{16,7},{20,7},{21,8},{21,18},{20,19},{4,19},{3,18},{3,8},{4,7}},true);
            painter->drawEllipse(QPointF(12,12.5),3.5,3.5); painter->drawPoint(QPointF(18,9.5));
        } else if (name_ == "pip") {
            painter->drawRoundedRect(QRectF(3,4,18,16),2,2);
            painter->setPen(Qt::NoPen); painter->setBrush(ink); painter->drawRoundedRect(QRectF(11,11,7,6),1,1);
        } else if (name_ == "tracks") {
            painter->drawRoundedRect(QRectF(3,5,18,14),2,2);
            path({{10,9},{7,9},{7,15},{10,15}}); path({{17,9},{14,9},{14,15},{17,15}});
        } else if (name_ == "library") {
            line(4,6,15,6); line(4,11,15,11); line(4,16,11,16);
            path({{17,13},{21,16},{17,19}},true);
        } else if (name_ == "more") {
            painter->setBrush(ink); painter->setPen(Qt::NoPen);
            for (int x : {5,12,19}) painter->drawEllipse(QPointF(x,12),1.5,1.5);
        } else if (name_ == "previous" || name_ == "next" || name_ == "frameBack" || name_ == "frameNext") {
            const bool back = name_ == "previous" || name_ == "frameBack";
            if (back) { painter->translate(24,0); painter->scale(-1,1); }
            path({{6,5},{16,12},{6,19}},true); line(19,5,19,19);
        } else if (name_ == "close") {
            line(6,6,18,18); line(18,6,6,18);
        } else if (name_ == "minimize") {
            line(5,12,19,12);
        } else if (name_ == "maximize") {
            painter->drawRoundedRect(QRectF(5,5,14,14),1.5,1.5);
        } else if (name_ == "restore") {
            path({{9,4},{20,4},{20,15}}); painter->drawRoundedRect(QRectF(4,9,11,11),1,1);
        } else {
            path({{4,9},{4,4},{9,4}}); path({{15,4},{20,4},{20,9}});
            path({{4,15},{4,20},{9,20}}); path({{15,20},{20,20},{20,15}});
        }
        painter->restore();
    }
private:
    QString name_;
};
}
QIcon luminaIcon(const QString& name) { return QIcon(new IconEngine(name)); }
