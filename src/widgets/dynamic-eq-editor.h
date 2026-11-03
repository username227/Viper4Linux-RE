// Dynamic EQ detailed editor: layout and widgets only.
// State transitions and native DSP values remain in the main window.
#pragma once
#include "widgets/eq-overviews.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace viper_eq_ui {
struct DynamicEqEditorWidgets {
    QDialog* dialog{};
    QSpinBox* bandCount{};
    DynamicEqOverview* graph{};
    QTableWidget* table{};
    QLabel* selectedLabel{};
    QComboBox* bandSelector{};
    QSpinBox* frequency{};
    QComboBox* filterType{};
    QDoubleSpinBox* q{};
    QDoubleSpinBox* gain{};
    QDoubleSpinBox* threshold{};
    QSpinBox* attack{};
    QSpinBox* release{};
    QPushButton* spread{};
};
inline DynamicEqEditorWidgets createDynamicEqEditor(QWidget* parent) {
    DynamicEqEditorWidgets result;
        result.dialog = new QDialog(parent);
        result.dialog->setObjectName("viperDynamicEqEditor");
        result.dialog->setWindowTitle("Viper4Linux — Dynamic EQ Editor");
        result.dialog->resize(670, 735);
        result.dialog->setMinimumSize(425, 440);
        auto* deq_editor_layout = new QVBoxLayout(result.dialog);
        auto* deq_scroll = new QScrollArea(result.dialog);
        deq_scroll->setObjectName("viperDynamicEqEditorScroll");
        deq_scroll->setWidgetResizable(true);
        deq_scroll->setFrameShape(QFrame::NoFrame);
        deq_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* deq_content = new QWidget(deq_scroll);
        auto* deq_content_layout = new QVBoxLayout(deq_content);
        deq_content_layout->setSpacing(9);
        auto* deq_explain = new QLabel(
            "Each band\'s Maximum gain is its individual setting; the curve estimates the combined "
            "response of all active filters. Frequency, Q (bandwidth), filter shape (especially shelves) "
            "and other bands can change the curve even when a band\'s gain stays the same. "
            "This is an approximate preview, not live metering: real processing also depends on "
            "threshold, attack, release and signal level.", deq_content);
        deq_explain->setWordWrap(true);
        deq_content_layout->addWidget(deq_explain);
        auto* deq_top = new QHBoxLayout();
        deq_top->addWidget(new QLabel("Active bands", deq_content));
        result.bandCount = new QSpinBox(deq_content);
        result.bandCount->setObjectName("viperDynamicEqEditorBandCount");
        result.bandCount->setRange(1, 10);
        result.bandCount->setFixedWidth(77);
        deq_top->addWidget(result.bandCount);
        result.spread = new QPushButton("Spread frequencies", deq_content);
        result.spread->setObjectName("viperDynamicEqSpreadFrequencies");
        result.spread->setToolTip("Distribute active band frequencies over bass to treble. Preserves gain, Q, threshold, attack, release and filter type.");
        deq_top->addWidget(result.spread);
        deq_top->addStretch();
        deq_content_layout->addLayout(deq_top);
        result.graph = new DynamicEqOverview(deq_content);
        result.graph->setObjectName("viperDynamicEqEditorGraph");
        result.graph->enableBandSelection();
        result.graph->setMinimumHeight(125);
        deq_content_layout->addWidget(result.graph);
        result.table = new QTableWidget(0, 4, deq_content);
        result.table->setObjectName("viperDynamicEqBandTable");
        result.table->setHorizontalHeaderLabels({"Band", "Frequency (Hz)", "Gain (dB)", "Threshold (dB)"});
        result.table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        result.table->verticalHeader()->hide();
        result.table->verticalHeader()->setDefaultSectionSize(25);
        result.table->setSelectionBehavior(QAbstractItemView::SelectRows);
        result.table->setSelectionMode(QAbstractItemView::SingleSelection);
        result.table->setEditTriggers(QAbstractItemView::DoubleClicked |
                                    QAbstractItemView::EditKeyPressed);
        result.table->setAlternatingRowColors(true);
        result.table->setWordWrap(false);
        // All active band summaries can fit without a second inner scrollbar.
        // The dialog's outer QScrollArea handles short screens instead.
        result.table->setMinimumHeight(115);
        result.table->setFixedHeight(115);
        result.table->setToolTip("Double-click frequency, gain, or threshold to edit; select a row to adjust its other parameters below.");
        deq_content_layout->addWidget(result.table);
        result.selectedLabel = new QLabel("Selected band", deq_content);
        QFont deq_label_font = result.selectedLabel->font();
        deq_label_font.setBold(true);
        result.selectedLabel->setFont(deq_label_font);
        deq_content_layout->addWidget(result.selectedLabel);
        auto* deq_form_editor = new QFormLayout();
        deq_form_editor->setRowWrapPolicy(QFormLayout::WrapLongRows);
        result.bandSelector = new QComboBox(deq_content);
        result.bandSelector->setObjectName("viperDynamicEqBandSelector");
        deq_form_editor->addRow("Selected band", result.bandSelector);
        result.frequency = new QSpinBox(deq_content);
        result.frequency->setRange(20,20000);
        result.frequency->setSuffix(" Hz");
        deq_form_editor->addRow("Center / shelf frequency", result.frequency);
        result.filterType = new QComboBox(deq_content);
        result.filterType->addItems({"Peak", "Low shelf", "High shelf"});
        deq_form_editor->addRow("Filter shape", result.filterType);
        result.q = new QDoubleSpinBox(deq_content);
        result.q->setRange(0.5,8.0); result.q->setDecimals(2); result.q->setSingleStep(0.1);
        deq_form_editor->addRow("Q (bandwidth)", result.q);
        result.gain = new QDoubleSpinBox(deq_content);
        result.gain->setRange(-12,12); result.gain->setDecimals(2);
        result.gain->setSingleStep(0.5); result.gain->setSuffix(" dB");
        deq_form_editor->addRow("Maximum gain", result.gain);
        result.threshold = new QDoubleSpinBox(deq_content);
        result.threshold->setRange(-80,0); result.threshold->setDecimals(1);
        result.threshold->setSuffix(" dB");
        deq_form_editor->addRow("Threshold", result.threshold);
        result.attack = new QSpinBox(deq_content);
        result.attack->setRange(1,100); result.attack->setSuffix(" ms");
        deq_form_editor->addRow("Attack", result.attack);
        result.release = new QSpinBox(deq_content);
        result.release->setRange(10,500); result.release->setSuffix(" ms");
        deq_form_editor->addRow("Release", result.release);
        deq_content_layout->addLayout(deq_form_editor);
        deq_content_layout->addStretch();
        deq_scroll->setWidget(deq_content);
        deq_editor_layout->addWidget(deq_scroll, 1);
        auto* deq_actions = new QHBoxLayout();
        deq_actions->addStretch();
        auto* deq_close = new QPushButton("Close", result.dialog);
        deq_actions->addWidget(deq_close);
        deq_editor_layout->addLayout(deq_actions);
        QObject::connect(deq_close, &QPushButton::clicked, result.dialog, &QDialog::hide);
    return result;
}
} // namespace viper_eq_ui
