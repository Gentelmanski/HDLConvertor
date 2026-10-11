#include "parser.h"
#include <QRegularExpression>

namespace {

bool vhdlEq(const QString& a, const QString& b) {
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

// Канонизация оператора исходного языка.
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
};

ExprPtr parsePrimary(ExprCtx& c);
ExprPtr parseUnary(ExprCtx& c);
ExprPtr parseBinary(ExprCtx& c, int minPrec);
ExprPtr parseExprFull(ExprCtx& c);

ExprPtr parsePrimary(ExprCtx& c) {
    if (!c.ok()) return nullptr;

    if (c.isSym("(")) {
        ++c.pos;
        ExprPtr inner = parseExprFull(c);
        if (c.isSym(")")) ++c.pos;
        return inner;
    }

    if (c.lang == HDL::VHDL && c.isSym("'")) {
        // 'event и подобные атрибуты: 'ident
        ++c.pos;
        if (c.ok()) {
            QString attr = c.txt();
            ++c.pos;
            // представляем как идентификатор 'event' — сохраняем как часть имени
            return mkIdent("'" + attr);
        }
        return nullptr;
    }

    const Token& tok = c.t[c.pos];
    if (tok.type == TokenType::Number || tok.type == TokenType::String) {
        ++c.pos;
        return mkLit(tok.text);
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
            ExprPtr a = parseExprFull(c);
            if (c.ok() && (vhdlEq(c.txt(), "downto") || vhdlEq(c.txt(), "to"))) {
                ++c.pos;
                ExprPtr b = parseExprFull(c);
                if (c.isSym(")")) ++c.pos;
                base = mkSlice(base, a, b);
            } else {
                if (c.isSym(")")) ++c.pos;
                base = mkIndex(base, a);
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
        const int prec = precedence(op, c.lang);
        if (prec < minPrec || prec == 0) break;
        ++c.pos;
        ExprPtr rhs = parseBinary(c, prec + 1);
        if (!rhs) break;
        lhs = mkBinary(canonicalOp(op, c.lang), lhs, rhs);
    }
    return lhs;
}

ExprPtr parseExprFull(ExprCtx& c) {
    ExprPtr e = parseBinary(c, 0);

    // Verilog ternary
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

int Parser::findToken(const QVector<Token>& t, const QString& v, int from, HDL lang) const {
    for (int i = from; i < t.size(); ++i) {
        if (lang == HDL::VHDL)
            if (t[i].text.compare(v, Qt::CaseInsensitive) == 0) return i;
            else
                if (t[i].text == v) return i;
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

static bool parseDirection(const QString& s, PortDirection& out, HDL lang) {
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

// Разбор типа и диапазона, начиная с индекса i (включительно).
// Возвращает позицию за последним токеном типа/диапазона.
static int parseTypeAndWidth(const QVector<Token>& t, int i, int close,
                             HDL lang, QString& typeOut, int& widthOut,
                             QVector<QString>* unsupported) {
    typeOut = t[i].text;
    widthOut = 1;
    ++i;

    if (i < close && t[i].text == "(") {
        // VHDL: type(n downto 0) / type(n to 0)
        const int open = i;
        const int end = /* локальный */ [&]{
            int d = 0;
            for (int k = open; k < close; ++k) {
                if (t[k].text=="(") ++d;
                else if (t[k].text==")") { --d; if (d==0) return k; }
            }
            return -1;
        }();
        if (end < 0) { /* не закрывается */ return open; }

        QString range;
        for (int k = open+1; k < end; ++k) range += t[k].text;

        QRegularExpression re(R"((\d+)\s*(?:downto|to)\s*(\d+))",
                              QRegularExpression::CaseInsensitiveOption);
        auto m = re.match(range);
        if (m.hasMatch()) {
            widthOut = qAbs(m.captured(1).toInt() - m.captured(3).toInt()) + 1;
        } else if (unsupported) {
            unsupported->append("Нестандартный диапазон типа: (" + range + ")");
        }
        i = end + 1;
    }
    else if (! (lang == HDL::VHDL) && i < close && t[i].text == "[") {
        // Verilog: type [msb:lsb]
        const int open = i;
        int d = 0, end = -1;
        for (int k = open; k < close; ++k) {
            if (t[k].text=="[") ++d;
            else if (t[k].text=="]") { --d; if (d==0) { end = k; break; } }
        }
        if (end < 0) return open;

        QString range;
        for (int k = open+1; k < end; ++k) range += t[k].text;

        QRegularExpression re(R"((\d+)\s*:\s*(\d+))");
        auto m = re.match(range);
        if (m.hasMatch()) {
            widthOut = qAbs(m.captured(1).toInt() - m.captured(2).toInt()) + 1;
        } else if (unsupported) {
            unsupported->append("Параметризованный диапазон: [" + range + "]");
        }
        i = end + 1;
    }
    return i;
}

// Разбор списка имён, разделённых запятыми.
static QStringList collectNames(const QVector<Token>& t, int& i, int close) {
    QStringList names;
    if (i >= close) return names;
    if (t[i].type != TokenType::Identifier &&
        t[i].type != TokenType::Keyword) return names;
    names << t[i].text;
    ++i;
    while (i < close && t[i].text == ",") {
        ++i;
        if (i >= close) break;
        if (t[i].type != TokenType::Identifier &&
            t[i].type != TokenType::Keyword) break;
        names << t[i].text;
        ++i;
    }
    return names;
}

bool Parser::parseVhdl(const QVector<Token>& t, IRModule& m,
                       QVector<Diagnostic>& d) const
{
    const int ent = findToken(t, "entity", 0, HDL::VHDL);
    if (ent < 0) { d.append({Severity::Error,1,1,"Не найдено 'entity'."}); return false; }
    if (ent+1 >= t.size()) { d.append({Severity::Error,t[ent].line,t[ent].column,"Ожидается имя сущности."}); return false; }
    m.name = t[ent+1].text;

    const int port = findToken(t, "port", ent+2, HDL::VHDL);
    if (port >= 0) {
        const int open = findToken(t, "(", port+1, HDL::VHDL);
        if (open < 0) { d.append({Severity::Error,t[port].line,t[port].column,"Ожидается '(' после 'port'."}); return false; }
        const int close = findMatchingParen(t, open);
        if (close < 0) { d.append({Severity::Error,t[open].line,t[open].column,"Незакрытый список портов."}); return false; }

        int i = open+1;
        while (i < close) {
            if (t[i].text == ";" || t[i].text == ",") { ++i; continue; }

            QStringList names = collectNames(t, i, close);
            if (names.isEmpty()) { ++i; continue; }

            if (i >= close || t[i].text != ":") {
                d.append({Severity::Error, t[i<close?i:close].line, t[i<close?i:close].column,
                          "Ожидается ':' после имени порта."});
                return false;
            }
            ++i;

            PortDirection dir;
            if (i >= close || !parseDirection(t[i].text, dir, HDL::VHDL)) {
                d.append({Severity::Error, t[i<close?i:close].line, t[i<close?i:close].column,
                          "Ожидается направление порта (in/out/inout)."});
                return false;
            }
            ++i;

            if (i >= close) {
                d.append({Severity::Error, t[close].line, t[close].column,
                          "Ожидается тип порта."});
                return false;
            }

            QString type; int width = 1;
            i = parseTypeAndWidth(t, i, close, HDL::VHDL, type, width, &m.unsupportedConstructs);

            // Пропустить до ';' или следующего имени
            while (i < close && t[i].text != ";") ++i;
            if (i < close) ++i;

            for (const QString& nm : names)
                m.ports.append({nm, dir, type, width});
        }
    } else {
        d.append({Severity::Error, t[ent].line, t[ent].column,
                  "В VHDL-сущности отсутствует раздел 'port'."});
    }

    // Внутренние сигналы.
    for (int i = 0; i < t.size(); ++i) {
        if (!vhdlEq(t[i].text, "signal")) continue;
        if (i+2 >= t.size()) break;
        int k = i+1;
        QStringList names;
        while (k < t.size() && t[k].text != ":") {
            if (t[k].type == TokenType::Identifier) names << t[k].text;
            ++k;
        }
        if (k >= t.size()) break;
        ++k;
        if (k >= t.size()) break;

        QString type; int width = 1;
        int end = k;
        while (end < t.size() && t[end].text != ";") ++end;
        if (end >= t.size()) end = t.size()-1;

        parseTypeAndWidth(t, k, end+1, HDL::VHDL, type, width, nullptr);
        for (const QString& nm : names)
            m.signalList.append({nm, type, width});
        i = end;
    }

    // Простые concurrent-присваивания на верхнем уровне архитектуры.
    const int beginIdx = findToken(t, "begin", 0, HDL::VHDL);
    const int endArch  = findToken(t, "end", 0, HDL::VHDL);
    const int stop     = endArch > beginIdx ? endArch : t.size();
    const int start    = beginIdx >= 0 ? beginIdx+1 : 0;

    // Отмечаем индексы токенов, входящих в process.
    QVector<bool> inProcess(t.size(), false);
    for (int i = start; i < stop; ++i) {
        if (!vhdlEq(t[i].text, "process")) continue;
        int endP = i+1;
        while (endP < stop && !vhdlEq(t[endP].text, "end")) ++endP;
        if (endP+1 < t.size() && vhdlEq(t[endP+1].text, "process")) endP += 1;
        for (int k = i; k <= endP && k < t.size(); ++k) inProcess[k] = true;
    }

    for (int i = start; i < stop; ++i) {
        if (inProcess[i]) continue;

        if (t[i].type == TokenType::Identifier || t[i].type == TokenType::Keyword) {
            // Возможен target с индексом/срезом.
            ExprCtx lhsCtx{t, HDL::VHDL, i};
            ExprPtr lhs = parsePostfix(lhsCtx);
            int afterLhs = lhsCtx.pos;

            if (lhs && afterLhs < stop && t[afterLhs].text == "<=") {
                ExprCtx rhsCtx{t, HDL::VHDL, afterLhs+1};
                ExprPtr rhs = parseExprFull(rhsCtx);
                if (rhs) {
                    m.assignments.append({lhs, rhs, false});
                    i = rhsCtx.pos - 1;
                    continue;
                }
            }
        }
    }

    // Процессы.
    for (int i = start; i < stop; ++i) {
        if (!vhdlEq(t[i].text, "process")) continue;
        IRProcess proc;
        proc.kind = HDL::VHDL;
        int k = i+1;
        if (k < stop && t[k].text == "(") {
            int end = findMatchingParen(t, k);
            if (end < 0) end = k;
            for (int q = k+1; q < end; ++q)
                if (t[q].type == TokenType::Identifier) proc.clockName = t[q].text;
            k = end+1;
        }
        // Тело: от begin до end process
        while (k < stop && !vhdlEq(t[k].text, "begin")) ++k;
        ++k;
        int endP = k;
        while (endP < stop && !vhdlEq(t[endP].text, "end")) ++endP;

        for (int q = k; q < endP; ++q) {
            if (t[q].type != TokenType::Identifier && t[q].type != TokenType::Keyword)
                continue;
            ExprCtx lhsCtx{t, HDL::VHDL, q};
            ExprPtr lhs = parsePostfix(lhsCtx);
            int afterLhs = lhsCtx.pos;
            if (lhs && afterLhs < endP && t[afterLhs].text == "<=") {
                ExprCtx rhsCtx{t, HDL::VHDL, afterLhs+1};
                ExprPtr rhs = parseExprFull(rhsCtx);
                if (rhs) {
                    proc.assignments.append({lhs, rhs, true});
                    // Проверка rising_edge/falling_edge в RHS
                    if (rhs->kind == ExprKind::Index &&
                        rhs->children.size()==1 &&
                        rhs->children[0]->kind == ExprKind::Ident) {
                        const QString fn = rhs->children[0]->text.toLower();
                        if (fn=="rising_edge" || fn=="falling_edge") {
                            proc.clocked = true;
                        }
                    }
                    q = rhsCtx.pos - 1;
                }
            }
        }
        if (!proc.assignments.isEmpty() || !proc.clockName.isEmpty())
            m.processes.append(proc);
        i = endP;
    }

    if (findToken(t, "generic", 0, HDL::VHDL) >= 0) {
        m.unsupportedConstructs.append("VHDL generic распознан, но не хранится в IR.");
    }
    return true;
}

bool Parser::parseVerilog(const QVector<Token>& t, IRModule& m,
                          QVector<Diagnostic>& d) const
{
    const int mod = findToken(t, "module", 0, HDL::Verilog);
    if (mod < 0) { d.append({Severity::Error,1,1,"Не найдено 'module'."}); return false; }
    if (mod+1 >= t.size()) { d.append({Severity::Error,t[mod].line,t[mod].column,"Ожидается имя модуля."}); return false; }
    m.name = t[mod+1].text;

    int search = mod+2;
    if (search < t.size() && t[search].text == "#") {
        if (search+1 < t.size() && t[search+1].text == "(") {
            int close = findMatchingParen(t, search+1);
            if (close < 0) { d.append({Severity::Error,t[search].line,t[search].column,"Незакрытый список параметров."}); return false; }
            m.unsupportedConstructs.append("Параметры module #( ... ) распознаны, но не хранятся в IR.");
            search = close+1;
        }
    }

    int open = -1, close = -1;
    for (int i = search; i < t.size(); ++i) {
        if (t[i].text == "(") { open = i; close = findMatchingParen(t, i); break; }
        if (t[i].text == ";") break;
    }
    if (open < 0 || close < 0) {
        d.append({Severity::Error,t[mod].line,t[mod].column,"Не найден список портов '(...)'."});
        return false;
    }

    // Разбор ANSI/NON-ANSI стиля порта: direction [type] [range] name[, name]*;
    // Мы обрабатываем токены между open и close, группируя по ';' и/или по ключевым
    // словам направления.
    int i = open+1;
    while (i < close) {
        if (t[i].text == "," || t[i].text == ";") { ++i; continue; }

        PortDirection dir;
        if (!parseDirection(t[i].text, dir, HDL::Verilog)) { ++i; continue; }
        const int dirTok = i;
        ++i;

        QString type = "wire";
        if (i < close && (t[i].text=="wire" || t[i].text=="reg" ||
                          t[i].text=="logic" || t[i].text=="bit" ||
                          t[i].text=="signed" || t[i].text=="integer" ||
                          t[i].text=="int")) {
            type = t[i].text;
            ++i;
        }

        int width = 1;
        if (i < close && t[i].text == "[") {
            int end = -1, dep = 0;
            for (int k = i; k < close; ++k) {
                if (t[k].text=="[") ++dep;
                else if (t[k].text=="]") { --dep; if (dep==0) { end = k; break; } }
            }
            if (end < 0) {
                d.append({Severity::Error,t[i].line,t[i].column,"Незакрытый диапазон '[...]'."});
                return false;
            }
            QString range;
            for (int k = i+1; k < end; ++k) range += t[k].text;
            QRegularExpression re(R"((\d+)\s*:\s*(\d+))");
            auto mt = re.match(range);
            if (mt.hasMatch())
                width = qAbs(mt.captured(1).toInt() - mt.captured(2).toInt()) + 1;
            else
                m.unsupportedConstructs.append("Параметризованный диапазон: [" + range + "]");
            i = end+1;
        }

        QStringList names = collectNames(t, i, close);
        if (names.isEmpty()) {
            d.append({Severity::Error,t[dirTok].line,t[dirTok].column,"Ожидается имя порта."});
            return false;
        }
        for (const QString& nm : names)
            m.ports.append({nm, dir, type, width});

        // пропустить до ';' или следующего direction
        while (i < close && t[i].text != ";" &&
               !parseDirection(t[i].text, *(new PortDirection), HDL::Verilog))
            ++i;
        if (i < close && t[i].text == ";") ++i;
    }

    // Тело модуля.
    for (int k = close+1; k < t.size(); ++k) {
        const QString& kw = t[k].text;

        // wire/reg/logic declarations
        if (kw=="wire"||kw=="reg"||kw=="logic"||kw=="bit"||
            kw=="integer"||kw=="int") {
            QString type = kw;
            int p = k+1;
            int width = 1;
            if (p < t.size() && t[p].text == "[") {
                int end=-1, dep=0;
                for (int q = p; q < t.size(); ++q) {
                    if (t[q].text=="[") ++dep;
                    else if (t[q].text=="]") { --dep; if(dep==0){end=q;break;} }
                }
                if (end<0) { d.append({Severity::Error,t[p].line,t[p].column,"Незакрытый диапазон."}); return false; }
                QString range; for (int q=p+1;q<end;++q) range += t[q].text;
                QRegularExpression re(R"((\d+)\s*:\s*(\d+))");
                auto mt = re.match(range);
                if (mt.hasMatch()) width = qAbs(mt.captured(1).toInt()-mt.captured(2).toInt())+1;
                p = end+1;
            }
            while (p < t.size() && t[p].text != ";") {
                if (t[p].type == TokenType::Identifier || t[p].type == TokenType::Keyword)
                    m.signalList.append({t[p].text, type, width});
                ++p;
            }
            k = p;
            continue;
        }

        // assign
        if (kw == "assign") {
            ExprCtx lhsCtx{t, HDL::Verilog, k+1};
            ExprPtr lhs = parsePostfix(lhsCtx);
            int eq = lhsCtx.pos;
            if (lhs && eq < t.size() && t[eq].text == "=") {
                ExprCtx rhsCtx{t, HDL::Verilog, eq+1};
                ExprPtr rhs = parseExprFull(rhsCtx);
                if (!rhs || rhsCtx.pos >= t.size() || t[rhsCtx.pos].text != ";") {
                    d.append({Severity::Error,t[k].line,t[k].column,
                              "В assign отсутствует ';' или некорректное выражение."});
                    return false;
                }
                m.assignments.append({lhs, rhs, false});
                k = rhsCtx.pos;
                continue;
            }
            d.append({Severity::Error,t[k].line,t[k].column,
                      "Некорректный assign: ожидалось 'lhs = expr;'."});
            continue;
        }

        // always / always_ff / always_comb / always_latch / initial
        if (kw=="always"||kw=="always_ff"||kw=="always_comb"||
            kw=="always_latch"||kw=="initial") {
            IRProcess proc;
            proc.kind = HDL::Verilog;
            int p = k+1;
            // @(...)
            if (p < t.size() && t[p].text == "@") {
                ++p;
                if (p < t.size() && t[p].text == "(") {
                    int end = findMatchingParen(t, p);
                    if (end > 0) {
                        for (int q = p+1; q < end; ++q) {
                            if (t[q].text=="posedge"||t[q].text=="negedge") {
                                proc.clocked = true;
                                if (q+1 < end) proc.clockName = t[q+1].text;
                            }
                        }
                        p = end+1;
                    }
                }
            }
            while (p < t.size() && t[p].text != "begin") ++p;
            if (p < t.size()) ++p;

            int endP = p;
            int dep = 1;
            while (endP < t.size() && dep > 0) {
                if (t[endP].text == "begin") ++dep;
                else if (t[endP].text == "end") { --dep; if (dep==0) break; }
                ++endP;
            }

            for (int q = p; q < endP; ++q) {
                if (t[q].type != TokenType::Identifier && t[q].type != TokenType::Keyword) continue;
                ExprCtx lc{t, HDL::Verilog, q};
                ExprPtr lhs = parsePostfix(lc);
                int eq = lc.pos;
                if (lhs && eq < endP && t[eq].text == "<=") {
                    ExprCtx rc{t, HDL::Verilog, eq+1};
                    ExprPtr rhs = parseExprFull(rc);
                    if (rhs) {
                        proc.assignments.append({lhs, rhs, true});
                        q = rc.pos-1;
                    }
                } else if (lhs && eq < endP && t[eq].text == "=") {
                    ExprCtx rc{t, HDL::Verilog, eq+1};
                    ExprPtr rhs = parseExprFull(rc);
                    if (rhs) {
                        proc.assignments.append({lhs, rhs, true});
                        q = rc.pos-1;
                    }
                }
            }
            if (!proc.assignments.isEmpty() || proc.clocked)
                m.processes.append(proc);
            k = endP;
            continue;
        }
    }

    // Отчёт о нераспознанных конструкциях.
    for (int k = close+1; k < t.size(); ++k) {
        if (t[k].type == TokenType::Keyword) {
            static const QSet<QString> known = {
                "wire","reg","logic","bit","integer","int",
                "assign","always","always_ff","always_comb","always_latch",
                "initial","begin","end","if","else","case","endcase",
                "default","parameter","localparam","module","endmodule",
                "input","output","inout","posedge","negedge","generate",
                "endgenerate","genvar","function","endfunction","task","endtask"
            };
            if (!known.contains(t[k].text))
                m.unsupportedConstructs.append("Нераспознанная конструкция: " + t[k].text);
        }
    }

    return true;
}

bool Parser::parse(const QString& source, HDL sourceLanguage,
                   IRModule& module, QVector<Diagnostic>& diagnostics) const
{
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