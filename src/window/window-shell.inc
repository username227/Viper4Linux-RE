// Private ViperWindow member definitions. Included once inside the class.
// Checkpoint 7 mechanical extraction; no additional Qt or DSP state owner.
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
    // Called only on actual GUI exit, never on hide/minimize.
    bool prepareBackendForGuiExit() {
        const pid_t backendPid = viperbass::running_backend_pid();
        if (backendPid <= 0) return true; // No backend to keep or stop.

        QString decision = settings_.value("ui/backendOnExit", "ask").toString();
        bool remember = false;
        QWidget* promptParent = isVisible() ? static_cast<QWidget*>(this) : nullptr;
        if (decision != "keep" && decision != "stop") {
            QMessageBox prompt(QMessageBox::Question, "Quit Viper4Linux",
                "The ViPER audio backend is still running. "
                "Should it continue processing after the GUI closes?",
                QMessageBox::NoButton, promptParent);
            prompt.setWindowIcon(QIcon(":/viper4linux/blue.png"));
            auto* keepButton = prompt.addButton("Keep backend running", QMessageBox::AcceptRole);
            auto* stopButton = prompt.addButton("Stop backend", QMessageBox::DestructiveRole);
            auto* cancelButton = prompt.addButton(QMessageBox::Cancel);
            auto* rememberChoice = new QCheckBox("Remember this choice", &prompt);
            prompt.setCheckBox(rememberChoice);
            prompt.setDefaultButton(keepButton);
            prompt.setEscapeButton(cancelButton);
            prompt.exec();
            if (prompt.clickedButton() == keepButton) decision = "keep";
            else if (prompt.clickedButton() == stopButton) decision = "stop";
            else return false;
            remember = rememberChoice->isChecked();
        }
        if (decision == "stop") {
            std::string error;
            const auto result = viper_gui_exit::stopMarkedBackend(backendPid, error);
            if (result != viper_gui_exit::StopResult::Signalled &&
                result != viper_gui_exit::StopResult::AlreadyStopped) {
                QMessageBox::warning(promptParent, "Could not stop ViPER backend",
                    QString::fromStdString(error) +
                    "\nThe GUI will stay open. The system PipeWire service was not touched.");
                return false;
            }
        }
        // Do not remember Cancel, and never change the preference when a stop fails.
        if (remember) {
            settings_.setValue("ui/backendOnExit", decision);
            settings_.sync();
        }
        return true;
    }
    void quitGui() {
        if (!prepareBackendForGuiExit()) return;
        saveUiGeometry();
        qApp->quit(); // Only an explicit Stop choice signals the marked backend.
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
        auto* closeQuit = new QRadioButton("Quit GUI (use backend exit preference)", closeBox);
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

        auto* exitBox = new QGroupBox("When quitting the GUI (menu, tray or close X)", &dialog);
        auto* exitLayout = new QVBoxLayout(exitBox);
        auto* exitGroup = new QButtonGroup(&dialog);
        auto* exitAsk = new QRadioButton("Ask every time", exitBox);
        auto* exitKeep = new QRadioButton("Keep audio backend running", exitBox);
        auto* exitStop = new QRadioButton("Stop audio backend", exitBox);
        exitGroup->addButton(exitAsk, 0);
        exitGroup->addButton(exitKeep, 1);
        exitGroup->addButton(exitStop, 2);
        for (auto* item : {exitAsk, exitKeep, exitStop}) exitLayout->addWidget(item);
        const QString backendExit = settings_.value("ui/backendOnExit", "ask").toString();
        if (backendExit == "keep") exitKeep->setChecked(true);
        else if (backendExit == "stop") exitStop->setChecked(true);
        else exitAsk->setChecked(true);
        body->addWidget(exitBox);

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
        settings_.setValue("ui/backendOnExit",
            exitGroup->checkedId() == 2 ? "stop" : exitGroup->checkedId() == 1 ? "keep" : "ask");
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
