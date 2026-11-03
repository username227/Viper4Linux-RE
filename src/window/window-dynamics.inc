// Private ViperWindow member definitions. Included once inside the class.
// Checkpoint 7 mechanical extraction; no additional Qt or DSP state owner.
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
