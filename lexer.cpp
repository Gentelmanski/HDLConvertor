#include "lexer.h"

#include <QSet>

namespace {

inline Token mkTok(TokenType t, const QString& s, int line, int col)
{
    Token v;
    v.type = t;
    v.text = s;
    v.line = line;
    v.column = col;
    return v;
}

inline Diagnostic mkDiag(Severity sev, int line, int col, const QString& msg)
{
    Diagnostic d;
    d.severity = sev;
    d.line = line;
    d.column = col;
    d.message = msg;
    return d;
}

} // namespace

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
    static const QSet<QString> kw = {
        "library","use","entity","architecture","of","is","begin","end",
        "port","in","out","inout","buffer","signal","constant","variable",
        "process","if","then","elsif","else","case","when","with","select",
        "others","component","generic","map","std_logic","std_logic_vector",
        "downto","to","not","and","or","xor","nand","nor","xnor","wait","for",
        "rising_edge","falling_edge","unsigned","signed","numeric_std","abs",
        "mod","rem","after","report","severity","assert"
    };
    return kw.contains(s.toLower());
}

bool Lexer::isVerilogKeyword(const QString& s) const
{
    static const QSet<QString> kw = {
        "module","endmodule","input","output","inout","wire","reg","logic","bit",
        "always","assign","begin","end","if","else","case","endcase","default",
        "parameter","localparam","integer","int","posedge","negedge","or","and",
        "not","generate","endgenerate","genvar","function","endfunction","task",
        "endtask","for","while","repeat","initial"
    };
    return kw.contains(s);
}

bool Lexer::isSystemVerilogKeyword(const QString& s) const
{
    static const QSet<QString> kw = {
        "always_ff","always_comb","always_latch","typedef","struct","package",
        "endpackage","interface","endinterface","clocking","endclocking",
        "unique","priority","rand","randc","const","ref","this","super"
    };
    return kw.contains(s);
}

bool Lexer::isSystemVerilogUnbasedLiteral(const QString& s, int i) const
{
    if (i < 0 || i + 1 >= s.size() || s[i] != '\'')
        return false;
    const QChar v = s[i + 1];
    return v == '0' || v == '1' || v == 'x' || v == 'X'
           || v == 'z' || v == 'Z';
}

bool Lexer::isBasedNumberStart(const QString& s, int i) const
{
    if (i < 0 || i >= s.size() || !s[i].isDigit())
        return false;
    int j = i;
    while (j < s.size() && (s[j].isDigit() || s[j] == '_'))
        ++j;
    if (j >= s.size() || s[j] != '\'')
        return false;
    ++j;
    if (j < s.size() && (s[j] == 's' || s[j] == 'S'))
        ++j;
    if (j >= s.size())
        return false;
    const QChar b = s[j].toLower();
    return b == 'b' || b == 'o' || b == 'd' || b == 'h';
}

