#include "lexer.h"

#include <QSet>

bool Lexer::isIdentifierStart(QChar c) const
{
    return c.isLetter() || c == '_';
}

bool Lexer::isIdentifierPart(QChar c) const
{
    return c.isLetterOrNumber() || c == '_';
}

bool Lexer::isVhdlKeyword(const QString& s) const
{
    static const QSet<QString> keywords = {
        "library", "use", "entity", "architecture", "of", "is", "begin",
        "end", "port", "in", "out", "inout", "buffer", "signal", "constant",
        "variable", "process", "if", "then", "elsif", "else", "case",
        "when", "with", "select", "others", "component", "generic",
        "map", "std_logic", "std_logic_vector", "downto", "to", "not",
        "and", "or", "xor", "nand", "nor", "xnor", "wait", "for",
        "rising_edge", "falling_edge", "unsigned", "signed", "numeric_std"
    };
    return keywords.contains(s.toLower());
}

bool Lexer::isVerilogKeyword(const QString& s) const
{
    static const QSet<QString> keywords = {
        "module", "endmodule", "input", "output", "inout", "wire", "reg",
        "logic", "bit", "always", "always_ff", "always_comb", "always_latch",
        "assign", "begin", "end", "if", "else", "case", "endcase", "default",
        "parameter", "localparam", "integer", "int", "posedge", "negedge",
        "or", "and", "not", "generate", "endgenerate", "genvar", "function",
        "endfunction", "task", "endtask", "typedef", "struct", "package",
        "endpackage", "interface", "endinterface", "clocking", "endclocking",
        "always_comb", "always_ff", "always_latch"
    };
    return keywords.contains(s.toLower());
}

bool Lexer::isSystemVerilogUnbasedLiteral(const QString& source, int index) const
{
    if (index < 0 || index + 1 >= source.size() || source[index] != '\'')
        return false;

    const QChar value = source[index + 1];
    return value == '0' || value == '1' || value == 'x' || value == 'X'
           || value == 'z' || value == 'Z';
}

bool Lexer::isBasedNumberStart(const QString& source, int index) const
{
    if (index < 0 || index >= source.size() || !source[index].isDigit())
        return false;

    int i = index;
    while (i < source.size()
           && (source[i].isDigit() || source[i] == '_')) {
        ++i;
    }

    if (i >= source.size() || source[i] != '\'')
        return false;

    ++i;
    if (i < source.size() && (source[i] == 's' || source[i] == 'S'))
        ++i;

    if (i >= source.size())
        return false;

    const QChar base = source[i].toLower();
    return base == 'b' || base == 'o' || base == 'd' || base == 'h';
}

