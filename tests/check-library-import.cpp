#include "window/library-import.h"
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cassert>

static void writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write(data) == data.size());
}

static QByteArray readFile(const QString& path) {
    QFile file(path);
    assert(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

int main() {
    QTemporaryDir root;
    assert(root.isValid());
    const QDir dirs(root.path());
    assert(dirs.mkpath("incoming"));
    assert(dirs.mkpath("other"));
    assert(dirs.mkpath("VDC"));
    assert(dirs.mkpath("IRS"));

    const QString vdc = dirs.filePath("VDC");
    const QString irs = dirs.filePath("IRS");
    const QString input = dirs.filePath("incoming/Headphones.VDC");
    writeFile(input, "original");

    const QString first = viper_library_import::copyOrSelect(input, vdc);
    assert(first == dirs.filePath("VDC/Headphones.VDC"));
    assert(readFile(first) == "original");

    // Reimporting an external file never overwrites the original.
    const QString second = viper_library_import::copyOrSelect(input, vdc);
    assert(second == dirs.filePath("VDC/Headphones (2).VDC"));
    assert(readFile(second) == "original");

    // Selecting a file already in the library does not produce a third copy.
    assert(viper_library_import::copyOrSelect(first, vdc) == first);
    assert(!QFile::exists(dirs.filePath("VDC/Headphones (3).VDC")));

    const QString different = dirs.filePath("other/Headphones.VDC");
    writeFile(different, "replacement");
    const QString third = viper_library_import::copyOrSelect(different, vdc);
    assert(third == dirs.filePath("VDC/Headphones (3).VDC"));
    assert(readFile(first) == "original");
    assert(readFile(third) == "replacement");

    const QString irInput = dirs.filePath("incoming/Hall.irs");
    const QString wavInput = dirs.filePath("incoming/Hall.wav");
    writeFile(irInput, "impulse");
    writeFile(wavInput, "wave");
    assert(viper_library_import::copyOrSelect(irInput, irs) ==
           dirs.filePath("IRS/Hall.irs"));
    assert(viper_library_import::copyOrSelect(wavInput, irs) ==
           dirs.filePath("IRS/Hall.wav"));
    assert(readFile(dirs.filePath("IRS/Hall.irs")) == "impulse");
    assert(readFile(dirs.filePath("IRS/Hall.wav")) == "wave");

    // The GUI validates readability; the helper still fails closed if absent.
    assert(viper_library_import::copyOrSelect(
        dirs.filePath("incoming/missing.vdc"), vdc).isEmpty());
    assert(viper_library_import::copyOrSelect(irInput,
        dirs.filePath("missing-library")).isEmpty());
}
