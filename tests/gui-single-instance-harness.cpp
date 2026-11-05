// Headless, real Qt local-socket and process-lock integration harness.
// Compiled by check-single-gui.py; no PipeWire backend or ViperWindow.
#include "gui-single-instance.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QTimer>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 20;
    const QString markers = QString::fromLocal8Bit(argv[1]);
    ViperGuiInstance instance;
    const auto result = instance.claim();
    if (result == ViperGuiInstance::StartResult::Forwarded) return 0;
    if (result != ViperGuiInstance::StartResult::Primary) return 21;

    instance.onActivation(&app, [markers](const QByteArray& token) {
        QFile activation(QDir(markers).filePath("activated"));
        if (activation.open(QIODevice::WriteOnly | QIODevice::Append)) {
            activation.write("restored\n");
            if (!token.isEmpty()) {
                activation.write("token=");
                activation.write(token);
                activation.write("\n");
            }
        }
    });
    QFile ready(QDir(markers).filePath("ready"));
    if (!ready.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 22;
    ready.write("primary\n");
    ready.close();
    QTimer::singleShot(10000, &app, &QCoreApplication::quit);
    return app.exec();
}
