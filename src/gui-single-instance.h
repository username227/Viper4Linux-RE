#pragma once
// One GUI per user session, whether started by viper4linux or viper-gui.
// Only an explicit, user-approved GUI exit may stop the independent DSP backend.
#include <QDebug>
#include <QByteArray>
#include <QSharedPointer>
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

    StartResult claim(const QByteArray& startupToken = qgetenv("XDG_ACTIVATION_TOKEN")) {
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
                // Send the compositor-issued Wayland activation token to the
                // existing GUI. A user-local socket never sends settings/DSP data.
                QByteArray token = startupToken;
                if (token.size() > 4096 || token.contains('\n') || token.contains('\r') ||
                    token.contains('\0'))
                    token.clear();
                const QByteArray payload = "V4L-ACTIVATE/1 " + token.toBase64() + "\n";
                client.write(payload);
                client.flush();
                client.waitForBytesWritten(300);
                client.disconnectFromServer();
                return StartResult::Forwarded;
            }
            QThread::msleep(60); // Lock may be held before the socket is listening.
        }
        qCritical().noquote() << "viper-gui: another GUI is starting or unresponsive;"
                                 " refusing to open a duplicate";
        return StartResult::Error;
    }

    // Compatibility overload: headless ownership tests and callers without a token.
    void onActivation(QObject* receiver, std::function<void()> showWindow) {
        onActivation(receiver,
            [callback = std::move(showWindow)](const QByteArray&) { callback(); });
    }

    void onActivation(QObject* receiver,
                      std::function<void(const QByteArray&)> showWindow) {
        QObject::connect(&server_, &QLocalServer::newConnection, receiver,
            [this, receiver, showWindow = std::move(showWindow)] {
                while (server_.hasPendingConnections()) {
                    QLocalSocket* client = server_.nextPendingConnection();
                    if (!client) continue;
                    auto buffer = QSharedPointer<QByteArray>::create();
                    auto delivered = QSharedPointer<bool>::create(false);
                    auto finish = [client, buffer, delivered, showWindow] {
                        if (*delivered) return;
                        buffer->append(client->readAll());
                        if (!buffer->contains('\n') &&
                            client->state() != QLocalSocket::UnconnectedState &&
                            buffer->size() <= 8192)
                            return;
                        *delivered = true;
                        QByteArray token;
                        if (buffer->startsWith("V4L-ACTIVATE/1 ") &&
                            buffer->size() <= 8192) {
                            const int newline = buffer->indexOf('\n');
                            if (newline > 15) {
                                const QByteArray encoded = buffer->mid(15, newline - 15);
                                token = QByteArray::fromBase64(encoded);
                                if (token.size() > 4096 || token.contains('\0'))
                                    token.clear();
                            }
                        }
                        client->disconnectFromServer();
                        client->deleteLater();
                        showWindow(token);
                    };
                    QObject::connect(client, &QLocalSocket::readyRead, receiver, finish);
                    QObject::connect(client, &QLocalSocket::disconnected, receiver, finish);
                    // Handle a peer which closed before we accepted the socket.
                    if (client->bytesAvailable() || client->state() == QLocalSocket::UnconnectedState)
                        finish();
                }
            });
    }

private:
    QString runtime_dir_;
    QString socket_path_;
    QLockFile lock_;
    QLocalServer server_;
};
