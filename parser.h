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
    int  findToken(const QVector<Token>& t, const QString& v, int from, HDL lang) const;
    int  findMatchingParen(const QVector<Token>& t, int openIdx) const;
};