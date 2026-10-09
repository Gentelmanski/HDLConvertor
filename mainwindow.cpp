#include "mainwindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFontDatabase>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    loadExample();
}

void MainWindow::setupUi()
{
    setWindowTitle("HDL Converter — VHDL / Verilog / SystemVerilog");
    resize(1200, 760);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* controls = new QHBoxLayout();

    controls->addWidget(new QLabel("Исходный язык:"));
    sourceCombo = new QComboBox();
    sourceCombo->addItem("VHDL");
    sourceCombo->addItem("Verilog");
    sourceCombo->addItem("SystemVerilog");
    controls->addWidget(sourceCombo);

    swapButton = new QPushButton("⇄ Поменять");
    controls->addWidget(swapButton);

    controls->addWidget(new QLabel("Целевой язык:"));
    targetCombo = new QComboBox();
    targetCombo->addItem("VHDL");
    targetCombo->addItem("Verilog");
    targetCombo->addItem("SystemVerilog");
    targetCombo->setCurrentIndex(1);
    controls->addWidget(targetCombo);

    controls->addStretch();

    checkButton = new QPushButton("Проверить синтаксис");
    convertButton = new QPushButton("Конвертировать");
    exampleButton = new QPushButton("Пример");
    clearButton = new QPushButton("Очистить");

    controls->addWidget(checkButton);
    controls->addWidget(convertButton);
    controls->addWidget(exampleButton);
    controls->addWidget(clearButton);

    root->addLayout(controls);

    auto* editors = new QGridLayout();

    auto* inputGroup = new QGroupBox("Исходный код");
    auto* inputLayout = new QVBoxLayout(inputGroup);
    inputEdit = new QPlainTextEdit();
    inputEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    inputEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    inputLayout->addWidget(inputEdit);

    auto* outputGroup = new QGroupBox("Результат");
    auto* outputLayout = new QVBoxLayout(outputGroup);
    outputEdit = new QPlainTextEdit();
    outputEdit->setReadOnly(false);
    outputEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    outputEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    outputLayout->addWidget(outputEdit);

    editors->addWidget(inputGroup, 0, 0);
    editors->addWidget(outputGroup, 0, 1);
    editors->setColumnStretch(0, 1);
    editors->setColumnStretch(1, 1);
    editors->setRowStretch(0, 1);

    root->addLayout(editors, 1);

    auto* diagnosticsGroup = new QGroupBox("Диагностика");
    auto* diagnosticsLayout = new QVBoxLayout(diagnosticsGroup);

    diagnosticsEdit = new QPlainTextEdit();
    diagnosticsEdit->setReadOnly(true);
    diagnosticsEdit->setMaximumHeight(150);
    diagnosticsEdit->setFont(
        QFontDatabase::systemFont(QFontDatabase::FixedFont));
    diagnosticsLayout->addWidget(diagnosticsEdit);

    root->addWidget(diagnosticsGroup);

    statusLabel = new QLabel("Готово.");
    root->addWidget(statusLabel);

    setCentralWidget(central);

    connect(checkButton, &QPushButton::clicked,
            this, &MainWindow::checkSyntax);
    connect(convertButton, &QPushButton::clicked,
            this, &MainWindow::convertCode);
    connect(swapButton, &QPushButton::clicked,
            this, &MainWindow::swapLanguages);
    connect(clearButton, &QPushButton::clicked,
            this, &MainWindow::clearAll);
    connect(exampleButton, &QPushButton::clicked,
            this, &MainWindow::loadExample);
}

HDL MainWindow::currentSourceLanguage() const
{
    switch (sourceCombo->currentIndex()) {
    case 0: return HDL::VHDL;
    case 1: return HDL::Verilog;
    default: return HDL::SystemVerilog;
    }
}

HDL MainWindow::currentTargetLanguage() const
{
    switch (targetCombo->currentIndex()) {
    case 0: return HDL::VHDL;
    case 1: return HDL::Verilog;
    default: return HDL::SystemVerilog;
    }
}

