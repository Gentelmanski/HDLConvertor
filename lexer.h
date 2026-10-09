#pragma once

#include "diagnostic.h"
#include <QString>
#include <QVector>

// Лексические категории общего токенизатора для VHDL, Verilog и SystemVerilog.
// Языковые различия учитываются на уровне Parser.
enum class TokenType
{
    Identifier,
    Number,
    String,
    Symbol,
    Keyword,
    EndOfFile
};

struct Token
{
    TokenType type = TokenType::Symbol;
    QString text;
    int line = 1;
    int column = 1;
};

class Lexer
{
public:
    QVector<Token> tokenize(const QString& source,
                            QVector<Diagnostic>& diagnostics) const;

private:
    bool isIdentifierStart(QChar c) const;
    bool isIdentifierPart(QChar c) const;
    bool isVhdlKeyword(const QString& s) const;
    bool isVerilogKeyword(const QString& s) const;

    // SystemVerilog: '0, '1, 'x, 'z — специальные unbased unsized literals.
    bool isSystemVerilogUnbasedLiteral(const QString& source, int index) const;

    // Verilog/SystemVerilog: 4'b1010, 8'hFF, 16'd123 и т.п.
    bool isBasedNumberStart(const QString& source, int index) const;
};
