#pragma once
// One GUI per user session, whether started by viper4linux or viper-gui.
// The GUI does not start, stop, or supervise the independent DSP backend.
#include <QDebug>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QObject>
#include <QStandardPaths>
#include <QThread>
#include <functional>
#include <utility>

class ViperGuiInstance final {
public:
    enum class StartResult { Primary, Forwarded, Error };

    ViperGuiInstance()
        : runtime_dir_(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)),
          socket_path_(QDir(runtime_dir_).filePath("viper4linux-re-gui.sock")),
          lock_(QDir(runtime_dir_).filePath("viper4linux-re-gui.lock")) {
        // A GUI may stay open for days. Only a dead process makes its lock stale.
        lock_.setStaleLockTime(0);
    }

    StartResult claim() {
        if (runtime_dir_.isEmpty() || !QDir(runtime_dir_).exists()) {
            qCritical().noquote() << "viper-gui: user runtime directory is unavailable";
            return StartResult::Error; // Fail closed rather than start two GUI writers.
        }
        // Startup lock is acquired before constructing ViperWindow/QSettings.
        // A second launch can connect even if the first is still constructing its UI.
        for (int attempt = 0; attempt < 25; ++attempt) {
            if (lock_.tryLock(0)) {
                // We are the sole lock owner: safe to remove a socket left by a crash.
                QLocalServer::removeServer(socket_path_);
                server_.setSocketOptions(QLocalServer::UserAccessOption);
                if (!server_.listen(socket_path_)) {
                    qCritical().noquote() << "viper-gui: activation socket:"
                                          << server_.errorString();
                    return StartResult::Error;
                }
                return StartResult::Primary;
            }
            QLocalSocket client;
            client.connectToServer(socket_path_);
            if (client.waitForConnected(100)) {
                // Connecting is the activation request; no settings are transferred.
                // The primary's newConnection handler restores its own window.
                client.disconnectFromServer();
                return StartResult::Forwarded;
            }
            QThread::msleep(60); // Lock may be held before the socket is listening.
        }
        qCritical().noquote() << "viper-gui: another GUI is starting or unresponsive;"
                                 " refusing to open a duplicate";
        return StartResult::Error;
    }

    void onActivation(QObject* receiver, std::function<void()> showWindow) {
        QObject::connect(&server_, &QLocalServer::newConnection, receiver,
            [this, showWindow = std::move(showWindow)] {
                while (server_.hasPendingConnections()) {
                    QLocalSocket* client = server_.nextPendingConnection();
                    if (!client) continue;
                    client->disconnectFromServer();
                    client->deleteLater();
                    showWindow();
                }
            });
    }

private:
    QString runtime_dir_;
    QString socket_path_;
    QLockFile lock_;
    QLocalServer server_;
};
