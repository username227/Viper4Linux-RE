// ViPER for Linux — Qt6 control surface.
// Audio runs in PipeWire's independent filter-chain, not inside this GUI.
#include "viper-control.h"
#include "viper-backend-state.h"
#include "viper-eq-presets.h"
#include "widgets/effect-widgets.h"
#include "widgets/irs-combo-box.h"
#include "viper-build-info.h"
#include <QApplication>
#include <QAction>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QRadioButton>
#include <QSaveFile>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QByteArray>
#include <QGuiApplication>
#include <QScreen>
#include <QCloseEvent>
#include <QEvent>
#include <QWindowStateChangeEvent>
#include <QIcon>
#include <QPixmap>
#include <QStyleHints>
#include <QtGlobal>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QProcess>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QUuid>
#include <QAbstractItemView>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QTextOption>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QStyle>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QTabBar>
#include <QToolButton>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>
#include <algorithm>
#include <array>
#include <QStringList>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <functional>
#include <vector>
#include <utility>
#include <cmath>

// The compact overview has NO faders: paint from the same EQ state used by
// the DSP.  It scales to any panel width without ever affecting its size hint.
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
        const QRectF plot = QRectF(rect()).adjusted(10, 8, -10, -8);
        if (plot.width() <= 0 || plot.height() <= 0) return;
        const QColor fg = palette().color(QPalette::Text);
        const QColor accent = palette().color(QPalette::Highlight);
        const QColor base = palette().color(QPalette::Base);
        p.setPen(QPen(fg, 1));
        p.setBrush(base);
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -2, -2), 8, 8);
        if (levels_.empty()) return;
        const auto [minimum, maximum] = std::minmax_element(levels_.begin(), levels_.end());
        // Auto-fit the displayed response; full positive/negative extrema
        // reach the top/bottom of the usable plot rather than sitting near
        // the middle of a fixed +/-12 dB graph.
        if (*minimum < 0 && *maximum > 0) {
            const qreal zeroY = plot.top() + plot.height() *
                viper_eq::normalizedY(0, *minimum, *maximum);
            p.setPen(QPen(fg, 1, Qt::DotLine));
            p.drawLine(QPointF(plot.left(), zeroY), QPointF(plot.right(), zeroY));
        }
        // The EQ is a log-frequency response, not an evenly spaced list of
        // arbitrary categories. Keep all band values in the full editor only.
        const auto& hz = viper_eq::frequencies(static_cast<int>(levels_.size()));
        const double lo = std::log(static_cast<double>(hz[0]));
        const double hi = std::log(static_cast<double>(hz[levels_.size()-1]));
        const qreal range = static_cast<qreal>(hi - lo);
        if (range <= 0) return;
        std::vector<QPointF> points;
        points.reserve(levels_.size());
        for (size_t i = 0; i < levels_.size(); ++i) {
            const qreal x = plot.left() + plot.width() *
                (std::log(static_cast<double>(hz[i])) - lo) / range;
            const qreal y = plot.top() + plot.height() *
                viper_eq::normalizedY(levels_[i], *minimum, *maximum);
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

class ViperWindow final : public QMainWindow {
public:
    ViperWindow() : settings_("ViPERforLinux", "ViPERClean") {
        setObjectName("viperWindow");
        setWindowTitle("Viper4Linux");
        // Keep the blue application/taskbar symbol stable across GUI themes.
        // Set the X11 WM icon from Qt, not by a .desktop file workaround.
        setWindowIcon(QIcon(":/viper4linux/blue.png"));
        setMinimumSize(440, 440);
        resize(740, 800);
        auto* central = new QWidget(this);
        central->setObjectName("viperCentral");
        auto* layout = new QVBoxLayout(central);
        layout->setSpacing(10);

        auto* title = new QLabel("Viper4Linux", central);
        QFont font = title->font();
        font.setPointSize(font.pointSize() + 6);
        font.setBold(true);
        title->setFont(font);
        auto* titleRow = new QHBoxLayout();
        header_icon_ = new QLabel(central);
        header_icon_->setObjectName("viperHeaderLogo");
        header_icon_->setFixedSize(38, 38);
        header_icon_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        header_icon_->setAccessibleName("Viper4Linux logo");
        header_icon_->setAlignment(Qt::AlignCenter);
        titleRow->addWidget(header_icon_);
        titleRow->addWidget(title, 1);
        auto* themeLabel = new QLabel("Theme", central);
        titleRow->addWidget(themeLabel);
        theme_selector_ = new QComboBox(central);
        theme_selector_->setObjectName("viperThemeSelector");
        theme_selector_->setAccessibleName("Interface color scheme");
        theme_selector_->setToolTip("Change only the GUI colors; audio settings are unaffected.");
        theme_selector_->addItem("Modern teal", "modern");
        theme_selector_->addItem("Classic Viper", "classic-v4a");
        theme_selector_->setCurrentIndex(
            settings_.value("ui/theme", "modern").toString() == "classic-v4a" ? 1 : 0);
        // Allow room for the longest label plus the custom arrow at different
        // font scales; the old 140px width clipped "Classic Viper" on Cinnamon.
        theme_selector_->setFixedWidth(qMax(168,
            theme_selector_->fontMetrics().horizontalAdvance("Classic Viper") + 62));
        titleRow->addWidget(theme_selector_);
        // Single compact menu instead of separate Help and About buttons.
        auto* menuButton = new QToolButton(central);
        menuButton->setObjectName("viperApplicationMenuButton");
        menuButton->setText(QString::fromUtf8("☰"));
        menuButton->setAccessibleName("Application menu");
        menuButton->setToolTip("Preferences, library, help, and quit");
        menuButton->setFixedWidth(34);
        menuButton->setPopupMode(QToolButton::InstantPopup);
        menuButton->setMenu(makeApplicationMenu(menuButton));
        titleRow->addWidget(menuButton);
        layout->addLayout(titleRow);
        // Read-only status from the background monitor. No CLI subprocesses.
        audio_output_label_ = new QLabel("Audio Output: detecting…", central);
        audio_output_label_->setObjectName("viperCurrentAudioOutput");
        audio_output_label_->setStyleSheet("color: palette(mid);");
        audio_output_label_->setToolTip("Current physical Audio Output; enable independent settings under Preferences.");
        layout->addWidget(audio_output_label_);

        // Master bypass lives outside both tabs, as in the previous working UI.
        auto* masterCard = new QFrame(central);
        masterCard->setObjectName("viperMasterCard");
        const bool darkUi = palette().color(QPalette::Window).lightness() < 130;
        const QString cardBg = darkUi ? "#293a3e" : "#ebf3f5";
        const QString cardBorder = darkUi ? "#405659" : "#d8e7e9";
        masterCard->setStyleSheet(QString("QFrame#viperMasterCard { background:%1; border:1px solid %2; border-radius:16px; }")
                                  .arg(cardBg, cardBorder));
        auto* masterRow = new QHBoxLayout(masterCard);
        masterRow->setContentsMargins(18, 13, 18, 13);
        auto* masterText = new QVBoxLayout();
        auto* masterTitle = new QLabel("Master effects", masterCard);
        QFont masterFont = masterTitle->font();
        masterFont.setBold(true);
        masterFont.setPointSize(masterFont.pointSize() + 1);
        masterTitle->setFont(masterFont);
        auto* masterDetail = new QLabel("Global processing bypass", masterCard);
        masterDetail->setStyleSheet("color: palette(mid);");
        masterText->addWidget(masterTitle);
        masterText->addWidget(masterDetail);
        masterRow->addLayout(masterText, 1);
        global_ = new EffectSwitch(masterCard);
        global_->setAccessibleName("Enable all effects globally");
        global_->setToolTip("Bypass all DSP effects without changing their individual settings or audio routing.");
        masterRow->addWidget(global_);
        layout->addWidget(masterCard);

        // Major categories use native tabs; each tab scrolls independently.
        // Master switch, presets and backend status stay outside the scrolling tabs.
        tabs_ = new QTabWidget(central);
        // Match the enabled effect switches: the active tab is dark teal with
        // white text, while inactive tabs remain quieter.  Presentation only:
        // changing tabs does not touch any DSP control or effect enable flag.
        tabs_->tabBar()->setExpanding(false);
        tabs_->tabBar()->setDrawBase(false);
        tabs_->tabBar()->setUsesScrollButtons(true); // All six categories remain reachable on small windows.
        const QString tabInactiveBg = darkUi ? "#25383c" : "#e4edef";
        const QString tabInactiveText = darkUi ? "#d9e6e8" : "#476268";
        const QString tabInactiveBorder = darkUi ? "#476268" : "#bfcfd2";
        const QString tabHoverBg = darkUi ? "#344d52" : "#d4e5e8";
        tabs_->tabBar()->setStyleSheet(QString(R"(
            QTabBar::tab {
                background-color: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 10px;
                padding: 9px 13px;
                min-width: 80px;
                margin-right: 7px;
                margin-bottom: 5px;
                font-size: 14px;
                font-weight: 600;
            }
            QTabBar::tab:selected {
                background-color: #236e76;
                color: #ffffff;
                border-color: #236e76;
            }
            QTabBar::tab:!selected:hover {
                background-color: %4;
            }
        )").arg(tabInactiveBg, tabInactiveText, tabInactiveBorder, tabHoverBg));
        // Output is the first category. The saved tab uses a stable name rather
        // than an index so future tab additions cannot silently switch views.
        // Six-tab public proof-of-concept layout. Empty categories are real
        // roadmap placeholders, never simulated DSP controls.
        auto* outputLayout = addEffectsTab("Output");
        auto* bassLayout = addEffectsTab("Bass");
        auto* effectLayout = addEffectsTab("EQ & Tone");
        auto* spatialLayout = addEffectsTab("Spatial");
        auto* dynamicsLayout = addEffectsTab("Dynamics");
        auto* correctionLayout = addEffectsTab("Sound Correction");

        // The two selectors share a flexible row; no selector grows to the
        // length of its longest label or forces the containing tab wider.
        auto* eq_group = new QGroupBox("Graphic Equalizer", tabs_);
        auto* eq_form = new QFormLayout(eq_group);
        eq_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        eq_enabled_ = new EffectSwitch(eq_group);
        eq_form->addRow("Enable native EQ", eq_enabled_);
        auto* eq_controls = new QWidget(eq_group);
        auto* eq_row = new QHBoxLayout(eq_controls);
        eq_row->setContentsMargins(0, 0, 0, 0);
        eq_row->setSpacing(6);
        eq_row->addWidget(new QLabel("Bands", eq_controls));
        eq_mode_ = new QComboBox(eq_controls);
        eq_mode_->setObjectName("viperEqBandCount");
        for (int count : {10, 15, 25, 31})
            eq_mode_->addItem(QString::number(count), count);
        eq_mode_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        eq_mode_->setMinimumContentsLength(2);
        eq_mode_->setMinimumWidth(54);
        eq_mode_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        eq_row->addWidget(eq_mode_, 2);
        eq_row->addWidget(new QLabel("Preset", eq_controls));
        eq_preset_ = new QComboBox(eq_controls);
        eq_preset_->setObjectName("viperEqPreset");
        eq_preset_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        eq_preset_->setMinimumContentsLength(9);
        eq_preset_->setMinimumWidth(92);
        eq_preset_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        eq_row->addWidget(eq_preset_, 3);
        eq_controls->setMinimumWidth(0);
        eq_controls->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        eq_form->addRow(eq_controls);
        eq_overview_ = new EqOverview(eq_group);
        eq_form->addRow(eq_overview_);
        auto* eq_actions = new QWidget(eq_group);
        auto* eq_action_row = new QHBoxLayout(eq_actions);
        eq_action_row->setContentsMargins(0, 0, 0, 0);
        eq_action_row->setSpacing(6);
        auto* eq_save = new QPushButton("Save preset…", eq_actions);
        auto* eq_delete = new QPushButton("Delete preset", eq_actions);
        eq_delete->setObjectName("viperEqDeletePreset");
        eq_delete_button_ = eq_delete;
        eq_action_row->addWidget(eq_save);
        eq_action_row->addWidget(eq_delete);
        eq_action_row->addStretch(1);
        eq_form->addRow(eq_actions);
        addEffectPanel(effectLayout, eq_group, "equalizer", eq_enabled_);

        // Dynamic EQ: matching compact, clickable maximum-target preview.
        // Ten configurable native filters live in one separate dialog.
        auto* deq_group = new QGroupBox("Dynamic EQ", tabs_);
        auto* deq_form = new QFormLayout(deq_group);
        deq_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        deq_enabled_ = new EffectSwitch(deq_group);
        deq_form->addRow("Enable native Dynamic EQ", deq_enabled_);
        deq_count_ = new QSpinBox(deq_group);
        deq_count_->setObjectName("viperDynamicEqMainBandCount");
        deq_count_->setRange(1, 10);
        deq_count_->setMinimumWidth(70);
        deq_count_->setMaximumWidth(104);
        deq_count_->setToolTip("Number of active native Dynamic EQ bands; also adjustable in the editor");
        deq_form->addRow("Active bands", deq_count_);
        auto* deq_label = new QLabel("Illustrative target curve — click to edit bands", deq_group);
        deq_label->setWordWrap(true);
        deq_form->addRow(deq_label);
        deq_overview_ = new DynamicEqOverview(deq_group);
        deq_form->addRow(deq_overview_);
        addEffectPanel(effectLayout, deq_group, "dynamic_eq", deq_enabled_);

        deq_editor_ = new QDialog(this);
        deq_editor_->setObjectName("viperDynamicEqEditor");
        deq_editor_->setWindowTitle("Viper4Linux — Dynamic EQ Editor");
        deq_editor_->resize(670, 735);
        deq_editor_->setMinimumSize(425, 440);
        auto* deq_editor_layout = new QVBoxLayout(deq_editor_);
        auto* deq_scroll = new QScrollArea(deq_editor_);
        deq_scroll->setObjectName("viperDynamicEqEditorScroll");
        deq_scroll->setWidgetResizable(true);
        deq_scroll->setFrameShape(QFrame::NoFrame);
        deq_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* deq_content = new QWidget(deq_scroll);
        auto* deq_content_layout = new QVBoxLayout(deq_content);
        deq_content_layout->setSpacing(9);
        auto* deq_explain = new QLabel(
            "Select a band in the table or on the curve. All filter settings remain editable below. "
            "The curve shows maximum configured gain, not live audio levels.", deq_content);
        deq_explain->setWordWrap(true);
        deq_content_layout->addWidget(deq_explain);
        auto* deq_top = new QHBoxLayout();
        deq_top->addWidget(new QLabel("Active bands", deq_content));
        deq_editor_count_ = new QSpinBox(deq_content);
        deq_editor_count_->setObjectName("viperDynamicEqEditorBandCount");
        deq_editor_count_->setRange(1, 10);
        deq_editor_count_->setFixedWidth(77);
        deq_top->addWidget(deq_editor_count_);
        auto* deq_spread = new QPushButton("Spread frequencies", deq_content);
        deq_spread->setObjectName("viperDynamicEqSpreadFrequencies");
        deq_spread->setToolTip("Distribute active band frequencies over bass to treble. Preserves gain, Q, threshold, attack, release and filter type.");
        deq_top->addWidget(deq_spread);
        deq_top->addStretch();
        deq_content_layout->addLayout(deq_top);
        deq_editor_graph_ = new DynamicEqOverview(deq_content);
        deq_editor_graph_->setObjectName("viperDynamicEqEditorGraph");
        deq_editor_graph_->enableBandSelection();
        deq_editor_graph_->setMinimumHeight(125);
        deq_content_layout->addWidget(deq_editor_graph_);
        deq_table_ = new QTableWidget(0, 4, deq_content);
        deq_table_->setObjectName("viperDynamicEqBandTable");
        deq_table_->setHorizontalHeaderLabels({"Band", "Frequency (Hz)", "Gain (dB)", "Threshold (dB)"});
        deq_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        deq_table_->verticalHeader()->hide();
        deq_table_->verticalHeader()->setDefaultSectionSize(25);
        deq_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        deq_table_->setSelectionMode(QAbstractItemView::SingleSelection);
        deq_table_->setEditTriggers(QAbstractItemView::DoubleClicked |
                                    QAbstractItemView::EditKeyPressed);
        deq_table_->setAlternatingRowColors(true);
        deq_table_->setWordWrap(false);
        // All active band summaries can fit without a second inner scrollbar.
        // The dialog's outer QScrollArea handles short screens instead.
        deq_table_->setMinimumHeight(115);
        deq_table_->setFixedHeight(115);
        deq_table_->setToolTip("Double-click frequency, gain, or threshold to edit; select a row to adjust its other parameters below.");
        deq_content_layout->addWidget(deq_table_);
        deq_selected_label_ = new QLabel("Selected band", deq_content);
        QFont deq_label_font = deq_selected_label_->font();
        deq_label_font.setBold(true);
        deq_selected_label_->setFont(deq_label_font);
        deq_content_layout->addWidget(deq_selected_label_);
        auto* deq_form_editor = new QFormLayout();
        deq_form_editor->setRowWrapPolicy(QFormLayout::WrapLongRows);
        deq_band_select_ = new QComboBox(deq_content);
        deq_band_select_->setObjectName("viperDynamicEqBandSelector");
        deq_form_editor->addRow("Selected band", deq_band_select_);
        deq_frequency_ = new QSpinBox(deq_content);
        deq_frequency_->setRange(20,20000);
        deq_frequency_->setSuffix(" Hz");
        deq_form_editor->addRow("Center / shelf frequency", deq_frequency_);
        deq_type_ = new QComboBox(deq_content);
        deq_type_->addItems({"Peak", "Low shelf", "High shelf"});
        deq_form_editor->addRow("Filter shape", deq_type_);
        deq_q_ = new QDoubleSpinBox(deq_content);
        deq_q_->setRange(0.5,8.0); deq_q_->setDecimals(2); deq_q_->setSingleStep(0.1);
        deq_form_editor->addRow("Q (bandwidth)", deq_q_);
        deq_gain_ = new QDoubleSpinBox(deq_content);
        deq_gain_->setRange(-12,12); deq_gain_->setDecimals(2);
        deq_gain_->setSingleStep(0.5); deq_gain_->setSuffix(" dB");
        deq_form_editor->addRow("Maximum gain", deq_gain_);
        deq_threshold_ = new QDoubleSpinBox(deq_content);
        deq_threshold_->setRange(-80,0); deq_threshold_->setDecimals(1);
        deq_threshold_->setSuffix(" dB");
        deq_form_editor->addRow("Threshold", deq_threshold_);
        deq_attack_ = new QSpinBox(deq_content);
        deq_attack_->setRange(1,100); deq_attack_->setSuffix(" ms");
        deq_form_editor->addRow("Attack", deq_attack_);
        deq_release_ = new QSpinBox(deq_content);
        deq_release_->setRange(10,500); deq_release_->setSuffix(" ms");
        deq_form_editor->addRow("Release", deq_release_);
        deq_content_layout->addLayout(deq_form_editor);
        deq_content_layout->addStretch();
        deq_scroll->setWidget(deq_content);
        deq_editor_layout->addWidget(deq_scroll, 1);
        auto* deq_actions = new QHBoxLayout();
        deq_actions->addStretch();
        auto* deq_close = new QPushButton("Close", deq_editor_);
        deq_actions->addWidget(deq_close);
        deq_editor_layout->addLayout(deq_actions);
        connect(deq_close, &QPushButton::clicked, deq_editor_, &QDialog::hide);
        deq_overview_->selectBand = [this](int index) {
            if (index >= 0 && index < deq_active_count_)
                deq_band_select_->setCurrentIndex(index);
        };
        deq_overview_->openEditor = [this] {
            deq_editor_->show(); deq_editor_->raise(); deq_editor_->activateWindow();
        };
        deq_editor_graph_->selectBand = [this](int index) {
            if (index >= 0 && index < deq_active_count_)
                deq_band_select_->setCurrentIndex(index);
        };
        connect(deq_table_, &QTableWidget::cellClicked, this,
                [this](int row, int) {
            if (row >= 0 && row < deq_active_count_)
                deq_band_select_->setCurrentIndex(row);
        });
        connect(deq_table_, &QTableWidget::currentCellChanged, this,
                [this](int row, int, int, int) {
            if (row >= 0 && row < deq_active_count_)
                deq_band_select_->setCurrentIndex(row);
        });
        connect(deq_table_, &QTableWidget::itemChanged, this,
                [this](QTableWidgetItem* item) { commitDeqTableItem(item); });

        // The only 31-fader bank lives in a retained, nonmodal editor dialog.
        // Closing the dialog HIDES it; switching modes never destroys faders.
        eq_editor_ = new QDialog(this);
        eq_editor_->setWindowTitle("ViPER Graphic EQ — Detailed Editor");
        eq_editor_->setObjectName("viperEqEditor");
        eq_editor_->resize(720, 355);
        eq_editor_->setMinimumSize(340, 320);
        auto* editorLayout = new QVBoxLayout(eq_editor_);
        editorLayout->addWidget(new QLabel("Drag individual faders to adjust the current EQ curve.", eq_editor_));
        auto* eq_scroll = new QScrollArea(eq_editor_);
        eq_scroll->setObjectName("viperEqScroller");
        eq_scroll->setWidgetResizable(false);
        eq_scroll->setFrameShape(QFrame::NoFrame);
        eq_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        eq_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        eq_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        eq_fader_container_ = new QWidget(eq_scroll);
        eq_fader_container_->setObjectName("viperEqFaders");
        auto* eq_fader_row = new QHBoxLayout(eq_fader_container_);
        eq_fader_row->setSpacing(3);
        eq_fader_row->setContentsMargins(3, 0, 3, 3);
        for (int i = 0; i < 31; ++i) {
            auto* column = new QWidget(eq_fader_container_);
            column->setFixedWidth(36);
            auto* stack = new QVBoxLayout(column);
            stack->setSpacing(2);
            stack->setContentsMargins(0, 0, 0, 0);
            auto* value = new QLabel("0", column);
            value->setObjectName("viperEqValue");
            value->setAlignment(Qt::AlignCenter);
            value->setProperty("viperSliderValue", true);
            auto* fader = new SafeSlider(Qt::Vertical, column);
            fader->setObjectName("viperEqFader");
            fader->setRange(-1200, 1200);  // hundredths of a decibel
            fader->setSingleStep(5);
            fader->setPageStep(25);
            fader->setTickPosition(QSlider::NoTicks);
            fader->setMinimumHeight(150);
            fader->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            auto* frequency = new QLabel("", column);
            frequency->setAlignment(Qt::AlignCenter);
            stack->addWidget(value);
            stack->addWidget(fader, 1, Qt::AlignHCenter);
            stack->addWidget(frequency);
            eq_columns_[i] = column;
            eq_faders_[i] = fader;
            eq_values_[i] = value;
            eq_frequencies_[i] = frequency;
            eq_fader_row->addWidget(column);
        }
        eq_fader_container_->setFixedSize(31 * 39 + 6, 235);
        eq_scroll->setWidget(eq_fader_container_);
        editorLayout->addWidget(eq_scroll, 1);
        auto* editorActions = new QHBoxLayout();
        auto* eq_reset = new QPushButton("Flat", eq_editor_);
        eq_reset->setToolTip("Set all visible bands in the current layout to 0 dB.");
        editorActions->addWidget(eq_reset);
        editorActions->addStretch();
        auto* eq_close = new QPushButton("Close", eq_editor_);
        editorActions->addWidget(eq_close);
        editorLayout->addLayout(editorActions);
        connect(eq_close, &QPushButton::clicked, eq_editor_, &QDialog::hide);
        eq_overview_->openEditor = [this] {
            eq_editor_->show();
            eq_editor_->raise();
            eq_editor_->activateWindow();
        };
        // Read the saved preset identifier BEFORE populating the combo box.
        // populateEqPresets() persists selection, so doing this later would
        // silently overwrite the previous launch's choice with "Custom".
        migrateEqPrecision();
        eq_selected_preset_ = settings_.value("eq/selected_preset", "custom").toString();
        populateEqPresets();
        connect(eq_save, &QPushButton::clicked, this, [this] { saveUserEqPreset(); });
        connect(eq_delete, &QPushButton::clicked, this, [this] { deleteUserEqPreset(); });
        connect(eq_preset_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (index < 0) return;
            const QString id = eq_preset_->itemData(index).toString();
            eq_selected_preset_ = id;
            if (id != "custom") applyEqPreset(id);
            else { loadEqLayout(); push(); }
            updateEqDeleteButton();
        });

        auto* fet_group = new QGroupBox("FET Compressor", tabs_);
        auto* fet_form = new QFormLayout(fet_group);
        fet_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        fet_enabled_ = new EffectSwitch(fet_group);
        fet_form->addRow("Enable FET dynamic compression", fet_enabled_);
        fet_threshold_ = addSlider(fet_form, "Threshold", -48, 0);
        fet_ratio_ = addSlider(fet_form, "Compression ratio", 10, 100);
        fet_knee_ = addSlider(fet_form, "Soft knee", 0, 12);
        fet_gain_ = addSlider(fet_form, "Makeup gain", 0, 24);
        fet_attack_ = addSlider(fet_form, "Attack", 1, 100);
        fet_release_ = addSlider(fet_form, "Release", 5, 500);
        auto* fet_note = new QLabel("Native ViPERDSP FET compression; start with conservative settings. "
                                    "Keep off when you want to preserve recording dynamics.", fet_group);
        fet_note->setWordWrap(true);
        fet_form->addRow(fet_note);
        addEffectPanel(dynamicsLayout, fet_group, "fet", fet_enabled_);
        auto* mb_group = new QGroupBox("Multiband Compressor", tabs_);
        auto* mb_form = new QFormLayout(mb_group);
        mb_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        mb_enabled_ = new EffectSwitch(mb_group);
        mb_form->addRow("Enable native multiband compression", mb_enabled_);
        mb_band_count_ = new QComboBox(mb_group);
        mb_band_count_->addItems({"3 bands", "5 bands"});
        mb_form->addRow("Processing bands", mb_band_count_);
        // Frequency bands follow bass -> treble. Select one to edit; all five
        // native compressor instances and their saved values remain intact.
        mb_band_tabs_ = new QTabWidget(mb_group);
        mb_band_tabs_->setObjectName("viperMultibandBandSelector");
        mb_band_tabs_->setDocumentMode(true);
        mb_band_tabs_->setUsesScrollButtons(true);
        mb_band_tabs_->tabBar()->setExpanding(true);
        mb_band_tabs_->setMinimumWidth(0);
        mb_band_tabs_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        mb_band_tabs_->setAccessibleName("Multiband frequency bands, bass to treble");
        for (int i = 0; i < 5; ++i) {
            auto* page = new QWidget(mb_band_tabs_);
            auto* form = new QFormLayout(page);
            form->setRowWrapPolicy(QFormLayout::WrapLongRows);
            mb_band_ranges_[i] = new QLabel(page);
            mb_band_ranges_[i]->setWordWrap(true);
            form->addRow(mb_band_ranges_[i]);
            mb_bands_[i].enabled = new QCheckBox("Compress this band", page);
            form->addRow(mb_bands_[i].enabled);
            mb_bands_[i].threshold = addSlider(form, "Threshold", -48, 0);
            mb_bands_[i].ratio = addSlider(form, "Ratio", 10, 100);
            mb_bands_[i].knee = addSlider(form, "Soft knee", 0, 12);
            mb_bands_[i].gain = addSlider(form, "Makeup gain", 0, 24);
            mb_bands_[i].attack = addSlider(form, "Attack", 1, 100);
            mb_bands_[i].release = addSlider(form, "Release", 5, 500);
            mb_band_tabs_->addTab(page, QString("Band %1").arg(i+1));
        }
        mb_form->addRow(mb_band_tabs_);
        auto* mb_crossover_toggle = new QToolButton(mb_group);
        mb_crossover_toggle->setObjectName("viperMultibandCrossoverToggle");
        mb_crossover_toggle->setText("Edit crossover frequencies");
        mb_crossover_toggle->setCheckable(true);
        mb_crossover_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        mb_crossover_toggle->setArrowType(Qt::RightArrow);
        mb_crossover_toggle->setToolTip("Adjust boundaries between the frequency bands");
        mb_form->addRow(mb_crossover_toggle);
        mb_crossover_panel_ = new QWidget(mb_group);
        auto* mb_crossover_form = new QFormLayout(mb_crossover_panel_);
        mb_crossover_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        for (int i = 0; i < 4; ++i) {
            const QByteArray caption = QString("Crossover %1").arg(i+1).toUtf8();
            mb_crossovers_[i] = addSlider(mb_crossover_form, caption.constData(), 80, 15000);
        }
        mb_form->addRow(mb_crossover_panel_);
        mb_crossover_panel_->setVisible(false);
        connect(mb_crossover_toggle, &QToolButton::toggled, this,
                [this, mb_crossover_toggle](bool expanded) {
            mb_crossover_panel_->setVisible(expanded);
            mb_crossover_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        });
        mb_band_tabs_->setCurrentIndex(std::clamp(settings_.value("ui/mb_selected_band", 0).toInt(), 0, 4));
        connect(mb_band_tabs_, &QTabWidget::currentChanged, this, [this](int index) {
            if (index >= 0) settings_.setValue("ui/mb_selected_band", index);
        });
        auto* mb_note = new QLabel("Each active band uses native ViPERDSP FET compression. "
                                   "Start with low ratios and watch loudness; crossovers must increase.", mb_group);
        mb_note->setWordWrap(true);
        mb_form->addRow(mb_note);
        addEffectPanel(dynamicsLayout, mb_group, "multiband", mb_enabled_);
        auto* ddc_group = new QGroupBox("ViPER-DDC", tabs_);
        auto* ddc_form = new QFormLayout(ddc_group);
        ddc_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        ddc_enabled_ = new EffectSwitch(ddc_group);
        ddc_form->addRow("Enable device correction", ddc_enabled_);
        auto* ddc_file_row = new QWidget(ddc_group);
        auto* ddc_buttons = new QHBoxLayout(ddc_file_row);
        ddc_buttons->setContentsMargins(0,0,0,0);
        ddc_file_row->setMinimumWidth(0);
        ddc_file_row->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        ddc_library_ = new IrsComboBox(ddc_file_row);
        ddc_library_->setMinimumWidth(0);
        ddc_library_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        ddc_library_->setToolTip("Select a .vdc profile from your local VDC library");
        ddc_library_->beforeOpen = [this] {
            if (ddc_library_dirty_) refreshDdcLibrary();
        };
        auto* importDdc = new QPushButton("Import…", ddc_file_row);
        auto* refreshDdc = new QPushButton("Refresh", ddc_file_row);
        ddc_buttons->addWidget(ddc_library_, 1);
        ddc_buttons->addWidget(importDdc);
        ddc_buttons->addWidget(refreshDdc);
        ddc_form->addRow("Correction profile", ddc_file_row);
        ddc_file_name_ = new QLabel(ddc_group);
        ddc_file_name_->setWordWrap(true);
        ddc_file_name_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        ddc_form->addRow(ddc_file_name_);
        ddc_status_ = new QLabel("No DDC profile loaded.", ddc_group);
        ddc_status_->setWordWrap(true);
        ddc_form->addRow(ddc_status_);
        auto* ddc_note = new QLabel(
            "Select a .vdc correction profile designed for your specific headphone model. "
            "Correction is available at 44.1 and 48 kHz; at other processing rates, "
            "ViPER-DDC is bypassed. Default: off.", ddc_group);
        ddc_note->setWordWrap(true);
        ddc_form->addRow(ddc_note);
        addEffectPanel(correctionLayout, ddc_group, "ddc", ddc_enabled_);
        refreshDdcLibrary();
        ddc_watcher_ = new QFileSystemWatcher(this);
        ddc_watcher_->addPath(ddcDirectory());
        connect(ddc_watcher_, &QFileSystemWatcher::directoryChanged, this,
                [this](const QString&) {
            ddc_library_dirty_ = true;
            if (!ddc_library_->popupIsOpen()) refreshDdcLibrary();
        });
        connect(ddc_library_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (index >= 0) selectDdcProfile(ddc_library_->currentData().toString());
        });
        connect(refreshDdc, &QPushButton::clicked, this, [this] { refreshDdcLibrary(); });
        connect(importDdc, &QPushButton::clicked, this, [this] {
            const QString selected = QFileDialog::getOpenFileName(this, "Import ViPER-DDC profile",
                ddcDirectory(), "ViPER correction (*.vdc *.VDC)");
            if (selected.isEmpty()) return;
            const QFileInfo file(selected);
            if (!file.isFile() || !file.isReadable() ||
                file.suffix().compare("vdc", Qt::CaseInsensitive) != 0) {
                QMessageBox::warning(this, "VDC import", "Select a readable .vdc profile.");
                return;
            }
            if (!QDir().mkpath(ddcDirectory())) {
                QMessageBox::warning(this, "VDC import", "Could not create VDC library directory.");
                return;
            }
            QString destination = QDir(ddcDirectory()).filePath(file.fileName());
            if (QFileInfo(destination).canonicalFilePath() != file.canonicalFilePath()) {
                unsigned serial = 2;
                while (QFile::exists(destination))
                    destination = QDir(ddcDirectory()).filePath(file.completeBaseName() +
                        " (" + QString::number(serial++) + ")." + file.suffix());
                if (!QFile::copy(selected, destination)) {
                    QMessageBox::warning(this, "VDC import", "Could not copy the selected profile.");
                    return;
                }
            }
            selectDdcProfile(destination);
        });
        auto* speaker_group = new QGroupBox("Speaker Correction", tabs_);
        auto* speaker_form = new QFormLayout(speaker_group);
        speaker_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        speaker_enabled_ = new EffectSwitch(speaker_group);
        speaker_form->addRow("Enable native speaker correction", speaker_enabled_);
        auto* speaker_note = new QLabel(
            "For loudspeakers only, not headphones. Uses ViPERDSP's built-in high-pass, "
            "low-pass and band-pass filters; no correction file required. Default: off.", speaker_group);
        speaker_note->setWordWrap(true);
        speaker_form->addRow(speaker_note);
        addEffectPanel(correctionLayout, speaker_group, "speaker_correction", speaker_enabled_);
        auto* conv_group = new QGroupBox("Convolver", tabs_);
        auto* conv_form = new QFormLayout(conv_group);
        // On a narrow screen, put the controls on a new line rather than
        // allowing a long label/row to force horizontal overflow.
        conv_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        convolver_enabled_ = new EffectSwitch(conv_group);
        conv_form->addRow("Enable native convolution", convolver_enabled_);
        auto* fileRow = new QWidget(conv_group);
        auto* fileLayout = new QHBoxLayout(fileRow);
        fileLayout->setContentsMargins(0,0,0,0);
        fileLayout->setSpacing(6);
        fileRow->setMinimumWidth(0);
        fileRow->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        convolver_library_ = new IrsComboBox(fileRow);
        convolver_library_->beforeOpen = [this] {
            // Refresh only after closing and reopening if the library changed
            // while the dropdown was open. Never reset an open popup's scroll.
            if (irs_library_dirty_) refreshIrsLibrary();
        };
        convolver_library_->setToolTip("Select an impulse response. Hover to see the full filename.");
        auto* browse = new QPushButton("Import…", fileRow);
        auto* refresh = new QPushButton("Refresh / Reload", fileRow);
        fileLayout->addWidget(convolver_library_, 1);
        fileLayout->addWidget(browse);
        fileLayout->addWidget(refresh);
        conv_form->addRow("Impulse response", fileRow);
        // Unlike the compact combo box, this line shows the complete selected
        // filename. It can wrap and be selected/copied even on a small screen.
        convolver_full_name_ = new QPlainTextEdit(conv_group);
        convolver_full_name_->setObjectName("viperIrsFullName");
        convolver_full_name_->setReadOnly(true);
        convolver_full_name_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        // Unlike QLabel wordWrap, this works even with 250 uninterrupted
        // characters. The entire original name remains selectable/copyable;
        // excess lines can scroll vertically, never horizontally.
        convolver_full_name_->setWordWrapMode(QTextOption::WrapAnywhere);
        convolver_full_name_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        convolver_full_name_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        convolver_full_name_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        convolver_full_name_->setFixedHeight(52);
        convolver_full_name_->setMinimumWidth(0);
        convolver_full_name_->setAccessibleName("Full selected IRS filename");
        conv_form->addRow(convolver_full_name_);
        convolver_cross_ = addSlider(conv_form, "Cross-channel mix", 0, 100);
        convolver_status_ = new QLabel("Select a kernel to load it into the running audio backend.", conv_group);
        convolver_status_->setWordWrap(true);
        conv_form->addRow(convolver_status_);
        addEffectPanel(correctionLayout, conv_group, "convolver", convolver_enabled_);
        refreshIrsLibrary();
        irs_watcher_ = new QFileSystemWatcher(this);
        irs_watcher_->addPath(irsDirectory());
        connect(irs_watcher_, &QFileSystemWatcher::directoryChanged, this,
                [this](const QString&) {
            // A closed library updates as soon as Qt reports a folder change.
            // An open popup is never rebuilt underneath the user.
            irs_library_dirty_ = true;
            if (!convolver_library_->popupIsOpen()) refreshIrsLibrary();
        });
        connect(convolver_library_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index < 0) return;
                selectIrPath(convolver_library_->currentData().toString());
            });
        connect(refresh, &QPushButton::clicked, this, [this] { refreshIrsLibrary(); settings_.sync(); requestIrsLoad(); });
        connect(browse, &QPushButton::clicked, this, [this] {
            const QStringList selected = QFileDialog::getOpenFileNames(this, "Import impulse responses",
                irsDirectory(), "Impulse responses (*.irs *.IRS *.wav *.WAV);;All files (*)");
            if (selected.isEmpty()) return;
            if (!QDir().mkpath(irsDirectory())) {
                QMessageBox::warning(this, "IRS import", "Could not create the IRS library directory.");
                return;
            }
            QString firstImported;
            QStringList failures;
            for (const QString& selectedFile : selected) {
                const QFileInfo file(selectedFile);
                if (!file.isFile() || !file.isReadable() ||
                    (file.suffix().compare("irs", Qt::CaseInsensitive) != 0 &&
                     file.suffix().compare("wav", Qt::CaseInsensitive) != 0)) {
                    failures << file.fileName() + ": not a readable .irs or .wav file";
                    continue;
                }
                QString destination = QDir(irsDirectory()).filePath(file.fileName());
                // Browsing a library file is a selection, not an import.
                if (QFileInfo(destination).canonicalFilePath() != file.canonicalFilePath()) {
                    const QString base = file.completeBaseName(), suffix = file.suffix();
                    unsigned serial = 2;
                    while (QFile::exists(destination)) {
                        destination = QDir(irsDirectory()).filePath(base + " (" + QString::number(serial++) + ")" +
                            (suffix.isEmpty() ? QString() : "." + suffix));
                    }
                    if (!QFile::copy(selectedFile, destination)) {
                        failures << file.fileName() + ": could not copy";
                        continue;
                    }
                }
                if (firstImported.isEmpty()) firstImported = destination;
            }
            // Only the first imported file becomes active; others appear in the
            // library for immediate switching. Keep previous choice on failure.
            if (!firstImported.isEmpty()) selectIrPath(firstImported);
            refreshIrsLibrary();
            if (!failures.isEmpty()) {
                QMessageBox::warning(this, "IRS import",
                    "Some files could not be imported:\n" + failures.join("\n"));
            }
        });

        auto* group = new QGroupBox("ViPER Bass", tabs_);
        auto* form = new QFormLayout(group);
        enabled_ = new EffectSwitch(group);
        form->addRow("Bass processing", enabled_);
        bass_mode_ = new QComboBox(group);
        bass_mode_->addItems({"Natural", "Pure Bass+", "Subwoofer"});
        bass_mode_->setToolTip("Select one of the three original ViPERDSP Bass processing algorithms.");
        form->addRow("Bass mode", bass_mode_);
        strength_ = addSlider(form, "Bass strength", 5, 100); // tenths
        frequency_ = addSlider(form, "Frequency", 15, 150);
        addEffectPanel(bassLayout, group, "bass", enabled_);

        auto* mono_group = new QGroupBox("ViPER Bass Mono", tabs_);
        auto* mono_form = new QFormLayout(mono_group);
        mono_enabled_ = new EffectSwitch(mono_group);
        mono_form->addRow("Mono bass processing", mono_enabled_);
        mono_mode_ = new QComboBox(mono_group);
        mono_mode_->addItems({"Natural", "Pure Bass+", "Subwoofer"});
        mono_form->addRow("Bass mode", mono_mode_);
        mono_strength_ = addSlider(mono_form, "Bass strength", 5, 100);
        mono_frequency_ = addSlider(mono_form, "Frequency", 15, 150);
        auto* mono_note = new QLabel("Mono-derived bass is blended into both channels. Start at a low strength.", mono_group);
        mono_note->setWordWrap(true);
        mono_form->addRow(mono_note);
        addEffectPanel(bassLayout, mono_group, "bass_mono", mono_enabled_);

        auto* psycho_group = new QGroupBox("Psychoacoustic Bass", tabs_);
        auto* psycho_form = new QFormLayout(psycho_group);
        psycho_enabled_ = new EffectSwitch(psycho_group);
        psycho_form->addRow("Harmonic bass enhancement", psycho_enabled_);
        psycho_cutoff_ = addSlider(psycho_form, "Bass cutoff", 60, 150);
        psycho_intensity_ = addSlider(psycho_form, "Harmonic intensity", 0, 100);
        psycho_order_ = new QComboBox(psycho_group);
        psycho_order_->addItems({"2nd order", "3rd order", "4th order", "5th order"});
        psycho_form->addRow("Harmonic order", psycho_order_);
        psycho_original_ = addSlider(psycho_form, "Original bass level", 0, 100);
        addEffectPanel(bassLayout, psycho_group, "psycho_bass", psycho_enabled_);

        auto* dyn_group = new QGroupBox("Dynamic System", tabs_);
        auto* dyn_form = new QFormLayout(dyn_group);
        dyn_enabled_ = new EffectSwitch(dyn_group);
        dyn_form->addRow("Dynamic bass processing", dyn_enabled_);
        dyn_xlo_ = addSlider(dyn_form, "Low X frequency", 0, 2400);
        dyn_xhi_ = addSlider(dyn_form, "High X frequency", 0, 12000);
        dyn_ylo_ = addSlider(dyn_form, "Low Y frequency", 0, 200);
        dyn_yhi_ = addSlider(dyn_form, "High Y frequency", 0, 300);
        dyn_side_lo_ = addSlider(dyn_form, "Low side gain", 0, 100);
        dyn_side_hi_ = addSlider(dyn_form, "High side gain", 0, 100);
        dyn_strength_ = addSlider(dyn_form, "Dynamic bass strength", 0, 100);
        auto* dyn_note = new QLabel("Advanced crossover/dynamic-bass controls. Enable only after setting a conservative strength.", dyn_group);
        dyn_note->setWordWrap(true);
        dyn_form->addRow(dyn_note);
        addEffectPanel(bassLayout, dyn_group, "dynamic_bass", dyn_enabled_);

        auto* clarity_group = new QGroupBox("ViPER Clarity", central);
        auto* clarity_form = new QFormLayout(clarity_group);
        clarity_enabled_ = new EffectSwitch(clarity_group);
        clarity_form->addRow("Clarity processing", clarity_enabled_);
        clarity_mode_ = new QComboBox(clarity_group);
        clarity_mode_->addItems({"Natural", "Ozone", "XHiFi"});
        clarity_form->addRow("Clarity mode", clarity_mode_);
        clarity_gain_ = addSlider(clarity_form, "Clarity strength", 0, 45); // tenths, 0..4.5
        addEffectPanel(effectLayout, clarity_group, "clarity", clarity_enabled_);

        auto* spectrum_group = new QGroupBox("Spectrum Extension", central);
        auto* spectrum_form = new QFormLayout(spectrum_group);
        spectrum_enabled_ = new EffectSwitch(spectrum_group);
        spectrum_form->addRow("Spectrum processing", spectrum_enabled_);
        spectrum_frequency_ = addSlider(spectrum_form, "Reference frequency", 22, 82); // 100 Hz increments
        spectrum_frequency_.slider->setToolTip("Reference frequency, 2200–8200 Hz (100 Hz steps).");
        spectrum_exciter_ = addSlider(spectrum_form, "Harmonic strength", 0, 60); // tenths, 0..6
        spectrum_exciter_.slider->setToolTip("Added harmonic enhancement, 0.0–6.0.");
        addEffectPanel(effectLayout, spectrum_group, "spectrum", spectrum_enabled_);

        auto* crossfeed_group = new QGroupBox("Crossfeed (ViPER Cure)", tabs_);
        auto* crossfeed_form = new QFormLayout(crossfeed_group);
        crossfeed_enabled_ = new EffectSwitch(crossfeed_group);
        crossfeed_form->addRow("Speaker-like headphone crossfeed", crossfeed_enabled_);
        crossfeed_mode_ = new QComboBox(crossfeed_group);
        crossfeed_mode_->addItems({"Mild", "Medium", "Strong"});
        crossfeed_mode_->setToolTip("Native ViPERDSP Cure: filtered left/right crossfeed presets.");
        crossfeed_form->addRow("Crossfeed strength", crossfeed_mode_);
        addEffectPanel(spatialLayout, crossfeed_group, "crossfeed", crossfeed_enabled_);

        auto* headphone_group = new QGroupBox("Headphone Surround+", tabs_);
        auto* headphone_form = new QFormLayout(headphone_group);
        headphone_enabled_ = new EffectSwitch(headphone_group);
        headphone_form->addRow("Surround processing", headphone_enabled_);
        headphone_quality_ = new QComboBox(headphone_group);
        headphone_quality_->addItems({"Level 1", "Level 2", "Level 3", "Level 4", "Level 5"});
        headphone_quality_->setToolTip("Five distinct upstream VHE surround profiles; higher levels are not necessarily better. Supported at 44.1 and 48 kHz.");
        headphone_form->addRow("Surround level", headphone_quality_);
        addEffectPanel(spatialLayout, headphone_group, "headphone", headphone_enabled_);

        auto* diff_group = new QGroupBox("Differential Surround", tabs_);
        auto* diff_form = new QFormLayout(diff_group);
        diff_enabled_ = new EffectSwitch(diff_group);
        diff_form->addRow("Surround processing", diff_enabled_);
        diff_delay_ = addSlider(diff_form, "Channel delay", 1, 20);
        diff_delay_.slider->setToolTip("Delay one stereo channel by 1–20 milliseconds.");
        diff_reverse_ = new QCheckBox("Delay left instead of right", diff_group);
        diff_form->addRow("Reverse channel", diff_reverse_);
        diff_mix_ = addSlider(diff_form, "Delayed channel mix", 0, 100);
        diff_mix_.slider->setToolTip("0% fully copies the direct channel; 100% uses only delayed channel.");
        diff_lowpass_ = addSlider(diff_form, "Delayed-channel low-pass", 0, 200);
        diff_lowpass_.slider->setToolTip("0 means off; other values are in 100 Hz steps, up to 20,000 Hz.");
        addEffectPanel(spatialLayout, diff_group, "differential", diff_enabled_);

        auto* field_group = new QGroupBox("Field Surround", tabs_);
        auto* field_form = new QFormLayout(field_group);
        field_enabled_ = new EffectSwitch(field_group);
        field_form->addRow("Spatial processing", field_enabled_);
        field_widen_ = addSlider(field_form, "Stereo widening", 0, 80);
        field_widen_.slider->setToolTip("Upstream widening parameter, 0.0–8.0 in 0.1 steps. Start low.");
        field_mid_ = addSlider(field_form, "Mid image", 100, 200);
        field_mid_.slider->setToolTip("Upstream mid-image parameter, 1.00–2.00 in 0.01 steps.");
        field_depth_ = addSlider(field_form, "Depth", 0, 16);
        field_depth_.slider->setToolTip("0 disables depth; 1–16 select upstream 200–950 in steps of 50.");
        addEffectPanel(spatialLayout, field_group, "field", field_enabled_);

        auto* reverb_group = new QGroupBox("Reverberation", tabs_);
        auto* reverb_form = new QFormLayout(reverb_group);
        reverb_enabled_ = new EffectSwitch(reverb_group);
        reverb_form->addRow("Room effect", reverb_enabled_);
        reverb_room_ = addSlider(reverb_form, "Room size", 0, 100);
        reverb_room_.slider->setToolTip("ViPERDSP room size, 0–100% (internally 0.0–1.0).");
        reverb_width_ = addSlider(reverb_form, "Stereo width", 0, 100);
        reverb_width_.slider->setToolTip("Width of the simulated reverberant field.");
        reverb_damp_ = addSlider(reverb_form, "Damping", 0, 100);
        reverb_damp_.slider->setToolTip("Higher damping reduces high-frequency reverberation.");
        reverb_wet_ = addSlider(reverb_form, "Reverb wet level", 0, 100);
        reverb_wet_.slider->setToolTip("Reverberant signal level. Start at 20% and adjust gently.");
        reverb_dry_ = addSlider(reverb_form, "Reverb dry level", 0, 100);
        reverb_dry_.slider->setToolTip("Original direct signal level in the reverberation model. Use 50% for unity dry gain (upstream scales dry by 2).");
        addEffectPanel(spatialLayout, reverb_group, "reverb", reverb_enabled_);
        // Native ViPERDSP Stereo Imager: independent low, mid and high width.
        // Set 100% for neutral width and keep the entire effect off by default.
        auto* imager_group = new QGroupBox("Stereo Imager", tabs_);
        auto* imager_form = new QFormLayout(imager_group);
        imager_enabled_ = new EffectSwitch(imager_group);
        imager_form->addRow("Three-band stereo imaging", imager_enabled_);
        imager_low_ = addSlider(imager_form, "Low-band width", 0, 200);
        imager_mid_ = addSlider(imager_form, "Mid-band width", 0, 200);
        imager_high_ = addSlider(imager_form, "High-band width", 0, 200);
        for (const auto& r : {imager_low_, imager_mid_, imager_high_})
            r.slider->setToolTip("100% = normal width; below narrows, above widens, up to 200%.");
        imager_low_xover_ = addSlider(imager_form, "Low crossover", 80, 400);
        imager_high_xover_ = addSlider(imager_form, "High crossover", 20, 80); // 100 Hz steps
        imager_low_xover_.slider->setToolTip("Low/mid crossover: 80 to 400 Hz.");
        imager_high_xover_.slider->setToolTip("Mid/high crossover: 2,000 to 8,000 Hz.");
        addEffectPanel(spatialLayout, imager_group, "imager", imager_enabled_);

        // Tube Simulator is the native ViPERDSP TubeSimulator: no third-party DSP.
        auto* tube_group = new QGroupBox("Tube Simulator", tabs_);
        auto* tube_form = new QFormLayout(tube_group);
        tube_enabled_ = new EffectSwitch(tube_group);
        tube_form->addRow("Enable tube simulation", tube_enabled_);
        tube_model_ = new QComboBox(tube_group);
        tube_model_->addItems({"12AX7 (High Gain)", "12AU7 (Low Gain)", "6N1P (Medium Gain)"});
        tube_model_->setToolTip("Native ViPERDSP vacuum-tube models.");
        tube_form->addRow("Tube model", tube_model_);
        tube_drive_ = addSlider(tube_form, "Drive", 0, 100);
        tube_drive_.slider->setToolTip("Tube harmonic drive; 0–100% (native range 0.0–1.0).");
        tube_mix_ = addSlider(tube_form, "Wet/Dry mix", 0, 100);
        tube_mix_.slider->setToolTip("How much processed tube signal to blend in; 0% dry, 100% wet.");
        addEffectPanel(effectLayout, tube_group, "tube", tube_enabled_);

        // AnalogX has exactly the three upstream modes; no invented controls.
        auto* analogx_group = new QGroupBox("AnalogX", tabs_);
        auto* analogx_form = new QFormLayout(analogx_group);
        analogx_enabled_ = new EffectSwitch(analogx_group);
        analogx_form->addRow("Enable AnalogX", analogx_enabled_);
        analogx_mode_ = new QComboBox(analogx_group);
        analogx_mode_->addItems({"Mild", "Medium", "Strong"});
        analogx_mode_->setToolTip("Native ViPERDSP AnalogX coloration modes.");
        analogx_form->addRow("Processing mode", analogx_mode_);
        addEffectPanel(effectLayout, analogx_group, "analogx", analogx_enabled_);

        // Audio enters native ViPERDSP at unity: no adapter input trim.

        // Output processing — native limiter is always present, so show its
        // switch as ON but inactive.  Its card remains independently collapsible.
        auto* limiter_group = new QGroupBox("Master Limiter", tabs_);
        auto* limiter_form = new QFormLayout(limiter_group);
        limiter_indicator_ = new EffectSwitch(limiter_group);
        limiter_indicator_->setChecked(true);
        limiter_indicator_->setEnabled(false);
        limiter_indicator_->setToolTip("The native Master Limiter is permanently active. Adjust its threshold, output level and balance below.");
        limiter_form->addRow("Limiter always active", limiter_indicator_);
        limiter_threshold_ = addSlider(limiter_form, "Peak threshold", 300, 1000); // thousandths
        limiter_threshold_.slider->setToolTip("Linear peak threshold, -10.46 to 0 dBFS; default -6.02 dBFS.");
        limiter_gain_ = addSlider(limiter_form, "Output gain", 1, 200); // percent
        limiter_gain_.slider->setToolTip("Final output gain, 1–200%. Raising gain can cause more limiting.");
        limiter_pan_ = addSlider(limiter_form, "Channel balance", -100, 100);
        limiter_pan_.slider->setToolTip("-100 left; 0 center; +100 right. Attenuates the opposite channel.");
        addEffectPanel(outputLayout, limiter_group, "limiter", limiter_indicator_);

        auto* agc_group = new QGroupBox("Playback Gain Control (AGC)", tabs_);
        auto* agc_form = new QFormLayout(agc_group);
        agc_enabled_ = new EffectSwitch(agc_group);
        agc_form->addRow("Automatic gain control", agc_enabled_);
        agc_strength_ = addSlider(agc_form, "Strength", 50, 300);
        agc_max_gain_ = addSlider(agc_form, "Maximum gain", 100, 1000);
        agc_threshold_ = addSlider(agc_form, "Output threshold", 300, 1000);
        addEffectPanel(outputLayout, agc_group, "agc", agc_enabled_);

        auto* lufs_group = new QGroupBox("LUFS Targeting", tabs_);
        auto* lufs_form = new QFormLayout(lufs_group);
        lufs_enabled_ = new EffectSwitch(lufs_group);
        lufs_form->addRow("Automatic loudness targeting", lufs_enabled_);
        lufs_target_ = addSlider(lufs_form, "Target loudness", -240, -80);
        lufs_gain_ = addSlider(lufs_form, "Maximum gain adjustment", 0, 120);
        lufs_speed_ = new QComboBox(lufs_group);
        lufs_speed_->addItems({"Slow", "Normal", "Fast"});
        lufs_form->addRow("Response speed", lufs_speed_);
        addEffectPanel(outputLayout, lufs_group, "lufs", lufs_enabled_);
        output_warning_ = new QLabel("AGC and LUFS are both enabled. Their gain adjustments may interact; test carefully.", tabs_);
        output_warning_->setWordWrap(true);
        output_warning_->setProperty("viperWarningText", true);
        output_warning_->setStyleSheet("color: #b36c20; font-weight: 600;");
        outputLayout->insertWidget(outputLayout->count() - 1, output_warning_);


        // Keep the primary interface small; all named-preset operations and
        // the guarded defaults reset live together in one manager dialog.
        auto* actions = new QHBoxLayout();
        auto* presetsButton = new QPushButton("Presets…", central);
        presetsButton->setObjectName("viperNamedPresetsButton");
        presetsButton->setToolTip("Save, load, rename or delete complete effect configurations.");
        actions->addStretch();
        actions->addWidget(presetsButton);
        layout->addWidget(tabs_, 1);
        layout->addLayout(actions);
        connect(presetsButton, &QPushButton::clicked, this, [this]{ showPresetManager(); });

        // New stable name key deliberately ignores the former numeric index
        // (Enhancements=0, Surround=1, Output=2). On first launch show Output.
        // Preserve the user's last category even though two display names
        // changed. No existing effect, preset or panel key is migrated.
        QString initialTabName = settings_.value("ui/active_tab_name", "Output").toString();
        if (initialTabName == "Enhancements") initialTabName = "EQ & Tone";
        if (initialTabName == "Surround") initialTabName = "Spatial";
        if (initialTabName == "IR & Correction" || initialTabName == "IR_Correction") initialTabName = "Sound Correction";
        if (initialTabName == "EQ_Tone") initialTabName = "EQ & Tone";
        for (int tab = 0; tab < tabs_->count(); ++tab) {
            if (tabs_->tabText(tab) == initialTabName ||
                (tabs_->tabText(tab) == "EQ && Tone" && initialTabName == "EQ & Tone")) {
                tabs_->setCurrentIndex(tab);
                break;
            }
        }
        connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
            settings_.setValue("ui/active_tab_name",
                tabs_->tabText(index) == "EQ && Tone" ? "EQ & Tone" : tabs_->tabText(index));
        });
        status_ = new QLabel("Checking backend...", central);
        auto* footer = new QHBoxLayout();
        footer->setContentsMargins(0, 0, 0, 0);
        footer->addWidget(status_, 1);
        // About is available from the application menu; the footer now only
        // displays live backend connection status.
        layout->addLayout(footer);
        setCentralWidget(central);

        save_timer_ = new QTimer(this);
        save_timer_->setSingleShot(true);
        connect(save_timer_, &QTimer::timeout, this, [this]{ settings_.sync(); });

        // The original Stage 5 GUI saved these keys on every adjustment.
        // Keep the format and keys compatible so old settings are preserved.
        global_->setChecked(settings_.value("master_enabled", true).toBool());
        enabled_->setChecked(settings_.value("enabled", true).toBool());
        bass_mode_->setCurrentIndex(std::clamp(settings_.value("bass_mode", 0).toInt(), 0, 2));
        strength_.slider->setValue(settings_.value("strength_tenths", 100).toInt());
        frequency_.slider->setValue(settings_.value("frequency", 65).toInt());
        // Remove obsolete settings so no hidden input attenuation survives.
        settings_.remove("input_percent");
        settings_.remove("preset/input_percent");
        eq_enabled_->setChecked(settings_.value("eq_enabled", false).toBool());
        const int savedEqMode = validEqBands(settings_.value("eq_band_count", 10).toInt());
        eq_active_bands_ = savedEqMode;
        eq_mode_->setCurrentIndex(eq_mode_->findData(savedEqMode));
        loadEqLayout();
        eq_selected_preset_ = settings_.value("eq/selected_preset", "custom").toString();
        selectEqPreset(eq_selected_preset_);
        ddc_enabled_->setChecked(settings_.value("ddc_enabled", false).toBool());
        updateDdcFileName(settings_.value("ddc_profile").toString());
        speaker_enabled_->setChecked(settings_.value("speaker_enabled", false).toBool());
        convolver_enabled_->setChecked(settings_.value("convolver_enabled", false).toBool());
        convolver_cross_.slider->setValue(std::clamp(settings_.value("convolver_cross_percent", 0).toInt(), 0, 100));
        refreshIrsLibrary();
        mono_enabled_->setChecked(settings_.value("bass_mono_enabled", false).toBool());
        mono_mode_->setCurrentIndex(std::clamp(settings_.value("bass_mono_mode", 0).toInt(), 0, 2));
        mono_strength_.slider->setValue(std::clamp(settings_.value("bass_mono_strength_centi", 200).toInt()/10, 5, 100));
        mono_frequency_.slider->setValue(std::clamp(settings_.value("bass_mono_frequency_hz", 65).toInt(), 15, 150));
        psycho_enabled_->setChecked(settings_.value("psycho_enabled", false).toBool());
        psycho_cutoff_.slider->setValue(std::clamp(settings_.value("psycho_cutoff_hz", 80).toInt(), 60, 150));
        psycho_intensity_.slider->setValue(std::clamp(settings_.value("psycho_intensity_percent", 30).toInt(), 0, 100));
        psycho_order_->setCurrentIndex(std::clamp(settings_.value("psycho_order", 3).toInt()-2, 0, 3));
        psycho_original_.slider->setValue(std::clamp(settings_.value("psycho_original_percent", 100).toInt(), 0, 100));
        dyn_enabled_->setChecked(settings_.value("dynamic_enabled", false).toBool());
        dyn_xlo_.slider->setValue(std::clamp(settings_.value("dynamic_x_low", 100).toInt(), 0, 2400));
        dyn_xhi_.slider->setValue(std::clamp(settings_.value("dynamic_x_high", 6000).toInt(), 0, 12000));
        dyn_ylo_.slider->setValue(std::clamp(settings_.value("dynamic_y_low", 100).toInt(), 0, 200));
        dyn_yhi_.slider->setValue(std::clamp(settings_.value("dynamic_y_high", 150).toInt(), 0, 300));
        dyn_side_lo_.slider->setValue(std::clamp(settings_.value("dynamic_side_low_percent", 50).toInt(), 0, 100));
        dyn_side_hi_.slider->setValue(std::clamp(settings_.value("dynamic_side_high_percent", 50).toInt(), 0, 100));
        dyn_strength_.slider->setValue(std::clamp(settings_.value("dynamic_strength_percent", 0).toInt(), 0, 100));
        clarity_enabled_->setChecked(settings_.value("clarity_enabled", false).toBool());
        clarity_mode_->setCurrentIndex(std::clamp(settings_.value("clarity_mode", 0).toInt(), 0, 2));
        clarity_gain_.slider->setValue(settings_.value("clarity_gain_tenths", 15).toInt());
        spectrum_enabled_->setChecked(settings_.value("spectrum_enabled", false).toBool());
        spectrum_frequency_.slider->setValue(settings_.value("spectrum_frequency_hundreds", 76).toInt());
        spectrum_exciter_.slider->setValue(settings_.value("spectrum_exciter_tenths", 10).toInt());
        tube_enabled_->setChecked(settings_.value("tube_enabled", false).toBool());
        tube_model_->setCurrentIndex(std::clamp(settings_.value("tube_model", 0).toInt(), 0, 2));
        tube_drive_.slider->setValue(std::clamp(settings_.value("tube_drive_percent", 50).toInt(), 0, 100));
        tube_mix_.slider->setValue(std::clamp(settings_.value("tube_mix_percent", 30).toInt(), 0, 100));
        analogx_enabled_->setChecked(settings_.value("analogx_enabled", false).toBool());
        analogx_mode_->setCurrentIndex(std::clamp(settings_.value("analogx_mode", 0).toInt(), 0, 2));
        crossfeed_enabled_->setChecked(settings_.value("crossfeed_enabled", false).toBool());
        crossfeed_mode_->setCurrentIndex(std::clamp(settings_.value("crossfeed_preset", 0).toInt(), 0, 2));
        loadMultibandSettings("");
        fet_enabled_->setChecked(settings_.value("fet_enabled", false).toBool());
        fet_threshold_.slider->setValue(std::clamp(settings_.value("fet_threshold_db", -18).toInt(), -48, 0));
        fet_ratio_.slider->setValue(std::clamp(settings_.value("fet_ratio_tenths", 20).toInt(), 10, 100));
        fet_knee_.slider->setValue(std::clamp(settings_.value("fet_knee_db", 6).toInt(), 0, 12));
        fet_gain_.slider->setValue(std::clamp(settings_.value("fet_makeup_db", 0).toInt(), 0, 24));
        fet_attack_.slider->setValue(std::clamp(settings_.value("fet_attack_ms", 10).toInt(), 1, 100));
        fet_release_.slider->setValue(std::clamp(settings_.value("fet_release_ms", 100).toInt(), 5, 500));
        imager_enabled_->setChecked(settings_.value("imager_enabled", false).toBool());
        imager_low_.slider->setValue(std::clamp(settings_.value("imager_low_width_percent", 100).toInt(), 0, 200));
        imager_mid_.slider->setValue(std::clamp(settings_.value("imager_mid_width_percent", 100).toInt(), 0, 200));
        imager_high_.slider->setValue(std::clamp(settings_.value("imager_high_width_percent", 100).toInt(), 0, 200));
        imager_low_xover_.slider->setValue(std::clamp(settings_.value("imager_low_crossover_hz", 200).toInt(), 80, 400));
        imager_high_xover_.slider->setValue(std::clamp(settings_.value("imager_high_crossover_hz", 4000).toInt()/100, 20, 80));
        loadDeqSettings();
        headphone_enabled_->setChecked(settings_.value("headphone_enabled", false).toBool());
        headphone_quality_->setCurrentIndex(std::clamp(settings_.value("headphone_quality", 0).toInt(), 0, 4));
        diff_enabled_->setChecked(settings_.value("diff_enabled", false).toBool());
        diff_delay_.slider->setValue(std::clamp(settings_.value("diff_delay_ms", 8).toInt(), 1, 20));
        diff_reverse_->setChecked(settings_.value("diff_reverse", false).toBool());
        diff_mix_.slider->setValue(std::clamp(settings_.value("diff_mix_percent", 50).toInt(), 0, 100));
        diff_lowpass_.slider->setValue(std::clamp(settings_.value("diff_lp_hundreds", 0).toInt(), 0, 200));
        field_enabled_->setChecked(settings_.value("field_enabled", false).toBool());
        field_widen_.slider->setValue(std::clamp(settings_.value("field_widen_tenths", 10).toInt(), 0, 80));
        field_mid_.slider->setValue(std::clamp(settings_.value("field_mid_hundredths", 100).toInt(), 100, 200));
        field_depth_.slider->setValue(std::clamp(settings_.value("field_depth_step", 3).toInt(), 0, 16));
        reverb_enabled_->setChecked(settings_.value("reverb_enabled", false).toBool());
        reverb_room_.slider->setValue(std::clamp(settings_.value("reverb_room_percent", 50).toInt(), 0, 100));
        reverb_width_.slider->setValue(std::clamp(settings_.value("reverb_width_percent", 50).toInt(), 0, 100));
        reverb_damp_.slider->setValue(std::clamp(settings_.value("reverb_damp_percent", 50).toInt(), 0, 100));
        reverb_wet_.slider->setValue(std::clamp(settings_.value("reverb_wet_percent", 20).toInt(), 0, 100));
        reverb_dry_.slider->setValue(std::clamp(settings_.value("reverb_dry_percent", 50).toInt(), 0, 100));
        limiter_threshold_.slider->setValue(std::clamp(settings_.value("limiter_threshold_permille", 500).toInt(), 300, 1000));
        limiter_gain_.slider->setValue(std::clamp(settings_.value("limiter_output_percent", 100).toInt(), 1, 200));
        limiter_pan_.slider->setValue(std::clamp(settings_.value("limiter_pan_percent", 0).toInt(), -100, 100));
        agc_enabled_->setChecked(settings_.value("agc_enabled", false).toBool());
        agc_strength_.slider->setValue(std::clamp(settings_.value("agc_strength_centi", 100).toInt(), 50, 300));
        agc_max_gain_.slider->setValue(std::clamp(settings_.value("agc_max_gain_centi", 300).toInt(), 100, 1000));
        agc_threshold_.slider->setValue(std::clamp(settings_.value("agc_threshold_permille", 900).toInt(), 300, 1000));
        lufs_enabled_->setChecked(settings_.value("lufs_enabled", false).toBool());
        lufs_target_.slider->setValue(std::clamp(settings_.value("lufs_target_tenths", -140).toInt(), -240, -80));
        lufs_gain_.slider->setValue(std::clamp(settings_.value("lufs_max_gain_tenths", 60).toInt(), 0, 120));
        lufs_speed_->setCurrentIndex(std::clamp(settings_.value("lufs_speed", 1).toInt(), 0, 2));
        updateLabels();

        connect(global_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(bass_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(strength_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(frequency_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(eq_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(eq_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            if (index < 0) return;
            saveEqLayout();
            eq_active_bands_ = validEqBands(eq_mode_->itemData(index).toInt());
            if (eq_selected_preset_ != "custom") applyEqPreset(eq_selected_preset_);
            else { loadEqLayout(); push(); }
        });
        for (int i = 0; i < 31; ++i) {
            connect(eq_faders_[i], &QSlider::valueChanged, this, [this, i](int value) {
                eq_values_[i]->setText(QString::number(value / 100.0, 'f', 2));
                eq_faders_[i]->setToolTip(eq_frequencies_[i]->text() + ": " +
                    QString::number(value / 100.0, 'f', 2) + " dB");
                saveEqLayout();
                selectEqPreset("custom");
                updateEqOverview();
                push();
            });
        }
        connect(eq_reset, &QPushButton::clicked, this, [this] {
            applyEqPreset("builtin:Flat");
            selectEqPreset("builtin:Flat");
        });
        connect(ddc_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(speaker_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(convolver_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(convolver_cross_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(mono_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(mono_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(psycho_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(psycho_order_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(dyn_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        for (auto* slider : {mono_strength_.slider, mono_frequency_.slider,
                             psycho_cutoff_.slider, psycho_intensity_.slider, psycho_original_.slider,
                             dyn_xlo_.slider, dyn_xhi_.slider, dyn_ylo_.slider, dyn_yhi_.slider,
                             dyn_side_lo_.slider, dyn_side_hi_.slider, dyn_strength_.slider}) {
            connect(slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(clarity_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(clarity_mode_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(clarity_gain_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(spectrum_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(spectrum_frequency_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(spectrum_exciter_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(tube_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(tube_model_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(tube_drive_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(tube_mix_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(analogx_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(analogx_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(deq_enabled_, &QCheckBox::toggled, this, [this]{
            updateDeqOverview(); push();
        });
        connect(deq_spread, &QPushButton::clicked, this,
                [this]{ spreadDeqFrequencies(); });
        connect(deq_count_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int n){ changeDeqBandCount(n); });
        connect(deq_editor_count_, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int n){ changeDeqBandCount(n); });
        connect(deq_band_select_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index){ if (index >= 0) loadDeqEditorBand(index); });
        for (QSpinBox* s : {deq_frequency_, deq_attack_, deq_release_})
            connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){ commitDeqEditorBand(); });
        for (QDoubleSpinBox* s : {deq_q_, deq_gain_, deq_threshold_})
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double){ commitDeqEditorBand(); });
        connect(deq_type_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int){ commitDeqEditorBand(); });
        connect(crossfeed_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(crossfeed_mode_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ push(); });
        connect(mb_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(mb_band_count_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
            if (index < 0) return;
            const std::array<int,4> defaults = index == 0
                ? std::array<int,4>{200, 4000, 5000, 8000}
                : std::array<int,4>{120, 500, 4000, 8000};
            for (int i=0; i<4; ++i) {
                const QSignalBlocker block(mb_crossovers_[i].slider);
                mb_crossovers_[i].slider->setValue(defaults[i]);
            }
            updateMultibandVisibility(); updateLabels(); push();
        });
        for (int i=0; i<4; ++i)
            connect(mb_crossovers_[i].slider, &QSlider::valueChanged, this, [this]{
                updateMultibandFrequencyLabels(); updateLabels(); push();
            });
        for (auto& band : mb_bands_) {
            connect(band.enabled, &QCheckBox::toggled, this, [this]{ push(); });
            for (const auto& r : {band.threshold, band.ratio, band.knee, band.gain, band.attack, band.release})
                connect(r.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(fet_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        for (const auto& row : {fet_threshold_, fet_ratio_, fet_knee_, fet_gain_, fet_attack_, fet_release_})
            connect(row.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(imager_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        for (const auto& r : {imager_low_, imager_mid_, imager_high_, imager_low_xover_, imager_high_xover_})
            connect(r.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(headphone_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(headphone_quality_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        connect(diff_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(diff_delay_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(diff_reverse_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(diff_mix_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(diff_lowpass_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(field_widen_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_mid_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(field_depth_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_enabled_, &QCheckBox::toggled, this, [this]{ push(); });
        connect(reverb_room_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_width_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_damp_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_wet_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        connect(reverb_dry_.slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        for (auto* slider : {limiter_threshold_.slider, limiter_gain_.slider, limiter_pan_.slider,
                              agc_strength_.slider, agc_max_gain_.slider, agc_threshold_.slider,
                              lufs_target_.slider, lufs_gain_.slider}) {
            connect(slider, &QSlider::valueChanged, this, [this]{ updateLabels(); push(); });
        }
        connect(agc_enabled_, &QCheckBox::toggled, this, [this]{ updateLabels(); push(); });
        connect(lufs_enabled_, &QCheckBox::toggled, this, [this]{ updateLabels(); push(); });
        connect(lufs_speed_, &QComboBox::currentIndexChanged, this, [this]{ push(); });
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]{
            updateAudioOutputDisplay();
            pollBackend();
        });
        timer->start(500);
        // Theme selection changes only the GUI, never DSP settings.
        connect(theme_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            settings_.setValue("ui/theme", index == 1 ? "classic-v4a" : "modern");
            applyTheme(index == 1);
        });
        // On Cinnamon/X11 Qt can report a dark colorScheme even while GTK
        // desktop windows use a light theme. The authoritative choice there is
        // Cinnamon's GTK theme setting, read asynchronously (no UI blocking).
        // Until discovered, the required fallback is LIGHT.
        if (runningCinnamon()) {
            cinnamon_theme_query_ = new QProcess(this);
            connect(cinnamon_theme_query_, &QProcess::finished,
                    this, [this](int code, QProcess::ExitStatus exitStatus) {
                if (exitStatus != QProcess::NormalExit || code != 0) return;
                const QString value = QString::fromUtf8(cinnamon_theme_query_->readAllStandardOutput()).trimmed();
                if (value.isEmpty() || !value.contains('\'')) return;
                const bool isDark = value.contains("dark", Qt::CaseInsensitive);
                if (!cinnamon_theme_known_ || cinnamon_theme_dark_ != isDark) {
                    cinnamon_theme_known_ = true;
                    cinnamon_theme_dark_ = isDark;
                    applyTheme(theme_selector_->currentIndex() == 1);
                }
            });
            auto* desktopThemeTimer = new QTimer(this);
            connect(desktopThemeTimer, &QTimer::timeout, this, [this] {
                if (cinnamon_theme_query_->state() == QProcess::NotRunning) {
                    cinnamon_theme_query_->start("gsettings",
                        {"get", "org.cinnamon.desktop.interface", "gtk-theme"});
                }
            });
            desktopThemeTimer->start(5000);
            // First query happens right away, without waiting five seconds.
            cinnamon_theme_query_->start("gsettings",
                {"get", "org.cinnamon.desktop.interface", "gtk-theme"});
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged,
                this, [this](Qt::ColorScheme) {
            QTimer::singleShot(0, this, [this] {
                applyTheme(theme_selector_->currentIndex() == 1);
            });
        });
#endif
        applyTheme(theme_selector_->currentIndex() == 1);
        restoreWindowSizes();
        setupTray();
        pollBackend();
    }
    void showAtStartup() {
        const QString choice = settings_.value("ui/startupBehavior", "show").toString();
        if (choice == "tray") {
            if (trayAvailable()) return; // Tray exists, so the window can stay hidden.
            show(); // Never hide without a working tray.
            statusMessage("System tray unavailable; showing the main window instead.");
        } else if (choice == "minimized") {
            // Explicit start-on-taskbar must override the independent
            // minimize-to-tray preference.
            force_taskbar_minimize_ = true;
            showMinimized();
            QTimer::singleShot(0, this, [this]{ force_taskbar_minimize_ = false; });
        } else {
            show();
        }
    }
protected:
    void closeEvent(QCloseEvent* event) override {
        saveUiGeometry();
        const QString choice = settings_.value("ui/closeBehavior", "quit").toString();
        if (choice == "tray") {
            if (!trayAvailable()) {
                event->ignore();
                statusMessage("System tray unavailable; the window stays open.");
                return;
            }
            event->ignore();
            hide();
            return;
        }
        if (choice == "minimized") {
            event->ignore();
            force_taskbar_minimize_ = true;
            showMinimized();
            QTimer::singleShot(0, this, [this]{ force_taskbar_minimize_ = false; });
            return;
        }
        event->accept();
        // quitOnLastWindowClosed is false to support tray-only startup;
        // a genuine close/quit must therefore explicitly terminate the GUI.
        QTimer::singleShot(0, qApp, [] { qApp->quit(); });
    }
    void changeEvent(QEvent* event) override {
        // The window manager has already iconified this native top-level window.
        // On Cinnamon/X11, clearing Qt::WindowMinimized *after* hide() can
        // recreate a black, taskbar-listed ghost window. Leave the minimized
        // state alone while hidden; restoreFromTray() resolves it on demand.
        const bool is_state_change = event->type() == QEvent::WindowStateChange;
        const bool was_maximized = is_state_change &&
            static_cast<QWindowStateChangeEvent*>(event)->oldState().testFlag(Qt::WindowMaximized);
        QMainWindow::changeEvent(event);
        if (!is_state_change || !isMinimized() || force_taskbar_minimize_
            || !settings_.value("ui/minimizeToTray", false).toBool()) return;
        if (!trayAvailable()) return; // Keep native taskbar behavior without a tray.
        // Deferring until after the WM's minimize event prevents reentrant
        // state changes. Only withdraw the window from the WM; don't remap it.
        QTimer::singleShot(0, this, [this, was_maximized] {
            if (!isMinimized() || !isVisible() || !trayAvailable()) return;
            tray_restore_maximized_ = was_maximized;
            hide();
        });
    }
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Hide) {
            if (watched == eq_editor_)
                settings_.setValue("ui/graphicEqEditorSize", eq_editor_->size());
            else if (watched == deq_editor_)
                settings_.setValue("ui/dynamicEqEditorSize", deq_editor_->size());
        }
        return QMainWindow::eventFilter(watched, event);
    }
private:
    // Stable per-user location for portable, named effect snapshots. We keep
    // QSettings's existing org/app config store intact for preferences and
    // automatic session state, including older one-slot presets.
    QString presetLibraryDirectory() const {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + "/viper4linux/presets";
    }
    struct NamedPreset {
        QString path;
        QString name;
    };
    QList<NamedPreset> namedPresets() const {
        QList<NamedPreset> found;
        QDir dir(presetLibraryDirectory());
        for (const QFileInfo& file : dir.entryInfoList({"*.ini"}, QDir::Files | QDir::NoSymLinks)) {
            QSettings preset(file.absoluteFilePath(), QSettings::IniFormat);
            if (preset.value("format/version").toInt() != 1) continue;
            const QString name = preset.value("format/name").toString().trimmed();
            if (!name.isEmpty()) found.append({file.absoluteFilePath(), name});
        }
        std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
        return found;
    }
    static bool acceptablePresetName(const QString& name) {
        if (name.isEmpty() || name.size() > 70) return false;
        for (QChar c : name)
            if (c == '/' || c == '\\' || c.isNull() || !c.isPrint()) return false;
        return true;
    }
    // Writes a complete snapshot with atomic QSettings/INI replacement.
    // Existing file identifiers are UUIDs, never derived from user input.
    bool writeNamedPreset(const QString& name, const QString& path, QString* error) {
        if (!QDir().mkpath(presetLibraryDirectory())) {
            *error = "Could not create the preset library directory.";
            return false;
        }
        QSettings preset(path, QSettings::IniFormat);
        preset.clear();
        preset.setValue("format/version", 1);
        preset.setValue("format/name", name);
        const auto keys = settings_.allKeys();
        for (const QString& key : keys) {
            if (key.startsWith("preset/") && key != "preset/valid")
                preset.setValue("effects/" + key.mid(7), settings_.value(key));
        }
        preset.sync();
        if (preset.status() != QSettings::NoError) {
            *error = "Could not write preset file: " + path;
            return false;
        }
        return true;
    }
    // Older versions stored exactly one snapshot in QSettings. Preserve it as
    // a standalone named preset before using that area as the staging buffer.
    bool migrateLegacyPreset(QString* error) {
        if (settings_.value("presets/legacy_migrated", false).toBool()) return true;
        // Mark even a first-run, empty one-slot store as migrated. Otherwise
        // the next named Save would populate the staging keys and a future
        // dialog open would incorrectly import that save as a legacy preset.
        if (settings_.value("preset/valid", false).toBool()) {
            const QString name = "Previously Saved Preset";
            const QString path = QDir(presetLibraryDirectory()).filePath(
                QUuid::createUuid().toString(QUuid::WithoutBraces) + ".ini");
            if (!writeNamedPreset(name, path, error)) return false;
        }
        settings_.setValue("presets/legacy_migrated", true);
        settings_.sync();
        if (settings_.status() != QSettings::NoError) {
            *error = "Could not record completion of the preset migration.";
            return false;
        }
        return true;
    }
    bool stageNamedPreset(const QString& path, QString* error) {
        QSettings preset(path, QSettings::IniFormat);
        if (preset.status() != QSettings::NoError ||
            preset.value("format/version").toInt() != 1 ||
            !preset.contains("effects/master_enabled") ||
            !preset.contains("effects/enabled") ||
            !preset.contains("effects/limiter_output_percent")) {
            *error = "This file is not a valid ViPER for Linux effect preset.";
            return false;
        }
        const QStringList keys = preset.allKeys();
        // Do not change the active configuration until the file is validated.
        settings_.remove("preset");
        for (const QString& key : keys)
            if (key.startsWith("effects/"))
                settings_.setValue("preset/" + key.mid(8), preset.value(key));
        settings_.setValue("preset/valid", true);
        settings_.sync();
        return true;
    }
    void snapshotToPresetStaging() {
            settings_.setValue("preset/valid", true);
            settings_.setValue("preset/master_enabled", global_->isChecked());
            settings_.setValue("preset/enabled", enabled_->isChecked());
            settings_.setValue("preset/bass_mode", bass_mode_->currentIndex());
            settings_.setValue("preset/strength_tenths", strength_.slider->value());
            settings_.setValue("preset/frequency", frequency_.slider->value());
            saveEqLayout();
            settings_.setValue("preset/eq_enabled", eq_enabled_->isChecked());
            settings_.setValue("preset/eq_selected_preset", eq_selected_preset_);
            settings_.setValue("preset/eq_band_count", eq_active_bands_);
            for (int n : {10, 15, 25, 31})
                settings_.setValue("preset/eq_levels_" + QString::number(n),
                    settings_.value("eq_levels_" + QString::number(n), flatEqCsv(n)));
            settings_.setValue("preset/ddc_enabled", ddc_enabled_->isChecked());
            settings_.setValue("preset/ddc_profile", settings_.value("ddc_profile", ""));
            settings_.setValue("preset/speaker_enabled", speaker_enabled_->isChecked());
            settings_.setValue("preset/convolver_enabled", convolver_enabled_->isChecked());
            settings_.setValue("preset/convolver_cross_percent", convolver_cross_.slider->value());
            settings_.setValue("preset/convolver_kernel", settings_.value("convolver_kernel"));
            saveBassPreset();
            settings_.setValue("preset/clarity_enabled", clarity_enabled_->isChecked());
            settings_.setValue("preset/clarity_mode", clarity_mode_->currentIndex());
            settings_.setValue("preset/clarity_gain_tenths", clarity_gain_.slider->value());
            settings_.setValue("preset/tube_enabled", tube_enabled_->isChecked());
            settings_.setValue("preset/tube_model", tube_model_->currentIndex());
            settings_.setValue("preset/tube_drive_percent", tube_drive_.slider->value());
            settings_.setValue("preset/tube_mix_percent", tube_mix_.slider->value());
            settings_.setValue("preset/analogx_enabled", analogx_enabled_->isChecked());
            settings_.setValue("preset/analogx_mode", analogx_mode_->currentIndex());
            settings_.setValue("preset/crossfeed_enabled", crossfeed_enabled_->isChecked());
            settings_.setValue("preset/crossfeed_preset", crossfeed_mode_->currentIndex());
            saveMultibandSettings("preset/");
            settings_.setValue("preset/fet_enabled", fet_enabled_->isChecked());
            settings_.setValue("preset/fet_threshold_db", fet_threshold_.slider->value());
            settings_.setValue("preset/fet_ratio_tenths", fet_ratio_.slider->value());
            settings_.setValue("preset/fet_knee_db", fet_knee_.slider->value());
            settings_.setValue("preset/fet_makeup_db", fet_gain_.slider->value());
            settings_.setValue("preset/fet_attack_ms", fet_attack_.slider->value());
            settings_.setValue("preset/fet_release_ms", fet_release_.slider->value());
            settings_.setValue("preset/imager_enabled", imager_enabled_->isChecked());
            settings_.setValue("preset/imager_low_width_percent", imager_low_.slider->value());
            settings_.setValue("preset/imager_mid_width_percent", imager_mid_.slider->value());
            settings_.setValue("preset/imager_high_width_percent", imager_high_.slider->value());
            settings_.setValue("preset/imager_low_crossover_hz", imager_low_xover_.slider->value());
            settings_.setValue("preset/imager_high_crossover_hundreds", imager_high_xover_.slider->value());
            saveDeqPreset();
            settings_.setValue("preset/spectrum_enabled", spectrum_enabled_->isChecked());
            settings_.setValue("preset/spectrum_frequency_hundreds", spectrum_frequency_.slider->value());
            settings_.setValue("preset/spectrum_exciter_tenths", spectrum_exciter_.slider->value());
            settings_.setValue("preset/headphone_enabled", headphone_enabled_->isChecked());
            settings_.setValue("preset/headphone_quality", headphone_quality_->currentIndex());
            settings_.setValue("preset/diff_enabled", diff_enabled_->isChecked());
            settings_.setValue("preset/diff_delay_ms", diff_delay_.slider->value());
            settings_.setValue("preset/diff_reverse", diff_reverse_->isChecked());
            settings_.setValue("preset/diff_mix_percent", diff_mix_.slider->value());
            settings_.setValue("preset/diff_lp_hundreds", diff_lowpass_.slider->value());
            settings_.setValue("preset/field_enabled", field_enabled_->isChecked());
            settings_.setValue("preset/field_widen_tenths", field_widen_.slider->value());
            settings_.setValue("preset/field_mid_hundredths", field_mid_.slider->value());
            settings_.setValue("preset/field_depth_step", field_depth_.slider->value());
            settings_.setValue("preset/reverb_enabled", reverb_enabled_->isChecked());
            settings_.setValue("preset/reverb_room_percent", reverb_room_.slider->value());
            settings_.setValue("preset/reverb_width_percent", reverb_width_.slider->value());
            settings_.setValue("preset/reverb_damp_percent", reverb_damp_.slider->value());
            settings_.setValue("preset/reverb_wet_percent", reverb_wet_.slider->value());
            settings_.setValue("preset/reverb_dry_percent", reverb_dry_.slider->value());
            saveOutputPreset();
            settings_.sync();
    }
    void restoreFromPresetStaging() {
            if (!settings_.value("preset/valid", false).toBool()) return;
            applySettings(settings_.value("preset/master_enabled", true).toBool(),
                          settings_.value("preset/enabled", true).toBool(),
                          settings_.value("preset/bass_mode", 0).toInt(),
                          settings_.value("preset/strength_tenths", 100).toInt(),
                          settings_.value("preset/frequency", 65).toInt(),
                          settings_.value("preset/clarity_enabled", false).toBool(),
                          settings_.value("preset/clarity_mode", 0).toInt(),
                          settings_.value("preset/clarity_gain_tenths", 15).toInt(),
                          settings_.value("preset/spectrum_enabled", false).toBool(),
                          settings_.value("preset/spectrum_frequency_hundreds", 76).toInt(),
                          settings_.value("preset/spectrum_exciter_tenths", 10).toInt(),
                          settings_.value("preset/headphone_enabled", false).toBool(),
                          settings_.value("preset/headphone_quality", 0).toInt(),
                          settings_.value("preset/diff_enabled", false).toBool(),
                          settings_.value("preset/diff_delay_ms", 8).toInt(),
                          settings_.value("preset/diff_reverse", false).toBool(),
                          settings_.value("preset/diff_mix_percent", 50).toInt(),
                          settings_.value("preset/diff_lp_hundreds", 0).toInt(),
                          settings_.value("preset/field_enabled", false).toBool(),
                          settings_.value("preset/field_widen_tenths", 10).toInt(),
                          settings_.value("preset/field_mid_hundredths", 100).toInt(),
                          settings_.value("preset/field_depth_step", 3).toInt(),
                          settings_.value("preset/reverb_enabled", false).toBool(),
                          settings_.value("preset/reverb_room_percent", 50).toInt(),
                          settings_.value("preset/reverb_width_percent", 50).toInt(),
                          settings_.value("preset/reverb_damp_percent", 50).toInt(),
                          settings_.value("preset/reverb_wet_percent", 20).toInt(),
                          settings_.value("preset/reverb_dry_percent", 50).toInt());
            for (int n : {10, 15, 25, 31})
                settings_.setValue("eq_levels_" + QString::number(n),
                    settings_.value("preset/eq_levels_" + QString::number(n),
                        flatEqCsv(n)));
            {
                const QSignalBlocker blockEq(eq_enabled_), blockMode(eq_mode_);
                eq_enabled_->setChecked(settings_.value("preset/eq_enabled", false).toBool());
                eq_active_bands_ = validEqBands(settings_.value("preset/eq_band_count", 10).toInt());
                eq_mode_->setCurrentIndex(eq_mode_->findData(eq_active_bands_));
                loadEqLayout();
                selectEqPreset(settings_.value("preset/eq_selected_preset", "custom").toString());
            }
            restoreOutputPreset();
            restoreBassPreset();
            applyTubeSettings(settings_.value("preset/tube_enabled", false).toBool(),
                settings_.value("preset/tube_model", 0).toInt(),
                settings_.value("preset/tube_drive_percent", 50).toInt(),
                settings_.value("preset/tube_mix_percent", 30).toInt());
            applyAnalogXSettings(settings_.value("preset/analogx_enabled", false).toBool(),
                settings_.value("preset/analogx_mode", 0).toInt());
            {
                const QSignalBlocker a(crossfeed_enabled_), b(crossfeed_mode_);
                crossfeed_enabled_->setChecked(settings_.value("preset/crossfeed_enabled", false).toBool());
                crossfeed_mode_->setCurrentIndex(std::clamp(settings_.value("preset/crossfeed_preset", 0).toInt(), 0, 2));
            }
            {
                const QSignalBlocker a(imager_enabled_);
                imager_enabled_->setChecked(settings_.value("preset/imager_enabled", false).toBool());
                for (const auto& item : std::initializer_list<std::pair<SliderRow, int>>{
                    {imager_low_, settings_.value("preset/imager_low_width_percent", 100).toInt()},
                    {imager_mid_, settings_.value("preset/imager_mid_width_percent", 100).toInt()},
                    {imager_high_, settings_.value("preset/imager_high_width_percent", 100).toInt()},
                    {imager_low_xover_, settings_.value("preset/imager_low_crossover_hz", 200).toInt()},
                    {imager_high_xover_, settings_.value("preset/imager_high_crossover_hundreds", 40).toInt()}}) {
                    const QSignalBlocker block(item.first.slider);
                    item.first.slider->setValue(item.second);
                }
                updateLabels();
            }
            {
                const QSignalBlocker b(ddc_enabled_);
                ddc_enabled_->setChecked(settings_.value("preset/ddc_enabled", false).toBool());
                settings_.setValue("ddc_profile", settings_.value("preset/ddc_profile", ""));
                updateDdcFileName(settings_.value("ddc_profile", "").toString());
                settings_.sync();
                requestDdcReload();
            }
            {
                const QSignalBlocker b(speaker_enabled_);
                speaker_enabled_->setChecked(settings_.value("preset/speaker_enabled", false).toBool());
            }
            restoreFetSettings("preset/");
            loadMultibandSettings("preset/");
            restoreDeqPreset();
            {
                const QSignalBlocker b1(convolver_enabled_), b2(convolver_cross_.slider);
                convolver_enabled_->setChecked(settings_.value("preset/convolver_enabled", false).toBool());
                convolver_cross_.slider->setValue(settings_.value("preset/convolver_cross_percent", 0).toInt());
                settings_.setValue("convolver_kernel", settings_.value("preset/convolver_kernel", ""));
                settings_.sync();
                refreshIrsLibrary();
                requestIrsLoad();
            }
            push();
    }
    void resetEffectsWithConfirmation() {
            QMessageBox confirm(this);
            confirm.setIcon(QMessageBox::Warning);
            confirm.setWindowTitle("Reset Effect Settings");
            confirm.setText("Reset all effect settings?");
            confirm.setInformativeText(
                "This will replace your current effect settings with the application's defaults "
                "(master processing and limiter at defaults; all optional effects off).\n\n"
                "Your named presets will not be deleted.");
            auto* resetButton = confirm.addButton("Reset Effects", QMessageBox::DestructiveRole);
            auto* cancelButton = confirm.addButton(QMessageBox::Cancel);
            confirm.setDefaultButton(qobject_cast<QPushButton*>(cancelButton));
            confirm.exec();
            if (confirm.clickedButton() != resetButton) return;

            applySettings(true, false, 0, 100, 65, false, 0, 15, false, 76, 10,
                          false, 0, false, 8, false, 50, 0,
                          false, 10, 100, 3,
                          false, 50, 50, 50, 20, 50);
            resetOutputControls();
            resetNewBassControls();
            applyTubeSettings(false, 0, 50, 30);
            applyAnalogXSettings(false, 0);
            {
                const QSignalBlocker a(crossfeed_enabled_), b(crossfeed_mode_);
                crossfeed_enabled_->setChecked(false);
                crossfeed_mode_->setCurrentIndex(0);
            }
            {
                const QSignalBlocker a(imager_enabled_);
                imager_enabled_->setChecked(false);
                for (const auto& item : std::initializer_list<std::pair<SliderRow, int>>{
                    {imager_low_, 100}, {imager_mid_, 100}, {imager_high_, 100},
                    {imager_low_xover_, 200}, {imager_high_xover_, 40}}) {
                    const QSignalBlocker block(item.first.slider);
                    item.first.slider->setValue(item.second);
                }
                updateLabels();
            }
            {
                const QSignalBlocker b(ddc_enabled_);
                ddc_enabled_->setChecked(false);
            }
            {
                const QSignalBlocker b(speaker_enabled_);
                speaker_enabled_->setChecked(false);
            }
            resetFetSettings();
            resetMultibandSettings();
            resetDeq();
            {
                const QSignalBlocker blockEq(eq_enabled_), blockMode(eq_mode_);
                eq_enabled_->setChecked(false);
                for (int n : {10, 15, 25, 31})
                    settings_.setValue("eq_levels_" + QString::number(n),
                        flatEqCsv(n));
                eq_active_bands_ = 10;
                eq_mode_->setCurrentIndex(eq_mode_->findData(10));
                loadEqLayout();
            }

            {
                const QSignalBlocker b1(convolver_enabled_), b2(convolver_cross_.slider);
                convolver_enabled_->setChecked(false);
                convolver_cross_.slider->setValue(0);
            }
            push();
            statusMessage("Effect settings reset to defaults; named presets preserved.");
    }
    void showPresetManager() {
        QString error;
        if (!migrateLegacyPreset(&error)) {
            QMessageBox::warning(this, "Preset Migration", error +
                "\nYour original saved preset has not been modified.");
            return;
        }
        QDialog dialog(this);
        dialog.setWindowTitle("Effect Presets");
        dialog.setMinimumSize(450, 355);
        auto* column = new QVBoxLayout(&dialog);
        auto* description = new QLabel(
            "Save complete effect settings under different names. "
            "Loading a preset replaces the current effect settings, not your GUI preferences.", &dialog);
        description->setWordWrap(true);
        column->addWidget(description);
        auto* list = new QListWidget(&dialog);
        list->setObjectName("viperNamedPresetList");
        column->addWidget(list, 1);
        auto* buttons = new QHBoxLayout();
        auto* save = new QPushButton("Save Preset…", &dialog);
        auto* load = new QPushButton("Load", &dialog);
        auto* rename = new QPushButton("Rename", &dialog);
        auto* remove = new QPushButton("Delete", &dialog);
        auto* assign = new QPushButton("Apply to Audio Output…", &dialog);
        buttons->addWidget(save);
        buttons->addWidget(load);
        buttons->addWidget(rename);
        buttons->addWidget(remove);
        column->addLayout(buttons);
        column->addWidget(assign);
        auto* bottom = new QHBoxLayout();
        auto* reset = new QPushButton("Reset Effects to Defaults…", &dialog);
        auto* close = new QPushButton("Close", &dialog);
        bottom->addWidget(reset);
        bottom->addStretch();
        bottom->addWidget(close);
        column->addLayout(bottom);
        auto selectedPath = [list] {
            auto* selected = list->currentItem();
            return selected ? selected->data(Qt::UserRole).toString() : QString();
        };
        auto refresh = [this, list](const QString& prefer = QString()) {
            list->clear();
            for (const auto& preset : namedPresets()) {
                auto* item = new QListWidgetItem(preset.name, list);
                item->setData(Qt::UserRole, preset.path);
                if (preset.path == prefer) list->setCurrentItem(item);
            }
            if (!list->currentItem() && list->count()) list->setCurrentRow(0);
        };
        auto updateSelection = [list, load, rename, remove, assign] {
            const bool selected = list->currentItem() != nullptr;
            load->setEnabled(selected);
            rename->setEnabled(selected);
            remove->setEnabled(selected);
            assign->setEnabled(selected);
        };
        QObject::connect(list, &QListWidget::itemSelectionChanged, &dialog, updateSelection);
        refresh(); updateSelection();
        QObject::connect(save, &QPushButton::clicked, &dialog, [&, this] {
            // An existing preset can be selected without retyping its name.
            // The first entry creates a new preset and is always selected by default.
            const auto existing = namedPresets();
            QStringList choices = {"New Preset…"};
            for (const auto& preset : existing) choices << preset.name;
            bool accepted = false;
            const QString choice = QInputDialog::getItem(&dialog, "Save Effect Preset",
                "Save current effect settings as:", choices, 0, false, &accepted);
            if (!accepted) return;
            QString name;
            QString path;
            if (choice == choices.first()) {
                name = QInputDialog::getText(&dialog, "New Effect Preset",
                    "Preset name:", QLineEdit::Normal, "", &accepted).trimmed();
                if (!accepted) return;
                if (!acceptablePresetName(name)) {
                    QMessageBox::warning(&dialog, "Invalid Name",
                        "Enter 1–70 characters without slashes or control characters.");
                    return;
                }
                // A name entered under New Preset may still match an existing
                // preset; never silently overwrite it.
                for (const auto& preset : existing) {
                    if (preset.name.compare(name, Qt::CaseInsensitive) == 0) {
                        name = preset.name;
                        path = preset.path;
                        break;
                    }
                }
            } else {
                for (const auto& preset : existing) {
                    if (preset.name == choice) {
                        name = preset.name;
                        path = preset.path;
                        break;
                    }
                }
                if (path.isEmpty()) return;
            }
            if (!path.isEmpty() &&
                QMessageBox::question(&dialog, "Replace Preset",
                    "Replace the saved preset '" + name + "' with the current settings?",
                    QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            if (path.isEmpty()) path = QDir(presetLibraryDirectory()).filePath(
                QUuid::createUuid().toString(QUuid::WithoutBraces) + ".ini");
            snapshotToPresetStaging();
            QString error;
            if (!writeNamedPreset(name, path, &error)) {
                QMessageBox::warning(&dialog, "Save Failed", error);
                return;
            }
            refresh(path);
            statusMessage("Effect preset saved: " + name);
        });
        QObject::connect(assign, &QPushButton::clicked, &dialog, [&, this] {
            const QString preset = selectedPath();
            if (preset.isEmpty()) return;
            QFile inventoryFile(outputMemoryDirectory() + "/inventory.json");
            if (!inventoryFile.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(&dialog, "Audio Outputs", "No remembered Audio Outputs found.");
                return;
            }
            const QJsonDocument inventory = QJsonDocument::fromJson(inventoryFile.readAll());
            const QJsonObject outputs = inventory.object().value("outputs").toObject();
            if (inventory.object().value("format").toInt() != 1 || outputs.isEmpty()) {
                QMessageBox::warning(&dialog, "Audio Outputs", "No remembered Audio Outputs found.");
                return;
            }
            QString current;
            QFile statusFile(QDir(qEnvironmentVariable("XDG_RUNTIME_DIR"))
                .filePath("viper4linux-output-status.json"));
            if (statusFile.open(QIODevice::ReadOnly)) {
                const QJsonObject status = QJsonDocument::fromJson(statusFile.readAll()).object();
                if (std::abs(status.value("timestamp").toDouble() -
                             double(QDateTime::currentSecsSinceEpoch())) < 5.0)
                    current = status.value("active").toObject().value("key").toString();
            }
            QDialog picker(&dialog);
            picker.setWindowTitle("Assign Preset to Audio Output");
            picker.setMinimumWidth(440);
            auto* layout = new QVBoxLayout(&picker);
            auto* explanation = new QLabel(
                "Select the physical Audio Output whose saved effect settings will be replaced.", &picker);
            explanation->setWordWrap(true);
            layout->addWidget(explanation);
            auto* targets = new QComboBox(&picker);
            for (auto it = outputs.begin(); it != outputs.end(); ++it) {
                const QString name = it.value().toObject().value("description").toString();
                targets->addItem(name + (it.key() == current ? " (active)" : " (remembered)"),
                                 it.key());
            }
            layout->addWidget(targets);
            auto* controls = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &picker);
            controls->button(QDialogButtonBox::Ok)->setText("Apply Preset");
            layout->addWidget(controls);
            connect(controls, &QDialogButtonBox::accepted, &picker, &QDialog::accept);
            connect(controls, &QDialogButtonBox::rejected, &picker, &QDialog::reject);
            if (picker.exec() != QDialog::Accepted) return;
            const QString target = targets->currentData().toString();
            if (target.size() != 64) return;
            const QString selectedName = targets->currentText();
            if (QMessageBox::question(&dialog, "Replace Audio Output Settings",
                "Assign '" + list->currentItem()->text() + "' to " + selectedName + "?\n\n"
                "This replaces that output's saved effect settings. "
                "The original named preset and other Audio Outputs are unchanged.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            QString program = "/usr/bin/viper4linux-output";
            QStringList args = {"--assign-preset", preset, "--target", target};
            if (QCoreApplication::applicationDirPath() != "/usr/bin") {
                program = "python3";
                args.prepend(QDir(QCoreApplication::applicationDirPath())
                    .filePath("../src/viper_output_info.py"));
            }
            QProcess process;
            process.start(program, args);
            if (!process.waitForStarted(3000) || !process.waitForFinished(13000) ||
                process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
                QMessageBox::warning(&dialog, "Preset Assignment Failed",
                    QString::fromUtf8(process.readAllStandardError()).trimmed() +
                    "\n" + QString::fromUtf8(process.readAllStandardOutput()).trimmed());
                return;
            }
            statusMessage("Preset assigned to " + selectedName);
            updateAudioOutputDisplay();
        });
        QObject::connect(load, &QPushButton::clicked, &dialog, [&, this] {
            const QString path = selectedPath();
            if (path.isEmpty()) return;
            if (QMessageBox::question(&dialog, "Load Effect Preset",
                "Load '" + list->currentItem()->text() + "'?\n\n"
                "This replaces the current effect settings. Unsaved changes will be lost. "
                "Your other saved presets will remain untouched.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            QString error;
            if (!stageNamedPreset(path, &error)) {
                QMessageBox::warning(&dialog, "Load Failed", error);
                return;
            }
            restoreFromPresetStaging();
            statusMessage("Effect preset loaded: " + list->currentItem()->text());
        });
        QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, [load](QListWidgetItem*) {
            load->click();
        });
        QObject::connect(rename, &QPushButton::clicked, &dialog, [&, this] {
            const QString path = selectedPath();
            if (path.isEmpty()) return;
            bool accepted = false;
            const QString oldName = list->currentItem()->text();
            const QString name = QInputDialog::getText(&dialog, "Rename Preset",
                "New name:", QLineEdit::Normal, oldName, &accepted).trimmed();
            if (!accepted || name == oldName) return;
            if (!acceptablePresetName(name)) {
                QMessageBox::warning(&dialog, "Invalid Name", "Use 1–70 characters without slashes.");
                return;
            }
            for (const auto& preset : namedPresets()) {
                if (preset.path != path && preset.name.compare(name, Qt::CaseInsensitive) == 0) {
                    QMessageBox::warning(&dialog, "Duplicate Name", "A preset already has that name.");
                    return;
                }
            }
            QSettings file(path, QSettings::IniFormat);
            file.setValue("format/name", name);
            file.sync();
            if (file.status() != QSettings::NoError)
                QMessageBox::warning(&dialog, "Rename Failed", "Could not update the preset file.");
            else refresh(path);
        });
        QObject::connect(remove, &QPushButton::clicked, &dialog, [&, this] {
            const QString path = selectedPath();
            if (path.isEmpty()) return;
            if (QMessageBox::question(&dialog, "Delete Preset",
                "Permanently delete the saved preset '" + list->currentItem()->text() + "'? "
                "Your currently active effect settings will not change.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                != QMessageBox::Yes) return;
            if (!QFile::remove(path))
                QMessageBox::warning(&dialog, "Delete Failed", "Could not delete preset file.");
            else refresh();
        });
        QObject::connect(reset, &QPushButton::clicked, &dialog, [this] {
            resetEffectsWithConfirmation();
        });
        QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        dialog.exec();
    }
    QSize savedWindowSize(const QString& key, const QSize& fallback, const QSize& minimum) const {
        QSize requested = settings_.value(key, fallback).toSize();
        if (!requested.isValid() || requested.isEmpty()) requested = fallback;
        QScreen* target = QGuiApplication::primaryScreen();
        if (!target) return requested.expandedTo(minimum);
        const QSize available = target->availableGeometry().size();
        return QSize(std::clamp(requested.width(), minimum.width(),
                              std::max(minimum.width(), available.width() - 24)),
                     std::clamp(requested.height(), minimum.height(),
                              std::max(minimum.height(), available.height() - 48)));
    }
    void restoreWindowSizes() {
        resize(savedWindowSize("ui/mainWindowSize", QSize(740, 800), minimumSize()));
        eq_editor_->resize(savedWindowSize("ui/graphicEqEditorSize", QSize(720, 355),
                                          eq_editor_->minimumSize()));
        deq_editor_->resize(savedWindowSize("ui/dynamicEqEditorSize", QSize(670, 735),
                                           deq_editor_->minimumSize()));
        eq_editor_->installEventFilter(this);
        deq_editor_->installEventFilter(this);
        if (settings_.value("ui/mainWindowMaximized", false).toBool())
            QTimer::singleShot(0, this, [this]{ showMaximized(); });
    }
    static bool runningCinnamon() {
        return qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("cinnamon", Qt::CaseInsensitive)
            || qEnvironmentVariable("XDG_SESSION_DESKTOP").contains("cinnamon", Qt::CaseInsensitive);
    }
    bool systemPrefersDark() const {
        if (runningCinnamon()) return cinnamon_theme_known_ && cinnamon_theme_dark_;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        return false; // On Qt < 6.5 Unknown falls back to LIGHT.
#endif
    }
    void applyTheme(bool classic) {
        // Fully specified pairings, not a mixture of old Qt palette + new QSS.
        // Never set a QApplication stylesheet or palette: a window-local theme
        // keeps other Qt windows, dialogs and the user's desktop unaffected.
        const bool dark = systemPrefersDark();
        const QString windowBg = dark ? (classic ? "#171719" : "#172428")
                                      : (classic ? "#f7f9fb" : "#f8fbfb");
        const QString textColor = dark ? "#edf4f6" : "#203038";
        const QString mutedColor = dark ? "#c0cdd1" : "#536a74";
        const QString accent = classic ? (dark ? "#12b8e8" : "#087eae")
                                       : (dark ? "#3299a5" : "#236e76");
        const QString bg = dark ? (classic ? "#252628" : "#273b40")
                                : (classic ? "#ffffff" : "#eaf3f5");
        const QString border = dark ? (classic ? "#414348" : "#426064")
                                    : (classic ? "#ccdae2" : "#c8dfe2");
        const QString roadBg = dark ? (classic ? "#202124" : "#203236")
                                    : (classic ? "#eef4f7" : "#f0f5f6");
        const QString roadBorder = dark ? (classic ? "#54565a" : "#52666a")
                                        : (classic ? "#b9cbd4" : "#bfd4d7");
        const QString inactiveBg = dark ? (classic ? "#26282b" : "#25383c")
                                        : (classic ? "#e6eef2" : "#e4edef");
        const QString inactiveText = dark ? "#e3edf0" : "#395660";
        const QString inactiveBorder = dark ? "#4d6064" : "#b9cdd2";
        const QString hoverBg = dark ? (classic ? "#363c40" : "#344d52")
                                     : (classic ? "#d3e9f2" : "#d4e5e8");
        const QString inputBg = dark ? "#303639" : "#ffffff";
        const QString inputBorder = dark ? "#617479" : "#b9cbd4";
        const QString inputSel = dark ? "#1b5a68" : "#c9eaf5";
        const QString disabledText = dark ? "#a5afb2" : "#687e87";
        const QString activeTabText = dark ? "#092129" : "#ffffff";
        theme_muted_color_ = mutedColor;
        // A window palette supplies consistent foreground colors even for
        // children with their own frame stylesheets, which do not necessarily
        // inherit color declarations from a parent stylesheet under Qt.
        QPalette pal = qApp->palette();
        pal.setColor(QPalette::Window, QColor(windowBg));
        pal.setColor(QPalette::WindowText, QColor(textColor));
        pal.setColor(QPalette::Base, QColor(inputBg));
        pal.setColor(QPalette::AlternateBase, QColor(bg));
        pal.setColor(QPalette::Text, QColor(textColor));
        pal.setColor(QPalette::Button, QColor(inputBg));
        pal.setColor(QPalette::ButtonText, QColor(textColor));
        pal.setColor(QPalette::Highlight, QColor(accent));
        pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        pal.setColor(QPalette::Mid, QColor(mutedColor));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor(disabledText));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(disabledText));
        setPalette(pal);  // Window-local only.
        if (deq_editor_) deq_editor_->setPalette(pal);
        if (eq_editor_) {
            eq_editor_->setPalette(pal);
            eq_editor_->setStyleSheet(QString(
                "QDialog#viperEqEditor { background:%1; color:%2; } "
                "QLabel { color:%2; } QPushButton { color:%2; background:%3; "
                "border:1px solid %4; border-radius:7px; padding:5px 8px; }")
                .arg(windowBg, textColor, inputBg, inputBorder));
        }
        setStyleSheet(QString(R"THEME(
            QMainWindow#viperWindow, QWidget#viperCentral,
            QWidget#viperEffectsContent, QScrollArea, QScrollArea > QWidget > QWidget {
                background-color:%1; color:%2; }
            QTabWidget::pane { border:0; background:%1; }
            QLabel, QGroupBox, QToolButton, QRadioButton, QCheckBox { color:%2; }
            QPlainTextEdit#viperIrsFullName {
                color:%2; background-color:transparent; border:0; padding:0;
            }
            QPushButton, QComboBox, QLineEdit, QTextBrowser {
                color:%2; background-color:%3;
                border:1px solid %4; border-radius:7px; padding:5px 8px; }
            QPushButton:hover, QComboBox:hover { border-color:%5; }
            /* The Theme selector was visually verified under Cinnamon in r26.
               Apply its complete rounded cap AND explicit chevron to every
               combo so Qt cannot fall back to a square native arrow button.
               The text reserve belongs on the combo, not the dropdown cap. */
            QComboBox { padding-right:26px; }
            QComboBox::drop-down {
                subcontrol-origin:border; subcontrol-position:top right;
                width:24px; border:0;
                border-top-right-radius:7px; border-bottom-right-radius:7px;
                background-color:transparent;
            }
            QComboBox::down-arrow {
                image:url(%8); width:12px; height:8px;
            }
            QPushButton:pressed { background-color:%6; }
            QPushButton:disabled, QComboBox:disabled { color:%7; }
            QComboBox QAbstractItemView {
                color:%2; background-color:%3;
                selection-color:%2; selection-background-color:%6; }
            QToolTip { color:%2; background-color:%3; border:1px solid %4; }
        )THEME").arg(windowBg, textColor, inputBg, inputBorder, accent,
                       inputSel, disabledText,
                       dark ? QStringLiteral(":/viper4linux/combo-chevron-light.png")
                            : QStringLiteral(":/viper4linux/combo-chevron-dark.png")));

        for (auto* frame : findChildren<QFrame*>()) {
            if (frame->objectName() == "viperMasterCard" ||
                frame->objectName() == "viperEffectCard") {
                const QString name = frame->objectName();
                frame->setStyleSheet(QString(
                    "QFrame#%1 { background-color:%2; border:1px solid %3; border-radius:16px; }")
                    .arg(name, bg, border));
            } else if (frame->objectName() == "viperRoadmapCard") {
                frame->setStyleSheet(QString(
                    "QFrame#viperRoadmapCard { background-color:%1; border:1px dashed %2; border-radius:13px; }")
                    .arg(roadBg, roadBorder));
            }
        }
        tabs_->tabBar()->setStyleSheet(QString(R"(
            QTabBar::tab { background-color:%1; color:%2; border:1px solid %3;
                border-radius:10px; padding:9px 13px; min-width:80px;
                margin-right:7px; margin-bottom:5px; font-size:14px; font-weight:600; }
            QTabBar::tab:selected { background-color:%4; color:%6; border-color:%4; }
            QTabBar::tab:!selected:hover { background-color:%5; }
        )").arg(inactiveBg, inactiveText, inactiveBorder, accent, hoverBg, activeTabText));
        const QString remaining = dark ? "#52666a" : "#cbdfe4";
        for (auto* slider : findChildren<QSlider*>()) {
            if (slider->orientation() == Qt::Vertical) {
                slider->setStyleSheet(QString(R"(
                    QSlider::groove:vertical { width:9px; background:%1; border-radius:4px; }
                    QSlider::sub-page:vertical { background:%1; border-radius:4px; }
                    QSlider::add-page:vertical { background:%2; border-radius:4px; }
                    QSlider::handle:vertical { background:%2; border:1px solid %3;
                        height:10px; margin:0 -7px; border-radius:4px; }
                )").arg(remaining, accent, dark ? "#c1e8ee" : "#ffffff"));
                continue;
            }
            slider->setStyleSheet(QString(R"(
                QSlider::groove:horizontal { height:10px; border-radius:5px; background:%1; }
                QSlider::sub-page:horizontal { border-radius:5px; background:%2; }
                QSlider::add-page:horizontal { border-radius:5px; background:%1; }
                QSlider::handle:horizontal { background:%2; border:1px solid %3;
                    width:9px; margin:-8px 0; border-radius:4px; }
            )").arg(remaining, accent, dark ? "#c1e8ee" : "#ffffff"));
        }
        // Title icon follows the selected UI style; the taskbar/window icon
        // stays blue for a stable application identity.
        if (header_icon_) {
            const QPixmap logo(classic ? ":/viper4linux/blue.png" : ":/viper4linux/teal.png");
            header_icon_->setPixmap(logo.scaled(36,36,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        }
        // The vector glyphs receive the theme accent directly, independently
        // of font availability and QLabel style inheritance.
        // Qt6 findChildren<T>() requires T to declare Q_OBJECT. EffectGlyph
        // is deliberately a lightweight QWidget with no signals or slots;
        // discover QWidget children and downcast only the glyph instances.
        for (auto* widget : findChildren<QWidget*>()) {
            if (auto* glyph = dynamic_cast<EffectGlyph*>(widget))
                glyph->setColor(QColor(accent));
        }
        for (auto* label : findChildren<QLabel*>()) {
            if (label == status_) continue; // live status color is set by pollBackend().
            if (label->property("viperWarningText").toBool()) {
                label->setStyleSheet(QString("color:%1; font-weight:600;")
                    .arg(dark ? "#ffce88" : "#9b5e12"));
            } else if (label->property("viperSliderValue").toBool()) {
                label->setStyleSheet(QString("color:%1; font-weight:600;").arg(accent));
            } else if (label->property("viperMutedText").toBool() ||
                       label->styleSheet().trimmed() == "color: palette(mid);") {
                label->setProperty("viperMutedText", true);
                label->setStyleSheet(QString("color:%1;").arg(mutedColor));
            } else {
                // Do not let a former Classic palette leave dark text on dark
                // Modern cards (or vice versa). All plain labels get an explicit
                // foreground; QFont-based bold headings remain bold.
                label->setStyleSheet(QString("color:%1;").arg(textColor));
            }
        }
        for (auto* button : findChildren<QToolButton*>()) {
            if (button->accessibleName().startsWith("Expand or collapse ")) {
                button->setStyleSheet(QString(
                    "QToolButton { color:%1; font-weight:600; font-size:14px; "
                    "border:none; padding:7px 3px; text-align:left; background:transparent; }")
                    .arg(textColor));
            }
        }
        for (auto* cb : findChildren<QCheckBox*>()) {
            if (auto* sw = dynamic_cast<EffectSwitch*>(cb)) sw->setClassicTheme(classic, dark);
        }
        if (status_ && status_->text().startsWith("○"))
            status_->setStyleSheet(QString("color:%1;").arg(mutedColor));
    }
    QVBoxLayout* addEffectsTab(const QString& tabName) {
        auto* scroll = new QScrollArea(tabs_);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* content = new QWidget(scroll);
        content->setObjectName("viperEffectsContent");
        auto* column = new QVBoxLayout(content);
        column->setSpacing(10);
        column->setContentsMargins(8, 10, 8, 10);
        column->addStretch(); // stretches after panels are added (moved below)
        scroll->setWidget(content);
        // Qt treats a single '&' as a mnemonic and draws an underscore.
        // Double it for the user-facing EQ & Tone tab label.
        tabs_->addTab(scroll, tabName == "EQ & Tone" ? "EQ && Tone" : tabName);
        return column;
    }
    void addTabNotice(QVBoxLayout* column, const QString& heading, const QString& detail) {
        // Informational only: deliberately no switches, mappings or DSP writes.
        auto* panel = new QFrame(tabs_);
        panel->setObjectName("viperRoadmapCard");
        const bool dark = palette().color(QPalette::Window).lightness() < 130;
        panel->setStyleSheet(QString(
            "QFrame#viperRoadmapCard { background:%1; border:1px dashed %2; border-radius:13px; }")
            .arg(dark ? "#273638" : "#f0f4f5", dark ? "#52666a" : "#bbcfd2"));
        auto* stack = new QVBoxLayout(panel);
        stack->setContentsMargins(18, 15, 18, 15);
        auto* headingLabel = new QLabel(heading, panel);
        QFont headingFont = headingLabel->font();
        headingFont.setBold(true);
        headingLabel->setFont(headingFont);
        stack->addWidget(headingLabel);
        auto* detailLabel = new QLabel(detail, panel);
        detailLabel->setWordWrap(true);
        detailLabel->setStyleSheet("color: palette(mid);");
        stack->addWidget(detailLabel);
        column->insertWidget(column->count() - 1, panel);
    }
    void addEffectPanel(QVBoxLayout* column, QGroupBox* group, const QString& key,
                        QCheckBox* enable = nullptr) {
        // Each effect is a rounded card: its header and independent toggle
        // remain visible even when the settings body is collapsed.
        auto* panel = new QFrame(tabs_);
        panel->setObjectName("viperEffectCard");
        const bool dark = palette().color(QPalette::Window).lightness() < 130;
        panel->setStyleSheet(QString(
            "QFrame#viperEffectCard { background:%1; border:1px solid %2; border-radius:16px; }")
            .arg(dark ? "#293a3e" : "#ebf3f5", dark ? "#405659" : "#d8e7e9"));
        auto* stack = new QVBoxLayout(panel);
        stack->setContentsMargins(16, 12, 16, 12);
        stack->setSpacing(4);

        const QString title = group->title();
        group->setTitle(QString());
        group->setFlat(true);
        group->setParent(panel);
        group->setStyleSheet("QGroupBox { background: transparent; border: none; }");
        if (auto* form = qobject_cast<QFormLayout*>(group->layout())) {
            form->setContentsMargins(5, 6, 5, 7);
            form->setHorizontalSpacing(16);
            form->setVerticalSpacing(8);
            form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        }
        auto* header = new QHBoxLayout();
        header->setContentsMargins(0, 0, 0, 0);
        header->setSpacing(10);
        auto* glyph = new EffectGlyph(key, panel);
        glyph->setColor(QColor(dark ? "#8fd2d7" : "#536c70"));
        header->addWidget(glyph);
        auto* expand = new QToolButton(panel);
        expand->setText(title);
        expand->setAccessibleName("Expand or collapse " + title + " settings");
        expand->setCheckable(true);
        expand->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        expand->setAutoRaise(true);
        expand->setCursor(Qt::PointingHandCursor);
        expand->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        expand->setMinimumHeight(43);
        expand->setStyleSheet("QToolButton { font-weight: 600; font-size: 14px; border: none; padding: 7px 3px; text-align:left; }");
        const bool open = settings_.value("ui/expanded/" + key, key == "bass").toBool();
        expand->setChecked(open);
        expand->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        header->addWidget(expand, 1);
        if (enable) {
            // Detach the original checkbox from its QFormLayout and move the
            // exact same QCheckBox into the permanent card header. No replacement
            // control is created, so persistence and DSP connections are intact.
            auto* form = qobject_cast<QFormLayout*>(group->layout());
            if (form) {
                for (int i = 0; i < form->rowCount(); ++i) {
                    const auto* item = form->itemAt(i, QFormLayout::FieldRole);
                    if (item && item->widget() == enable) {
                        const auto taken = form->takeRow(i);
                        if (taken.labelItem) {
                            delete taken.labelItem->widget();
                            delete taken.labelItem;
                        }
                        delete taken.fieldItem;
                        break;
                    }
                }
            }
            enable->setParent(panel);
            enable->setAccessibleName("Enable " + title);
            if (enable->isEnabled())
                enable->setToolTip("Turn " + title + " processing on or off independently of the expanded settings panel.");
            header->addWidget(enable);
        }
        stack->addLayout(header);
        stack->addWidget(group);
        group->setVisible(open);
        connect(expand, &QToolButton::toggled, this, [this, group, expand, key](bool expanded) {
            // Visual state only. NEVER call push(), setChecked(enable), or write
            // the effect's processing flags from this handler.
            group->setVisible(expanded);
            expand->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            settings_.setValue("ui/expanded/" + key, expanded);
        });
        column->insertWidget(column->count() - 1, panel);
    }
    struct SliderRow { QSlider* slider; QLabel* value; };
    struct MbBandRows {
        QCheckBox* enabled{};
        SliderRow threshold{}, ratio{}, knee{}, gain{}, attack{}, release{};
    };
    SliderRow addSlider(QFormLayout* form, const char* label, int min, int max) {
        // Android-inspired parameter presentation: label and live value share
        // a row, with a full-width slider underneath.  This is visual only.
        auto* container = new QWidget(this);
        auto* vertical = new QVBoxLayout(container);
        vertical->setContentsMargins(0, 5, 0, 8);
        vertical->setSpacing(8);
        auto* heading = new QHBoxLayout();
        heading->setContentsMargins(0, 0, 0, 0);
        auto* caption = new QLabel(QString::fromUtf8(label), container);
        auto* value = new QLabel(container);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value->setProperty("viperSliderValue", true);
        value->setStyleSheet("color: #23818b; font-weight: 600;");
        heading->addWidget(caption, 1);
        heading->addWidget(value);
        vertical->addLayout(heading);
        auto* slider = new SafeSlider(Qt::Horizontal, container);
        slider->setRange(min, max);
        slider->setSingleStep(1);
        slider->setMinimumHeight(28);
        const bool dark = palette().color(QPalette::Window).lightness() < 130;
        const QString remaining = dark ? "#52666a" : "#cde4e8";
        slider->setStyleSheet(QString(R"(
            QSlider::groove:horizontal { height: 10px; border-radius: 5px; background: %1; }
            QSlider::sub-page:horizontal { border-radius: 5px; background: #236e76; }
            QSlider::add-page:horizontal { border-radius: 5px; background: %1; }
            QSlider::handle:horizontal { background: #236e76; border: 1px solid #d4e8ea;
                                         width: 9px; margin: -8px 0; border-radius: 4px; }
        )").arg(remaining));
        vertical->addWidget(slider);
        form->addRow(container);
        return {slider, value};
    }
    void updateLabels() {
        strength_.value->setText(QString::number(strength_.slider->value() / 10.0, 'f', 1) + " / 10");
        frequency_.value->setText(QString::number(frequency_.slider->value()) + " Hz");
        mono_strength_.value->setText(QString::number(mono_strength_.slider->value()/10.0, 'f', 1) + " / 10");
        mono_frequency_.value->setText(QString::number(mono_frequency_.slider->value()) + " Hz");
        psycho_cutoff_.value->setText(QString::number(psycho_cutoff_.slider->value()) + " Hz");
        psycho_intensity_.value->setText(QString::number(psycho_intensity_.slider->value()) + "%");
        psycho_original_.value->setText(QString::number(psycho_original_.slider->value()) + "%");
        for (const auto& row : {dyn_xlo_, dyn_xhi_, dyn_ylo_, dyn_yhi_})
            row.value->setText(QString::number(row.slider->value()) + " Hz");
        for (const auto& row : {dyn_side_lo_, dyn_side_hi_, dyn_strength_})
            row.value->setText(QString::number(row.slider->value()) + "%");
        convolver_cross_.value->setText(QString::number(convolver_cross_.slider->value()) + "%");
        clarity_gain_.value->setText(QString::number(clarity_gain_.slider->value() / 10.0, 'f', 1) + " / 4.5");
        spectrum_frequency_.value->setText(QString::number(spectrum_frequency_.slider->value() * 100) + " Hz");
        spectrum_exciter_.value->setText(QString::number(spectrum_exciter_.slider->value() / 10.0, 'f', 1) + " / 6.0");
        tube_drive_.value->setText(QString::number(tube_drive_.slider->value()) + "%");
        tube_mix_.value->setText(QString::number(tube_mix_.slider->value()) + "%");
        reverb_room_.value->setText(QString::number(reverb_room_.slider->value()) + "%");
        reverb_width_.value->setText(QString::number(reverb_width_.slider->value()) + "%");
        reverb_damp_.value->setText(QString::number(reverb_damp_.slider->value()) + "%");
        reverb_wet_.value->setText(QString::number(reverb_wet_.slider->value()) + "%");
        reverb_dry_.value->setText(QString::number(reverb_dry_.slider->value()) + "%");
        diff_delay_.value->setText(QString::number(diff_delay_.slider->value()) + " ms");
        diff_mix_.value->setText(QString::number(diff_mix_.slider->value()) + "%");
        field_widen_.value->setText(QString::number(field_widen_.slider->value() / 10.0, 'f', 1));
        field_mid_.value->setText(QString::number(field_mid_.slider->value() / 100.0, 'f', 2));
        for (int i=0; i<4; ++i)
            mb_crossovers_[i].value->setText(QString::number(mb_crossovers_[i].slider->value()) + " Hz");
        updateMultibandFrequencyLabels();
        for (auto& band : mb_bands_) {
            band.threshold.value->setText(QString::number(band.threshold.slider->value()) + " dBFS");
            band.ratio.value->setText(QString::number(band.ratio.slider->value()/10.0, 'f', 1) + ":1");
            band.knee.value->setText(QString::number(band.knee.slider->value()) + " dB");
            band.gain.value->setText(QString::number(band.gain.slider->value()) + " dB");
            band.attack.value->setText(QString::number(band.attack.slider->value()) + " ms");
            band.release.value->setText(QString::number(band.release.slider->value()) + " ms");
        }
        fet_threshold_.value->setText(QString::number(fet_threshold_.slider->value()) + " dBFS");
        fet_ratio_.value->setText(QString::number(fet_ratio_.slider->value() / 10.0, 'f', 1) + ":1");
        fet_knee_.value->setText(QString::number(fet_knee_.slider->value()) + " dB");
        fet_gain_.value->setText(QString::number(fet_gain_.slider->value()) + " dB");
        fet_attack_.value->setText(QString::number(fet_attack_.slider->value()) + " ms");
        fet_release_.value->setText(QString::number(fet_release_.slider->value()) + " ms");
        for (const auto& row : {imager_low_, imager_mid_, imager_high_})
            row.value->setText(QString::number(row.slider->value()) + "%");
        imager_low_xover_.value->setText(QString::number(imager_low_xover_.slider->value()) + " Hz");
        imager_high_xover_.value->setText(QString::number(imager_high_xover_.slider->value() * 100) + " Hz");
        field_depth_.value->setText(field_depth_.slider->value() == 0 ? "Off" :
            QString::number(150 + field_depth_.slider->value() * 50));
        limiter_threshold_.value->setText(QString::number(20.0 * std::log10(limiter_threshold_.slider->value() / 1000.0), 'f', 1) + " dBFS");
        limiter_gain_.value->setText(QString::number(limiter_gain_.slider->value()) + "%");
        limiter_pan_.value->setText(limiter_pan_.slider->value() == 0 ? "Center" :
            QString::number(std::abs(limiter_pan_.slider->value())) + "% " +
            (limiter_pan_.slider->value() < 0 ? "Left" : "Right"));
        agc_strength_.value->setText(QString::number(agc_strength_.slider->value() / 100.0, 'f', 2));
        agc_max_gain_.value->setText(QString::number(agc_max_gain_.slider->value() / 100.0, 'f', 2) + "×");
        agc_threshold_.value->setText(QString::number(agc_threshold_.slider->value() / 1000.0, 'f', 3));
        lufs_target_.value->setText(QString::number(lufs_target_.slider->value() / 10.0, 'f', 1) + " LUFS");
        lufs_gain_.value->setText(QString::number(lufs_gain_.slider->value() / 10.0, 'f', 1) + " dB");
        if (output_warning_) output_warning_->setVisible(agc_enabled_->isChecked() && lufs_enabled_->isChecked());
        diff_lowpass_.value->setText(diff_lowpass_.slider->value() == 0
            ? "Off" : QString::number(diff_lowpass_.slider->value() * 100) + " Hz");
    }
    QString ddcDirectory() const {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/viper4linux/VDC";
    }
    void refreshDdcLibrary() {
        if (!ddc_library_ || ddc_library_->popupIsOpen()) {
            ddc_library_dirty_ = true;
            return;
        }
        QDir().mkpath(ddcDirectory());
        if (ddc_watcher_ && !ddc_watcher_->directories().contains(ddcDirectory()))
            ddc_watcher_->addPath(ddcDirectory());
        const QString selected = settings_.value("ddc_profile", "").toString();
        const QDir dir(ddcDirectory());
        QList<QPair<QString, QString>> entries;
        entries.append({"None", QString()});
        for (const QString& name : dir.entryList({"*.vdc", "*.VDC"},
                QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase))
            entries.append({name, dir.absoluteFilePath(name)});
        bool selectedFound = selected.isEmpty();
        for (const auto& entry : entries) {
            if (entry.second == selected) { selectedFound = true; break; }
        }
        // Preserve older settings pointing outside the VDC directory.
        if (!selectedFound)
            entries.append({"External: " + QFileInfo(selected).fileName(), selected});
        bool identical = entries.size() == ddc_library_->count();
        if (identical) {
            for (int i = 0; i < entries.size(); ++i) {
                if (ddc_library_->itemText(i) != entries[i].first ||
                    ddc_library_->itemData(i).toString() != entries[i].second) {
                    identical = false;
                    break;
                }
            }
        }
        const QSignalBlocker block(ddc_library_);
        if (!identical) {
            ddc_library_->clear();
            for (const auto& entry : entries) {
                const int index = ddc_library_->count();
                ddc_library_->addItem(entry.first, entry.second);
                ddc_library_->setItemData(index, entry.first, Qt::ToolTipRole);
            }
        }
        ddc_library_->setCurrentIndex(std::max(0, ddc_library_->findData(selected)));
        updateDdcFileName(selected);
        ddc_library_dirty_ = false;
    }
    QString irsDirectory() const {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/viper4linux/IRS";
    }
    void refreshIrsLibrary() {
        if (convolver_library_->popupIsOpen()) {
            irs_library_dirty_ = true;
            return;
        }
        QDir().mkpath(irsDirectory());
        if (irs_watcher_ && !irs_watcher_->directories().contains(irsDirectory()))
            irs_watcher_->addPath(irsDirectory());
        const QString selected = settings_.value("convolver_kernel").toString();
        const QDir dir(irsDirectory());
        QList<QPair<QString, QString>> entries;
        entries.append({"None", QString()});
        for (const QString& name : dir.entryList({"*.irs", "*.IRS", "*.wav", "*.WAV"}, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase))
            entries.append({name, dir.absoluteFilePath(name)});
        bool selectedFound = selected.isEmpty();
        for (const auto& entry : entries) {
            if (entry.second == selected) { selectedFound = true; break; }
        }
        if (!selectedFound)
            entries.append({"External: " + QFileInfo(selected).fileName(), selected});

        bool identical = entries.size() == convolver_library_->count();
        if (identical) {
            for (int i = 0; i < entries.size(); ++i) {
                if (convolver_library_->itemText(i) != entries[i].first ||
                    convolver_library_->itemData(i).toString() != entries[i].second) {
                    identical = false;
                    break;
                }
            }
        }
        const QSignalBlocker block(convolver_library_);
        if (!identical) {
            convolver_library_->clear();
            for (const auto& entry : entries)
            {
                const int index = convolver_library_->count();
                convolver_library_->addItem(entry.first, entry.second);
                // The popup may elide a long entry, but its tooltip is complete.
                convolver_library_->setItemData(index, entry.first, Qt::ToolTipRole);
            }
        }
        // Preserve selection without an accidental DSP reload or a popup jump.
        convolver_library_->setCurrentIndex(std::max(0, convolver_library_->findData(selected)));
        updateIrsName(selected);
        irs_library_dirty_ = false;
    }
    void updateIrsName(const QString& path) {
        const QString name = path.isEmpty() ? QString("None") : QFileInfo(path).fileName();
        convolver_full_name_->setPlainText("Selected IRS: " + name);
        convolver_full_name_->setToolTip(path.isEmpty() ? QString("No impulse response") : path);
        // The native dropdown's text may be elided. Tooltips and this
        // wrapping, selectable line always retain the full original name.
        convolver_library_->setToolTip(path.isEmpty() ? QString("No impulse response") : path);
    }
    void updateDdcFileName(const QString& path) {
        ddc_file_name_->setText(path.isEmpty() ? "Selected profile: None" :
            "Selected profile: " + QFileInfo(path).fileName());
        ddc_file_name_->setToolTip(path);
    }
    void requestDdcReload() {
        if (ensureBackend()) {
            mapping_.get()->ddc_request_serial.fetch_add(1, std::memory_order_release);
            ddc_status_->setText("Loading DDC profile…");
        } else {
            ddc_status_->setText("DDC selection saved; will load at backend startup.");
        }
    }
    void selectDdcProfile(const QString& path) {
        if (path.isEmpty()) settings_.remove("ddc_profile");
        else settings_.setValue("ddc_profile", path);
        settings_.sync();
        updateDdcFileName(path);
        requestDdcReload();
        refreshDdcLibrary();
    }
    void requestIrsLoad() {
        if (ensureBackend()) {
            auto* c = mapping_.get();
            c->convolver_request_serial.fetch_add(1, std::memory_order_release);
            convolver_status_->setText("Loading IRS into running backend…");
        } else {
            convolver_status_->setText("Selection saved; will load when backend starts.");
        }
    }
    void selectIrPath(const QString& path) {
        if (path.isEmpty()) settings_.remove("convolver_kernel");
        else settings_.setValue("convolver_kernel", path);
        settings_.sync(); // worker reads this file after receiving the IPC request
        requestIrsLoad();
        updateIrsName(path); // even if the popup is still open and frozen
        refreshIrsLibrary();
    }
    // Keep all window/tray policy separate from DSP parameter persistence.
    void saveUiGeometry() {
        if (isVisible() && !isMinimized()) {
            settings_.setValue("ui/mainWindowSize", isMaximized() ? normalGeometry().size() : size());
            settings_.setValue("ui/mainWindowMaximized", isMaximized());
        }
        settings_.setValue("ui/graphicEqEditorSize", eq_editor_->size());
        settings_.setValue("ui/dynamicEqEditorSize", deq_editor_->size());
        settings_.sync();
    }
    bool trayAvailable() const {
        return tray_icon_ && QSystemTrayIcon::isSystemTrayAvailable();
    }
    void restoreFromTray() {
        // Restore the real GUI *before* asking Qt to clear its minimized state.
        // When minimized-to-tray, we only hid the native window; we did not
        // mutate its window state while hidden (which confused Cinnamon/X11).
        if (isMinimized()) {
            if (tray_restore_maximized_) showMaximized();
            else showNormal();
        } else if (!isVisible()) {
            show();
        }
        tray_restore_maximized_ = false;
        raise();
        activateWindow();
    }
    void quitGui() {
        saveUiGeometry();
        qApp->quit(); // Does not touch the separate PipeWire backend.
    }
    void setupTray() {
        tray_icon_ = new QSystemTrayIcon(QIcon(":/viper4linux/blue.png"), this);
        tray_icon_->setToolTip("Viper4Linux — PipeWire effects");
        auto* menu = new QMenu(this);
        auto* showAction = menu->addAction("Show / Hide Window");
        connect(showAction, &QAction::triggered, this, [this] {
            if (isVisible() && !isMinimized()) { saveUiGeometry(); hide(); }
            else restoreFromTray();
        });
        tray_processing_action_ = menu->addAction("Enable Processing");
        tray_processing_action_->setCheckable(true);
        connect(tray_processing_action_, &QAction::triggered, this, [this](bool enabled) {
            global_->setChecked(enabled); // Existing master toggle handles DSP publishing.
        });
        connect(global_, &QCheckBox::toggled, this, [this](bool enabled) {
            if (tray_processing_action_) tray_processing_action_->setChecked(enabled);
        });
        connect(menu, &QMenu::aboutToShow, this, [this] {
            tray_processing_action_->setChecked(global_->isChecked());
        });
        menu->addSeparator();
        auto* prefs = menu->addAction("Preferences…");
        connect(prefs, &QAction::triggered, this, [this] { showPreferences(); });
        menu->addSeparator();
        auto* quit = menu->addAction("Quit GUI");
        connect(quit, &QAction::triggered, this, [this] { quitGui(); });
        tray_icon_->setContextMenu(menu);
        connect(tray_icon_, &QSystemTrayIcon::activated, this,
                [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick)
                return;
            if (isVisible() && !isMinimized()) { saveUiGeometry(); hide(); }
            else restoreFromTray();
        });
        tray_icon_->show();
    }
    QString autostartFile() const {
        const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        return config + "/autostart/viper4linux-re.desktop";
    }
    bool setAutostart(bool enabled) {
        const QString target = autostartFile();
        if (!enabled) {
            if (!QFile::exists(target)) return true;
            return QFile::remove(target);
        }
        if (!QDir().mkpath(QFileInfo(target).absolutePath())) return false;
        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly)) return false;
        const QByteArray desktop =
            "[Desktop Entry]\nType=Application\nName=Viper4Linux\n"
            "Comment=Start Viper4Linux at login\nExec=viper4linux\n"
            "TryExec=viper4linux\nIcon=viper4linux\n"
            "Terminal=false\nX-GNOME-Autostart-enabled=true\n";
        if (file.write(desktop) != desktop.size()) { file.cancelWriting(); return false; }
        return file.commit();
    }
    QString outputMemoryDirectory() const {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + "/viper4linux/outputs";
    }
    bool outputMemoryEnabled() const {
        QFile file(outputMemoryDirectory() + "/options.json");
        if (!file.open(QIODevice::ReadOnly)) return false;
        const auto doc = QJsonDocument::fromJson(file.readAll());
        return doc.isObject() && doc.object().value("format").toInt() == 1 &&
               doc.object().value("remember_outputs").toBool(false);
    }
    bool setOutputMemoryEnabled(bool enabled, QString* error) {
        const QString directory = outputMemoryDirectory();
        if (!QDir().mkpath(directory)) {
            *error = "Could not create Audio Output settings directory.";
            return false;
        }
        QSaveFile file(directory + "/options.json");
        if (!file.open(QIODevice::WriteOnly)) {
            *error = file.errorString();
            return false;
        }
        QJsonObject options;
        options.insert("format", 1);
        options.insert("remember_outputs", enabled);
        const QByteArray content = QJsonDocument(options).toJson(QJsonDocument::Indented);
        if (file.write(content) != content.size() || !file.commit()) {
            *error = "Could not save Audio Output preferences.";
            return false;
        }
        return true;
    }
    void updateAudioOutputDisplay() {
        const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        if (runtime.isEmpty()) {
            audio_output_label_->setText("Audio Output: unavailable");
            return;
        }
        QFile file(QDir(runtime).filePath("viper4linux-output-status.json"));
        if (!file.open(QIODevice::ReadOnly)) {
            audio_output_label_->setText("Audio Output: detecting…");
            return;
        }
        const QJsonDocument status = QJsonDocument::fromJson(file.readAll());
        if (!status.isObject() || status.object().value("format").toInt() != 1 ||
            std::abs(status.object().value("timestamp").toDouble() -
                     double(QDateTime::currentSecsSinceEpoch())) > 5.0) {
            audio_output_label_->setText("Audio Output: unavailable");
            return;
        }
        const QJsonObject active = status.object().value("active").toObject();
        const QString description = active.value("description").toString();
        const QString identity = active.value("key").toString();
        if (description.isEmpty() || identity.size() != 64) {
            audio_output_label_->setText("Audio Output: detecting…");
            return;
        }
        audio_output_label_->setText("Audio Output: " + description);
        const bool memoryEnabled = status.object().value("memory_enabled").toBool(false);
        audio_output_label_->setToolTip("Audio Output identity: " + identity.left(12) +
            (memoryEnabled ? "\nAutomatic per-output effect restoration enabled."
                           : "\nAutomatic per-output effect restoration disabled."));
        const int revision = status.object().value("memory_revision").toInt();
        const QString session = QString::number(status.object().value("manager_pid").toInt());
        const QString token = session + ":" + QString::number(revision);
        if (revision > 0 && token != last_output_memory_revision_) {
            last_output_memory_revision_ = token;
            settings_.sync();
            // These controls have their own editor-side arrays. Basic sliders
            // and switches already follow the live IPC through readBackend().
            loadDeqSettings();
            loadMultibandSettings("");
            updateDdcFileName(settings_.value("ddc_profile").toString());
            refreshDdcLibrary();
            refreshIrsLibrary();
        }
    }

    void showPreferences() {
        QDialog dialog(this);
        dialog.setWindowTitle("Viper4Linux — Preferences");
        dialog.setMinimumWidth(395);
        auto* body = new QVBoxLayout(&dialog);
        auto* startup = new QGroupBox("When Viper4Linux starts", &dialog);
        auto* startLayout = new QVBoxLayout(startup);
        auto* startGroup = new QButtonGroup(&dialog);
        auto* startNormal = new QRadioButton("Show main window", startup);
        auto* startMin = new QRadioButton("Start minimized to taskbar", startup);
        auto* startTray = new QRadioButton("Start hidden in system tray", startup);
        startGroup->addButton(startNormal, 0);
        startGroup->addButton(startMin, 1);
        startGroup->addButton(startTray, 2);
        for (auto* item : {startNormal, startMin, startTray}) startLayout->addWidget(item);
        const QString initial = settings_.value("ui/startupBehavior", "show").toString();
        if (initial == "minimized") startMin->setChecked(true);
        else if (initial == "tray") startTray->setChecked(true);
        else startNormal->setChecked(true);
        body->addWidget(startup);

        auto* minimize = new QCheckBox("Minimize to tray instead of taskbar", &dialog);
        minimize->setChecked(settings_.value("ui/minimizeToTray", false).toBool());
        body->addWidget(minimize);

        auto* closeBox = new QGroupBox("When the close (X) button is clicked", &dialog);
        auto* closeLayout = new QVBoxLayout(closeBox);
        auto* closeGroup = new QButtonGroup(&dialog);
        auto* closeQuit = new QRadioButton("Quit GUI (keep audio backend running)", closeBox);
        auto* closeMin = new QRadioButton("Minimize to taskbar", closeBox);
        auto* closeTray = new QRadioButton("Hide to system tray", closeBox);
        closeGroup->addButton(closeQuit, 0);
        closeGroup->addButton(closeMin, 1);
        closeGroup->addButton(closeTray, 2);
        for (auto* item : {closeQuit, closeMin, closeTray}) closeLayout->addWidget(item);
        const QString ending = settings_.value("ui/closeBehavior", "quit").toString();
        if (ending == "minimized") closeMin->setChecked(true);
        else if (ending == "tray") closeTray->setChecked(true);
        else closeQuit->setChecked(true);
        body->addWidget(closeBox);

        auto* autostart = new QCheckBox("Launch automatically when I log in", &dialog);
        autostart->setChecked(QFile::exists(autostartFile()));
        body->addWidget(autostart);
        auto* memoryGroup = new QGroupBox("Audio Output", &dialog);
        auto* memoryLayout = new QVBoxLayout(memoryGroup);
        auto* rememberOutputs = new QCheckBox("Remember effects separately for each Audio Output",
                                             memoryGroup);
        rememberOutputs->setChecked(outputMemoryEnabled());
        memoryLayout->addWidget(rememberOutputs);
        auto* memoryExplanation = new QLabel(
            "When enabled, changes are saved for each physical output and restored "
            "when you switch back. Works while the GUI is closed. "
            "The current sound is retained when enabling; named presets are independent.",
            memoryGroup);
        memoryExplanation->setWordWrap(true);
        memoryLayout->addWidget(memoryExplanation);
        body->addWidget(memoryGroup);
        auto* notice = new QLabel(
            "If the desktop tray is unavailable, Viper4Linux stays accessible\n"
            "as an ordinary window or taskbar application.", &dialog);
        notice->setWordWrap(true);
        body->addWidget(notice);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        body->addWidget(buttons);
        if (dialog.exec() != QDialog::Accepted) return;
        settings_.setValue("ui/startupBehavior",
            startGroup->checkedId() == 2 ? "tray" : startGroup->checkedId() == 1 ? "minimized" : "show");
        settings_.setValue("ui/minimizeToTray", minimize->isChecked());
        settings_.setValue("ui/closeBehavior",
            closeGroup->checkedId() == 2 ? "tray" : closeGroup->checkedId() == 1 ? "minimized" : "quit");
        settings_.sync();
        if (!setAutostart(autostart->isChecked())) {
            QMessageBox::warning(this, "Autostart",
                "Could not update the XDG autostart entry:\n" + autostartFile());
        }
        if (rememberOutputs->isChecked() != outputMemoryEnabled()) {
            // Enabling while the native channel is unavailable cannot capture
            // the current sound. Never claim to remember an unknown state.
            if (rememberOutputs->isChecked() && !ensureBackend()) {
                QMessageBox::warning(this, "Audio Output Settings",
                    "Audio processing must be ready before enabling per-output memory. "
                    "Check that the ViPER backend and native DSP channel are running, then try again.");
            } else {
                QString error;
                if (!setOutputMemoryEnabled(rememberOutputs->isChecked(), &error))
                    QMessageBox::warning(this, "Audio Output Settings", error);
            }
        }
    }
    QMenu* makeApplicationMenu(QWidget* parent) {
        auto* menu = new QMenu(parent);
        auto* preferences = menu->addAction("Preferences…");
        connect(preferences, &QAction::triggered, this, [this] { showPreferences(); });
        auto* presets = menu->addAction("Presets…");
        connect(presets, &QAction::triggered, this, [this] { showPresetManager(); });
        auto* libraries = menu->addMenu("Open Library");
        auto openFolder = [this](const QString& path) {
            if (!QDir().mkpath(path) ||
                !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
                QMessageBox::warning(this, "Open Library", "Could not open: " + path);
        };
        auto* irs = libraries->addAction("IRS impulse responses");
        connect(irs, &QAction::triggered, this, [openFolder] {
            openFolder(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                       + "/viper4linux/IRS");
        });
        auto* vdc = libraries->addAction("VDC correction profiles");
        connect(vdc, &QAction::triggered, this, [openFolder] {
            openFolder(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                       + "/viper4linux/VDC");
        });
        auto* logAction = menu->addAction("View Backend Log");
        connect(logAction, &QAction::triggered, this, [this] {
            const QString state = qEnvironmentVariable("XDG_STATE_HOME", QDir::homePath() + "/.local/state");
            const QString path = state + "/viper4linux/backend.log";
            QFile log(path);
            if (!log.open(QIODevice::ReadOnly)) {
                QMessageBox::information(this, "Backend Log", "No backend log found at:\n" + path);
                return;
            }
            const qint64 start = std::max<qint64>(0, log.size() - 128 * 1024);
            log.seek(start);
            QDialog viewer(this);
            viewer.setWindowTitle("Viper4Linux — Backend Log");
            viewer.resize(750, 480);
            auto* layout = new QVBoxLayout(&viewer);
            auto* text = new QPlainTextEdit(&viewer);
            text->setReadOnly(true);
            text->setPlainText(QString::fromUtf8(log.readAll()));
            layout->addWidget(text);
            auto* close = new QDialogButtonBox(QDialogButtonBox::Close, &viewer);
            connect(close, &QDialogButtonBox::rejected, &viewer, &QDialog::reject);
            layout->addWidget(close);
            viewer.exec();
        });
        menu->addSeparator();
        auto* guide = menu->addAction("Effect Guide");
        connect(guide, &QAction::triggered, this, [this] { showHelp(); });
        auto* about = menu->addAction("About Viper4Linux");
        connect(about, &QAction::triggered, this, [this] { showAbout(); });
        menu->addSeparator();
        auto* quit = menu->addAction("Quit GUI");
        connect(quit, &QAction::triggered, this, [this] { quitGui(); });
        return menu;
    }
    void showAbout() {
        // A minimal, non-intrusive About dialog, kept independent of the
        // effect help. The development revision comes from the source commit.
        QMessageBox about(this);
        about.setWindowTitle("About Viper4Linux");
        about.setWindowIcon(QIcon(":/viper4linux/blue.png"));
        about.setIconPixmap(QPixmap(":/viper4linux/blue.png").scaled(
            48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        about.setText("<b>Viper4Linux</b>");
        about.setInformativeText(
            QString("Version: <b>%1</b><br>"
                    "Application commit: <code>%2</code><br>"
                    "ViPERDSP upstream: <code>%3</code><br><br>"
                    "Native ViPERDSP processing via PipeWire; Qt6 control interface.")
                .arg(VIPER4LINUX_DEV_VERSION,
                     VIPER4LINUX_DEV_REVISION,
                     VIPER4LINUX_VIPERDSP_REVISION));
        about.setStandardButtons(QMessageBox::Ok);
        about.exec();
    }
    void showHelp() {
        QDialog help(this);
        help.setWindowTitle("Viper4Linux — Effect Guide");
        help.resize(650, 590);
        auto* layout = new QVBoxLayout(&help);
        auto* text = new QTextBrowser(&help);
        text->setOpenExternalLinks(true);
        text->setHtml(R"HTML(<h2>Viper4Linux — Effects</h2>
<p>These descriptions explain the available audio controls and their behavior.</p>
<h3>Output</h3><p><b>Master Limiter:</b> Keeps output peaks under the selected threshold; remains active during bypass.</p>
<p><b>Playback Gain Control (AGC):</b> Automatically adjusts changing playback levels.</p>
<p><b>LUFS Targeting:</b> Adjusts perceived loudness toward a target over time; avoid combining with AGC without testing.</p>
<h3>Bass</h3><p><b>ViPER Bass:</b> Low-frequency enhancement, with Natural, Pure Bass+ and Subwoofer modes.</p>
<p><b>Mono Bass:</b> Uses a mono-derived bass component for both channels.</p>
<p><b>Psychoacoustic Bass:</b> Generates harmonics to make low frequencies more perceptible on smaller drivers.</p>
<p><b>Dynamic System:</b> Shapes bass and dynamic frequency response using crossover and gain parameters.</p>
<h3>EQ &amp; Tone</h3><p><b>Graphic Equalizer:</b> Native 10/15/25/31-band minimum-phase IIR EQ. Select a curve preset or click the read-only response graph to open vertical faders. Save your own named curves; zero dB is flat.</p><p><b>Clarity:</b> Enhances high-frequency detail in Natural, Ozone or XHiFi modes.</p>
<p><b>Spectrum Extension:</b> Adds upper-frequency harmonics above a reference frequency.</p>
<p><b>Tube Simulator:</b> Native ViPERDSP tube-coloration effect with 12AX7, 12AU7 and 6N1P models, drive and wet/dry mix.</p>
<p><b>AnalogX:</b> Native ViPERDSP harmonic coloration with Mild, Medium and Strong modes. The underlying effect briefly resets its filters when enabled or switched.</p>
<h3>Spatial</h3><p><b>Crossfeed (ViPER Cure):</b> Filters a small portion of each stereo channel into the opposite ear (Mild/Medium/Strong), for more speaker-like headphone listening.</p><p><b>Headphone Surround+:</b> Headphone-oriented spatial processing.</p>
<p><b>Differential Surround:</b> Uses interchannel delay and filtering to alter stereo width.</p>
<p><b>Field Surround:</b> Adjusts perceived width, center image and depth.</p>
<p><b>Reverberation:</b> Adds simulated room reflections and ambience.</p>
<p><b>Stereo Imager:</b> Native three-band stereo width processing. Low, mid and high widths each range from 0 to 200%; 100% is neutral. Separate low/mid (80–400 Hz) and mid/high (2–8 kHz) crossovers. It is off by default, and excessive widening may reduce mono compatibility.</p>
<h3>Sound Correction</h3><p><b>Convolver:</b> Applies a WAV-encoded .irs or .wav impulse response. Select from the local IRS library, or use Import to copy a new file into it. Kernel changes apply live.</p>
<p><b>IRS library:</b> ~/.local/share/viper4linux/IRS/ (or the XDG data home equivalent). Import multiple files at once; the dropdown updates when the directory changes, never while open. Different-rate IRs are resampled in the backend without restarting audio.</p>
<h3>Device correction</h3><p>Native ViPER-DDC reads standard .vdc correction profiles at 44.1/48 kHz; native Speaker Correction uses built-in fixed filters for loudspeakers.</p>)HTML");
        layout->addWidget(text);
        auto* close = new QPushButton("Close", &help);
        connect(close, &QPushButton::clicked, &help, &QDialog::accept);
        layout->addWidget(close, 0, Qt::AlignRight);
        help.exec();
    }
    void statusMessage(const QString& message) {
        status_->setText(message);
    }
    void migrateEqPrecision() {
        // Older masters saved 0.1 dB integers; new native EQ IPC uses 0.01 dB.
        // Convert settings exactly once, including saved custom EQ presets.
        if (settings_.value("eq_precision_hundredths", false).toBool()) return;
        auto convert = [this](const QString& key) {
            if (!settings_.contains(key)) return;
            const auto fields = settings_.value(key).toString().split(',');
            QStringList values;
            for (const auto& field : fields) {
                bool ok = false;
                const int prior = field.trimmed().toInt(&ok);
                if (!ok) return; // leave malformed user data untouched
                values << QString::number(std::clamp(prior * 10, -1200, 1200));
            }
            settings_.setValue(key, values.join(','));
        };
        for (int count : {10, 15, 25, 31}) {
            convert("eq_levels_" + QString::number(count));
            convert("preset/eq_levels_" + QString::number(count));
        }
        settings_.beginGroup("eq/user_presets");
        const auto names = settings_.childGroups();
        settings_.endGroup();
        for (const auto& name : names)
            convert("eq/user_presets/" + name + "/levels");
        settings_.setValue("eq_precision_hundredths", true);
        settings_.sync();
    }
    static QString flatEqCsv(int n) {
        QStringList values;
        for (int i = 0; i < n; ++i) values << "0";
        return values.join(",");
    }
    static int validEqBands(int n) {
        return (n == 10 || n == 15 || n == 25 || n == 31) ? n : 10;
    }
    static const std::array<float, 31>& eqFrequencies(int bands) {
        return viper_eq::frequencies(bands);
    }
    static const std::array<viper_eq::NamedCurve, 12>& builtInEq() {
        return viper_eq::builtIns();
    }
    static std::vector<int> interpolateEq(const std::vector<float>& srcHz,
                                           const std::vector<int>& srcLevels, int bands) {
        return viper_eq::interpolate(srcHz, srcLevels, bands);
    }
    void populateEqPresets() {
        const QString selected = eq_selected_preset_;
        const QSignalBlocker blocker(eq_preset_);
        eq_preset_->clear();
        for (const auto& item : builtInEq())
            eq_preset_->addItem(QString::fromLatin1(item.name),
                                "builtin:" + QString::fromLatin1(item.name));
        eq_preset_->insertSeparator(eq_preset_->count());
        eq_preset_->addItem("Custom", "custom");
        settings_.beginGroup("eq/user_presets");
        const QStringList names = settings_.childGroups();
        settings_.endGroup();
        if (!names.isEmpty()) eq_preset_->insertSeparator(eq_preset_->count());
        for (const QString& name : names)
            eq_preset_->addItem(name, "user:" + name);
        selectEqPreset(selected.isEmpty() ? "custom" : selected);
    }
    void selectEqPreset(const QString& requested) {
        const QSignalBlocker blocker(eq_preset_);
        int index = eq_preset_->findData(requested);
        if (index < 0) index = eq_preset_->findData("custom");
        if (index < 0) return;
        eq_preset_->setCurrentIndex(index);
        eq_selected_preset_ = eq_preset_->itemData(index).toString();
        settings_.setValue("eq/selected_preset", eq_selected_preset_);
        updateEqDeleteButton();
    }
    void updateEqDeleteButton() {
        if (eq_delete_button_)
            eq_delete_button_->setEnabled(eq_selected_preset_.startsWith("user:"));
    }
    void applyEqPreset(const QString& preset) {
        std::vector<float> freq;
        std::vector<int> vals;
        if (preset.startsWith("builtin:")) {
            const QString name = preset.mid(8);
            for (const auto& item : builtInEq()) {
                if (name != QString::fromLatin1(item.name)) continue;
                vals = item.levels(eq_active_bands_); // exact upstream layout, no interpolation
                break;
            }
        } else if (preset.startsWith("user:")) {
            const QString path = "eq/user_presets/" + preset.mid(5) + "/";
            const int savedBands = validEqBands(settings_.value(path + "bands", 10).toInt());
            const auto saved = settings_.value(path + "levels", "").toString().split(',');
            if (saved.size() == savedBands) {
                const auto& f = eqFrequencies(savedBands);
                for (int i = 0; i < savedBands; ++i) {
                    bool ok = false;
                    const int value = saved[i].toInt(&ok);
                    if (!ok) { freq.clear(); vals.clear(); break; }
                    freq.push_back(f[i]);
                    vals.push_back(std::clamp(value, -1200, 1200));
                }
            }
        }
        if (vals.empty()) { selectEqPreset("custom"); return; }
        const auto mapped = preset.startsWith("builtin:") ? vals :
            interpolateEq(freq, vals, eq_active_bands_);
        for (int i = 0; i < eq_active_bands_; ++i) {
            const QSignalBlocker blocker(eq_faders_[i]);
            eq_faders_[i]->setValue(mapped[i]);
            eq_values_[i]->setText(QString::number(mapped[i] / 100.0, 'f', 2));
        }
        saveEqLayout();
        updateEqOverview();
        selectEqPreset(preset);
        push();
    }
    void saveUserEqPreset() {
        bool accepted = false;
        QString name = QInputDialog::getText(this, "Save EQ preset", "Preset name:",
                                             QLineEdit::Normal, "", &accepted).trimmed();
        if (!accepted || name.isEmpty()) return;
        if (name.contains('/') || name.contains('\\') || name.size() > 70) {
            QMessageBox::warning(this, "Invalid preset name",
                "Use a name of at most 70 characters without slashes.");
            return;
        }
        settings_.beginGroup("eq/user_presets");
        const auto existing = settings_.childGroups();
        settings_.endGroup();
        for (const auto& other : existing) {
            if (other.compare(name, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, "Preset already exists",
                    "A saved EQ preset already has that name. Nothing was overwritten.");
                return;
            }
        }
        QStringList vals;
        for (int i = 0; i < eq_active_bands_; ++i)
            vals << QString::number(eq_faders_[i]->value());
        const QString path = "eq/user_presets/" + name + "/";
        settings_.setValue(path + "bands", eq_active_bands_);
        settings_.setValue(path + "levels", vals.join(','));
        settings_.sync();
        eq_selected_preset_ = "user:" + name;
        populateEqPresets();
    }
    void deleteUserEqPreset() {
        if (!eq_selected_preset_.startsWith("user:")) return;
        const QString name = eq_selected_preset_.mid(5);
        if (QMessageBox::question(this, "Delete EQ preset",
                "Delete saved preset '" + name + "'? The current EQ curve will be retained.")
                != QMessageBox::Yes) return;
        settings_.remove("eq/user_presets/" + name);
        eq_selected_preset_ = "custom";
        populateEqPresets();
    }
    void updateEqOverview() {
        if (!eq_overview_) return;
        std::vector<int> levels;
        levels.reserve(eq_active_bands_);
        for (int i = 0; i < eq_active_bands_; ++i)
            levels.push_back(eq_faders_[i]->value());
        eq_overview_->setLevels(levels);
    }

    void saveEqLayout() {
        QStringList levels;
        for (int i = 0; i < eq_active_bands_; ++i)
            levels << QString::number(eq_faders_[i]->value());
        settings_.setValue("eq_levels_" + QString::number(eq_active_bands_), levels.join(","));
    }
    void loadEqLayout() {
        const int n = validEqBands(eq_active_bands_);
        const QStringList stored = settings_.value("eq_levels_" + QString::number(n), "").toString().split(',');
        const auto& freqs = eqFrequencies(n);
        for (int i = 0; i < 31; ++i) {
            const QSignalBlocker blocker(eq_faders_[i]);
            if (i < n) {
                int value = 0;
                if (i < stored.size()) {
                    bool ok = false;
                    const int parsed = stored[i].toInt(&ok);
                    if (ok) value = std::clamp(parsed, -1200, 1200);
                }
                eq_faders_[i]->setValue(value);
                eq_values_[i]->setText(QString::number(value / 100.0, 'f', 2));
                const float f = freqs[i];
                const QString label = f >= 1000 ? QString::number(f / 1000.0f, 'g', 3) + "k"
                                                : QString::number(f, 'g', 4);
                eq_frequencies_[i]->setText(label);
                eq_faders_[i]->setToolTip(QString::number(f, 'g', 6) + " Hz: " + eq_values_[i]->text() + " dB");
            }
            eq_columns_[i]->setVisible(i < n);
        }
        eq_fader_container_->setFixedWidth(n * 39 + 6);
        eq_fader_container_->updateGeometry();
        updateEqOverview();
    }
    void persistCurrentSettings() {
        settings_.setValue("master_enabled", global_->isChecked());
        settings_.setValue("enabled", enabled_->isChecked());
        settings_.setValue("bass_mode", bass_mode_->currentIndex());
        settings_.setValue("strength_tenths", strength_.slider->value());
        settings_.setValue("frequency", frequency_.slider->value());
        saveEqLayout();
        settings_.setValue("eq_enabled", eq_enabled_->isChecked());
        settings_.setValue("eq_band_count", eq_active_bands_);
        settings_.setValue("eq/selected_preset", eq_selected_preset_);
        settings_.setValue("ddc_enabled", ddc_enabled_->isChecked());
        settings_.setValue("speaker_enabled", speaker_enabled_->isChecked());
        settings_.setValue("convolver_enabled", convolver_enabled_->isChecked());
        settings_.setValue("convolver_cross_percent", convolver_cross_.slider->value());
        settings_.setValue("bass_mono_enabled", mono_enabled_->isChecked());
        settings_.setValue("bass_mono_mode", mono_mode_->currentIndex());
        settings_.setValue("bass_mono_strength_centi", mono_strength_.slider->value()*10);
        settings_.setValue("bass_mono_frequency_hz", mono_frequency_.slider->value());
        settings_.setValue("psycho_enabled", psycho_enabled_->isChecked());
        settings_.setValue("psycho_cutoff_hz", psycho_cutoff_.slider->value());
        settings_.setValue("psycho_intensity_percent", psycho_intensity_.slider->value());
        settings_.setValue("psycho_order", psycho_order_->currentIndex()+2);
        settings_.setValue("psycho_original_percent", psycho_original_.slider->value());
        settings_.setValue("dynamic_enabled", dyn_enabled_->isChecked());
        settings_.setValue("dynamic_x_low", dyn_xlo_.slider->value());
        settings_.setValue("dynamic_x_high", dyn_xhi_.slider->value());
        settings_.setValue("dynamic_y_low", dyn_ylo_.slider->value());
        settings_.setValue("dynamic_y_high", dyn_yhi_.slider->value());
        settings_.setValue("dynamic_side_low_percent", dyn_side_lo_.slider->value());
        settings_.setValue("dynamic_side_high_percent", dyn_side_hi_.slider->value());
        settings_.setValue("dynamic_strength_percent", dyn_strength_.slider->value());
        settings_.setValue("clarity_enabled", clarity_enabled_->isChecked());
        settings_.setValue("clarity_mode", clarity_mode_->currentIndex());
        settings_.setValue("clarity_gain_tenths", clarity_gain_.slider->value());
        settings_.setValue("spectrum_enabled", spectrum_enabled_->isChecked());
        settings_.setValue("spectrum_frequency_hundreds", spectrum_frequency_.slider->value());
        settings_.setValue("spectrum_exciter_tenths", spectrum_exciter_.slider->value());
        settings_.setValue("tube_enabled", tube_enabled_->isChecked());
        settings_.setValue("tube_model", tube_model_->currentIndex());
        settings_.setValue("tube_drive_percent", tube_drive_.slider->value());
        settings_.setValue("tube_mix_percent", tube_mix_.slider->value());
        settings_.setValue("analogx_enabled", analogx_enabled_->isChecked());
        settings_.setValue("analogx_mode", analogx_mode_->currentIndex());
        settings_.setValue("crossfeed_enabled", crossfeed_enabled_->isChecked());
        settings_.setValue("crossfeed_preset", crossfeed_mode_->currentIndex());
        saveMultibandSettings("");
        settings_.setValue("fet_enabled", fet_enabled_->isChecked());
        settings_.setValue("fet_threshold_db", fet_threshold_.slider->value());
        settings_.setValue("fet_ratio_tenths", fet_ratio_.slider->value());
        settings_.setValue("fet_knee_db", fet_knee_.slider->value());
        settings_.setValue("fet_makeup_db", fet_gain_.slider->value());
        settings_.setValue("fet_attack_ms", fet_attack_.slider->value());
        settings_.setValue("fet_release_ms", fet_release_.slider->value());
        settings_.setValue("imager_enabled", imager_enabled_->isChecked());
        settings_.setValue("imager_low_width_percent", imager_low_.slider->value());
        settings_.setValue("imager_mid_width_percent", imager_mid_.slider->value());
        settings_.setValue("imager_high_width_percent", imager_high_.slider->value());
        settings_.setValue("imager_low_crossover_hz", imager_low_xover_.slider->value());
        settings_.setValue("imager_high_crossover_hz", imager_high_xover_.slider->value() * 100);
        settings_.setValue("deq_enabled", deq_enabled_->isChecked());
        settings_.setValue("deq_band_count", deq_active_count_);
        for (int i=0; i<10; ++i) {
            const QString k = "deq_b"+QString::number(i)+"_";
            const DeqBand& b=deq_bands_[i];
            settings_.setValue(k+"freq",b.frequency);
            settings_.setValue(k+"user_modified", deq_user_modified_[i]);
            settings_.setValue(k+"q",b.q);
            settings_.setValue(k+"gain",b.gain);
            settings_.setValue(k+"threshold",b.threshold);
            settings_.setValue(k+"attack",b.attack);
            settings_.setValue(k+"release",b.release);
            settings_.setValue(k+"type",b.type);
        }
        settings_.setValue("headphone_enabled", headphone_enabled_->isChecked());
        settings_.setValue("headphone_quality", headphone_quality_->currentIndex());
        settings_.setValue("diff_enabled", diff_enabled_->isChecked());
        settings_.setValue("diff_delay_ms", diff_delay_.slider->value());
        settings_.setValue("diff_reverse", diff_reverse_->isChecked());
        settings_.setValue("diff_mix_percent", diff_mix_.slider->value());
        settings_.setValue("diff_lp_hundreds", diff_lowpass_.slider->value());
        settings_.setValue("field_enabled", field_enabled_->isChecked());
        settings_.setValue("field_widen_tenths", field_widen_.slider->value());
        settings_.setValue("field_mid_hundredths", field_mid_.slider->value());
        settings_.setValue("field_depth_step", field_depth_.slider->value());
        settings_.setValue("reverb_enabled", reverb_enabled_->isChecked());
        settings_.setValue("reverb_room_percent", reverb_room_.slider->value());
        settings_.setValue("reverb_width_percent", reverb_width_.slider->value());
        settings_.setValue("reverb_damp_percent", reverb_damp_.slider->value());
        settings_.setValue("reverb_wet_percent", reverb_wet_.slider->value());
        settings_.setValue("reverb_dry_percent", reverb_dry_.slider->value());
        settings_.setValue("limiter_threshold_permille", limiter_threshold_.slider->value());
        settings_.setValue("limiter_output_percent", limiter_gain_.slider->value());
        settings_.setValue("limiter_pan_percent", limiter_pan_.slider->value());
        settings_.setValue("agc_enabled", agc_enabled_->isChecked());
        settings_.setValue("agc_strength_centi", agc_strength_.slider->value());
        settings_.setValue("agc_max_gain_centi", agc_max_gain_.slider->value());
        settings_.setValue("agc_threshold_permille", agc_threshold_.slider->value());
        settings_.setValue("lufs_enabled", lufs_enabled_->isChecked());
        settings_.setValue("lufs_target_tenths", lufs_target_.slider->value());
        settings_.setValue("lufs_max_gain_tenths", lufs_gain_.slider->value());
        settings_.setValue("lufs_speed", lufs_speed_->currentIndex());
        save_timer_->start(300); // batch writes while a slider is dragged
    }
    // Always check the named control-channel object, not just the owner PID.
    // If an older object was unlinked while its owner still lived, a cached
    // pointer could silently accept edits that the current backend never saw.
    bool ensureBackend() {
        if (mapping_.current()) return true;
        mapping_.close();
        return mapping_.attach();
    }
    void showOffline() {
        audio_activity_.reset();
        if (viperbass::detected_backend_pid() > 0) {
            status_->setText(pending_edit_
                ? "○ Ready to process audio — changes queued until DSP activates"
                : "○ Ready to process audio — backend started; DSP not active yet");
        } else {
            status_->setText(pending_edit_
                ? "○ Offline — changes queued until ViPER reconnects"
                : "○ Offline — start viper4linux");
        }
        status_->setStyleSheet(QString("color:%1;").arg(theme_muted_color_));
    }
    bool publishToBackend() {
        if (!ensureBackend()) {
            pending_edit_ = true;
            showOffline();
            return false;
        }
        auto* c = mapping_.get();
        // Preserve native precision when a different GUI control is edited.
        // Only a deliberate move of this slider replaces the DSP's precise value.
        const QObject* editedControl = sender();
        const auto preciseSlider = [this, editedControl](QSlider* slider, int liveValue,
                                                         int sliderValue, const char* setting) {
            // An unrelated edit must not quantize another slider, even before
            // the next GUI poll has reflected a concurrent CLI/output change.
            const int result = replace_precision_on_push_ || editedControl == slider
                ? sliderValue : liveValue;
            settings_.setValue(setting, result);
            return result;
        };
        c->master_enabled.store(global_->isChecked() ? 1 : 0);
        c->enabled.store(enabled_->isChecked() ? 1 : 0);
        c->bass_mode.store(bass_mode_->currentIndex());
        c->strength_centi.store(preciseSlider(strength_.slider, c->strength_centi.load(),
            strength_.slider->value() * 10,
            "precision/strength_centi"));
        c->frequency_hz.store(frequency_.slider->value());
        bool eq_changed = false;
        const auto setEqValue = [&eq_changed](std::atomic<std::int32_t>& atom, int value) {
            if (atom.load(std::memory_order_relaxed) != value) {
                atom.store(value, std::memory_order_relaxed);
                eq_changed = true;
            }
        };
        setEqValue(c->eq_enabled, eq_enabled_->isChecked() ? 1 : 0);
        setEqValue(c->eq_band_count, eq_active_bands_);
        for (int i = 0; i < eq_active_bands_; ++i)
            setEqValue(c->eq_levels_hundredths[i], eq_faders_[i]->value());
        if (eq_changed) c->eq_request_serial.fetch_add(1, std::memory_order_release);
        const int ddc_request = ddc_enabled_->isChecked() ? 1 : 0;
        if (c->ddc_enabled.exchange(ddc_request) != ddc_request)
            c->ddc_request_serial.fetch_add(1, std::memory_order_release);
        const int speaker_request = speaker_enabled_->isChecked() ? 1 : 0;
        if (c->speaker_enabled.exchange(speaker_request) != speaker_request)
            c->speaker_request_serial.fetch_add(1, std::memory_order_release);
        c->convolver_enabled.store(convolver_enabled_->isChecked() ? 1 : 0);
        c->convolver_cross_percent.store(convolver_cross_.slider->value());
        c->bass_mono_enabled.store(mono_enabled_->isChecked() ? 1 : 0);
        c->bass_mono_mode.store(mono_mode_->currentIndex());
        c->bass_mono_strength_centi.store(preciseSlider(mono_strength_.slider,
            c->bass_mono_strength_centi.load(),
            mono_strength_.slider->value() * 10, "precision/bass_mono_strength_centi"));
        c->bass_mono_frequency_hz.store(mono_frequency_.slider->value());
        c->psycho_enabled.store(psycho_enabled_->isChecked() ? 1 : 0);
        c->psycho_cutoff_hz.store(psycho_cutoff_.slider->value());
        c->psycho_intensity_percent.store(psycho_intensity_.slider->value());
        c->psycho_order.store(psycho_order_->currentIndex()+2);
        c->psycho_original_percent.store(psycho_original_.slider->value());
        c->dynamic_enabled.store(dyn_enabled_->isChecked() ? 1 : 0);
        c->dynamic_x_low.store(dyn_xlo_.slider->value());
        c->dynamic_x_high.store(dyn_xhi_.slider->value());
        c->dynamic_y_low.store(dyn_ylo_.slider->value());
        c->dynamic_y_high.store(dyn_yhi_.slider->value());
        c->dynamic_side_low_percent.store(dyn_side_lo_.slider->value());
        c->dynamic_side_high_percent.store(dyn_side_hi_.slider->value());
        c->dynamic_strength_percent.store(dyn_strength_.slider->value());
        c->clarity_enabled.store(clarity_enabled_->isChecked() ? 1 : 0);
        c->clarity_mode.store(clarity_mode_->currentIndex());
        c->clarity_gain_milli.store(preciseSlider(clarity_gain_.slider, c->clarity_gain_milli.load(),
            clarity_gain_.slider->value() * 100,
            "precision/clarity_gain_milli"));
        c->spectrum_enabled.store(spectrum_enabled_->isChecked() ? 1 : 0);
        c->spectrum_frequency_hz.store(preciseSlider(spectrum_frequency_.slider,
            c->spectrum_frequency_hz.load(),
            spectrum_frequency_.slider->value() * 100, "precision/spectrum_frequency_hz"));
        c->spectrum_exciter_milli.store(preciseSlider(spectrum_exciter_.slider,
            c->spectrum_exciter_milli.load(),
            spectrum_exciter_.slider->value() * 100, "precision/spectrum_exciter_milli"));
        bool tube_changed = false;
        const auto setTubeValue = [&tube_changed](std::atomic<std::int32_t>& atom, int value) {
            if (atom.load(std::memory_order_relaxed) != value) {
                atom.store(value, std::memory_order_relaxed);
                tube_changed = true;
            }
        };
        setTubeValue(c->tube_enabled, tube_enabled_->isChecked() ? 1 : 0);
        setTubeValue(c->tube_model, tube_model_->currentIndex());
        setTubeValue(c->tube_drive_percent, tube_drive_.slider->value());
        setTubeValue(c->tube_mix_percent, tube_mix_.slider->value());
        if (tube_changed) c->tube_request_serial.fetch_add(1, std::memory_order_release);
        bool analogx_changed = false;
        const auto setAnalogXValue = [&analogx_changed](std::atomic<std::int32_t>& atom, int value) {
            if (atom.load(std::memory_order_relaxed) != value) {
                atom.store(value, std::memory_order_relaxed);
                analogx_changed = true;
            }
        };
        setAnalogXValue(c->analogx_enabled, analogx_enabled_->isChecked() ? 1 : 0);
        setAnalogXValue(c->analogx_mode, analogx_mode_->currentIndex());
        if (analogx_changed) c->analogx_request_serial.fetch_add(1, std::memory_order_release);
        bool crossfeed_changed = false;
        const auto setCrossfeed = [&crossfeed_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed) != value) {
                dst.store(value, std::memory_order_relaxed);
                crossfeed_changed = true;
            }
        };
        setCrossfeed(c->crossfeed_enabled, crossfeed_enabled_->isChecked() ? 1 : 0);
        setCrossfeed(c->crossfeed_preset, crossfeed_mode_->currentIndex());
        if (crossfeed_changed) c->crossfeed_request_serial.fetch_add(1, std::memory_order_release);
        bool mb_changed = false;
        const auto setMb = [&mb_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed) != value) {
                dst.store(value, std::memory_order_relaxed);
                mb_changed = true;
            }
        };
        setMb(c->mb_enabled, mb_enabled_->isChecked() ? 1 : 0);
        setMb(c->mb_band_count, mb_band_count_->currentIndex() == 1 ? 5 : 3);
        for (int i=0; i<4; ++i) setMb(c->mb_crossovers_hz[i], mb_crossovers_[i].slider->value());
        for (int i=0; i<5; ++i) {
            const auto& src = mb_bands_[i];
            auto& dst = c->mb_bands[i];
            setMb(dst.enabled, src.enabled->isChecked() ? 1 : 0);
            setMb(dst.threshold_db, src.threshold.slider->value());
            setMb(dst.ratio_tenths, src.ratio.slider->value());
            setMb(dst.knee_db, src.knee.slider->value());
            setMb(dst.makeup_db, src.gain.slider->value());
            setMb(dst.attack_ms, src.attack.slider->value());
            setMb(dst.release_ms, src.release.slider->value());
        }
        if (mb_changed) c->mb_request_serial.fetch_add(1, std::memory_order_release);
        bool fet_changed = false;
        const auto setFet = [&fet_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed) != value) {
                dst.store(value, std::memory_order_relaxed);
                fet_changed = true;
            }
        };
        setFet(c->fet_enabled, fet_enabled_->isChecked() ? 1 : 0);
        setFet(c->fet_threshold_db, fet_threshold_.slider->value());
        setFet(c->fet_ratio_tenths, fet_ratio_.slider->value());
        setFet(c->fet_knee_db, fet_knee_.slider->value());
        setFet(c->fet_makeup_db, fet_gain_.slider->value());
        setFet(c->fet_attack_ms, fet_attack_.slider->value());
        setFet(c->fet_release_ms, fet_release_.slider->value());
        if (fet_changed) c->fet_request_serial.fetch_add(1, std::memory_order_release);
        bool imager_changed = false;
        const auto setImager = [&imager_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed) != value) {
                dst.store(value, std::memory_order_relaxed);
                imager_changed = true;
            }
        };
        setImager(c->imager_enabled, imager_enabled_->isChecked() ? 1 : 0);
        setImager(c->imager_low_width_percent, imager_low_.slider->value());
        setImager(c->imager_mid_width_percent, imager_mid_.slider->value());
        setImager(c->imager_high_width_percent, imager_high_.slider->value());
        setImager(c->imager_low_crossover_hz, imager_low_xover_.slider->value());
        setImager(c->imager_high_crossover_hz, imager_high_xover_.slider->value() * 100);
        if (imager_changed) c->imager_request_serial.fetch_add(1, std::memory_order_release);
        bool deq_changed = false;
        const auto setDeq = [&deq_changed](std::atomic<std::int32_t>& dst, int value) {
            if (dst.load(std::memory_order_relaxed)!=value) {
                dst.store(value,std::memory_order_relaxed);
                deq_changed=true;
            }
        };
        setDeq(c->deq_enabled,deq_enabled_->isChecked()?1:0);
        setDeq(c->deq_band_count,deq_active_count_);
        for(int i=0; i<10; ++i) {
            const auto& b=deq_bands_[i];
            auto& dst=c->deq_bands[i];
            setDeq(dst.frequency_hz,b.frequency);
            setDeq(dst.q_hundredths,b.q);
            setDeq(dst.gain_hundredths,b.gain);
            setDeq(dst.threshold_tenths,b.threshold);
            setDeq(dst.attack_ms,b.attack);
            setDeq(dst.release_ms,b.release);
            setDeq(dst.filter_type,b.type);
        }
        if (deq_changed) c->deq_request_serial.fetch_add(1,std::memory_order_release);
        c->headphone_enabled.store(headphone_enabled_->isChecked() ? 1 : 0);
        c->headphone_quality.store(headphone_quality_->currentIndex());
        c->diff_enabled.store(diff_enabled_->isChecked() ? 1 : 0);
        c->diff_delay_ms.store(diff_delay_.slider->value());
        c->diff_reverse.store(diff_reverse_->isChecked() ? 1 : 0);
        c->diff_mix_percent.store(diff_mix_.slider->value());
        c->diff_lp_hz.store(preciseSlider(diff_lowpass_.slider, c->diff_lp_hz.load(),
            diff_lowpass_.slider->value() * 100,
            "precision/diff_lp_hz"));
        c->field_enabled.store(field_enabled_->isChecked() ? 1 : 0);
        c->field_widen_tenths.store(field_widen_.slider->value());
        c->field_mid_hundredths.store(field_mid_.slider->value());
        const int liveFieldDepth = c->field_depth.load();
        c->field_depth.store(preciseSlider(field_depth_.slider, liveFieldDepth,
            field_depth_.slider->value() == 0 ? 0 : 150 + 50 * field_depth_.slider->value(),
            "precision/field_depth"));
        c->reverb_enabled.store(reverb_enabled_->isChecked() ? 1 : 0);
        c->reverb_room_percent.store(reverb_room_.slider->value());
        c->reverb_width_percent.store(reverb_width_.slider->value());
        c->reverb_damp_percent.store(reverb_damp_.slider->value());
        c->reverb_wet_percent.store(reverb_wet_.slider->value());
        c->reverb_dry_percent.store(reverb_dry_.slider->value());
        c->limiter_threshold_permille.store(limiter_threshold_.slider->value());
        c->limiter_output_percent.store(limiter_gain_.slider->value());
        c->limiter_pan_percent.store(limiter_pan_.slider->value());
        c->agc_enabled.store(agc_enabled_->isChecked() ? 1 : 0);
        c->agc_strength_centi.store(agc_strength_.slider->value());
        c->agc_max_gain_centi.store(agc_max_gain_.slider->value());
        c->agc_threshold_permille.store(agc_threshold_.slider->value());
        c->lufs_enabled.store(lufs_enabled_->isChecked() ? 1 : 0);
        c->lufs_target_tenths.store(lufs_target_.slider->value());
        c->lufs_max_gain_tenths.store(lufs_gain_.slider->value());
        c->lufs_speed.store(lufs_speed_->currentIndex());
        // A backend can exit while we're writing. Detect the unlink/replacement
        // so the edit will be sent again when a new processor becomes available.
        if (!mapping_.current()) {
            mapping_.close();
            pending_edit_ = true;
            showOffline();
            return false;
        }
        pending_edit_ = false;
        replace_precision_on_push_ = false;
        return true;
    }
    // Evenly sampled, log-spaced standard frequency centers. Keep the original
    // three-band starting point. This is GUI initialization only, NOT a DSP
    // filter change; manual frequencies are never silently rewritten.
    static int defaultDeqFrequency(int i, int count) {
        count = std::clamp(count, 1, 10);
        i = std::clamp(i, 0, count - 1);
        if (count == 1) return 1000;
        if (count == 2) return i == 0 ? 125 : 8000;
        if (count == 3) {
            static constexpr int three[] = {100, 1000, 8000};
            return three[i];
        }
        static constexpr int ten[] = {31, 62, 125, 250, 500, 1000,
                                      2000, 4000, 8000, 16000};
        return ten[std::clamp(qRound(i * 9.0 / (count - 1)), 0, 9)];
    }
    static DeqBand defaultDeqBand(int i, int count = 3) {
        DeqBand b;
        b.frequency = defaultDeqFrequency(i, count);
        return b;
    }
    void changeDeqBandCount(int count) {
        deq_active_count_ = std::clamp(count, 1, 10);
        for (int i = 0; i < deq_active_count_; ++i) {
            // Saved/manual bands remain unchanged. Only unconfigured defaults
            // follow the selected band count.
            if (!deq_user_modified_[i])
                deq_bands_[i].frequency = defaultDeqFrequency(i, deq_active_count_);
        }
        {
            const QSignalBlocker mainBlock(deq_count_), editorBlock(deq_editor_count_);
            deq_count_->setValue(deq_active_count_);
            deq_editor_count_->setValue(deq_active_count_);
        }
        refreshDeqBandSelector();
        updateDeqOverview();
        push();
    }
    void spreadDeqFrequencies() {
        // This is an explicit user action: replace only the frequencies of
        // active bands, retaining all the existing gain/dynamics parameters.
        for (int i = 0; i < deq_active_count_; ++i) {
            deq_bands_[i].frequency = defaultDeqFrequency(i, deq_active_count_);
            deq_user_modified_[i] = false;
        }
        refreshDeqBandSelector();
        updateDeqOverview();
        push();
    }
    void loadDeqSettings() {
        const QSignalBlocker b1(deq_enabled_), b2(deq_count_), b3(deq_editor_count_);
        deq_enabled_->setChecked(settings_.value("deq_enabled", false).toBool());
        deq_active_count_ = std::clamp(settings_.value("deq_band_count", 3).toInt(),1,10);
        deq_count_->setValue(deq_active_count_);
        deq_editor_count_->setValue(deq_active_count_);
        for (int i=0; i<10; ++i) {
            DeqBand d = defaultDeqBand(i, i < deq_active_count_ ? deq_active_count_ : 10);
            const QString k = "deq_b" + QString::number(i) + "_";
            // Older releases stored all 10 bands even when they were unused.
            // Treat legacy stored values as custom: never guess whether a
            // coincident 1 kHz band was intentional. The user can choose
            // Spread frequencies explicitly in the editor.
            deq_user_modified_[i] = settings_.contains(k + "user_modified")
                ? settings_.value(k + "user_modified").toBool()
                : settings_.contains(k + "freq");
            d.frequency = std::clamp(settings_.value(k+"freq",d.frequency).toInt(),20,20000);
            d.q = std::clamp(settings_.value(k+"q",100).toInt(),50,800);
            d.gain = std::clamp(settings_.value(k+"gain",0).toInt(),-1200,1200);
            d.threshold = std::clamp(settings_.value(k+"threshold",-240).toInt(),-800,0);
            d.attack = std::clamp(settings_.value(k+"attack",10).toInt(),1,100);
            d.release = std::clamp(settings_.value(k+"release",100).toInt(),10,500);
            d.type = std::clamp(settings_.value(k+"type",0).toInt(),0,2);
            deq_bands_[i] = d;
        }
        refreshDeqBandSelector();
        updateDeqOverview();
    }
    void refreshDeqBandSelector() {
        const int wanted = std::clamp(deq_band_select_->currentIndex(),0,deq_active_count_-1);
        const QSignalBlocker block(deq_band_select_);
        deq_band_select_->clear();
        for (int i=0; i<deq_active_count_; ++i)
            deq_band_select_->addItem(QString("Band %1").arg(i+1));
        deq_band_select_->setCurrentIndex(wanted);
        {
            // Rebuild the compact summary only when the band count or saved
            // settings change. Row selection and item text edits stay stable.
            const QSignalBlocker tableBlock(deq_table_);
            deq_table_->setRowCount(deq_active_count_);
            deq_table_->setFixedHeight(36 + deq_active_count_ * 25);
            for (int i = 0; i < deq_active_count_; ++i)
                updateDeqTableRow(i);
        }
        loadDeqEditorBand(wanted);
    }
    void updateDeqTableRow(int index) {
        if (!deq_table_ || index < 0 || index >= deq_active_count_) return;
        const QSignalBlocker guard(deq_table_);
        const DeqBand& b = deq_bands_[index];
        const QStringList values = {
            QString::number(index + 1), QString::number(b.frequency),
            QString::number(b.gain / 100.0, 'f', 2),
            QString::number(b.threshold / 10.0, 'f', 1)
        };
        for (int col = 0; col < 4; ++col) {
            auto* item = deq_table_->item(index, col);
            if (!item) {
                item = new QTableWidgetItem();
                deq_table_->setItem(index, col, item);
            }
            item->setText(values[col]);
            item->setTextAlignment(Qt::AlignCenter);
            if (col == 0) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setToolTip(col == 0 ? "Select this band to edit all parameters below" :
                "Double-click to edit this value; other band controls are below");
        }
    }
    void commitDeqTableItem(QTableWidgetItem* item) {
        if (!item || !deq_table_) return;
        const int index = item->row();
        const int col = item->column();
        if (index < 0 || index >= deq_active_count_ || col < 1 || col > 3) return;
        bool ok = false;
        const double value = item->text().trimmed().toDouble(&ok);
        if (ok && std::isfinite(value)) {
            DeqBand& band = deq_bands_[index];
            deq_user_modified_[index] = true;
            if (col == 1) band.frequency = std::clamp(qRound(value), 20, 20000);
            if (col == 2) band.gain = std::clamp(qRound(value * 100.0), -1200, 1200);
            if (col == 3) band.threshold = std::clamp(qRound(value * 10.0), -800, 0);
            // Keep the detailed editor, both previews and the live backend in
            // lockstep. QSignalBlocker avoids recursive itemChanged updates.
            updateDeqTableRow(index);
            if (index == deq_band_select_->currentIndex()) loadDeqEditorBand(index);
            updateDeqOverview();
            push();
        } else {
            // Reject invalid text without changing saved DSP values.
            updateDeqTableRow(index);
        }
    }
    void loadDeqEditorBand(int index) {
        if (index<0 || index>=deq_active_count_) return;
        const DeqBand& b = deq_bands_[index];
        const QSignalBlocker b1(deq_frequency_), b2(deq_q_), b3(deq_gain_),
            b4(deq_threshold_), b5(deq_attack_), b6(deq_release_), b7(deq_type_);
        deq_frequency_->setValue(b.frequency);
        deq_q_->setValue(b.q / 100.0);
        deq_gain_->setValue(b.gain / 100.0);
        deq_threshold_->setValue(b.threshold / 10.0);
        deq_attack_->setValue(b.attack);
        deq_release_->setValue(b.release);
        deq_type_->setCurrentIndex(b.type);
        if (deq_selected_label_)
            deq_selected_label_->setText(QString("Band %1 — %2 Hz").arg(index + 1).arg(b.frequency));
        if (deq_editor_graph_) deq_editor_graph_->setSelectedBand(index);
        if (deq_overview_) deq_overview_->setSelectedBand(index);
        if (deq_table_) {
            const QSignalBlocker guard(deq_table_);
            deq_table_->selectRow(index);
            deq_table_->scrollToItem(deq_table_->item(index, 0));
        }
    }
    void commitDeqEditorBand() {
        const int index = deq_band_select_->currentIndex();
        if (index<0 || index>=deq_active_count_) return;
        DeqBand& b = deq_bands_[index];
        deq_user_modified_[index] = true;
        b.frequency = deq_frequency_->value();
        b.q = qRound(deq_q_->value()*100.0);
        b.gain = qRound(deq_gain_->value()*100.0);
        b.threshold = qRound(deq_threshold_->value()*10.0);
        b.attack = deq_attack_->value();
        b.release = deq_release_->value();
        b.type = deq_type_->currentIndex();
        updateDeqTableRow(index);
        if (deq_selected_label_)
            deq_selected_label_->setText(QString("Band %1 — %2 Hz").arg(index + 1).arg(b.frequency));
        updateDeqOverview();
        push();
    }
    void updateDeqOverview() {
        if (deq_overview_)
            deq_overview_->setBands(deq_bands_, deq_active_count_, deq_enabled_->isChecked());
        if (deq_editor_graph_)
            deq_editor_graph_->setBands(deq_bands_, deq_active_count_, deq_enabled_->isChecked());
    }
    void resetDeq() {
        const QSignalBlocker a(deq_enabled_), b(deq_count_), c(deq_editor_count_);
        deq_enabled_->setChecked(false);
        deq_active_count_=3; deq_count_->setValue(3); deq_editor_count_->setValue(3);
        for (int i=0; i<10; ++i) {
            deq_user_modified_[i] = false;
            deq_bands_[i] = defaultDeqBand(i, i < 3 ? 3 : 10);
        }
        refreshDeqBandSelector(); updateDeqOverview();
    }
    void saveDeqPreset() {
        settings_.setValue("preset/deq_enabled",deq_enabled_->isChecked());
        settings_.setValue("preset/deq_band_count",deq_active_count_);
        for (int i=0;i<10;++i) {
            const QString k = "preset/deq_b"+QString::number(i)+"_";
            const auto& b=deq_bands_[i];
            settings_.setValue(k+"freq", b.frequency);
            settings_.setValue(k+"user_modified", deq_user_modified_[i]);
            settings_.setValue(k+"q",b.q);
            settings_.setValue(k+"gain",b.gain);
            settings_.setValue(k+"threshold",b.threshold);
            settings_.setValue(k+"attack",b.attack);
            settings_.setValue(k+"release",b.release);
            settings_.setValue(k+"type",b.type);
        }
    }
    void restoreDeqPreset() {
        const QSignalBlocker a(deq_enabled_), b(deq_count_), c(deq_editor_count_);
        deq_enabled_->setChecked(settings_.value("preset/deq_enabled",false).toBool());
        deq_active_count_=std::clamp(settings_.value("preset/deq_band_count",3).toInt(),1,10);
        deq_count_->setValue(deq_active_count_);
        deq_editor_count_->setValue(deq_active_count_);
        for (int i=0;i<10;++i) {
            const QString k = "preset/deq_b"+QString::number(i)+"_";
            DeqBand d=defaultDeqBand(i, i < deq_active_count_ ? deq_active_count_ : 10);
            deq_user_modified_[i] = settings_.contains(k + "user_modified")
                ? settings_.value(k + "user_modified").toBool()
                : settings_.contains(k + "freq");
            d.frequency=std::clamp(settings_.value(k+"freq",d.frequency).toInt(),20,20000);
            d.q=std::clamp(settings_.value(k+"q",100).toInt(),50,800);
            d.gain=std::clamp(settings_.value(k+"gain",0).toInt(),-1200,1200);
            d.threshold=std::clamp(settings_.value(k+"threshold",-240).toInt(),-800,0);
            d.attack=std::clamp(settings_.value(k+"attack",10).toInt(),1,100);
            d.release=std::clamp(settings_.value(k+"release",100).toInt(),10,500);
            d.type=std::clamp(settings_.value(k+"type",0).toInt(),0,2);
            deq_bands_[i]=d;
        }
        refreshDeqBandSelector(); updateDeqOverview();
    }
    void applyAnalogXSettings(bool enabled, int mode) {
        const QSignalBlocker a(analogx_enabled_), b(analogx_mode_);
        analogx_enabled_->setChecked(enabled);
        analogx_mode_->setCurrentIndex(std::clamp(mode, 0, 2));
    }
    void applyTubeSettings(bool enabled, int model, int drive, int mix) {
        const QSignalBlocker a(tube_enabled_), b(tube_model_), c(tube_drive_.slider), d(tube_mix_.slider);
        tube_enabled_->setChecked(enabled);
        tube_model_->setCurrentIndex(std::clamp(model, 0, 2));
        tube_drive_.slider->setValue(std::clamp(drive, 0, 100));
        tube_mix_.slider->setValue(std::clamp(mix, 0, 100));
        updateLabels();
    }
    void updateMultibandFrequencyLabels() {
        const int n = mb_band_count_->currentIndex() == 1 ? 5 : 3;
        // Labels identify the actual native DSP frequency regions, not EQ gains.
        static constexpr const char* kThreeNames[3] = {"Bass", "Mids", "Treble"};
        static constexpr const char* kFiveNames[5] = {"Sub", "Bass", "Mids", "Presence", "Treble"};
        for (int i = 0; i < n; ++i) {
            const int low = i == 0 ? 20 : mb_crossovers_[i-1].slider->value();
            const QString range = i == n-1
                ? QString("%1 Hz and above").arg(low)
                : QString("%1–%2 Hz").arg(low).arg(mb_crossovers_[i].slider->value());
            const QString name = QString::fromUtf8(n == 3 ? kThreeNames[i] : kFiveNames[i]);
            mb_band_tabs_->setTabText(i, name);
            mb_band_tabs_->setTabToolTip(i, range);
            mb_band_ranges_[i]->setText(name + " · " + range);
        }
    }
    void updateMultibandVisibility() {
        const int n = mb_band_count_->currentIndex() == 1 ? 5 : 3;
        for (int i = 0; i < 5; ++i) mb_band_tabs_->setTabVisible(i, i < n);
        if (mb_band_tabs_->currentIndex() >= n) mb_band_tabs_->setCurrentIndex(n - 1);
        for (int i = 0; i < 4; ++i) {
            // addSlider creates a QWidget for each QFormLayout row.
            QWidget* row = mb_crossovers_[i].slider->parentWidget();
            if (row) row->setVisible(i < n - 1);
        }
        updateMultibandFrequencyLabels();
    }
    void saveMultibandSettings(const QString& prefix) {
        settings_.setValue(prefix + "mb_enabled", mb_enabled_->isChecked());
        settings_.setValue(prefix + "mb_band_count", mb_band_count_->currentIndex() == 1 ? 5 : 3);
        for (int i=0; i<4; ++i)
            settings_.setValue(prefix + QString("mb_x%1_hz").arg(i), mb_crossovers_[i].slider->value());
        for (int i=0; i<5; ++i) {
            const auto key = prefix + QString("mb_b%1_").arg(i);
            const auto& b = mb_bands_[i];
            settings_.setValue(key + "enabled", b.enabled->isChecked());
            settings_.setValue(key + "threshold_db", b.threshold.slider->value());
            settings_.setValue(key + "ratio_tenths", b.ratio.slider->value());
            settings_.setValue(key + "knee_db", b.knee.slider->value());
            settings_.setValue(key + "makeup_db", b.gain.slider->value());
            settings_.setValue(key + "attack_ms", b.attack.slider->value());
            settings_.setValue(key + "release_ms", b.release.slider->value());
        }
    }
    void loadMultibandSettings(const QString& prefix) {
        const QSignalBlocker en(mb_enabled_), count(mb_band_count_);
        mb_enabled_->setChecked(settings_.value(prefix + "mb_enabled", false).toBool());
        const int bands = settings_.value(prefix + "mb_band_count", 3).toInt() == 5 ? 5 : 3;
        mb_band_count_->setCurrentIndex(bands == 5 ? 1 : 0);
        const std::array<int,4> defaults = bands == 5
            ? std::array<int,4>{120,500,4000,8000} : std::array<int,4>{200,4000,5000,8000};
        for (int i=0; i<4; ++i) {
            const QSignalBlocker block(mb_crossovers_[i].slider);
            mb_crossovers_[i].slider->setValue(std::clamp(settings_.value(prefix + QString("mb_x%1_hz").arg(i), defaults[i]).toInt(),80,15000));
        }
        for (int i=0; i<5; ++i) {
            const auto key = prefix + QString("mb_b%1_").arg(i);
            auto& b = mb_bands_[i];
            const QSignalBlocker block(b.enabled);
            b.enabled->setChecked(settings_.value(key + "enabled", true).toBool());
            for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
                {b.threshold, std::clamp(settings_.value(key + "threshold_db", -18).toInt(), -48, 0)},
                {b.ratio, std::clamp(settings_.value(key + "ratio_tenths", 20).toInt(),10,100)},
                {b.knee, std::clamp(settings_.value(key + "knee_db", 6).toInt(),0,12)},
                {b.gain, std::clamp(settings_.value(key + "makeup_db", 0).toInt(),0,24)},
                {b.attack, std::clamp(settings_.value(key + "attack_ms", 10).toInt(),1,100)},
                {b.release, std::clamp(settings_.value(key + "release_ms", 100).toInt(),5,500)}}) {
                const QSignalBlocker lock(item.first.slider);
                item.first.slider->setValue(item.second);
            }
        }
        updateMultibandVisibility(); updateLabels();
    }
    void resetMultibandSettings() {
        const QSignalBlocker en(mb_enabled_), count(mb_band_count_);
        mb_enabled_->setChecked(false);
        mb_band_count_->setCurrentIndex(0);
        const std::array<int,4> defaults{200,4000,5000,8000};
        for (int i=0; i<4; ++i) {
            const QSignalBlocker lock(mb_crossovers_[i].slider);
            mb_crossovers_[i].slider->setValue(defaults[i]);
        }
        for (auto& b : mb_bands_) {
            const QSignalBlocker enBand(b.enabled);
            b.enabled->setChecked(true);
            for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
                {b.threshold,-18}, {b.ratio,20}, {b.knee,6},
                {b.gain,0}, {b.attack,10}, {b.release,100}}) {
                const QSignalBlocker lock(item.first.slider);
                item.first.slider->setValue(item.second);
            }
        }
        updateMultibandVisibility(); updateLabels();
    }
    void restoreFetSettings(const QString& prefix) {
        const QSignalBlocker on(fet_enabled_);
        fet_enabled_->setChecked(settings_.value(prefix + "fet_enabled", false).toBool());
        for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
            {fet_threshold_, std::clamp(settings_.value(prefix + "fet_threshold_db", -18).toInt(), -48, 0)},
            {fet_ratio_, std::clamp(settings_.value(prefix + "fet_ratio_tenths", 20).toInt(), 10, 100)},
            {fet_knee_, std::clamp(settings_.value(prefix + "fet_knee_db", 6).toInt(), 0, 12)},
            {fet_gain_, std::clamp(settings_.value(prefix + "fet_makeup_db", 0).toInt(), 0, 24)},
            {fet_attack_, std::clamp(settings_.value(prefix + "fet_attack_ms", 10).toInt(), 1, 100)},
            {fet_release_, std::clamp(settings_.value(prefix + "fet_release_ms", 100).toInt(), 5, 500)}}) {
            const QSignalBlocker block(item.first.slider);
            item.first.slider->setValue(item.second);
        }
        updateLabels();
    }
    void resetFetSettings() {
        const QSignalBlocker on(fet_enabled_);
        fet_enabled_->setChecked(false);
        for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
            {fet_threshold_, -18}, {fet_ratio_, 20}, {fet_knee_, 6},
            {fet_gain_, 0}, {fet_attack_, 10}, {fet_release_, 100}}) {
            const QSignalBlocker block(item.first.slider);
            item.first.slider->setValue(item.second);
        }
        updateLabels();
    }
    void push() {
        persistCurrentSettings();
        publishToBackend();
    }
    void applySettings(bool master, bool bass, int bass_mode, int strength, int frequency,
                       bool clarity_on, int clarity_mode, int clarity_gain,
                       bool spectrum_on, int spectrum_frequency, int spectrum_exciter,
                       bool headphone_on, int headphone_quality,
                       bool diff_on, int diff_delay, bool diff_reverse, int diff_mix, int diff_lp,
                       bool field_on, int field_widen, int field_mid, int field_depth_step,
                       bool reverb_on, int reverb_room, int reverb_width, int reverb_damp, int reverb_wet, int reverb_dry) {
        // Loading a complete named preset or resetting to defaults explicitly
        // replaces the previous full-precision CLI values.
        settings_.remove("precision");
        replace_precision_on_push_ = true;
        // Suppress intermediate updates: don't send four DSP changes per reset.
        const QSignalBlocker block0(global_);
        const QSignalBlocker block1(enabled_);
        const QSignalBlocker blockBassMode(bass_mode_);
        const QSignalBlocker block2(strength_.slider);
        const QSignalBlocker block3(frequency_.slider);
        const QSignalBlocker block5(clarity_enabled_);
        const QSignalBlocker block6(clarity_mode_);
        const QSignalBlocker block7(clarity_gain_.slider);
        const QSignalBlocker block8(spectrum_enabled_);
        const QSignalBlocker block9(spectrum_frequency_.slider);
        const QSignalBlocker block10(spectrum_exciter_.slider);
        const QSignalBlocker block11(headphone_enabled_);
        const QSignalBlocker block12(headphone_quality_);
        const QSignalBlocker block13(diff_enabled_);
        const QSignalBlocker block14(diff_delay_.slider);
        const QSignalBlocker block15(diff_reverse_);
        const QSignalBlocker block16(diff_mix_.slider);
        const QSignalBlocker block17(diff_lowpass_.slider);
        const QSignalBlocker block18(field_enabled_);
        const QSignalBlocker block19(field_widen_.slider);
        const QSignalBlocker block20(field_mid_.slider);
        const QSignalBlocker block21(field_depth_.slider);
        const QSignalBlocker block22(reverb_enabled_);
        const QSignalBlocker block23(reverb_room_.slider);
        const QSignalBlocker block24(reverb_width_.slider);
        const QSignalBlocker block25(reverb_damp_.slider);
        const QSignalBlocker block26(reverb_wet_.slider);
        const QSignalBlocker block27(reverb_dry_.slider);
        global_->setChecked(master);
        enabled_->setChecked(bass);
        bass_mode_->setCurrentIndex(std::clamp(bass_mode, 0, 2));
        strength_.slider->setValue(std::clamp(strength, 5, 100));
        frequency_.slider->setValue(std::clamp(frequency, 15, 150));
        clarity_enabled_->setChecked(clarity_on);
        clarity_mode_->setCurrentIndex(std::clamp(clarity_mode, 0, 2));
        clarity_gain_.slider->setValue(std::clamp(clarity_gain, 0, 45));
        spectrum_enabled_->setChecked(spectrum_on);
        spectrum_frequency_.slider->setValue(std::clamp(spectrum_frequency, 22, 82));
        spectrum_exciter_.slider->setValue(std::clamp(spectrum_exciter, 0, 60));
        headphone_enabled_->setChecked(headphone_on);
        headphone_quality_->setCurrentIndex(std::clamp(headphone_quality, 0, 4));
        diff_enabled_->setChecked(diff_on);
        diff_delay_.slider->setValue(std::clamp(diff_delay, 1, 20));
        diff_reverse_->setChecked(diff_reverse);
        diff_mix_.slider->setValue(std::clamp(diff_mix, 0, 100));
        diff_lowpass_.slider->setValue(std::clamp(diff_lp, 0, 200));
        field_enabled_->setChecked(field_on);
        field_widen_.slider->setValue(std::clamp(field_widen, 0, 80));
        field_mid_.slider->setValue(std::clamp(field_mid, 100, 200));
        field_depth_.slider->setValue(std::clamp(field_depth_step, 0, 16));
        reverb_enabled_->setChecked(reverb_on);
        reverb_room_.slider->setValue(std::clamp(reverb_room, 0, 100));
        reverb_width_.slider->setValue(std::clamp(reverb_width, 0, 100));
        reverb_damp_.slider->setValue(std::clamp(reverb_damp, 0, 100));
        reverb_wet_.slider->setValue(std::clamp(reverb_wet, 0, 100));
        reverb_dry_.slider->setValue(std::clamp(reverb_dry, 0, 100));
        updateLabels();
        push();
    }
    void saveBassPreset() {
        for (const auto& item : std::initializer_list<std::pair<QString, int>>{
            {"bass_mono_enabled", mono_enabled_->isChecked()}, {"bass_mono_mode", mono_mode_->currentIndex()},
            {"bass_mono_strength_centi", mono_strength_.slider->value()*10},
            {"bass_mono_frequency_hz", mono_frequency_.slider->value()},
            {"psycho_enabled", psycho_enabled_->isChecked()}, {"psycho_cutoff_hz", psycho_cutoff_.slider->value()},
            {"psycho_intensity_percent", psycho_intensity_.slider->value()}, {"psycho_order", psycho_order_->currentIndex()+2},
            {"psycho_original_percent", psycho_original_.slider->value()},
            {"dynamic_enabled", dyn_enabled_->isChecked()}, {"dynamic_x_low", dyn_xlo_.slider->value()},
            {"dynamic_x_high", dyn_xhi_.slider->value()}, {"dynamic_y_low", dyn_ylo_.slider->value()},
            {"dynamic_y_high", dyn_yhi_.slider->value()},
            {"dynamic_side_low_percent", dyn_side_lo_.slider->value()},
            {"dynamic_side_high_percent", dyn_side_hi_.slider->value()},
            {"dynamic_strength_percent", dyn_strength_.slider->value()} }) {
            settings_.setValue("preset/" + item.first, item.second);
        }
    }
    void applyBassExtra(int mono, int monoMode, int monoStrength, int monoFreq,
                        int psycho, int cutoff, int intensity, int order, int original,
                        int dynamic, int xlo, int xhi, int ylo, int yhi, int sideLo, int sideHi, int strength) {
        const QSignalBlocker blocks[] = {
            QSignalBlocker(mono_enabled_), QSignalBlocker(mono_mode_),
            QSignalBlocker(mono_strength_.slider), QSignalBlocker(mono_frequency_.slider),
            QSignalBlocker(psycho_enabled_), QSignalBlocker(psycho_cutoff_.slider),
            QSignalBlocker(psycho_intensity_.slider), QSignalBlocker(psycho_order_),
            QSignalBlocker(psycho_original_.slider), QSignalBlocker(dyn_enabled_),
            QSignalBlocker(dyn_xlo_.slider), QSignalBlocker(dyn_xhi_.slider),
            QSignalBlocker(dyn_ylo_.slider), QSignalBlocker(dyn_yhi_.slider),
            QSignalBlocker(dyn_side_lo_.slider), QSignalBlocker(dyn_side_hi_.slider),
            QSignalBlocker(dyn_strength_.slider)
        };
        mono_enabled_->setChecked(mono != 0);
        mono_mode_->setCurrentIndex(std::clamp(monoMode, 0, 2));
        mono_strength_.slider->setValue(std::clamp(monoStrength, 50, 1000)/10);
        mono_frequency_.slider->setValue(std::clamp(monoFreq, 15, 150));
        psycho_enabled_->setChecked(psycho != 0);
        psycho_cutoff_.slider->setValue(std::clamp(cutoff, 60, 150));
        psycho_intensity_.slider->setValue(std::clamp(intensity, 0, 100));
        psycho_order_->setCurrentIndex(std::clamp(order, 2, 5)-2);
        psycho_original_.slider->setValue(std::clamp(original, 0, 100));
        dyn_enabled_->setChecked(dynamic != 0);
        dyn_xlo_.slider->setValue(std::clamp(xlo, 0, 2400));
        dyn_xhi_.slider->setValue(std::clamp(xhi, 0, 12000));
        dyn_ylo_.slider->setValue(std::clamp(ylo, 0, 200));
        dyn_yhi_.slider->setValue(std::clamp(yhi, 0, 300));
        dyn_side_lo_.slider->setValue(std::clamp(sideLo, 0, 100));
        dyn_side_hi_.slider->setValue(std::clamp(sideHi, 0, 100));
        dyn_strength_.slider->setValue(std::clamp(strength, 0, 100));
        updateLabels();
        push();
    }
    void resetNewBassControls() {
        applyBassExtra(0,0,200,65,0,80,30,3,100,0,100,6000,100,150,50,50,0);
    }
    void restoreBassPreset() {
        auto get = [this](const char* key,int defaultValue){return settings_.value(QString("preset/")+key,defaultValue).toInt();};
        applyBassExtra(get("bass_mono_enabled",0),get("bass_mono_mode",0),get("bass_mono_strength_centi",200),
                       get("bass_mono_frequency_hz",65),get("psycho_enabled",0),get("psycho_cutoff_hz",80),
                       get("psycho_intensity_percent",30),get("psycho_order",3),get("psycho_original_percent",100),
                       get("dynamic_enabled",0),get("dynamic_x_low",100),get("dynamic_x_high",6000),
                       get("dynamic_y_low",100),get("dynamic_y_high",150),get("dynamic_side_low_percent",50),
                       get("dynamic_side_high_percent",50),get("dynamic_strength_percent",0));
    }
    // Output fields are separate from the older seven-effect preset schema;
    // restore them atomically from a logical UI operation, not one slider at a time.
    void applyOutputSettings(int threshold, int gain, int pan, bool agc, int agcStrength,
                             int agcMaxGain, int agcThreshold, bool lufs,
                             int lufsTarget, int lufsMaxGain, int speed) {
        const QSignalBlocker b0(limiter_threshold_.slider), b1(limiter_gain_.slider), b2(limiter_pan_.slider);
        const QSignalBlocker b3(agc_enabled_), b4(agc_strength_.slider), b5(agc_max_gain_.slider);
        const QSignalBlocker b6(agc_threshold_.slider), b7(lufs_enabled_), b8(lufs_target_.slider);
        const QSignalBlocker b9(lufs_gain_.slider), b10(lufs_speed_);
        limiter_threshold_.slider->setValue(std::clamp(threshold, 300, 1000));
        limiter_gain_.slider->setValue(std::clamp(gain, 1, 200));
        limiter_pan_.slider->setValue(std::clamp(pan, -100, 100));
        agc_enabled_->setChecked(agc);
        agc_strength_.slider->setValue(std::clamp(agcStrength, 50, 300));
        agc_max_gain_.slider->setValue(std::clamp(agcMaxGain, 100, 1000));
        agc_threshold_.slider->setValue(std::clamp(agcThreshold, 300, 1000));
        lufs_enabled_->setChecked(lufs);
        lufs_target_.slider->setValue(std::clamp(lufsTarget, -240, -80));
        lufs_gain_.slider->setValue(std::clamp(lufsMaxGain, 0, 120));
        lufs_speed_->setCurrentIndex(std::clamp(speed, 0, 2));
        updateLabels();
        push();
    }
    void saveOutputPreset() {
        settings_.setValue("preset/limiter_threshold_permille", limiter_threshold_.slider->value());
        settings_.setValue("preset/limiter_output_percent", limiter_gain_.slider->value());
        settings_.setValue("preset/limiter_pan_percent", limiter_pan_.slider->value());
        settings_.setValue("preset/agc_enabled", agc_enabled_->isChecked());
        settings_.setValue("preset/agc_strength_centi", agc_strength_.slider->value());
        settings_.setValue("preset/agc_max_gain_centi", agc_max_gain_.slider->value());
        settings_.setValue("preset/agc_threshold_permille", agc_threshold_.slider->value());
        settings_.setValue("preset/lufs_enabled", lufs_enabled_->isChecked());
        settings_.setValue("preset/lufs_target_tenths", lufs_target_.slider->value());
        settings_.setValue("preset/lufs_max_gain_tenths", lufs_gain_.slider->value());
        settings_.setValue("preset/lufs_speed", lufs_speed_->currentIndex());
    }
    void restoreOutputPreset() {
        applyOutputSettings(settings_.value("preset/limiter_threshold_permille", 500).toInt(),
            settings_.value("preset/limiter_output_percent", 100).toInt(),
            settings_.value("preset/limiter_pan_percent", 0).toInt(),
            settings_.value("preset/agc_enabled", false).toBool(),
            settings_.value("preset/agc_strength_centi", 100).toInt(),
            settings_.value("preset/agc_max_gain_centi", 300).toInt(),
            settings_.value("preset/agc_threshold_permille", 900).toInt(),
            settings_.value("preset/lufs_enabled", false).toBool(),
            settings_.value("preset/lufs_target_tenths", -140).toInt(),
            settings_.value("preset/lufs_max_gain_tenths", 60).toInt(),
            settings_.value("preset/lufs_speed", 1).toInt());
    }
    void resetOutputControls() {
        applyOutputSettings(500, 100, 0, false, 100, 300, 900, false, -140, 60, 1);
    }
    // Compare the widgets before/after a backend read. Signal blockers are
    // intentional: reflecting CLI changes must not republish a stale GUI
    // snapshot over them. We then persist the resulting live settings without
    // sending a second change to the DSP. This is GUI-thread-only code.
    std::vector<int> liveUiSnapshot() const {
        std::vector<int> values;
        for (const auto* widget : findChildren<QCheckBox*>())
            values.push_back(widget->isChecked() ? 1 : 0);
        for (const auto* widget : findChildren<QComboBox*>())
            values.push_back(widget->currentIndex());
        for (const auto* widget : findChildren<QSlider*>())
            values.push_back(widget->value());
        values.push_back(deq_active_count_);
        for (const auto& b : deq_bands_) {
            values.insert(values.end(), {b.frequency, b.q, b.gain, b.threshold,
                                         b.attack, b.release, b.type});
        }
        return values;
    }
    void readBackend() {
        if (!viperbass::running(mapping_.get())) return;
        auto* c = mapping_.get();
        const auto before = liveUiSnapshot();
        // Reflect changes made from the CLI; avoid tug-of-war while dragging sliders.
        const QSignalBlocker block0(global_);
        const QSignalBlocker block1(enabled_);
        const QSignalBlocker blockBassMode(bass_mode_);
        const QSignalBlocker block2(clarity_enabled_);
        const QSignalBlocker block3(clarity_mode_);
        const QSignalBlocker block4(spectrum_enabled_);
        const QSignalBlocker blockTube(tube_enabled_), blockTubeModel(tube_model_);
        const QSignalBlocker blockAnalogX(analogx_enabled_), blockAnalogMode(analogx_mode_);
        const QSignalBlocker blockCrossfeed(crossfeed_enabled_), blockCrossfeedMode(crossfeed_mode_);
        const QSignalBlocker blockImager(imager_enabled_);
        const QSignalBlocker blockFet(fet_enabled_);
        const QSignalBlocker blockMb(mb_enabled_), blockMbCount(mb_band_count_);
        const QSignalBlocker block5(headphone_enabled_);
        const QSignalBlocker block6(headphone_quality_);
        const QSignalBlocker block7(diff_enabled_);
        const QSignalBlocker block8(diff_reverse_);
        const QSignalBlocker block9(field_enabled_);
        const QSignalBlocker blockReverb(reverb_enabled_);
        const QSignalBlocker blockMono(mono_enabled_), blockMonoMode(mono_mode_);
        const QSignalBlocker blockPsycho(psycho_enabled_), blockPsychoOrder(psycho_order_);
        const QSignalBlocker blockDynamic(dyn_enabled_);
        const QSignalBlocker blockAgc(agc_enabled_), blockLufs(lufs_enabled_);
        const QSignalBlocker blockSpeed(lufs_speed_);
        const QSignalBlocker blockConv(convolver_enabled_);
        const QSignalBlocker blockDdc(ddc_enabled_), blockSpeaker(speaker_enabled_);
        const QSignalBlocker blockDeq(deq_enabled_);
        const QSignalBlocker blockEq(eq_enabled_);
        settings_.sync();
        const QString remoteEqPreset = settings_.value("eq/selected_preset", "custom").toString();
        eq_enabled_->setChecked(c->eq_enabled.load() != 0);
        const int remoteEqBands = validEqBands(c->eq_band_count.load());
        if (remoteEqBands != eq_active_bands_) {
            saveEqLayout();
            eq_active_bands_ = remoteEqBands;
            const QSignalBlocker blockMode(eq_mode_);
            eq_mode_->setCurrentIndex(eq_mode_->findData(remoteEqBands));
            loadEqLayout();
            // Preserve a preset chosen by the CLI; ordinary EQ-band edits
            // already save "custom" to the same settings key.
            selectEqPreset(remoteEqPreset);
        }
        bool eqRemoteEdit = false;
        for (int i = 0; i < eq_active_bands_; ++i) {
            if (eq_faders_[i]->isSliderDown()) continue;
            const int level = std::clamp(c->eq_levels_hundredths[i].load(), -1200, 1200);
            if (eq_faders_[i]->value() == level) continue;
            const QSignalBlocker blocker(eq_faders_[i]);
            eq_faders_[i]->setValue(level);
            eq_values_[i]->setText(QString::number(level / 100.0, 'f', 2));
            eqRemoteEdit = true;
        }
        if (eqRemoteEdit) { saveEqLayout(); selectEqPreset(remoteEqPreset); updateEqOverview(); }
        if (remoteEqPreset != eq_selected_preset_) selectEqPreset(remoteEqPreset);
        tube_enabled_->setChecked(c->tube_enabled.load() != 0);
        {
            const QSignalBlocker a(crossfeed_enabled_), b(crossfeed_mode_);
            crossfeed_enabled_->setChecked(c->crossfeed_enabled.load() != 0);
            crossfeed_mode_->setCurrentIndex(std::clamp(c->crossfeed_preset.load(), 0, 2));
        }
        mb_enabled_->setChecked(c->mb_enabled.load() != 0);
        const int newCount = c->mb_band_count.load() == 5 ? 1 : 0;
        if (mb_band_count_->currentIndex() != newCount) {
            mb_band_count_->setCurrentIndex(newCount);
            updateMultibandVisibility();
        }
        for (int i=0; i<4; ++i) {
            auto* slider = mb_crossovers_[i].slider;
            if (!slider->isSliderDown()) {
                const QSignalBlocker lock(slider);
                slider->setValue(std::clamp(c->mb_crossovers_hz[i].load(),80,15000));
            }
        }
        for (int i=0; i<5; ++i) {
            auto& b = mb_bands_[i];
            const auto& src = c->mb_bands[i];
            {
                const QSignalBlocker lock(b.enabled);
                b.enabled->setChecked(src.enabled.load() != 0);
            }
            for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
                {b.threshold, std::clamp(src.threshold_db.load(),-48,0)},
                {b.ratio, std::clamp(src.ratio_tenths.load(),10,100)},
                {b.knee, std::clamp(src.knee_db.load(),0,12)},
                {b.gain, std::clamp(src.makeup_db.load(),0,24)},
                {b.attack, std::clamp(src.attack_ms.load(),1,100)},
                {b.release, std::clamp(src.release_ms.load(),5,500)}}) {
                if (!item.first.slider->isSliderDown()) {
                    const QSignalBlocker lock(item.first.slider);
                    item.first.slider->setValue(item.second);
                }
            }
        }
        fet_enabled_->setChecked(c->fet_enabled.load() != 0);
        deq_enabled_->setChecked(c->deq_enabled.load() != 0);
        updateDeqOverview();
        for (const auto& item : std::initializer_list<std::pair<SliderRow,int>>{
            {fet_threshold_, std::clamp(c->fet_threshold_db.load(), -48, 0)},
            {fet_ratio_, std::clamp(c->fet_ratio_tenths.load(), 10, 100)},
            {fet_knee_, std::clamp(c->fet_knee_db.load(), 0, 12)},
            {fet_gain_, std::clamp(c->fet_makeup_db.load(), 0, 24)},
            {fet_attack_, std::clamp(c->fet_attack_ms.load(), 1, 100)},
            {fet_release_, std::clamp(c->fet_release_ms.load(), 5, 500)}}) {
            if (!item.first.slider->isSliderDown()) {
                const QSignalBlocker blocker(item.first.slider);
                item.first.slider->setValue(item.second);
            }
        }
        imager_enabled_->setChecked(c->imager_enabled.load() != 0);
        for (const auto& item : std::initializer_list<std::pair<SliderRow, int>>{
            {imager_low_, std::clamp(c->imager_low_width_percent.load(), 0, 200)},
            {imager_mid_, std::clamp(c->imager_mid_width_percent.load(), 0, 200)},
            {imager_high_, std::clamp(c->imager_high_width_percent.load(), 0, 200)},
            {imager_low_xover_, std::clamp(c->imager_low_crossover_hz.load(), 80, 400)},
            {imager_high_xover_, std::clamp(c->imager_high_crossover_hz.load()/100, 20, 80)}}) {
            if (!item.first.slider->isSliderDown()) {
                const QSignalBlocker block(item.first.slider);
                item.first.slider->setValue(item.second);
            }
        }
        tube_model_->setCurrentIndex(std::clamp(c->tube_model.load(), 0, 2));
        analogx_enabled_->setChecked(c->analogx_enabled.load() != 0);
        analogx_mode_->setCurrentIndex(std::clamp(c->analogx_mode.load(), 0, 2));
        if (!tube_drive_.slider->isSliderDown()) {
            const QSignalBlocker blocker(tube_drive_.slider);
            tube_drive_.slider->setValue(std::clamp(c->tube_drive_percent.load(), 0, 100));
        }
        if (!tube_mix_.slider->isSliderDown()) {
            const QSignalBlocker blocker(tube_mix_.slider);
            tube_mix_.slider->setValue(std::clamp(c->tube_mix_percent.load(), 0, 100));
        }
        spectrum_enabled_->setChecked(c->spectrum_enabled.load() != 0);
        headphone_enabled_->setChecked(c->headphone_enabled.load() != 0);
        headphone_quality_->setCurrentIndex(std::clamp(c->headphone_quality.load(), 0, 4));
        diff_enabled_->setChecked(c->diff_enabled.load() != 0);
        diff_reverse_->setChecked(c->diff_reverse.load() != 0);
        field_enabled_->setChecked(c->field_enabled.load() != 0);
        reverb_enabled_->setChecked(c->reverb_enabled.load() != 0);
        mono_enabled_->setChecked(c->bass_mono_enabled.load() != 0);
        mono_mode_->setCurrentIndex(std::clamp(c->bass_mono_mode.load(), 0, 2));
        psycho_enabled_->setChecked(c->psycho_enabled.load() != 0);
        psycho_order_->setCurrentIndex(std::clamp(c->psycho_order.load(), 2, 5)-2);
        dyn_enabled_->setChecked(c->dynamic_enabled.load() != 0);
        agc_enabled_->setChecked(c->agc_enabled.load() != 0);
        lufs_enabled_->setChecked(c->lufs_enabled.load() != 0);
        ddc_enabled_->setChecked(c->ddc_enabled.load() != 0);
        speaker_enabled_->setChecked(c->speaker_enabled.load() != 0);
        convolver_enabled_->setChecked(c->convolver_enabled.load() != 0);
        const int ddcStatus = c->ddc_status.load();
        const bool pendingDdc = c->ddc_request_serial.load() != c->ddc_applied_serial.load();
        ddc_status_->setText(pendingDdc || ddcStatus == 2 ? "Loading DDC profile…" :
            ddcStatus == 1 ? (ddc_enabled_->isChecked() ? "Native DDC correction active" : "DDC profile loaded (disabled)") :
            ddcStatus == -2 ? "DDC unavailable at this audio rate (requires 44.1 or 48 kHz)" :
            ddcStatus == -1 ? "Invalid/unsupported DDC profile; correction bypassed" :
            "No DDC profile loaded; correction bypassed");
        const int convstatus = c->convolver_status.load();
        const bool pendingIr = c->convolver_request_serial.load() != c->convolver_applied_serial.load();
        convolver_status_->setText(pendingIr || convstatus == 2 ? "Loading IRS in the running backend…" :
            convstatus == 1 ? "Native IR kernel loaded and ready" :
            convstatus == -2 ? "IR resampling failed; previous kernel may remain active" :
            convstatus == -1 ? "Invalid/unsupported IR; previous kernel may remain active" :
            "No impulse response loaded.");
        if (!convolver_cross_.slider->isSliderDown()) {
            const QSignalBlocker b(convolver_cross_.slider);
            convolver_cross_.slider->setValue(std::clamp(c->convolver_cross_percent.load(), 0, 100));
        }
        lufs_speed_->setCurrentIndex(std::clamp(c->lufs_speed.load(), 0, 2));
        const struct { SliderRow* row; std::int32_t value; int lo; int hi; } bassRows[] = {
            {&mono_strength_, c->bass_mono_strength_centi.load()/10, 5, 100},
            {&mono_frequency_, c->bass_mono_frequency_hz.load(), 15, 150},
            {&psycho_cutoff_, c->psycho_cutoff_hz.load(), 60, 150},
            {&psycho_intensity_, c->psycho_intensity_percent.load(), 0, 100},
            {&psycho_original_, c->psycho_original_percent.load(), 0, 100},
            {&dyn_xlo_, c->dynamic_x_low.load(), 0, 2400},
            {&dyn_xhi_, c->dynamic_x_high.load(), 0, 12000},
            {&dyn_ylo_, c->dynamic_y_low.load(), 0, 200},
            {&dyn_yhi_, c->dynamic_y_high.load(), 0, 300},
            {&dyn_side_lo_, c->dynamic_side_low_percent.load(), 0, 100},
            {&dyn_side_hi_, c->dynamic_side_high_percent.load(), 0, 100},
            {&dyn_strength_, c->dynamic_strength_percent.load(), 0, 100}
        };
        for (const auto& item : bassRows) {
            if (item.row->slider->isSliderDown()) continue;
            const QSignalBlocker blocker(item.row->slider);
            item.row->slider->setValue(std::clamp(item.value,item.lo,item.hi));
        }
        const struct { SliderRow* row; std::int32_t value; int lo; int hi; } outputRows[] = {
            {&limiter_threshold_, c->limiter_threshold_permille.load(), 300, 1000},
            {&limiter_gain_, c->limiter_output_percent.load(), 1, 200},
            {&limiter_pan_, c->limiter_pan_percent.load(), -100, 100},
            {&agc_strength_, c->agc_strength_centi.load(), 50, 300},
            {&agc_max_gain_, c->agc_max_gain_centi.load(), 100, 1000},
            {&agc_threshold_, c->agc_threshold_permille.load(), 300, 1000},
            {&lufs_target_, c->lufs_target_tenths.load(), -240, -80},
            {&lufs_gain_, c->lufs_max_gain_tenths.load(), 0, 120}
        };
        for (const auto& item : outputRows) {
            if (item.row->slider->isSliderDown()) continue;
            const QSignalBlocker blocker(item.row->slider);
            item.row->slider->setValue(std::clamp(item.value, item.lo, item.hi));
        }
        const struct { SliderRow* row; std::int32_t value; } reverberation[] = {
            {&reverb_room_, c->reverb_room_percent.load()},
            {&reverb_width_, c->reverb_width_percent.load()},
            {&reverb_damp_, c->reverb_damp_percent.load()},
            {&reverb_wet_, c->reverb_wet_percent.load()},
            {&reverb_dry_, c->reverb_dry_percent.load()}
        };
        for (const auto& item : reverberation) {
            if (item.row->slider->isSliderDown()) continue;
            const QSignalBlocker blocker(item.row->slider);
            item.row->slider->setValue(std::clamp(item.value, 0, 100));
        }
        if (!field_widen_.slider->isSliderDown()) {
            const QSignalBlocker block(field_widen_.slider);
            field_widen_.slider->setValue(std::clamp(c->field_widen_tenths.load(), 0, 80));
        }
        if (!field_mid_.slider->isSliderDown()) {
            const QSignalBlocker block(field_mid_.slider);
            field_mid_.slider->setValue(std::clamp(c->field_mid_hundredths.load(), 100, 200));
        }
        if (!field_depth_.slider->isSliderDown()) {
            const QSignalBlocker block(field_depth_.slider);
            const int depth = c->field_depth.load();
            field_depth_.slider->setValue(depth == 0 ? 0 :
                std::clamp(static_cast<int>(std::lround((depth - 150) / 50.0)), 1, 16));
        }
        if (!diff_delay_.slider->isSliderDown()) {
            const QSignalBlocker block(diff_delay_.slider);
            diff_delay_.slider->setValue(std::clamp(c->diff_delay_ms.load(), 1, 20));
        }
        if (!diff_mix_.slider->isSliderDown()) {
            const QSignalBlocker block(diff_mix_.slider);
            diff_mix_.slider->setValue(std::clamp(c->diff_mix_percent.load(), 0, 100));
        }
        if (!diff_lowpass_.slider->isSliderDown()) {
            const QSignalBlocker block(diff_lowpass_.slider);
            diff_lowpass_.slider->setValue(std::clamp(c->diff_lp_hz.load() / 100, 0, 200));
        }
        const int rate = c->sample_rate_hz.load();
        if (rate != 0 && rate != 44100 && rate != 48000) {
            const QString warning = QString(
                "Current processing rate %1 Hz is unsupported by upstream VHE; "
                "Headphone Surround+ is inactive. Supported rates: 44.1 and 48 kHz.").arg(rate);
            headphone_quality_->setToolTip(warning);
            headphone_enabled_->setToolTip(warning);
        } else {
            const QString details =
                "Five distinct upstream VHE surround profiles, not a quality ranking. "
                "Supported at 44.1 and 48 kHz; switching levels may rebuild filters.";
            headphone_quality_->setToolTip(details);
            headphone_enabled_->setToolTip(details);
        }
        global_->setChecked(c->master_enabled.load() != 0);
        enabled_->setChecked(c->enabled.load() != 0);
        bass_mode_->setCurrentIndex(std::clamp(c->bass_mode.load(), 0, 2));
        clarity_enabled_->setChecked(c->clarity_enabled.load() != 0);
        clarity_mode_->setCurrentIndex(std::clamp(c->clarity_mode.load(), 0, 2));
        if (!clarity_gain_.slider->isSliderDown()) {
            const QSignalBlocker block(clarity_gain_.slider);
            clarity_gain_.slider->setValue(std::clamp(c->clarity_gain_milli.load() / 100, 0, 45));
        }
        if (!spectrum_frequency_.slider->isSliderDown()) {
            const QSignalBlocker block(spectrum_frequency_.slider);
            spectrum_frequency_.slider->setValue(std::clamp(c->spectrum_frequency_hz.load() / 100, 22, 82));
        }
        if (!spectrum_exciter_.slider->isSliderDown()) {
            const QSignalBlocker block(spectrum_exciter_.slider);
            spectrum_exciter_.slider->setValue(std::clamp(c->spectrum_exciter_milli.load() / 100, 0, 60));
        }
        if (!strength_.slider->isSliderDown()) {
            const QSignalBlocker block(strength_.slider);
            strength_.slider->setValue(std::clamp(c->strength_centi.load() / 10, 5, 100));
        }
        if (!frequency_.slider->isSliderDown()) {
            const QSignalBlocker block(frequency_.slider);
            frequency_.slider->setValue(std::clamp(c->frequency_hz.load(), 15, 150));
        }
        updateLabels();
        // CLI edits become visible within the polling interval and are also
        // retained in QSettings for the next backend startup. Persist only on
        // actual UI changes: otherwise the 500ms poll would keep resetting the
        // batched disk-save timer and prevent it from ever firing.
        if (before != liveUiSnapshot()) persistCurrentSettings();
    }
    void pollBackend() {
        if (!ensureBackend()) {
            showOffline();
            return;
        }
        // If the user changed a control during disconnection, send that change
        // instead of overwriting it with an old backend snapshot.
        if (pending_edit_) {
            if (!publishToBackend()) return;
        } else {
            // No pending GUI edit: the backend is authoritative, including
            // modifications made with the command-line control utility.
            readBackend();
        }
        // Merely having shared memory does not mean the DSP is receiving audio.
        // Compare two consecutive 500ms polls without touching the audio thread.
        const auto* control = mapping_.get();
        const auto state = audio_activity_.sample(
            static_cast<pid_t>(control->owner_pid.load(std::memory_order_relaxed)),
            control->audio_callbacks.load(std::memory_order_relaxed));
        switch (state) {
        case viperbass::BackendAudioActivity::Active:
            status_->setText("● Processing audio — DSP callbacks active");
            status_->setStyleSheet("color: #209454;");
            break;
        case viperbass::BackendAudioActivity::Idle:
            status_->setText("○ Connected — idle (effects processor ready)");
            status_->setStyleSheet(QString("color:%1;").arg(theme_muted_color_));
            break;
        case viperbass::BackendAudioActivity::Checking:
            status_->setText("○ Connected — checking audio activity…");
            status_->setStyleSheet(QString("color:%1;").arg(theme_muted_color_));
            break;
        }
    }
    QSettings settings_;
    viperbass::AudioActivityTracker audio_activity_;
    QSystemTrayIcon* tray_icon_{};
    QAction* tray_processing_action_{};
    bool force_taskbar_minimize_ = false;
    bool tray_restore_maximized_ = false;
    QString theme_muted_color_ = "#536a74";
    QProcess* cinnamon_theme_query_{};
    bool cinnamon_theme_known_ = false;
    bool cinnamon_theme_dark_ = false;
    QLabel* header_icon_{};
    QLabel* audio_output_label_{};
    QString last_output_memory_revision_;
    QComboBox* theme_selector_{};
    QCheckBox* eq_enabled_{};
    QComboBox* eq_mode_{};
    QComboBox* eq_preset_{};
    QPushButton* eq_delete_button_{};
    EqOverview* eq_overview_{};
    QDialog* eq_editor_{};
    QString eq_selected_preset_ = "custom";
    int eq_active_bands_ = 10;
    QWidget* eq_fader_container_{};
    std::array<QWidget*, 31> eq_columns_{};
    std::array<QSlider*, 31> eq_faders_{};
    std::array<QLabel*, 31> eq_values_{};
    std::array<QLabel*, 31> eq_frequencies_{};
    QCheckBox* crossfeed_enabled_{};
    QComboBox* crossfeed_mode_{};
    QCheckBox* mb_enabled_{};
    QComboBox* mb_band_count_{};
    std::array<SliderRow,4> mb_crossovers_{};
    QTabWidget* mb_band_tabs_{};
    QWidget* mb_crossover_panel_{};
    std::array<QLabel*,5> mb_band_ranges_{};
    std::array<MbBandRows,5> mb_bands_{};
    QCheckBox* fet_enabled_{};
    SliderRow fet_threshold_{}, fet_ratio_{}, fet_knee_{}, fet_gain_{}, fet_attack_{}, fet_release_{};
    QCheckBox* imager_enabled_{};
    SliderRow imager_low_{}, imager_mid_{}, imager_high_{};
    SliderRow imager_low_xover_{}, imager_high_xover_{};
    QCheckBox* deq_enabled_{};
    DynamicEqOverview* deq_overview_{};
    DynamicEqOverview* deq_editor_graph_{};
    QTableWidget* deq_table_{};
    QLabel* deq_selected_label_{};
    QDialog* deq_editor_{};
    QSpinBox* deq_count_{};
    QSpinBox* deq_editor_count_{};
    QComboBox* deq_band_select_{};
    QSpinBox* deq_frequency_{};
    QComboBox* deq_type_{};
    QDoubleSpinBox* deq_q_{};
    QDoubleSpinBox* deq_gain_{};
    QDoubleSpinBox* deq_threshold_{};
    QSpinBox* deq_attack_{};
    QSpinBox* deq_release_{};
    std::array<DeqBand,10> deq_bands_{};
    std::array<bool,10> deq_user_modified_{};
    int deq_active_count_=3;
    QCheckBox* ddc_enabled_{};
    IrsComboBox* ddc_library_{};
    QFileSystemWatcher* ddc_watcher_{};
    bool ddc_library_dirty_ = false;
    QLabel* ddc_file_name_{};
    QLabel* ddc_status_{};
    QCheckBox* speaker_enabled_{};
    QCheckBox* convolver_enabled_{};
    IrsComboBox* convolver_library_{};
    QFileSystemWatcher* irs_watcher_{};
    bool irs_library_dirty_ = false;
    QPlainTextEdit* convolver_full_name_ = nullptr;
    QLabel* convolver_status_{};
    SliderRow convolver_cross_{};
    QTabWidget* tabs_{};
    QTimer* save_timer_{};
    viperbass::Mapping mapping_;
    bool pending_edit_ = false;
    bool replace_precision_on_push_ = false;
    QCheckBox* limiter_indicator_{};
    SliderRow limiter_threshold_{};
    SliderRow limiter_gain_{};
    SliderRow limiter_pan_{};
    QCheckBox* agc_enabled_{};
    SliderRow agc_strength_{};
    SliderRow agc_max_gain_{};
    SliderRow agc_threshold_{};
    QCheckBox* lufs_enabled_{};
    SliderRow lufs_target_{};
    SliderRow lufs_gain_{};
    QComboBox* lufs_speed_{};
    QLabel* output_warning_{};
    QCheckBox* global_{};
    QCheckBox* enabled_{};
    QComboBox* bass_mode_{};
    QCheckBox* mono_enabled_{};
    QComboBox* mono_mode_{};
    SliderRow mono_strength_{};
    SliderRow mono_frequency_{};
    QCheckBox* psycho_enabled_{};
    SliderRow psycho_cutoff_{};
    SliderRow psycho_intensity_{};
    QComboBox* psycho_order_{};
    SliderRow psycho_original_{};
    QCheckBox* dyn_enabled_{};
    SliderRow dyn_xlo_{};
    SliderRow dyn_xhi_{};
    SliderRow dyn_ylo_{};
    SliderRow dyn_yhi_{};
    SliderRow dyn_side_lo_{};
    SliderRow dyn_side_hi_{};
    SliderRow dyn_strength_{};
    QCheckBox* clarity_enabled_{};
    QComboBox* clarity_mode_{};
    SliderRow clarity_gain_{};
    QCheckBox* headphone_enabled_{};
    QComboBox* headphone_quality_{};

    QCheckBox* field_enabled_{};
    SliderRow field_widen_{};
    SliderRow field_mid_{};
    SliderRow field_depth_{};
    QCheckBox* reverb_enabled_{};
    SliderRow reverb_room_{};
    SliderRow reverb_width_{};
    SliderRow reverb_damp_{};
    SliderRow reverb_wet_{};
    SliderRow reverb_dry_{};
    QCheckBox* diff_enabled_{};
    SliderRow diff_delay_{};
    QCheckBox* diff_reverse_{};
    SliderRow diff_mix_{};
    SliderRow diff_lowpass_{};
    QCheckBox* tube_enabled_{};
    QComboBox* tube_model_{};
    SliderRow tube_drive_{};
    SliderRow tube_mix_{};
    QCheckBox* analogx_enabled_{};
    QComboBox* analogx_mode_{};
    QCheckBox* spectrum_enabled_{};
    SliderRow spectrum_frequency_{};
    SliderRow spectrum_exciter_{};
    SliderRow strength_{};
    SliderRow frequency_{};
    QLabel* status_{};
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Viper4Linux");
    app.setWindowIcon(QIcon(":/viper4linux/blue.png"));
    // A tray-only launch has no visible top-level window. Explicit Quit GUI
    // and closeEvent remain the only reasons for the GUI to terminate.
    app.setQuitOnLastWindowClosed(false);
    ViperWindow window;
    window.showAtStartup();
    return app.exec();
}
