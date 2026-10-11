#include "parser.h"
#include <QRegularExpression>

namespace {

bool vhdlEq(const QString& a, const QString& b) {
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

QString canonicalOp(const QString& op, HDL lang) {
    const QString l = op.toLower();
    if (lang == HDL::VHDL) {
        if (l=="and")  return "AND";
        if (l=="or")   return "OR";
        if (l=="xor")  return "XOR";
        if (l=="nand") return "NAND";
        if (l=="nor")  return "NOR";
        if (l=="xnor") return "XNOR";
        if (l=="not")  return "NOT";
        if (l=="abs")  return "ABS";
        if (l=="mod")  return "MOD";
        if (l=="rem")  return "REM";
        if (op=="=")   return "EQ";
        if (op=="/=")  return "NEQ";
        if (op=="<")   return "LT";
        if (op=="<=")  return "LE";
        if (op==">")   return "GT";
        if (op==">=")  return "GE";
        if (op=="+")   return "ADD";
        if (op=="-")   return "SUB";
        if (op=="*")   return "MUL";
        if (op=="/")   return "DIV";
        if (op=="**")  return "POW";
        if (op=="&")   return "CONCAT";
    } else {
        if (op=="&&")  return "LAND";
        if (op=="||")  return "LOR";
        if (op=="&")   return "AND";
        if (op=="|")   return "OR";
        if (op=="^")   return "XOR";
        if (op=="~&")  return "NAND";
        if (op=="~|")  return "NOR";
        if (op=="~^"||op=="^~") return "XNOR";
        if (op=="~")   return "NOT";
        if (op=="!")   return "LNOT";
        if (op=="==")  return "EQ";
        if (op=="!=")  return "NEQ";
        if (op=="<")   return "LT";
        if (op=="<=")  return "LE";
        if (op==">")   return "GT";
        if (op==">=")  return "GE";
        if (op=="+")   return "ADD";
        if (op=="-")   return "SUB";
        if (op=="*")   return "MUL";
        if (op=="/")   return "DIV";
        if (op=="%")   return "MOD";
        if (op=="**")  return "POW";
        if (op=="<<")  return "SHL";
        if (op==">>")  return "SHR";
        if (op=="&")   return "AND";
    }
    return op;
}

int precedence(const QString& op, HDL lang) {
    const QString o = canonicalOp(op, lang);
    if (lang == HDL::VHDL) {
        if (o=="OR"||o=="NOR")   return 1;
        if (o=="AND"||o=="NAND"||o=="XOR"||o=="XNOR") return 2;
        if (o=="EQ"||o=="NEQ"||o=="LT"||o=="LE"||o=="GT"||o=="GE") return 3;
        if (o=="ADD"||o=="SUB"||o=="CONCAT") return 4;
        if (o=="MUL"||o=="DIV"||o=="MOD"||o=="REM") return 5;
        if (o=="POW") return 7;
        return 0;
    } else {
        if (o=="LOR") return 1;
        if (o=="LAND") return 2;
        if (o=="OR") return 3;
        if (o=="XOR"||o=="XNOR") return 4;
        if (o=="AND") return 5;
        if (o=="EQ"||o=="NEQ") return 6;
        if (o=="LT"||o=="LE"||o=="GT"||o=="GE") return 7;
        if (o=="SHL"||o=="SHR") return 8;
        if (o=="ADD"||o=="SUB") return 9;
        if (o=="MUL"||o=="DIV"||o=="MOD") return 10;
        if (o=="POW") return 12;
        return 0;
    }
}

bool isUnaryOp(const QString& t, HDL lang) {
    const QString l = t.toLower();
    if (lang == HDL::VHDL)
        return l=="not" || l=="abs" || t=="-" || t=="+";
    return t=="~" || t=="!" || t=="-" || t=="+"
           || t=="&" || t=="|" || t=="^"
           || t=="~&" || t=="~|" || t=="~^" || t=="^~";
}

struct ExprCtx {
    const QVector<Token>& t;
    HDL lang;
    int pos = 0;

    bool ok() const { return pos < t.size() && t[pos].type != TokenType::EndOfFile; }
    const QString& txt() const { return t[pos].text; }
    bool isSym(const char* s) const { return ok() && t[pos].text == s; }
    bool isKw(const char* s) const {
        return ok() && t[pos].text.compare(QString::fromLatin1(s),
                                           Qt::CaseInsensitive) == 0;
    }
};

ExprPtr parsePrimary(ExprCtx& c);
ExprPtr parseUnary(ExprCtx& c);
ExprPtr parseBinary(ExprCtx& c, int minPrec);
ExprPtr parsePostfix(ExprCtx& c);
ExprPtr parseExprFull(ExprCtx& c);

ExprPtr parsePrimary(ExprCtx& c) {
    if (!c.ok()) return nullptr;

    if (c.isSym("(")) {
        ++c.pos;
        ExprPtr inner = parseExprFull(c);
        if (c.isSym(")")) ++c.pos;
        return inner;
    }

    // VHDL атрибут clk'event
    if (c.lang == HDL::VHDL && c.isSym("'")) {
        ++c.pos;
        if (c.ok()) {
            QString attr = c.txt();
            ++c.pos;
            return mkIdent("'" + attr);
        }
        return nullptr;
    }

    // VHDL (others => '0')
    if (c.lang == HDL::VHDL && c.isSym("(")) {
        // уже обработано выше
    }
    if (c.ok() && c.txt().compare("others", Qt::CaseInsensitive) == 0) {
        ++c.pos;
        // допускаем "others => val"
        if (c.isSym("=>")) {
            ++c.pos;
            ExprPtr v = parseExprFull(c);
            auto e = mkOthers(v ? v->text : "");
            if (v) e->children = {v};
            return e;
        }
        return mkOthers("");
    }

    const Token& tok = c.t[c.pos];
    if (tok.type == TokenType::Number || tok.type == TokenType::String) {
        ++c.pos;
        // Одиночный символ '0' / '1' / 'Z' / 'X' → пометим кавычками для конвертера.
        QString s = tok.text;
        if (s.size() == 1 && (s[0]=='0'||s[0]=='1'||s[0]=='Z'||s[0]=='z'
                              ||s[0]=='X'||s[0]=='x'||s[0]=='U'||s[0]=='u'
                              ||s[0]=='H'||s[0]=='h'||s[0]=='L'||s[0]=='l'
                              ||s[0]=='W'||s[0]=='w'||s[0]=='-'))
            s = "'" + s + "'";
        return mkLit(s);
    }
    if (tok.type == TokenType::Identifier || tok.type == TokenType::Keyword) {
        ++c.pos;
        return mkIdent(tok.text);
    }
    return nullptr;
}

ExprPtr parsePostfix(ExprCtx& c) {
    ExprPtr base = parsePrimary(c);
    if (!base) return nullptr;

    while (c.ok()) {
        if (c.isSym("[")) {
            ++c.pos;
            ExprPtr a = parseExprFull(c);
            if (c.isSym(":")) {
                ++c.pos;
                ExprPtr b = parseExprFull(c);
                if (c.isSym("]")) ++c.pos;
                base = mkSlice(base, a, b);
            } else {
                if (c.isSym("]")) ++c.pos;
                base = mkIndex(base, a);
            }
        } else if (c.isSym("(")) {
            ++c.pos;
            QVector<ExprPtr> args;
            if (!c.isSym(")")) {
                for (;;) {
                    ExprPtr a = parseExprFull(c);
                    if (a) args.append(a);
                    if (c.isSym(",")) { ++c.pos; continue; }
                    break;
                }
            }
            if (c.isSym(")")) ++c.pos;

            // VHDL: name(range) — там где name не функция, а диапазон
            if (c.lang == HDL::VHDL && args.size() == 1 && base->kind == ExprKind::Ident) {
                // Различить: a(7) — индекс; но func(a) — уже FuncCall (name уникально)
                base = mkIndex(base, args[0]);
            } else if (base->kind == ExprKind::Ident) {
                base = mkFunc(base->text, args);
            } else {
                base = mkIndex(base, args.isEmpty() ? mkLit("0") : args[0]);
            }
        } else {
            break;
        }
    }
    return base;
}

ExprPtr parseUnary(ExprCtx& c) {
    if (c.ok() && isUnaryOp(c.txt(), c.lang)) {
        QString op = c.txt(); ++c.pos;
        ExprPtr a = parseUnary(c);
        return mkUnary(canonicalOp(op, c.lang), a);
    }
    return parsePostfix(c);
}

ExprPtr parseBinary(ExprCtx& c, int minPrec) {
    ExprPtr lhs = parseUnary(c);
    if (!lhs) return nullptr;

    while (c.ok()) {
        const QString op = c.txt();
        // VHDL: "when"/"else" останавливают бинарный разбор
        if (c.lang == HDL::VHDL &&
            (op.compare("when", Qt::CaseInsensitive)==0 ||
             op.compare("else", Qt::CaseInsensitive)==0))
            break;
        const int prec = precedence(op, c.lang);
        if (prec < minPrec || prec == 0) break;
        ++c.pos;
        ExprPtr rhs = parseBinary(c, prec + 1);
        if (!rhs) break;
        const QString cop = canonicalOp(op, c.lang);
        if (cop == "CONCAT" && c.lang == HDL::VHDL) {
            // VHDL & — конкатенация (ассоциативна слева)
            QVector<ExprPtr> parts;
            if (lhs->kind == ExprKind::Concat) parts = lhs->children;
            else parts.append(lhs);
            if (rhs->kind == ExprKind::Concat) parts += rhs->children;
            else parts.append(rhs);
            lhs = mkConcat(parts);
        } else {
            lhs = mkBinary(cop, lhs, rhs);
        }
    }
    return lhs;
}

ExprPtr parseExprFull(ExprCtx& c) {
    ExprPtr e = parseBinary(c, 0);

    // VHDL when/else: expr when cond else else_expr (право-ассоциативно)
    if (c.lang == HDL::VHDL && c.ok() && c.isKw("when")) {
        ++c.pos;
        ExprPtr cond = parseExprFull(c);
        if (c.ok() && c.isKw("else")) ++c.pos;
        ExprPtr els = parseExprFull(c);
        return mkTernary(cond, e, els);
    }
    // Verilog: cond ? a : b
    if (c.lang != HDL::VHDL && c.isSym("?")) {
        ++c.pos;
        ExprPtr t = parseExprFull(c);
        if (c.isSym(":")) ++c.pos;
        ExprPtr f = parseExprFull(c);
        return mkTernary(e, t, f);
    }
    return e;
}

} // namespace

int Parser::findToken(const QVector<Token>& t, const QString& v,
                      int from, HDL lang) const {
    for (int i = from; i < t.size(); ++i) {
        if (lang == HDL::VHDL) {
            if (t[i].text.compare(v, Qt::CaseInsensitive) == 0) return i;
        } else {
            if (t[i].text == v) return i;
        }
    }
    return -1;
}

int Parser::findMatchingParen(const QVector<Token>& t, int openIdx) const {
    if (openIdx < 0 || openIdx >= t.size() || t[openIdx].text != "(")
        return -1;
    int depth = 0;
    for (int i = openIdx; i < t.size(); ++i) {
        if (t[i].text == "(") ++depth;
        else if (t[i].text == ")") {
            --depth;
            if (depth == 0) return i;
        }
    }
    return -1;
}

// ---- Внутренние парсеры VHDL ----

static bool readDirection(const QString& s, HDL lang, PortDirection& out) {
    const QString v = s.toLower();
    if (lang == HDL::VHDL) {
        if (v=="in")    { out = PortDirection::Input;  return true; }
        if (v=="out")   { out = PortDirection::Output; return true; }
        if (v=="inout") { out = PortDirection::Inout;  return true; }
    } else {
        if (v=="input")  { out = PortDirection::Input;  return true; }
        if (v=="output") { out = PortDirection::Output; return true; }
        if (v=="inout")  { out = PortDirection::Inout;  return true; }
    }
    return false;
}

// Собирает "WIDTH-1 downto 0" (VHDL) или "[WIDTH-1:0]" (Verilog)
static QString collectRange(const QVector<Token>& t, int open, int close,
                            HDL lang) {
    QString s;
    QChar openC = lang == HDL::VHDL ? '(' : '[';
    QChar closeC = lang == HDL::VHDL ? ')' : ']';
    if (t[open].text != QString(openC)) return s;
    int depth = 0;
    for (int k = open; k <= close; ++k) {
        if (t[k].text == QString(openC)) { ++depth; continue; }
        if (t[k].text == QString(closeC)) { --depth; if (depth==0) break; continue; }
        if (!s.isEmpty() && t[k].text != ":" && t[k].text != ")" &&
            t[k].text != "]" && t[k-1].text != "(" && t[k-1].text != "["
            && t[k].text != "downto" && t[k].text != "to")
            s += ' ';
        s += t[k].text;
        if (t[k].text.compare("downto",Qt::CaseInsensitive)==0 ||
            t[k].text.compare("to",Qt::CaseInsensitive)==0)
            s += ' ';
    }
    return s;
}

static int parseRange(const QString& rangeText, HDL lang,
                      QString& rangeVhdl, QString& rangeVerilog,
                      int& width) {
    QString s = rangeText;
    s.replace(QRegularExpression("\\s+"), " ").trimmed();
    if (s.isEmpty()) return 0;

    // Литеральные числа?
    QRegularExpression re("^(\\d+)\\s*(downto|to|:)\\s*(\\d+)$",
                          QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(s);
    if (m.hasMatch()) {
        const int a = m.captured(1).toInt();
        const int b = m.captured(3).toInt();
        width = qAbs(a-b)+1;
        if (lang == HDL::VHDL) {
            rangeVhdl = QString("(%1 downto 0)").arg(width-1);
            rangeVerilog = QString("[%1:0]").arg(width-1);
        } else {
            rangeVhdl = QString("(%1 downto 0)").arg(width-1);
            rangeVerilog = QString("[%1:0]").arg(width-1);
        }
        return 1;
    }
    // Параметрический
    if (lang == HDL::VHDL) rangeVhdl = "(" + s + ")";
    else rangeVerilog = "[" + s + "]";
    return 2;
}

void Parser::parseVhdlGeneric(const QVector<Token>& t, int from, int to,
                              IRModule& m) const {
    int i = from;
    // ожидаем: generic ( name : type := value; ... );
    if (i >= to || !vhdlEq(t[i].text, "generic")) return;
    ++i;
    if (i >= to || t[i].text != "(") return;
    int close = findMatchingParen(t, i);
    if (close < 0) close = to;
    ++i;
    while (i < close) {
        if (t[i].type != TokenType::Identifier && t[i].type != TokenType::Keyword) {
            ++i; continue;
        }
        IRParameter p;
        p.name = t[i].text;
        ++i;
        if (i < close && t[i].text == ":") {
            ++i;
            if (i < close) { p.type = t[i].text; ++i; }
        }
        // := value
        while (i < close && t[i].text != ":=" && t[i].text != ";" &&
               t[i].text != ")") ++i;
        if (i < close && t[i].text == ":=") {
            ++i;
            QString v;
            while (i < close && t[i].text != ";" && t[i].text != ")") {
                if (!v.isEmpty()) v += ' ';
                v += t[i].text;
                ++i;
            }
            p.defaultValue = v.trimmed();
        }
        while (i < close && t[i].text != ";") ++i;
        if (i < close && t[i].text == ";") ++i;
        m.parameters.append(p);
    }
}

void Parser::parseVhdlPorts(const QVector<Token>& t, int from, int to,
                            IRModule& m, QVector<Diagnostic>& d) const {
    int i = from;
    if (i >= to || !vhdlEq(t[i].text, "port")) return;
    ++i;
    if (i >= to || t[i].text != "(") {
        d.append({Severity::Error, t[from].line, t[from].column,
                  "Ожидается '(' после 'port'."});
        return;
    }
    int close = findMatchingParen(t, i);
    if (close < 0) {
        d.append({Severity::Error, t[i].line, t[i].column,
                  "Незакрытый список портов."});
        return;
    }
    ++i;

    while (i < close) {
        if (t[i].text == ";" || t[i].text == ",") { ++i; continue; }
        if (t[i].type != TokenType::Identifier && t[i].type != TokenType::Keyword) {
            ++i; continue;
        }

        QStringList names;
        names << t[i].text;
        ++i;
        while (i < close && t[i].text == ",") {
            ++i;
            if (i >= close) break;
            names << t[i].text;
            ++i;
        }

        if (i >= close || t[i].text != ":") {
            d.append({Severity::Error,
                      t[i<close?i:close].line, t[i<close?i:close].column,
                      "Ожидается ':' после имени порта."});
            return;
        }
        ++i;

        PortDirection dir;
        if (i >= close || !readDirection(t[i].text, HDL::VHDL, dir)) {
            d.append({Severity::Error,
                      t[i<close?i:close].line, t[i<close?i:close].column,
                      "Ожидается направление порта (in/out/inout)."});
            return;
        }
        ++i;

        if (i >= close) {
            d.append({Severity::Error, t[close].line, t[close].column,
                      "Ожидается тип порта."});
            return;
        }

        QString type = t[i].text;
        ++i;
        QString rangeVhdl, rangeVerilog;
        int width = 1;
        if (i < close && t[i].text == "(") {
            int rend = findMatchingParen(t, i);
            if (rend < 0) rend = i;
            QString rt = collectRange(t, i, rend, HDL::VHDL);
            parseRange(rt, HDL::VHDL, rangeVhdl, rangeVerilog, width);
            i = rend + 1;
        }

        // пропустить ';'
        while (i < close && t[i].text != ";") ++i;
        if (i < close) ++i;

        for (const QString& nm : names) {
            IRPort p;
            p.name = nm;
            p.direction = dir;
            p.type = type;
            p.rangeVhdl = rangeVhdl;
            p.rangeVerilog = rangeVerilog;
            p.width = width;
            m.ports.append(p);
        }
    }
}

void Parser::parseVhdlSignals(const QVector<Token>& t, int from, int to,
                              IRModule& m) const {
    for (int i = from; i < to; ++i) {
        if (!vhdlEq(t[i].text, "signal")) continue;
        int k = i+1;
        QStringList names;
        while (k < to && t[k].text != ":") {
            if (t[k].type == TokenType::Identifier) names << t[k].text;
            ++k;
        }
        if (k >= to) break;
        ++k;
        if (k >= to) break;
        QString type = t[k].text;
        ++k;
        QString rangeVhdl, rangeVerilog;
        int width = 1;
        if (k < to && t[k].text == "(") {
            int rend = findMatchingParen(t, k);
            if (rend < 0) rend = k;
            QString rt = collectRange(t, k, rend, HDL::VHDL);
            parseRange(rt, HDL::VHDL, rangeVhdl, rangeVerilog, width);
            k = rend + 1;
        }
        while (k < to && t[k].text != ";") ++k;
        for (const QString& nm : names) {
            IRSignal s;
            s.name = nm;
            s.type = type;
            s.rangeVhdl = rangeVhdl;
            s.rangeVerilog = rangeVerilog;
            s.width = width;
            m.signalList.append(s);
        }
        i = k;
    }
}

// ---- Разбор тела процесса ----

// Собирает одиночное присваивание lhs <= rhs; или lhs := rhs;
static StmtPtr parseAssignStmt(const QVector<Token>& t, int& i, int end,
                               HDL lang) {
    if (i >= end) return nullptr;

    ExprCtx lhsCtx{t, lang, i};
    ExprPtr lhs = parsePostfix(lhsCtx);
    int afterLhs = lhsCtx.pos;
    if (!lhs || afterLhs >= end) return nullptr;

    bool seq = false;
    if (t[afterLhs].text == "<=") seq = true;
    else if (t[afterLhs].text == "=" && lang != HDL::VHDL) seq = false;
    else if (t[afterLhs].text == ":=") seq = false;
    else return nullptr;

    ExprCtx rhsCtx{t, lang, afterLhs+1};
    ExprPtr rhs = parseExprFull(rhsCtx);
    if (!rhs) return nullptr;

    // Пропускаем до ';'
    int j = rhsCtx.pos;
    while (j < end && t[j].text != ";") ++j;
    if (j < end) ++j;
    i = j;

    auto s = std::make_shared<IRStmt>();
    s->kind = StmtKind::Assign;
    s->lhs = lhs;
    s->rhs = rhs;
    s->sequential = seq;
    return s;
}

// if cond then ... elsif ... else ... end if;
static StmtPtr parseIfStmt(const QVector<Token>& t, int& i, int end,
                           HDL lang, QVector<Diagnostic>& d);

// case expr is when choices => ... when others => ... end case;
static StmtPtr parseCaseStmt(const QVector<Token>& t, int& i, int end,
                             HDL lang, QVector<Diagnostic>& d);

static bool isEndIfAt(const QVector<Token>& t, int i, int end) {
    if (i+1 >= end) return false;
    return vhdlEq(t[i].text, "end") && vhdlEq(t[i+1].text, "if");
}
static bool isEndCaseAt(const QVector<Token>& t, int i, int end) {
    if (i+1 >= end) return false;
    return vhdlEq(t[i].text, "end") && vhdlEq(t[i+1].text, "case");
}

// Парсит список операторов до стоп-условия
static void parseStmtsUntil(const QVector<Token>& t, int& i, int end,
                            HDL lang, QVector<StmtPtr>& out,
                            QVector<Diagnostic>& d,
                            bool stopOnEndIf, bool stopOnEndCase,
                            bool stopOnElse, bool stopOnElsif,
                            bool stopOnWhen) {
    while (i < end) {
        const QString& w = t[i].text;

        if (stopOnEndIf && isEndIfAt(t, i, end)) return;
        if (stopOnEndCase && isEndCaseAt(t, i, end)) return;
        if (stopOnElse && vhdlEq(w, "else")) return;
        if (stopOnElsif && vhdlEq(w, "elsif")) return;
        if (stopOnWhen && vhdlEq(w, "when")) return;

        if (vhdlEq(w, "if") && lang == HDL::VHDL) {
            out.append(parseIfStmt(t, i, end, lang, d));
            continue;
        }
        if (vhdlEq(w, "case") && lang == HDL::VHDL) {
            out.append(parseCaseStmt(t, i, end, lang, d));
            continue;
        }
        if (w == ";") { ++i; continue; }

        StmtPtr a = parseAssignStmt(t, i, end, lang);
        if (a) { out.append(a); continue; }
        ++i;
    }
}

static StmtPtr parseIfStmt(const QVector<Token>& t, int& i, int end,
                           HDL lang, QVector<Diagnostic>& d) {
    auto s = std::make_shared<IRStmt>();
    s->kind = StmtKind::If;
    ++i; // if
    ExprCtx cc{t, lang, i};
    s->cond = parseExprFull(cc);
    i = cc.pos;
    if (i < end && vhdlEq(t[i].text, "then")) ++i;

    parseStmtsUntil(t, i, end, lang, s->thenStmts, d,
                    true, false, true, true, false);

    while (i < end && vhdlEq(t[i].text, "elsif")) {
        ++i;
        ExprCtx c2{t, lang, i};
        ExprPtr cond2 = parseExprFull(c2);
        i = c2.pos;
        if (i < end && vhdlEq(t[i].text, "then")) ++i;
        QVector<StmtPtr> body;
        parseStmtsUntil(t, i, end, lang, body, d,
                        true, false, true, true, false);
        s->elsifStmts.append({cond2, body});
    }
    if (i < end && vhdlEq(t[i].text, "else")) {
        ++i;
        parseStmtsUntil(t, i, end, lang, s->elseStmts, d,
                        true, false, false, false, false);
    }
    if (isEndIfAt(t, i, end)) i += 2;
    if (i < end && t[i].text == ";") ++i;
    return s;
}

static StmtPtr parseCaseStmt(const QVector<Token>& t, int& i, int end,
                             HDL lang, QVector<Diagnostic>& d) {
    auto s = std::make_shared<IRStmt>();
    s->kind = StmtKind::Case;
    ++i; // case
    ExprCtx cc{t, lang, i};
    s->caseExpr = parseExprFull(cc);
    i = cc.pos;
    if (i < end && vhdlEq(t[i].text, "is")) ++i;

    while (i < end && !isEndCaseAt(t, i, end)) {
        if (!vhdlEq(t[i].text, "when")) { ++i; continue; }
        ++i;
        IRCaseItem item;
        // choice list до =>
        while (i < end && t[i].text != "=>") {
            if (t[i].text == "," || t[i].text == "|") { ++i; continue; }
            if (vhdlEq(t[i].text, "others")) {
                item.choices.clear();
                ++i;
                break;
            }
            ExprCtx c2{t, lang, i};
            ExprPtr v = parseExprFull(c2);
            if (v) item.choices.append(v);
            i = c2.pos;
        }
        if (i < end && t[i].text == "=>") ++i;
        parseStmtsUntil(t, i, end, lang, item.stmts, d,
                        false, true, false, false, true);
        s->caseItems.append(item);
    }
    if (isEndCaseAt(t, i, end)) i += 2;
    if (i < end && t[i].text == ";") ++i;
    return s;
}

bool Parser::parseVhdlProcess(const QVector<Token>& t, int start, int end,
                              IRProcess& proc, QVector<Diagnostic>& d) const {
    proc.kind = HDL::VHDL;
    int i = start + 1; // за process

    // sensitivity: process(clk) или process(clk, rst)
    if (i < end && t[i].text == "(") {
        int close = findMatchingParen(t, i);
        if (close < 0) close = i;
        for (int k = i+1; k < close; ++k)
            if (t[k].type == TokenType::Identifier && proc.clockName.isEmpty())
                proc.clockName = t[k].text;
        i = close+1;
    }

    // ищем begin
    while (i < end && !vhdlEq(t[i].text, "begin")) ++i;
    if (i >= end) return false;
    ++i;

    // ищем end process
    int bodyEnd = i;
    while (bodyEnd < end) {
        if (vhdlEq(t[bodyEnd].text, "end") &&
            bodyEnd+1 < end && vhdlEq(t[bodyEnd+1].text, "process"))
            break;
        // "end" без "process" — тоже конец
        if (vhdlEq(t[bodyEnd].text, "end") &&
            (bodyEnd+1 >= end || !vhdlEq(t[bodyEnd+1].text, "process"))) {
            // может быть end if/end case внутри
        }
        ++bodyEnd;
    }

    int j = i;
    parseStmtsUntil(t, j, bodyEnd, HDL::VHDL, proc.body, d,
                    false, false, false, false, false);

    // Ищем rising_edge/falling_edge в дереве, чтобы выставить clocked
    std::function<bool(const ExprPtr&)> scanExpr = [&](const ExprPtr& e)->bool {
        if (!e) return false;
        if (e->kind == ExprKind::FunctionCall) {
            const QString n = e->text.toLower();
            if (n == "rising_edge" || n == "falling_edge") {
                proc.clocked = (n == "rising_edge");
                if (!e->children.isEmpty() &&
                    e->children[0]->kind == ExprKind::Ident)
                    proc.clockName = e->children[0]->text;
                return true;
            }
        }
        for (auto& c : e->children) if (scanExpr(c)) return true;
        return false;
    };
    std::function<bool(const StmtPtr&)> scanStmt = [&](const StmtPtr& s)->bool {
        if (!s) return false;
        if (scanExpr(s->lhs) || scanExpr(s->rhs) || scanExpr(s->cond) ||
            scanExpr(s->caseExpr)) return true;
        for (auto& x : s->thenStmts) if (scanStmt(x)) return true;
        for (auto& x : s->elseStmts) if (scanStmt(x)) return true;
        for (auto& pr : s->elsifStmts) for (auto& x : pr.second) if (scanStmt(x)) return true;
        for (auto& ci : s->caseItems) for (auto& x : ci.stmts) if (scanStmt(x)) return true;
        return false;
    };
    for (auto& s : proc.body) if (scanStmt(s)) break;

    return true;
}

// ---- Разбор архитектурного тела ----

void Parser::parseVhdlArchitectureBody(const QVector<Token>& t,
                                       int from, int to,
                                       IRModule& m,
                                       QVector<Diagnostic>& d) const {
    // Внутренние сигналы
    parseVhdlSignals(t, from, to, m);

    // Найти begin
    int beginIdx = -1;
    for (int i = from; i < to; ++i)
        if (vhdlEq(t[i].text, "begin")) { beginIdx = i; break; }

    // Найти конец архитектуры: последний "end"
    int endIdx = to;
    for (int i = to-1; i >= from; --i)
        if (vhdlEq(t[i].text, "end")) { endIdx = i; break; }

    if (beginIdx < 0) return;
    int i = beginIdx + 1;

    // Разбираем операторы между begin и end
    while (i < endIdx) {
        if (t[i].text == ";") { ++i; continue; }

        // process
        if (vhdlEq(t[i].text, "process")) {
            // Найдём конец процесса
            int pend = i;
            int depth = 0;
            while (pend < endIdx) {
                if (vhdlEq(t[pend].text, "process")) ++depth;
                if (vhdlEq(t[pend].text, "end") && pend+1 < endIdx &&
                    vhdlEq(t[pend+1].text, "process")) {
                    --depth;
                    if (depth == 0) { pend = pend+2; break; }
                }
                ++pend;
            }
            IRProcess p;
            if (parseVhdlProcess(t, i, pend, p, d))
                m.processes.append(p);
            i = pend;
            if (i < endIdx && t[i].text == ";") ++i;
            continue;
        }

        // with sel select  y <= val when choice, ... ;
        if (vhdlEq(t[i].text, "with")) {
            IRCaseStatement cs;
            ++i;
            ExprCtx sc{t, HDL::VHDL, i};
            cs.selectExpr = parseExprFull(sc);
            i = sc.pos;
            if (i < endIdx && vhdlEq(t[i].text, "select")) ++i;

            // target <=
            ExprCtx tc{t, HDL::VHDL, i};
            cs.target = parsePostfix(tc);
            i = tc.pos;
            if (i < endIdx && t[i].text == "<=") ++i;

            while (i < endIdx) {
                ExprCtx vc{t, HDL::VHDL, i};
                ExprPtr val = parseExprFull(vc);
                i = vc.pos;
                if (i >= endIdx || !vhdlEq(t[i].text, "when")) break;
                ++i;
                if (vhdlEq(t[i].text, "others")) {
                    cs.defaultExpr = val;
                    ++i;
                } else {
                    ExprCtx chc{t, HDL::VHDL, i};
                    ExprPtr choice = parseExprFull(chc);
                    i = chc.pos;
                    cs.items.append({{choice}, val});
                }
                if (i < endIdx && t[i].text == ",") { ++i; continue; }
                if (i < endIdx && t[i].text == ";") { ++i; break; }
                break;
            }
            m.withSelect.append(cs);
            continue;
        }

        // Инстанс: label : entity work.name port map (...)  или label : name port map(...)
        if (t[i].type == TokenType::Identifier &&
            i+1 < endIdx && t[i+1].text == ":") {
            // Отличить сигнальное присваивание (не бывает) — это инстанс
            IRInstance inst;
            inst.label = t[i].text;
            int k = i+2;
            if (k < endIdx && vhdlEq(t[k].text, "entity")) ++k;
            if (k < endIdx && vhdlEq(t[k].text, "work")) {
                ++k;
                if (k < endIdx && t[k].text == ".") ++k;
            }
            if (k < endIdx) {
                inst.moduleName = t[k].text;
                ++k;
            }
            // ищем port map
            while (k < endIdx && !vhdlEq(t[k].text, "port")) ++k;
            if (k < endIdx && vhdlEq(t[k].text, "port")) {
                ++k;
                if (k < endIdx && vhdlEq(t[k].text, "map")) ++k;
                if (k < endIdx && t[k].text == "(") {
                    int close = findMatchingParen(t, k);
                    if (close < 0) close = k;
                    int q = k+1;
                    while (q < close) {
                        if (t[q].text == "," || t[q].text == ";") { ++q; continue; }
                        QString formal = t[q].text; ++q;
                        if (q < close && t[q].text == "=>") ++q;
                        QString actual;
                        if (q < close) {
                            actual = t[q].text;
                            ++q;
                        }
                        inst.portMap.append({formal, actual});
                    }
                    k = close + 1;
                }
            }
            while (k < endIdx && t[k].text != ";") ++k;
            i = k;
            if (i < endIdx && t[i].text == ";") ++i;
            m.instances.append(inst);
            continue;
        }

        // Concurrent assign lhs <= rhs;
        if (t[i].type == TokenType::Identifier ||
            t[i].type == TokenType::Keyword) {
            ExprCtx lc{t, HDL::VHDL, i};
            ExprPtr lhs = parsePostfix(lc);
            int after = lc.pos;
            if (lhs && after < endIdx && t[after].text == "<=") {
                ExprCtx rc{t, HDL::VHDL, after+1};
                ExprPtr rhs = parseExprFull(rc);
                if (rhs) {
                    m.assignments.append({lhs, rhs});
                    i = rc.pos;
                    while (i < endIdx && t[i].text != ";") ++i;
                    if (i < endIdx) ++i;
                    continue;
                }
            }
        }
        ++i;
    }
}

bool Parser::parseVhdl(const QVector<Token>& t, IRModule& m,
                       QVector<Diagnostic>& d) const {
    int ent = findToken(t, "entity", 0, HDL::VHDL);
    if (ent < 0) { d.append({Severity::Error,1,1,"Не найдено 'entity'."}); return false; }
    if (ent+1 >= t.size()) {
        d.append({Severity::Error,t[ent].line,t[ent].column,"Ожидается имя сущности."});
        return false;
    }
    m.name = t[ent+1].text;

    int arch = findToken(t, "architecture", ent+2, HDL::VHDL);
    int entityEnd = arch > 0 ? arch : t.size();

    // generic
    int gen = findToken(t, "generic", ent+2, HDL::VHDL);
    if (gen > 0 && gen < entityEnd) parseVhdlGeneric(t, gen, entityEnd, m);

    // port
    int port = findToken(t, "port", ent+2, HDL::VHDL);
    if (port > 0 && port < entityEnd) parseVhdlPorts(t, port, entityEnd, m, d);

    if (arch > 0) {
        // Найти тело архитектуры: от "is" до конца
        int is = findToken(t, "is", arch+2, HDL::VHDL);
        if (is < 0) is = arch+2;
        parseVhdlArchitectureBody(t, is+1, t.size(), m, d);
    }
    return true;
}

// ---- Verilog (оставлен минимальным для совместимости) ----

bool Parser::parseVerilog(const QVector<Token>& t, IRModule& m,
                          QVector<Diagnostic>& d) const {
    int mod = findToken(t, "module", 0, HDL::Verilog);
    if (mod < 0) { d.append({Severity::Error,1,1,"Не найдено 'module'."}); return false; }
    if (mod+1 >= t.size()) { d.append({Severity::Error,t[mod].line,t[mod].column,"Ожидается имя."}); return false; }
    m.name = t[mod+1].text;

    // #( parameters )
    int search = mod+2;
    if (search < t.size() && t[search].text == "#") {
        if (search+1 < t.size() && t[search+1].text == "(") {
            int close = findMatchingParen(t, search+1);
            if (close < 0) close = search+1;
            int q = search+2;
            while (q < close) {
                if (t[q].text == "," || t[q].text == ";") { ++q; continue; }
                if (vhdlEq(t[q].text, "parameter") || vhdlEq(t[q].text, "localparam")) {
                    ++q;
                    if (q < close && (t[q].text == "integer" || t[q].text == "int")) ++q;
                    IRParameter p;
                    if (q < close) { p.name = t[q].text; ++q; }
                    if (q < close && t[q].text == "=") {
                        ++q;
                        QString v;
                        while (q < close && t[q].text != "," && t[q].text != ")") {
                            if (!v.isEmpty()) v += ' ';
                            v += t[q].text;
                            ++q;
                        }
                        p.defaultValue = v;
                    }
                    m.parameters.append(p);
                }
                ++q;
            }
            search = close+1;
        }
    }

    int open = -1, close = -1;
    for (int i = search; i < t.size(); ++i) {
        if (t[i].text == "(") { open = i; close = findMatchingParen(t, i); break; }
        if (t[i].text == ";") break;
    }
    if (open < 0 || close < 0) {
        d.append({Severity::Error,t[mod].line,t[mod].column,"Не найден список портов."});
        return false;
    }

    int i = open+1;
    while (i < close) {
        if (t[i].text == "," || t[i].text == ";") { ++i; continue; }
        PortDirection dir;
        if (!readDirection(t[i].text, HDL::Verilog, dir)) { ++i; continue; }
        ++i;
        QString type = "wire";
        if (i < close && (t[i].text=="wire"||t[i].text=="reg"||t[i].text=="logic"||
                          t[i].text=="bit"||t[i].text=="integer"||t[i].text=="int")) {
            type = t[i].text;
            ++i;
        }
        QString rangeVhdl, rangeVerilog;
        int width = 1;
        if (i < close && t[i].text == "[") {
            int end = -1, dep = 0;
            for (int k = i; k < close; ++k) {
                if (t[k].text=="[") ++dep;
                else if (t[k].text=="]") { --dep; if (dep==0) { end = k; break; } }
            }
            if (end < 0) end = i;
            QString rt = collectRange(t, i, end, HDL::Verilog);
            parseRange(rt, HDL::Verilog, rangeVhdl, rangeVerilog, width);
            i = end + 1;
        }
        QStringList names;
        if (i < close && (t[i].type == TokenType::Identifier ||
                          t[i].type == TokenType::Keyword)) {
            names << t[i].text;
            ++i;
        }
        while (i < close && t[i].text == ",") {
            ++i;
            if (i < close && (t[i].type == TokenType::Identifier ||
                              t[i].type == TokenType::Keyword)) {
                names << t[i].text;
                ++i;
            }
        }
        while (i < close && t[i].text != ";" &&
               !readDirection(t[i].text, HDL::Verilog, *(new PortDirection))) ++i;
        if (i < close && t[i].text == ";") ++i;

        for (const QString& nm : names) {
            IRPort p;
            p.name = nm;
            p.direction = dir;
            p.type = type;
            p.rangeVhdl = rangeVhdl;
            p.rangeVerilog = rangeVerilog;
            p.width = width;
            m.ports.append(p);
        }
    }

    // wire/reg/logic
    for (int k = close+1; k < t.size(); ++k) {
        const QString& kw = t[k].text;
        if (kw=="wire"||kw=="reg"||kw=="logic"||kw=="bit"||kw=="integer"||kw=="int") {
            QString type = kw;
            int p = k+1;
            QString rv, rveril;
            int width = 1;
            if (p < t.size() && t[p].text == "[") {
                int end=-1, dep=0;
                for (int q=p; q<t.size(); ++q) {
                    if (t[q].text=="[") ++dep;
                    else if (t[q].text=="]") { --dep; if (dep==0) { end=q; break; } }
                }
                if (end < 0) end = p;
                QString rt = collectRange(t, p, end, HDL::Verilog);
                parseRange(rt, HDL::Verilog, rv, rveril, width);
                p = end+1;
            }
            while (p < t.size() && t[p].text != ";") {
                if (t[p].type == TokenType::Identifier) {
                    IRSignal s;
                    s.name = t[p].text;
                    s.type = type;
                    s.rangeVhdl = rv;
                    s.rangeVerilog = rveril;
                    s.width = width;
                    m.signalList.append(s);
                }
                ++p;
            }
            k = p;
            continue;
        }
        if (kw == "assign") {
            ExprCtx lc{t, HDL::Verilog, k+1};
            ExprPtr lhs = parsePostfix(lc);
            int eq = lc.pos;
            if (lhs && eq < t.size() && t[eq].text == "=") {
                ExprCtx rc{t, HDL::Verilog, eq+1};
                ExprPtr rhs = parseExprFull(rc);
                if (rhs) {
                    m.assignments.append({lhs, rhs});
                    k = rc.pos;
                    while (k < t.size() && t[k].text != ";") ++k;
                }
            }
        }
    }
    return true;
}

bool Parser::parse(const QString& source, HDL sourceLanguage,
                   IRModule& module, QVector<Diagnostic>& diagnostics) const {
    module = IRModule{};
    Lexer lexer;
    const QVector<Token> tokens = lexer.tokenize(source, sourceLanguage, diagnostics);

    bool ok = false;
    if (sourceLanguage == HDL::VHDL)
        ok = parseVhdl(tokens, module, diagnostics);
    else
        ok = parseVerilog(tokens, module, diagnostics);

    for (const auto& d : diagnostics)
        if (d.severity == Severity::Error) return false;
    return ok;
}