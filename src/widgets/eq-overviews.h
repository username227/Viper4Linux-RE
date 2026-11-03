// Graphic and Dynamic EQ overviews and shared band value structure.
// Extracted without changing paint, click-selection, smoothing, or numeric scales.
#pragma once
#include "viper-eq-presets.h"
#include <QColor>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QSizePolicy>
#include <QWidget>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

// The compact overview has NO faders: paint from the same EQ state used by
// the DSP. Fixed axes allow comparison between presets and band layouts.
// Only a click on the graphic opens the separately managed detailed editor.
class EqOverview final : public QWidget {
public:
    explicit EqOverview(QWidget* parent) : QWidget(parent) {
        setObjectName("viperEqOverview");
        setMinimumHeight(116);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setToolTip("Click the EQ curve to open the detailed graphic equalizer");
        setAccessibleName("EQ frequency response overview; click to edit bands");
    }
    std::function<void()> openEditor;
    void setLevels(const std::vector<int>& levels) { levels_ = levels; update(); }
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (openEditor) openEditor();
            event->accept();
        } else QWidget::mousePressEvent(event);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        // Fixed 20 Hz–20 kHz horizontal axis and fixed +/-12 dB vertical
        // axis: different presets and band counts must be directly comparable.
        // Levels are in hundredths of a decibel, as are the fader values.
        const QRectF plot = QRectF(rect()).adjusted(36, 9, -10, -9);
        if (plot.width() <= 0 || plot.height() <= 0) return;
        const QColor fg = palette().color(QPalette::Text);
        const QColor accent = palette().color(QPalette::Highlight);
        const QColor base = palette().color(QPalette::Base);
        p.setPen(QPen(fg, 1));
        p.setBrush(base);
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -2, -2), 8, 8);
        constexpr double kMinHz = 20.0;
        constexpr double kMaxHz = 20000.0;
        constexpr int kMaxGain = 1200;
        const qreal zeroY = plot.center().y();
        p.setPen(QPen(fg, 1, Qt::DotLine));
        p.drawLine(QPointF(plot.left(), zeroY), QPointF(plot.right(), zeroY));
        p.setPen(fg);
        const QFontMetrics metrics = p.fontMetrics();
        const int textWidth = 30;
        p.drawText(QRectF(3, plot.top()-4, textWidth, metrics.height()),
                   Qt::AlignRight | Qt::AlignVCenter, "+12");
        p.drawText(QRectF(3, zeroY-metrics.height()/2.0, textWidth, metrics.height()),
                   Qt::AlignRight | Qt::AlignVCenter, "0");
        p.drawText(QRectF(3, plot.bottom()-metrics.height()+4, textWidth, metrics.height()),
                   Qt::AlignRight | Qt::AlignVCenter, "-12");
        if (levels_.empty()) return;
        const auto& hz = viper_eq::frequencies(static_cast<int>(levels_.size()));
        const double lo = std::log(kMinHz);
        const double range = std::log(kMaxHz) - lo;
        std::vector<QPointF> points;
        points.reserve(levels_.size());
        for (size_t i = 0; i < levels_.size(); ++i) {
            const qreal x = plot.left() + plot.width() *
                (std::log(static_cast<double>(hz[i])) - lo) / range;
            const qreal y = zeroY - plot.height() *
                static_cast<double>(std::clamp(levels_[i], -kMaxGain, kMaxGain))
                / (2.0 * kMaxGain);
            points.emplace_back(x, y);
        }
        // Cubic segments PASS THROUGH each band's actual value, unlike a
        // midpoint quadratic which could visibly flatten the highest peak.
        // Constrain tangents to each segment so smoothing never overshoots
        // above or below the fitted extrema.
        QPainterPath response(points.front());
        for (size_t i = 0; i + 1 < points.size(); ++i) {
            const QPointF& a = points[i];
            const QPointF& b = points[i+1];
            const QPointF prev = points[i == 0 ? i : i-1];
            const QPointF next = points[i + 2 < points.size() ? i+2 : i+1];
            QPointF c1 = a + (b - prev) / 6.0;
            QPointF c2 = b - (next - a) / 6.0;
            const qreal top = std::min(a.y(), b.y());
            const qreal bottom = std::max(a.y(), b.y());
            c1.setY(std::clamp(c1.y(), top, bottom));
            c2.setY(std::clamp(c2.y(), top, bottom));
            response.cubicTo(c1, c2, b);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(accent, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(response);
    }
private:
    std::vector<int> levels_;
};