QVector<Token> Lexer::tokenize(const QString& src,
                               HDL lang,
                               QVector<Diagnostic>& diags) const
{
    QVector<Token> out;
    int i = 0;
    int line = 1;
    int col = 1;

    const bool vhdl = (lang == HDL::VHDL);
    const bool sv   = (lang == HDL::SystemVerilog);

    auto adv = [&](QChar c) {
        if (c == '\n') { ++line; col = 1; } else { ++col; }
        ++i;
    };

    auto pushSym = [&](const QString& t, int l, int c) {
        out.append(mkTok(TokenType::Symbol, t, l, c));
    };

    while (i < src.size()) {
        const QChar c = src[i];

        if (c.isSpace()) {
            adv(c);
            continue;
        }

        // VHDL комментарий --
        if (vhdl && c == '-' && i + 1 < src.size() && src[i + 1] == '-') {
            while (i < src.size() && src[i] != '\n')
                adv(src[i]);
            continue;
        }

        // // комментарий
        if (!vhdl && c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n')
                adv(src[i]);
            continue;
        }

        // /* ... */ комментарий
        if (!vhdl && c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            const int cl = line;
            const int cc = col;
            adv(src[i]);
            adv(src[i]);
            bool closed = false;
            while (i < src.size()) {
                if (src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/') {
                    adv(src[i]);
                    adv(src[i]);
                    closed = true;
                    break;
                }
                adv(src[i]);
            }
            if (!closed) {
                diags.append(mkDiag(Severity::Error, cl, cc,
                                    "Незакрытый многострочный комментарий."));
            }
            continue;
        }

        // SV атрибут (* ... *)
        if (sv && c == '(' && i + 1 < src.size() && src[i + 1] == '*') {
            adv(src[i]);
            adv(src[i]);
            while (i < src.size()) {
                if (src[i] == '*' && i + 1 < src.size() && src[i + 1] == ')') {
                    adv(src[i]);
                    adv(src[i]);
                    break;
                }
                adv(src[i]);
            }
            continue;
        }

        const int sl = line;
        const int sc = col;

        // SV unbased unsized literal '0 '1 'x 'z
        if (sv && isSystemVerilogUnbasedLiteral(src, i)) {
            QString lit;
            lit += src[i]; adv(src[i]);
            lit += src[i]; adv(src[i]);
            out.append(mkTok(TokenType::Number, lit, sl, sc));
            continue;
        }

        // SV системная задача $display и т.п.
        if (sv && c == '$') {
            QString w = "$";
            adv(src[i]);
            while (i < src.size() && isIdentifierPart(src[i])) {
                w += src[i];
                adv(src[i]);
            }
            out.append(mkTok(TokenType::Identifier, w, sl, sc));
            continue;
        }

        // SV директива `define и т.п.
        if (sv && c == '`') {
            QString w = "`";
            adv(src[i]);
            while (i < src.size() && isIdentifierPart(src[i])) {
                w += src[i];
                adv(src[i]);
            }
            out.append(mkTok(TokenType::Keyword, w, sl, sc));
            continue;
        }

                // VHDL extended identifier (escaped with backslashes)
        if (vhdl && c == '\\') {
            QString w;
            adv(src[i]);
            while (i < src.size() && src[i] != '\\' && src[i] != '\n') {
                w += src[i];
                adv(src[i]);
            }
            if (i < src.size() && src[i] == '\\')
                adv(src[i]);
            out.append(mkTok(TokenType::Identifier, w, sl, sc));
            continue;
        }

        // Идентификатор / ключевое слово
        if (isIdentifierStart(c)) {
            QString w;
            while (i < src.size() && isIdentifierPart(src[i])) {
                w += src[i];
                adv(src[i]);
            }

            // VHDL bit-string literal: x"FF", b"1010", ub"1100" и т.п.
            if (vhdl && w.size() >= 1 && w.size() <= 2
                && i < src.size() && src[i] == '"')
            {
                const QString pre = w.toLower();
                const bool isBitStr =
                    pre == "x"  || pre == "b"  || pre == "o"  || pre == "d"  ||
                    pre == "sx" || pre == "sb" || pre == "so" || pre == "sd" ||
                    pre == "ux" || pre == "ub" || pre == "uo" || pre == "ud";
                if (isBitStr) {
                    QString lit = w;
                    adv(src[i]);
                    while (i < src.size() && src[i] != '"') {
                        lit += src[i];
                        adv(src[i]);
                    }
                    if (i < src.size()) {
                        lit += src[i];
                        adv(src[i]);
                    }
                    out.append(mkTok(TokenType::String, lit, sl, sc));
                    continue;
                }
            }

            bool kw = false;
            if (vhdl)          kw = isVhdlKeyword(w);
            else if (sv)       kw = isVerilogKeyword(w) || isSystemVerilogKeyword(w);
            else               kw = isVerilogKeyword(w);

            out.append(mkTok(kw ? TokenType::Keyword : TokenType::Identifier,
                             w, sl, sc));
            continue;
        }

        // Число или based literal
        if (c.isDigit()) {
            QString num;
            while (i < src.size() && (src[i].isDigit() || src[i] == '_')) {
                num += src[i];
                adv(src[i]);
            }

            // VHDL based literal: 16#FF#
            if (vhdl && i < src.size() && src[i] == '#') {
                num += src[i];
                adv(src[i]);
                while (i < src.size() && src[i] != '#') {
                    num += src[i];
                    adv(src[i]);
                }
                if (i < src.size()) {
                    num += src[i];
                    adv(src[i]);
                }
                out.append(mkTok(TokenType::Number, num, sl, sc));
                continue;
            }

            // Verilog based literal: 4'b1010, 8'hFF, 16'd123
            if (!vhdl && i < src.size() && src[i] == '\'') {
                num += src[i];
                adv(src[i]);
                if (i < src.size() && (src[i] == 's' || src[i] == 'S')) {
                    num += src[i];
                    adv(src[i]);
                }
                if (i < src.size()) {
                    const QChar b = src[i].toLower();
                    if (b == 'b' || b == 'o' || b == 'd' || b == 'h') {
                        num += src[i];
                        adv(src[i]);
                        while (i < src.size()
                               && (src[i].isLetterOrNumber() || src[i] == '_')) {
                            num += src[i];
                            adv(src[i]);
                        }
                    }
                }
            }
            out.append(mkTok(TokenType::Number, num, sl, sc));
            continue;
        }

        // Строковые и (в VHDL) символьные литералы
        const bool vhdlChar = vhdl && c == '\'' && i + 2 < src.size()
                              && src[i + 2] == '\'' && src[i + 1] != '\'';
        if (c == '"' || vhdlChar) {
            const QChar q = c;
            int close = i + 1;
            while (close < src.size()) {
                if (src[close] == q) {
                    if (q == '"' && close + 1 < src.size()
                        && src[close + 1] == q) {
                        close += 2;
                        continue;
                    }
                    break;
                }
                if (src[close] == '\n' && q == '\'')
                    break;
                ++close;
            }
            if (close >= src.size() || src[close] != q) {
                diags.append(mkDiag(Severity::Error, sl, sc,
                                    "Незакрытая строковая/символьная константа."));
                pushSym(QString(q), sl, sc);
                adv(c);
                continue;
            }
            QString val;
            adv(src[i]);
            while (i < src.size()) {
                if (src[i] == q) {
                    adv(src[i]);
                    if (q == '"' && i < src.size() && src[i] == q) {
                        val += q;
                        adv(src[i]);
                        continue;
                    }
                    break;
                }
                val += src[i];
                adv(src[i]);
            }
            out.append(mkTok(TokenType::String, val, sl, sc));
            continue;
        }

        // VHDL атрибут clk'event
        if (vhdl && c == '\'') {
            pushSym("'", sl, sc);
            adv(c);
            continue;
        }

        // Многосимвольные операторы
        if (i + 1 < src.size()) {
            const QString two = src.mid(i, 2);
            static const QSet<QString> twoSet = {
                "<=", ":=", "==", "!=", ">=", "&&", "||", "=>", "**", "<<", ">>",
                "~&", "~|", "~^", "^~"
            };
            if (twoSet.contains(two)) {
                pushSym(two, sl, sc);
                adv(src[i]);
                adv(src[i]);
                continue;
            }
        }

        pushSym(QString(c), sl, sc);
        adv(c);
    }

    out.append(mkTok(TokenType::EndOfFile, "", line, col));
    return out;
}