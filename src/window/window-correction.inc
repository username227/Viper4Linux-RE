// Private ViperWindow member definitions. Included once inside the class.
// Checkpoint 7 mechanical extraction; no additional Qt or DSP state owner.
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
    // Keep filesystem notifications, popup stability and import callbacks
    // with the correction-library operations, not the GUI constructor.
    void setupDdcLibrary(QPushButton* importDdc, QPushButton* refreshDdc) {
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
            const QString destination = viper_library_import::copyOrSelect(selected, ddcDirectory());
            if (destination.isEmpty()) {
                QMessageBox::warning(this, "VDC import", "Could not copy the selected profile.");
                return;
            }
            selectDdcProfile(destination);
        });
    }

    void setupIrsLibrary(QPushButton* browse, QPushButton* refresh) {
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
                // Browsing a library file is a selection, not an import.
                const QString destination = viper_library_import::copyOrSelect(selectedFile, irsDirectory());
                if (destination.isEmpty()) {
                    failures << file.fileName() + ": could not copy";
                    continue;
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
    }
