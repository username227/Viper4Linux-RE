// Private ViperWindow member definitions. Included once inside the class.
// Checkpoint 7 mechanical extraction; no additional Qt or DSP state owner.
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
        syncGraphicEqEditorColumns();
        updateEqOverview();
    }
    // A hidden nonmodal QDialog contains a QScrollArea that can reveal its
    // children on first show. Inactive faders are retained for mode switching
    // but must never be accessible or mistaken for the active EQ curve.
    void syncGraphicEqEditorColumns() {
        const int n = validEqBands(eq_active_bands_);
        for (int i = 0; i < 31; ++i)
            eq_columns_[i]->setHidden(i >= n);
        eq_fader_container_->setFixedWidth(n * 39 + 6);
        eq_fader_container_->updateGeometry();
    }
