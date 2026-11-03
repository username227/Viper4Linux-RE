// ViPER4Linux-RE: shared presentation-only widgets.
// Extracted unchanged from viper-gui.cpp; no DSP or settings state here.
#pragma once

#include <QCheckBox>
#include <QColor>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QSlider>
#include <QString>
#include <QWheelEvent>
#include <QWidget>

class SafeSlider final : public QSlider {
public:
    explicit SafeSlider(Qt::Orientation orientation, QWidget* parent)
        : QSlider(orientation, parent) {
        setFocusPolicy(Qt::ClickFocus);
    }
protected:
    void wheelEvent(QWheelEvent* event) override {
        if (hasFocus()) QSlider::wheelEvent(event);
        else event->ignore(); // pass wheel to QScrollArea
    }
};

// A real checkable Qt widget, drawn as an Android-style switch.  It emits the
// same QCheckBox::toggled signal as before; changing panel visibility never
// calls setChecked() and therefore cannot change DSP processing.
// Consistent, font-independent monochrome glyphs for EVERY effect card.
// The icons are a visual guide only: they do not read or change DSP settings.
class EffectGlyph final : public QWidget {
public:
    EffectGlyph(const QString& kind, QWidget* parent) : QWidget(parent), kind_(kind) {
        setFixedSize(26, 26);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAccessibleName(kind + " effect icon");
    }
    void setColor(const QColor& color) {
        if (color_ != color) { color_ = color; update(); }
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(1, 1);
        p.setPen(QPen(color_, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        auto line = [&](qreal x1, qreal y1, qreal x2, qreal y2) {
            p.drawLine(QPointF(x1,y1), QPointF(x2,y2));
        };
        auto wave = [&](qreal y, qreal amplitude) {
            QPainterPath w(QPointF(1, y));
            w.cubicTo(3, y-amplitude, 5, y-amplitude, 7, y);
            w.cubicTo(9, y+amplitude, 11, y+amplitude, 13, y);
            w.cubicTo(15, y-amplitude, 17, y-amplitude, 19, y);
            w.cubicTo(21, y+amplitude, 22, y+amplitude, 23, y);
            p.drawPath(w);
        };
        if (kind_ == "bass") {
            wave(9, 7); wave(16, 4);
        } else if (kind_ == "bass_mono") {
            wave(12, 7); line(12,2,12,5); line(12,19,12,22);
        } else if (kind_ == "psycho_bass") {
            // Ear and the low-frequency wave reaching it.
            QPainterPath ear(QPointF(16,19));
            ear.cubicTo(16,16,21,15,21,9);
            ear.cubicTo(21,1,11,1,11,8);
            ear.cubicTo(11,11,16,11,16,8);
            p.drawPath(ear);
            QPainterPath w(QPointF(1,11));
            w.cubicTo(3,4,5,4,7,11); w.cubicTo(8,14,9,14,10,11);
            p.drawPath(w);
        } else if (kind_ == "dynamic_bass") {
            wave(15, 4);
            line(11,2,8,9); line(8,9,12,9); line(12,9,10,13);
            line(18,2,22,2); line(22,2,22,6);
        } else if (kind_ == "equalizer") {
            for (int x : {4, 10, 16, 22}) line(x,3,x,21);
            for (const QPointF q : {QPointF(4,14),QPointF(10,7),QPointF(16,17),QPointF(22,10)}) {
                p.setBrush(color_); p.drawRoundedRect(QRectF(q.x()-2.0,q.y()-1.5,4,3),1,1);
            }
            p.setBrush(Qt::NoBrush);
        } else if (kind_ == "clarity") {
            QPainterPath star(QPointF(12,1));
            star.lineTo(14.5,9); star.lineTo(22,12); star.lineTo(14.5,14.5);
            star.lineTo(12,22); star.lineTo(9.5,14.5); star.lineTo(2,12);
            star.lineTo(9.5,9); star.closeSubpath();
            p.drawPath(star);
        } else if (kind_ == "spectrum") {
            // Additional high-frequency content on the right.
            for (int i = 0; i < 6; ++i) {
                const qreal x = 2 + i*4;
                const qreal h = 3 + i*2.5;
                line(x,12-h/2,x,12+h/2);
            }
        } else if (kind_ == "tube") {
            p.drawRoundedRect(QRectF(6,3,12,17),4,4);
            line(9,3,15,3); line(9,20,9,23); line(15,20,15,23);
            line(9,9,15,9); line(9,14,15,14);
            p.drawArc(QRectF(8,10,8,5),180*16,180*16);
        } else if (kind_ == "dynamic_eq") {
            // A response envelope and an adjustable threshold line.
            line(2,12,22,12);
            QPainterPath profile(QPointF(2,17));
            profile.cubicTo(7,17,7,5,12,5);
            profile.cubicTo(17,5,17,17,22,17);
            p.drawPath(profile);
            p.setPen(QPen(color_, 1.5, Qt::DashLine));
            line(2,9,22,9);
        } else if (kind_ == "analogx") {
            // Analog tape/vinyl signal path: two parallel tracks with a stylus.
            line(3,5,21,5); line(3,10,21,10);
            p.drawEllipse(QPointF(6,18),3,3);
            p.drawEllipse(QPointF(18,18),3,3);
            line(9,18,15,18); line(12,10,12,14);
        } else if (kind_ == "crossfeed") {
            // Two distinct channels exchanging a small, shaped signal.
            line(2,5,22,5); line(2,19,22,19);
            QPainterPath a(QPointF(6,5)); a.cubicTo(9,5,9,19,14,19);
            QPainterPath b(QPointF(18,19)); b.cubicTo(15,19,15,5,10,5);
            p.drawPath(a); p.drawPath(b);
        } else if (kind_ == "multiband") {
            for (int i = 0; i < 4; ++i) {
                const qreal x = 3 + i*6;
                const qreal h = 5 + ((i+1)%3)*3;
                p.drawRect(QRectF(x, 21-h, 3, h));
            }
            p.setPen(QPen(color_, 1.3, Qt::DashLine)); line(2,9,22,9);
        } else if (kind_ == "fet") {
            // Compression curve bends at the knee into a quieter slope.
            line(2,20,10,12); line(10,12,15,9); line(15,9,22,6);
            p.setPen(QPen(color_, 1.3, Qt::DashLine)); line(2,11,22,11);
        } else if (kind_ == "imager") {
            // Three distinct frequency bands spreading outward from center.
            for (int y : {5, 12, 19}) {
                line(11, y, 13, y);
                line(8, y-2, 5, y);
                line(8, y+2, 5, y);
                line(16, y-2, 19, y);
                line(16, y+2, 19, y);
            }
        } else if (kind_ == "headphone") {
            p.drawArc(QRectF(3,3,18,18),0,180*16);
            p.drawRoundedRect(QRectF(2,12,5,9),1.5,1.5);
            p.drawRoundedRect(QRectF(17,12,5,9),1.5,1.5);
            line(7,8,7,14); line(17,8,17,14);
        } else if (kind_ == "differential") {
            line(3,7,21,7); line(3,7,7,3); line(3,7,7,11);
            line(3,17,21,17); line(21,17,17,13); line(21,17,17,21);
        } else if (kind_ == "field") {
            p.setBrush(color_); p.drawEllipse(QPointF(12,12),2,2); p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(5,5,14,14),120*16,120*16);
            p.drawArc(QRectF(5,5,14,14),-60*16,120*16);
            p.drawArc(QRectF(1,1,22,22),120*16,120*16);
            p.drawArc(QRectF(1,1,22,22),-60*16,120*16);
        } else if (kind_ == "reverb") {
            // A pulse followed by three decaying echo peaks.
            line(1,17,4,17); line(4,17,6,4); line(6,4,8,17); line(8,17,10,17);
            line(11,17,13,9); line(13,9,15,17);
            line(16,17,18,12); line(18,12,20,17);
            line(21,17,22,15); line(22,15,23,17);
        } else if (kind_ == "ddc") {
            // Two curves — corrected response relative to source response.
            line(2,18,5,18); line(5,18,8,8); line(8,8,12,13);
            line(12,13,15,6); line(15,6,19,10); line(19,10,22,5);
        } else if (kind_ == "speaker_correction") {
            // Speaker cone plus acoustic waves.
            p.drawRoundedRect(QRectF(3,4,10,17),1.4,1.4);
            p.drawEllipse(QPointF(8,9),2,2); p.drawEllipse(QPointF(8,16),3,3);
            p.drawArc(QRectF(14,5,7,14),-80*16,160*16);
        } else if (kind_ == "convolver") {
            // A single impulse becomes a longer, decaying response.
            line(1,18,4,18); line(4,18,5,4); line(5,4,6,18); line(6,18,9,18);
            line(10,10,12,14); line(12,14,14,7); line(14,7,16,17);
            line(16,17,18,12); line(18,12,20,16); line(20,16,22,14);
        } else if (kind_ == "limiter") {
            line(2,5,22,5); // ceiling
            QPainterPath signal(QPointF(2,19));
            signal.lineTo(6,12); signal.lineTo(10,10); signal.lineTo(11,5);
            signal.lineTo(16,5); signal.lineTo(20,13); signal.lineTo(22,19);
            p.drawPath(signal);
        } else if (kind_ == "agc") {
            // Level meter converging to a steady reference level.
            for (const QRectF bar : {QRectF(2,14,3,8),QRectF(8,10,3,12),QRectF(14,6,3,16),QRectF(20,10,3,12)})
                p.drawRect(bar);
            line(1,9,23,9);
        } else if (kind_ == "lufs") {
            p.drawEllipse(QPointF(12,12),10,10);
            p.drawEllipse(QPointF(12,12),5,5);
            p.setBrush(color_); p.drawEllipse(QPointF(12,12),1.5,1.5); p.setBrush(Qt::NoBrush);
            line(17,7,22,2);
        } else {
            // Unknown new effects are unmistakable during development, rather
            // than silently getting the generic dot that caused this issue.
            p.drawText(QRectF(0,0,24,24), Qt::AlignCenter, "?");
        }
    }
private:
    QString kind_;
    QColor color_{"#536c70"};
};

class EffectSwitch final : public QCheckBox {
public:
    explicit EffectSwitch(QWidget* parent) : QCheckBox(parent) {
        setText(QString());
        setFixedSize(58, 34);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
    }
    void setClassicTheme(bool classic, bool dark) { classic_theme_ = classic; classic_dark_ = dark; update(); }
protected:
    bool hitButton(const QPoint& pos) const override { return rect().contains(pos); }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const bool dark = classic_theme_ ? classic_dark_ :
            palette().color(QPalette::Window).lightness() < 130;
        const QColor accent = classic_theme_ ? QColor(dark ? "#12b8e8" : "#087eae") : QColor("#236e76");
        const QColor track = !isEnabled() ? (dark ? QColor("#536467") : QColor("#a4babc"))
                           : isChecked() ? accent
                                        : (dark ? QColor("#515f62") : QColor("#dce5e6"));
        const QColor stroke = !isEnabled() ? (dark ? QColor("#6e8588") : QColor("#9eb6b9"))
                            : isChecked() ? accent
                                         : (dark ? QColor("#87999b") : QColor("#819396"));
        const QColor thumb = !isEnabled() ? QColor("#e2ebec")
                           : isChecked() ? QColor("#f4fdff")
                                        : (dark ? QColor("#bbcacc") : QColor("#75888a"));
        const QRectF outer(2, 3, 54, 28);
        painter.setPen(QPen(stroke, 1.5));
        painter.setBrush(track);
        painter.drawRoundedRect(outer, 14, 14);
        painter.setPen(Qt::NoPen);
        painter.setBrush(thumb);
        const qreal knob = isChecked() ? 42 : 16;
        painter.drawEllipse(QPointF(knob, 17), isChecked() ? 11 : 9, isChecked() ? 11 : 9);
        if (hasFocus()) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(classic_theme_ ? QColor("#69d6f8") : QColor("#529ca1"), 1.5, Qt::DashLine));
            painter.drawRoundedRect(QRectF(0.75, 1.75, 56.5, 30.5), 15, 15);
        }
    }
private:
    bool classic_theme_ = false;
    bool classic_dark_ = false;
};
