// Exercise the shared DDC/IRS file-list reconciliation without starting ViPER.
#include "widgets/irs-combo-box.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QPersistentModelIndex>
#include <QTemporaryDir>
#include <cassert>

static void createFile(const QString& path) {
    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write("test", 4) == 4);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    assert(temporary.isValid());
    const QDir root(temporary.path());
    assert(root.mkpath("VDC") && root.mkpath("IRS") && root.mkpath("external"));
    const QString vdc = root.filePath("VDC");
    const QString irs = root.filePath("IRS");
    const QString vdcA = root.filePath("VDC/A.VDC");
    const QString vdcB = root.filePath("VDC/B.vdc");
    createFile(vdcA);
    createFile(vdcB);
    createFile(root.filePath("VDC/not-a-profile.txt"));

    IrsComboBox ddc(nullptr);
    QFileSystemWatcher ddcWatcher;
    int notifications = 0;
    QObject::connect(&ddc, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [&notifications](int) { ++notifications; });
    const QStringList vdcFilters = {"*.vdc", "*.VDC"};
    ddc.refreshFiles(vdc, vdcFilters, vdcA, &ddcWatcher);
    assert(ddcWatcher.directories().contains(vdc));
    assert(ddc.count() == 3); // None, A, B; ignore .txt
    assert(ddc.itemText(0) == "None");
    assert(ddc.currentData().toString() == vdcA);
    assert(notifications == 0);
    for (int i = 0; i < ddc.count(); ++i)
        assert(ddc.itemData(i, Qt::ToolTipRole).toString() == ddc.itemText(i));

    // An unchanged library must keep the item model stable and emit no
    // selection signals, including when settings now select another file.
    const QPersistentModelIndex originalItem(ddc.model()->index(1, 0));
    ddc.refreshFiles(vdc, vdcFilters, vdcB, &ddcWatcher);
    assert(originalItem.isValid());
    assert(ddc.currentData().toString() == vdcB);
    assert(notifications == 0);

    // An externally referenced profile remains selectable, even if missing.
    const QString external = root.filePath("external/Missing.vdc");
    ddc.refreshFiles(vdc, vdcFilters, external, &ddcWatcher);
    assert(ddc.count() == 4);
    assert(ddc.itemText(3) == "External: Missing.vdc");
    assert(ddc.currentData().toString() == external);
    assert(notifications == 0);

    // Reconcile additions and removals without replaying the selection signal.
    createFile(root.filePath("VDC/C.vdc"));
    ddc.refreshFiles(vdc, vdcFilters, QString(), &ddcWatcher);
    assert(ddc.count() == 4);
    assert(ddc.currentIndex() == 0);
    assert(notifications == 0);
    assert(QFile::remove(vdcA));
    ddc.refreshFiles(vdc, vdcFilters, vdcB, &ddcWatcher);
    assert(ddc.count() == 3);
    assert(ddc.currentData().toString() == vdcB);

    IrsComboBox convolver(nullptr);
    QFileSystemWatcher irsWatcher;
    const QString wav = root.filePath("IRS/Room.wav");
    const QString ir = root.filePath("IRS/Plate.IRS");
    createFile(wav);
    createFile(ir);
    createFile(root.filePath("IRS/ignore.vdc"));
    convolver.refreshFiles(irs, {"*.irs", "*.IRS", "*.wav", "*.WAV"}, wav, &irsWatcher);
    assert(irsWatcher.directories().contains(irs));
    assert(convolver.count() == 3);
    assert(convolver.currentData().toString() == wav);
    assert(convolver.itemText(0) == "None");
}
