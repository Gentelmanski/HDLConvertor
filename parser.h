#pragma once
#include "diagnostic.h"
#include "expr.h"
#include "ir.h"
#include "lexer.h"

class Parser {
public:
    bool parse(const QString& source, HDL sourceLanguage,
               IRModule& module, QVector<Diagnostic>& diagnostics) const;

private:
    bool parseVhdl(const QVector<Token>& t, IRModule& m, QVector<Diagnostic>& d) const;
    bool parseVerilog(const QVector<Token>& t, IRModule& m, QVector<Diagnostic>& d) const;

    int findToken(const QVector<Token>& t, const QString& v, int from, HDL lang) const;
    int findMatchingParen(const QVector<Token>& t, int openIdx) const;

    // Разбор сущности/архитектуры
    void parseVhdlGeneric(const QVector<Token>& t, int from, int to,
                          IRModule& m) const;
    void parseVhdlPorts(const QVector<Token>& t, int from, int to,
                        IRModule& m, QVector<Diagnostic>& d) const;
    void parseVhdlSignals(const QVector<Token>& t, int from, int to,
                          IRModule& m) const;
    void parseVhdlArchitectureBody(const QVector<Token>& t, int from, int to,
                                   IRModule& m, QVector<Diagnostic>& d) const;

    // Разбор стейтментов внутри процесса
    void parseProcessBody(const QVector<Token>& t, int& i, int end,
                          QVector<StmtPtr>& out, HDL lang,
                          QVector<Diagnostic>& d) const;

    bool parseVhdlProcess(const QVector<Token>& t, int start, int end,
                          IRProcess& proc, QVector<Diagnostic>& d) const;

    void parseVhdlInstance(const QVector<Token>& t, int start, int end,
                           IRInstance& inst) const;
};