void MainWindow::addDiagnostic(const Diagnostic& d)
{
    QString location;
    if (d.line > 0)
        location = QString(" [%1:%2]").arg(d.line).arg(d.column);

    diagnosticsEdit->appendPlainText(
        QString("%1%2: %3")
            .arg(severityToString(d.severity))
            .arg(location)
            .arg(d.message));
}

void MainWindow::showDiagnostics(const QVector<Diagnostic>& diagnostics)
{
    diagnosticsEdit->clear();

    if (diagnostics.isEmpty()) {
        diagnosticsEdit->appendPlainText("Ошибок и предупреждений не обнаружено.");
        statusLabel->setText("Проверка завершена успешно.");
        return;
    }

    int errors = 0;
    int warnings = 0;

    for (const auto& d : diagnostics) {
        addDiagnostic(d);
        if (d.severity == Severity::Error)
            ++errors;
        else if (d.severity == Severity::Warning)
            ++warnings;
    }

    statusLabel->setText(
        QString("Диагностика: ошибок %1, предупреждений %2.")
            .arg(errors).arg(warnings));
}

void MainWindow::checkSyntax()
{
    IRModule module;
    QVector<Diagnostic> diagnostics;

    Parser parser;
    const bool ok = parser.parse(
        inputEdit->toPlainText(),
        currentSourceLanguage(),
        module,
        diagnostics);

    showDiagnostics(diagnostics);

    if (ok && diagnostics.isEmpty())
        statusLabel->setText("Синтаксическая структура распознана.");
}

void MainWindow::convertCode()
{
    IRModule module;
    QVector<Diagnostic> diagnostics;

    Parser parser;
    const bool parsed = parser.parse(
        inputEdit->toPlainText(),
        currentSourceLanguage(),
        module,
        diagnostics);

    bool hasErrors = false;
    for (const auto& d : diagnostics)
        if (d.severity == Severity::Error)
            hasErrors = true;

    if (!parsed || hasErrors) {
        showDiagnostics(diagnostics);
        outputEdit->clear();
        statusLabel->setText("Конвертация отменена: найдены синтаксические ошибки.");
        return;
    }

    Converter converter;
    QString result = converter.convert(
        module,
        currentSourceLanguage(),
        currentTargetLanguage(),
        diagnostics);

    outputEdit->setPlainText(result);
    showDiagnostics(diagnostics);
    statusLabel->setText("Конвертация завершена с учётом ограничений переносимости.");
}

void MainWindow::swapLanguages()
{
    const int source = sourceCombo->currentIndex();
    const int target = targetCombo->currentIndex();

    sourceCombo->setCurrentIndex(target);
    targetCombo->setCurrentIndex(source);

    const QString input = inputEdit->toPlainText();
    const QString output = outputEdit->toPlainText();

    inputEdit->setPlainText(output);
    outputEdit->setPlainText(input);
}

void MainWindow::clearAll()
{
    inputEdit->clear();
    outputEdit->clear();
    diagnosticsEdit->clear();
    statusLabel->setText("Поля очищены.");
}

void MainWindow::loadExample()
{
    sourceCombo->setCurrentIndex(0);
    targetCombo->setCurrentIndex(1);

    inputEdit->setPlainText(
        "library ieee;\n"
        "use ieee.std_logic_1164.all;\n\n"
        "entity and_gate is\n"
        "    port (\n"
        "        a : in std_logic;\n"
        "        b : in std_logic;\n"
        "        y : out std_logic\n"
        "    );\n"
        "end entity and_gate;\n\n"
        "architecture rtl of and_gate is\n"
        "begin\n"
        "    y <= a and b;\n"
        "end architecture rtl;\n");

    outputEdit->clear();
    diagnosticsEdit->clear();
    statusLabel->setText("Загружен пример VHDL.");
}