struct DeqBand {
    int frequency = 1000;
    int q = 100;             // hundredths, 0.5..8
    int gain = 0;            // hundredths dB, -12..12
    int threshold = -240;   // tenths dB, -80..0
    int attack = 10;        // milliseconds
    int release = 100;      // milliseconds
    int type = 0;           // peak, low shelf, high shelf
};

// Configured maximum curve, not real-time gain reduction or measured DSP
// frequency response. Always resizes inside the tab; no animation/timer.
class DynamicEqOverview final : public QWidget {
public:
    explicit DynamicEqOverview(QWidget* parent) : QWidget(parent) {
        setObjectName("viperDynamicEqOverview");
        setMinimumHeight(116);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName("Dynamic EQ overview with numbered markers for all active bands; click to edit filters");
        setToolTip("Numbered markers represent every active band. Illustrative target response, not live metering. Click to edit.");
    }
    std::function<void()> openEditor;
    std::function<void(int)> selectBand;
    void enableBandSelection() {
        editor_mode_ = true;
        setToolTip("Click a band marker to edit its complete settings. This is not live metering.");
        setAccessibleName("Configured Dynamic EQ curve; click near a marker to select a band");
    }
    void setSelectedBand(int index) { selected_band_ = index; update(); }
    void setBands(std::array<DeqBand, 10> values, int count, bool enabled) {
        bands_ = values; count_ = std::clamp(count, 1, 10); enabled_ = enabled; update();
    }
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        // Both graphs use the same coordinates computed by paintEvent().
        // Only a deliberate click near a numbered marker selects a band from
        // the main overview. A click elsewhere still opens the editor at its
        // previous selection. Inside the editor, clicking anywhere selects
        // the nearest active band, as before.
        int nearest = -1;
        double distance = std::numeric_limits<double>::infinity();
        for (int i = 0; i < count_; ++i) {
            const double d = std::hypot(markers_[i].x() - event->position().x(),
                                        markers_[i].y() - event->position().y());
            if (d < distance) { distance = d; nearest = i; }
        }
        if (nearest >= 0 && (editor_mode_ || distance <= 19.0)) {
            setSelectedBand(nearest);
            if (selectBand) selectBand(nearest);
        }
        if (!editor_mode_ && openEditor) openEditor();
        event->accept();
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF plot = QRectF(rect()).adjusted(10, 8, -10, -8);
        if (plot.width() < 10 || plot.height() < 10) return;
        const QColor fg = palette().color(QPalette::Text);
        QColor accent = palette().color(QPalette::Highlight);
        if (!enabled_) accent.setAlpha(110);
        p.setPen(QPen(fg, 1));
        p.setBrush(palette().color(QPalette::Base));
        p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-2,-2),8,8);
        // Illustrative static max-target shape. Not the exact complex native
        // biquad transfer function; actual gain
        // depends on threshold, signal level, attack and release.
        constexpr int kSteps = 120;
        std::array<double, kSteps+1> db{};
        for (int j = 0; j <= kSteps; ++j) {
            const double hz = 20.0 * std::pow(1000.0, j / double(kSteps));
            const double logHz = std::log(hz);
            double level = 0;
            for (int i = 0; i < count_; ++i) {
                const DeqBand& b = bands_[i];
                const double d = logHz - std::log(double(b.frequency));
                const double width = 0.8 / (b.q / 100.0);
                double shape = 0;
                if (b.type == 0) shape = std::exp(-0.5 * std::pow(d / width, 2));
                else if (b.type == 1) shape = 1.0 / (1.0 + std::exp(std::clamp(d / width * 3.0, -60.0, 60.0)));
                else shape = 1.0 / (1.0 + std::exp(std::clamp(-d / width * 3.0, -60.0, 60.0)));
                level += b.gain / 100.0 * shape;
            }
            db[j] = level;
        }
        // Symmetric +/-12 dB (or wider for stacked bands) preserves the 0 dB
        // reference, unlike the graphic EQ's auto-fit positive/negative range.
        double extreme = 12.0;
        for (double d : db) extreme = std::max(extreme, std::abs(d));
        const double zero = plot.center().y();
        p.setPen(QPen(fg, 1, Qt::DotLine));
        p.drawLine(QPointF(plot.left(),zero), QPointF(plot.right(),zero));
        QPainterPath curve;
        for (int j = 0; j <= kSteps; ++j) {
            const double x = plot.left() + j / double(kSteps) * plot.width();
            const double y = zero - db[j] / extreme * plot.height() * 0.5;
            if (j == 0) curve.moveTo(x,y);
            else curve.lineTo(x,y);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(accent, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(curve);
        // Each active band gets a permanently visible, numbered marker in BOTH
        // the tab overview and the editor. Coincident/near-coincident frequencies
        // are clustered in a small two-row fan with leader lines back to the
        // true frequency/curve position. This changes only presentation, NEVER
        // the actual band frequency, filter settings, or drawn response curve.
        std::array<QPointF,10> anchors{};
        for (int i = 0; i < count_; ++i) {
            const double fraction = std::clamp(
                std::log(double(std::clamp(bands_[i].frequency, 20, 20000)) / 20.0)
                    / std::log(1000.0), 0.0, 1.0);
            const int bin = std::clamp(int(std::round(fraction * kSteps)), 0, kSteps);
            anchors[i] = QPointF(plot.left() + fraction * plot.width(),
                zero - db[bin] / extreme * plot.height() * 0.5);
            markers_[i] = anchors[i];
        }
        std::array<bool,10> grouped{};
        for (int i = 0; i < count_; ++i) {
            if (grouped[i]) continue;
            std::vector<int> cluster{i};
            grouped[i] = true;
            for (int j = i + 1; j < count_; ++j) {
                // Close x positions are visually indistinguishable when bands
                // share a frequency; group them even when gains differ.
                if (!grouped[j] && std::abs(anchors[j].x()-anchors[i].x()) < 18.0) {
                    cluster.push_back(j);
                    grouped[j] = true;
                }
            }
            if (cluster.size() == 1) continue;
            const int cols = int((cluster.size() + 1) / 2);
            double anchorX = 0.0, anchorY = 0.0;
            for (int n : cluster) { anchorX += anchors[n].x(); anchorY += anchors[n].y(); }
            anchorX /= cluster.size();
            anchorY /= cluster.size();
            for (size_t k = 0; k < cluster.size(); ++k) {
                const int col = int(k / 2);
                const int row = int(k % 2);
                const double x = anchorX + (col - (cols - 1) * 0.5) * 19.0;
                const double y = anchorY + (row == 0 ? -13.0 : 13.0);
                markers_[cluster[k]] = QPointF(
                    std::clamp(x, plot.left()+10, plot.right()-10),
                    std::clamp(y, plot.top()+10, plot.bottom()-10));
            }
        }
        // Connect displaced labels to their correct locations on the curve.
        QPen leader(accent, 1.0);
        leader.setStyle(Qt::DotLine);
        p.setPen(leader);
        for (int i = 0; i < count_; ++i) {
            if (std::hypot(markers_[i].x()-anchors[i].x(),
                           markers_[i].y()-anchors[i].y()) > 4.0)
                p.drawLine(anchors[i], markers_[i]);
        }
        QFont markerFont = p.font();
        markerFont.setPixelSize(10);
        markerFont.setBold(true);
        p.setFont(markerFont);
        for (int i = 0; i < count_; ++i) {
            const bool selected = selected_band_ == i;
            const QPointF center = markers_[i];
            const double radius = selected ? 10.0 : 8.5;
            p.setPen(QPen(accent, selected ? 2.0 : 1.5));
            p.setBrush(palette().color(QPalette::Base));
            p.drawEllipse(center, radius, radius);
            p.setPen(fg);
            p.drawText(QRectF(center.x()-radius, center.y()-radius,
                               radius*2, radius*2),
                       Qt::AlignCenter, QString::number(i+1));
        }
    }
private:
    std::array<DeqBand,10> bands_{};
    int count_ = 3;
    int selected_band_ = 0;
    std::array<QPointF,10> markers_{};
    bool editor_mode_ = false;
    bool enabled_ = false;
};