QVector<Token> Lexer::tokenize(const QString& source,
                               QVector<Diagnostic>& diagnostics) const
{
    QVector<Token> result;
    int i = 0;
    int line = 1;
    int column = 1;

    auto advance = [&](QChar c) {
        if (c == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
        ++i;
    };

    auto appendSymbol = [&](const QString& text, int startLine, int startColumn) {
        result.append({TokenType::Symbol, text, startLine, startColumn});
    };

    while (i < source.size()) {
        const QChar c = source[i];

        if (c.isSpace()) {
            advance(c);
            continue;
        }

        // VHDL comment.
        if (c == '-' && i + 1 < source.size() && source[i + 1] == '-') {
            while (i < source.size() && source[i] != '\n')
                advance(source[i]);
            continue;
        }

        // Verilog/SystemVerilog line comment.
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            while (i < source.size() && source[i] != '\n')
                advance(source[i]);
            continue;
        }

        // Verilog/SystemVerilog block comment.
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            const int commentLine = line;
            const int commentColumn = column;
            advance(source[i]);
            advance(source[i]);

            bool closed = false;
            while (i < source.size()) {
                if (source[i] == '*' && i + 1 < source.size()
                    && source[i + 1] == '/') {
                    advance(source[i]);
                    advance(source[i]);
                    closed = true;
                    break;
                }
                advance(source[i]);
            }

            if (!closed) {
                diagnostics.append({
                    Severity::Error,
                    commentLine,
                    commentColumn,
                    "Незакрытый многострочный комментарий."
                });
            }
            continue;
        }

        const int startLine = line;
        const int startColumn = column;

        // SystemVerilog unbased unsized literals: '0, '1, 'x, 'z.
        // Они НЕ являются символьными строками.
        if (isSystemVerilogUnbasedLiteral(source, i)) {
            QString literal;
            literal += source[i];
            advance(source[i]);
            literal += source[i];
            advance(source[i]);

            result.append({TokenType::Number, literal, startLine, startColumn});
            continue;
        }

        // Identifier / keyword.
        if (isIdentifierStart(c)) {
            QString word;
            while (i < source.size() && isIdentifierPart(source[i])) {
                word += source[i];
                advance(source[i]);
            }

            const QString lower = word.toLower();
            const bool keyword = isVhdlKeyword(lower) || isVerilogKeyword(lower);

            result.append({
                keyword ? TokenType::Keyword : TokenType::Identifier,
                word,
                startLine,
                startColumn
            });
            continue;
        }

        // Verilog/SystemVerilog based number literal:
        // 4'b1010, 8'hFF, 16'd123, 32'o777, optionally with s.
        if (c.isDigit()) {
            QString number;

            while (i < source.size()
                   && (source[i].isDigit() || source[i] == '_')) {
                number += source[i];
                advance(source[i]);
            }

            if (i < source.size() && source[i] == '\'') {
                number += source[i];
                advance(source[i]);

                if (i < source.size()
                    && (source[i] == 's' || source[i] == 'S')) {
                    number += source[i];
                    advance(source[i]);
                }

                if (i < source.size()) {
                    const QChar base = source[i].toLower();
                    if (base == 'b' || base == 'o' || base == 'd' || base == 'h') {
                        number += source[i];
                        advance(source[i]);

                        while (i < source.size()
                               && (source[i].isLetterOrNumber()
                                   || source[i] == '_')) {
                            number += source[i];
                            advance(source[i]);
                        }
                    }
                }
            }

            result.append({TokenType::Number, number, startLine, startColumn});
            continue;
        }

        // VHDL string literals and character literals.
        // A single quote is treated as a character literal only when a closing
        // quote exists. This prevents SystemVerilog '0/'1 from being misread.
        if (c == '"' || c == '\'') {
            const QChar quote = c;

            int closing = i + 1;
            while (closing < source.size()) {
                if (source[closing] == quote) {
                    // VHDL can represent a quote inside a string by doubling it.
                    if (quote == '"' && closing + 1 < source.size()
                        && source[closing + 1] == quote) {
                        closing += 2;
                        continue;
                    }
                    break;
                }
                if (source[closing] == '\n' && quote == '\'')
                    break;
                ++closing;
            }

            if (closing >= source.size() || source[closing] != quote) {
                diagnostics.append({
                    Severity::Error,
                    startLine,
                    startColumn,
                    "Незакрытая строковая/символьная константа."
                });

                // Do not swallow the rest of the file. Emit the opening quote
                // as a symbol so the parser can continue and report more errors.
                appendSymbol(QString(quote), startLine, startColumn);
                advance(c);
                continue;
            }

            QString value;
            advance(source[i]);

            while (i < source.size()) {
                if (source[i] == quote) {
                    advance(source[i]);

                    if (quote == '"' && i < source.size() && source[i] == quote) {
                        value += quote;
                        advance(source[i]);
                        continue;
                    }
                    break;
                }

                value += source[i];
                advance(source[i]);
            }

            result.append({TokenType::String, value, startLine, startColumn});
            continue;
        }

        // Multi-character operators.
        if (i + 1 < source.size()) {
            const QString two = source.mid(i, 2);
            if (two == "<=" || two == ":=" || two == "==" ||
                two == "!=" || two == ">=" || two == "&&" ||
                two == "||" || two == "=>" || two == "**" ||
                two == "<<" || two == ">>") {
                appendSymbol(two, startLine, startColumn);
                advance(source[i]);
                advance(source[i]);
                continue;
            }
        }

        appendSymbol(QString(c), startLine, startColumn);
        advance(c);
    }

    result.append({TokenType::EndOfFile, "", line, column});
    return result;
}
