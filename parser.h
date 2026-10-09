#pragma once

#include "diagnostic.h"
#include "ir.h"
#include "lexer.h"

class Parser
{
public:
    bool parse(const QString& source,
               HDL sourceLanguage,
               IRModule& module,
               QVector<Diagnostic>& diagnostics) const;

private:
    bool parseVhdl(const QVector<Token>& tokens,
                   IRModule& module,
                   QVector<Diagnostic>& diagnostics) const;

    bool parseVerilog(const QVector<Token>& tokens,
                      IRModule& module,
                      QVector<Diagnostic>& diagnostics) const;

    QString joinTokens(const QVector<Token>& tokens, int from, int to) const;
    int findToken(const QVector<Token>& tokens, const QString& value,
                  int start) const;
};
