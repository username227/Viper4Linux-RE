// Private ViperWindow member definitions. Included once inside the class.
// Checkpoint 7 mechanical extraction; no additional Qt or DSP state owner.
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
        theme_muted_color_ = viper_theme::apply(
            this, classic, systemPrefersDark(), deq_editor_, eq_editor_,
            tabs_, header_icon_, status_);
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
