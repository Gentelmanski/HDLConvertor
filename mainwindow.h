#pragma once

#include "converter.h"
#include "parser.h"

#include <QComboBox>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>

class MainWindow : public QMainWindow
{

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void checkSyntax();
    void convertCode();
    void swapLanguages();
    void clearAll();
    void loadExample();

private:
    HDL currentSourceLanguage() const;
    HDL currentTargetLanguage() const;

    void setupUi();
    void addDiagnostic(const Diagnostic& d);
    void showDiagnostics(const QVector<Diagnostic>& diagnostics);
    void setLanguageCombo(QComboBox* combo, HDL language);

    QComboBox* sourceCombo = nullptr;
    QComboBox* targetCombo = nullptr;

    QPlainTextEdit* inputEdit = nullptr;
    QPlainTextEdit* outputEdit = nullptr;
    QPlainTextEdit* diagnosticsEdit = nullptr;

    QLabel* statusLabel = nullptr;

    QPushButton* checkButton = nullptr;
    QPushButton* convertButton = nullptr;
    QPushButton* swapButton = nullptr;
    QPushButton* clearButton = nullptr;
    QPushButton* exampleButton = nullptr;
};
