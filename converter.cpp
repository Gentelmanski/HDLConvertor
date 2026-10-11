#include "converter.h"
#include <functional>

QString Converter::directionVerilog(PortDirection d) const {
    switch (d) {
    case PortDirection::Input:  return "input";
    case PortDirection::Output: return "output";
    case PortDirection::Inout:  return "inout";
    }
    return "input";
}
QString Converter::directionVhdl(PortDirection d) const {
    switch (d) {
    case PortDirection::Input:  return "in";
    case PortDirection::Output: return "out";
    case PortDirection::Inout:  return "inout";
    }
    return "in";
}

QString Converter::portTypeVhdl(const IRPort& p) const {
    if (!p.rangeVhdl.isEmpty()) return "std_logic_vector" + p.rangeVhdl;
    if (p.width <= 1) return "std_logic";
    return QString("std_logic_vector(%1 downto 0)").arg(p.width - 1);
}

static QString opFor(HDL target, const QString& canon) {
    if (target == HDL::VHDL) {
        if (canon=="AND")  return "and";
        if (canon=="OR")   return "or";
        if (canon=="XOR")  return "xor";
        if (canon=="NAND") return "nand";
        if (canon=="NOR")  return "nor";
        if (canon=="XNOR") return "xnor";
        if (canon=="NOT")  return "not";
        if (canon=="LNOT") return "not";
        if (canon=="EQ")   return "=";
        if (canon=="NEQ")  return "/=";
        if (canon=="LT")   return "<";
        if (canon=="LE")   return "<=";
        if (canon=="GT")   return ">";
        if (canon=="GE")   return ">=";
        if (canon=="ADD")  return "+";
        if (canon=="SUB")  return "-";
        if (canon=="MUL")  return "*";
        if (canon=="DIV")  return "/";
        if (canon=="MOD")  return "mod";
        if (canon=="REM")  return "rem";
        if (canon=="POW")  return "**";
    } else {
        if (canon=="AND")  return "&";
        if (canon=="OR")   return "|";
        if (canon=="XOR")  return "^";
        if (canon=="NAND") return "~&";
        if (canon=="NOR")  return "~|";
        if (canon=="XNOR") return "~^";
        if (canon=="NOT")  return "~";
        if (canon=="LNOT") return "!";
        if (canon=="LAND") return "&&";
        if (canon=="LOR")  return "||";
        if (canon=="EQ")   return "==";
        if (canon=="NEQ")  return "!=";
        if (canon=="LT")   return "<";
        if (canon=="LE")   return "<=";
        if (canon=="GT")   return ">";
        if (canon=="GE")   return ">=";
        if (canon=="ADD")  return "+";
        if (canon=="SUB")  return "-";
        if (canon=="MUL")  return "*";
        if (canon=="DIV")  return "/";
        if (canon=="MOD")  return "%";
        if (canon=="POW")  return "**";
        if (canon=="SHL")  return "<<";
        if (canon=="SHR")  return ">>";
    }
    return canon;
}

// Функции-касты, которые нужно просто «снять».
static bool isTypeCast(const QString& name) {
    const QString n = name.toLower();
    return n == "std_logic_vector" || n == "std_logic" || n == "unsigned"
           || n == "signed" || n == "bit_vector" || n == "integer"
           || n == "to_unsigned" || n == "to_signed" || n == "conv_std_logic_vector";
}

QString Converter::emitExpr(const ExprPtr& e, HDL target) const {
    if (!e) return QString();
    switch (e->kind) {
    case ExprKind::Ident:   return e->text;

    case ExprKind::Literal: {
        const QString s = e->text;
        // VHDL character literal '0'/'1'/'Z'/'X'
        if (s.size() == 3 && s.startsWith('\'') && s.endsWith('\'')) {
            const QChar c = s[1].toLower();
            if (target == HDL::VHDL) return "'" + QString(c) + "'";
            if (c=='0') return "1'b0";
            if (c=='1') return "1'b1";
            if (c=='z') return "1'bz";
            if (c=='x') return "1'bx";
            return "1'b0";
        }
        // VHDL bit-string "00" → Verilog 2'b00
        if (target != HDL::VHDL && !s.isEmpty() && s[0].isDigit()) {
            // Если это чисто число — оставляем
            return s;
        }
        // VHDL "00" (с двоичными значениями) — целевой Verilog → 2'b00
        if (target != HDL::VHDL && !s.isEmpty()) {
            bool allBits = true;
            for (QChar ch : s)
                if (ch!='0'&&ch!='1'&&ch!='z'&&ch!='Z'&&ch!='x'&&ch!='X'&&ch!='-')
                { allBits = false; break; }
            if (allBits && s.size() > 0)
                return QString("%1'b%2").arg(s.size()).arg(s);
        }
        // VHDL x"FF"
        if (target != HDL::VHDL && s.size() > 1 && s[1] == '"') {
            const QChar base = s[0].toLower();
            QString digits = s.mid(2);
            if (digits.endsWith('"')) digits.chop(1);
            if (base=='x') return "'h" + digits;
            if (base=='b') return "'b" + digits;
            if (base=='o') return "'o" + digits;
            if (base=='d') return "'d" + digits;
        }
        return s;
    }

    case ExprKind::Unary: {
        QString op = opFor(target, e->text);
        QString operand = emitExpr(e->children[0], target);
        if (op == "not" || op == "~" || op == "!")
            return QString("%1%2").arg(op, operand);
        return QString("%1%2").arg(op, operand);
    }

    case ExprKind::Binary: {
        QString op = opFor(target, e->text);
        QString a = emitExpr(e->children[0], target);
        QString b = emitExpr(e->children[1], target);
        if (e->text == "CONCAT" && target != HDL::VHDL) {
            // На всякий случай (если пришло сюда)
            return "{" + a + ", " + b + "}";
        }
        return QString("(%1 %2 %3)").arg(a, op, b);
    }

    case ExprKind::Index: {
        const QString a = emitExpr(e->children[0], target);
        const QString idx = emitExpr(e->children[1], target);
        if (target == HDL::VHDL) return QString("%1(%2)").arg(a, idx);
        return QString("%1[%2]").arg(a, idx);
    }

    case ExprKind::Slice: {
        const QString a = emitExpr(e->children[0], target);
        const QString hi = emitExpr(e->children[1], target);
        const QString lo = emitExpr(e->children[2], target);
        if (target == HDL::VHDL) return QString("%1(%2 downto %3)").arg(a, hi, lo);
        return QString("%1[%2:%3]").arg(a, hi, lo);
    }

    case ExprKind::Concat: {
        QStringList parts;
        for (auto& ch : e->children) parts << emitExpr(ch, target);
        if (target == HDL::VHDL) return parts.join(" & ");
        return "{" + parts.join(", ") + "}";
    }

    case ExprKind::Ternary: {
        const QString c  = emitExpr(e->children[0], target);
        const QString tv = emitExpr(e->children[1], target);
        const QString fv = emitExpr(e->children[2], target);
        if (target == HDL::VHDL)
            return QString("%1 when %2 else %3").arg(tv, c, fv);
        return QString("(%1 ? %2 : %3)").arg(c, tv, fv);
    }

    case ExprKind::FunctionCall: {
        if (isTypeCast(e->text)) {
            if (e->children.size() >= 1) return emitExpr(e->children[0], target);
            return QString();
        }
        QStringList args;
        for (auto& a : e->children) args << emitExpr(a, target);
        return QString("%1(%2)").arg(e->text, args.join(", "));
    }

    case ExprKind::Others: {
        if (target != HDL::VHDL)
            return "'0"; // упрощённо
        return "(others => '0')";
    }

    case ExprKind::Aggregate: {
        QStringList parts;
        for (auto& ch : e->children) parts << emitExpr(ch, target);
        return "{" + parts.join(", ") + "}";
    }
    }
    return QString();
}

// ---- Дерево операторов процесса ----

static QString indentLines(const QString& s, const QString& pad) {
    QStringList lines = s.split('\n', Qt::SkipEmptyParts);
    for (auto& l : lines) l = pad + l;
    return lines.join('\n');
}

QString emitStmt(const StmtPtr& s, HDL target, int depth);

QString emitStmts(const QVector<StmtPtr>& list, HDL target, int depth) {
    QString pad(depth * 4, ' ');
    QString out;
    for (const auto& s : list) out += emitStmt(s, target, depth);
    return out;
}

QString emitStmt(const StmtPtr& s, HDL target, int depth) {
    if (!s) return {};
    QString pad(depth * 4, ' ');
    Converter conv;

    if (s->kind == StmtKind::Assign) {
        const QString lhs = conv.emitExpr(s->lhs, target);
        const QString rhs = conv.emitExpr(s->rhs, target);
        const QString op = s->sequential
                               ? (target == HDL::VHDL ? "<=" : "<=")
                               : (target == HDL::VHDL ? ":=" : "=");
        return pad + QString("%1 %2 %3;\n").arg(lhs, op, rhs);
    }
    if (s->kind == StmtKind::If) {
        QString out = pad + "if (" + conv.emitExpr(s->cond, target) + ") begin\n";
        out += emitStmts(s->thenStmts, target, depth+1);
        for (auto& p : s->elsifStmts) {
            out += pad + "end else if (" + conv.emitExpr(p.first, target) + ") begin\n";
            out += emitStmts(p.second, target, depth+1);
        }
        if (!s->elseStmts.isEmpty()) {
            out += pad + "end else begin\n";
            out += emitStmts(s->elseStmts, target, depth+1);
        }
        out += pad + "end\n";
        return out;
    }
    if (s->kind == StmtKind::Case) {
        QString out = pad + "case (" + conv.emitExpr(s->caseExpr, target) + ")\n";
        for (auto& ci : s->caseItems) {
            if (ci.choices.isEmpty()) {
                out += pad + "    default: begin\n";
            } else {
                QStringList labels;
                for (auto& ch : ci.choices) labels << conv.emitExpr(ch, target);
                out += pad + "    " + labels.join(", ") + ": begin\n";
            }
            out += emitStmts(ci.stmts, target, depth+2);
            out += pad + "    end\n";
        }
        out += pad + "endcase\n";
        return out;
    }
    return {};
}

// ---- Диагностика ----

void Converter::addCompatibilityDiagnostics(const IRModule& m,
                                            HDL /*s*/, HDL t,
                                            QVector<Diagnostic>& d) const {
    for (const QString& w : m.unsupportedConstructs)
        d.append({Severity::Warning, 0, 0, "Ограничение преобразования: " + w});

    if (t == HDL::VHDL)
        d.append({Severity::Info, 0, 0,
                  "Генерация VHDL выполняется через ограниченное переносимое IR."});
    if (t == HDL::SystemVerilog)
        d.append({Severity::Info, 0, 0,
                  "SystemVerilog поддерживается как расширение Verilog-совместимого генератора."});
}

// ---- Порт / сигнал ----

static QString verilogPortDecl(const IRPort& p, bool sv) {
    QString kind = sv ? "logic" : "wire";
    if (!sv && p.direction == PortDirection::Output) {
        if (p.type.compare("reg", Qt::CaseInsensitive) == 0) kind = "reg";
    }
    QString range;
    if (!p.rangeVerilog.isEmpty()) range = " " + p.rangeVerilog;
    else if (p.width > 1) range = QString(" [%1:0]").arg(p.width-1);
    return QString("%1 %2%3").arg(kind, p.name, range);
}

// ---- Основной вывод ----

QString Converter::convert(const IRModule& m, HDL source, HDL target,
                           QVector<Diagnostic>& d) const
{
    if (source == target) {
        d.append({Severity::Warning, 0, 0, "Исходный и целевой языки совпадают."});
        return QString();
    }
    addCompatibilityDiagnostics(m, source, target, d);

    QString out;
    const bool sv = (target == HDL::SystemVerilog);

    // ---------- VHDL ----------
    if (target == HDL::VHDL) {
        out += "library ieee;\n";
        out += "use ieee.std_logic_1164.all;\n";
        out += "use ieee.numeric_std.all;\n\n";
        out += QString("entity %1 is\n").arg(m.name);
        if (!m.parameters.isEmpty()) {
            out += "    generic (\n";
            for (int i = 0; i < m.parameters.size(); ++i) {
                const auto& p = m.parameters[i];
                out += QString("        %1 : %2 := %3")
                           .arg(p.name, p.type.isEmpty() ? "integer" : p.type,
                                p.defaultValue);
                if (i+1 < m.parameters.size()) out += ";";
                out += "\n";
            }
            out += "    );\n";
        }
        if (!m.ports.isEmpty()) {
            out += "    port (\n";
            for (int i = 0; i < m.ports.size(); ++i) {
                const auto& p = m.ports[i];
                QString type = "std_logic";
                if (!p.rangeVhdl.isEmpty()) type = "std_logic_vector" + p.rangeVhdl;
                else if (p.width > 1)
                    type = QString("std_logic_vector(%1 downto 0)").arg(p.width-1);
                out += QString("        %1 : %2 %3")
                           .arg(p.name, directionVhdl(p.direction), type);
                if (i+1 < m.ports.size()) out += ";";
                out += "\n";
            }
            out += "    );\n";
        }
        out += QString("end entity %1;\n\n").arg(m.name);
        out += QString("architecture rtl of %1 is\n").arg(m.name);
        for (const auto& s : m.signalList) {
            QString type = "std_logic";
            if (!s.rangeVhdl.isEmpty()) type = "std_logic_vector" + s.rangeVhdl;
            else if (s.width > 1)
                type = QString("std_logic_vector(%1 downto 0)").arg(s.width-1);
            out += QString("    signal %1 : %2;\n").arg(s.name, type);
        }
        out += "begin\n";

        for (auto& a : m.assignments) {
            out += QString("    %1 <= %2;\n")
            .arg(emitExpr(a.first, HDL::VHDL),
                 emitExpr(a.second, HDL::VHDL));
        }
        for (auto& cs : m.withSelect) {
            out += QString("    with %1 select\n")
            .arg(emitExpr(cs.selectExpr, HDL::VHDL));
            out += QString("        %1 <= ").arg(emitExpr(cs.target, HDL::VHDL));
            for (int i = 0; i < cs.items.size(); ++i) {
                QStringList chs;
                for (auto& c : cs.items[i].first)
                    chs << emitExpr(c, HDL::VHDL);
                out += QString("%1 when %2")
                           .arg(emitExpr(cs.items[i].second, HDL::VHDL),
                                chs.join(" | "));
                out += (i+1 < cs.items.size() ? ",\n                " : ",\n");
            }
            out += QString("%1 when others;\n").arg(emitExpr(cs.defaultExpr, HDL::VHDL));
        }
        for (auto& inst : m.instances) {
            out += QString("    %1 : entity work.%2\n")
            .arg(inst.label, inst.moduleName);
            out += "        port map (\n";
            for (int i = 0; i < inst.portMap.size(); ++i) {
                out += QString("            %1 => %2")
                .arg(inst.portMap[i].first, inst.portMap[i].second);
                if (i+1 < inst.portMap.size()) out += ",";
                out += "\n";
            }
            out += "        );\n";
        }
        for (const auto& p : m.processes) {
            if (p.clocked && !p.clockName.isEmpty())
                out += QString("    process(%1)\n").arg(p.clockName);
            else
                out += "    process\n";
            out += "    begin\n";
            out += emitStmts(p.body, HDL::VHDL, 2);
            out += "    end process;\n";
        }
        out += "end architecture rtl;\n";
        return out;
    }

    // ---------- Verilog / SystemVerilog ----------
    out += sv ? "// Generated SystemVerilog\n" : "// Generated Verilog-2001\n";
    out += QString("module %1").arg(m.name);

    if (!m.parameters.isEmpty()) {
        out += " #(\n";
        for (int i = 0; i < m.parameters.size(); ++i) {
            const auto& p = m.parameters[i];
            out += QString("    parameter %1 = %2")
                       .arg(p.name, p.defaultValue.isEmpty() ? "0" : p.defaultValue);
            if (i+1 < m.parameters.size()) out += ",";
            out += "\n";
        }
        out += ")";
    }
    out += " (\n";
    for (int i = 0; i < m.ports.size(); ++i) {
        const auto& p = m.ports[i];
        out += "    " + QString("%1 %2").arg(directionVerilog(p.direction),
                                             verilogPortDecl(p, sv));
        if (i+1 < m.ports.size()) out += ",";
        out += "\n";
    }
    out += ");\n\n";

    for (const auto& s : m.signalList) {
        QString kind = sv ? "logic" : "wire";
        if (!sv && s.type.compare("reg", Qt::CaseInsensitive) == 0) kind = "reg";
        QString range;
        if (!s.rangeVerilog.isEmpty()) range = " " + s.rangeVerilog;
        else if (s.width > 1) range = QString(" [%1:0]").arg(s.width-1);
        out += QString("%1%2 %3;\n").arg(kind, range, s.name);
    }
    if (!m.signalList.isEmpty()) out += "\n";

    for (auto& a : m.assignments) {
        out += QString("assign %1 = %2;\n")
        .arg(emitExpr(a.first, target), emitExpr(a.second, target));
    }

    for (auto& cs : m.withSelect) {
        out += "always @(*) begin\n";
        out += QString("    case (%1)\n").arg(emitExpr(cs.selectExpr, target));
        for (auto& it : cs.items) {
            QStringList chs;
            for (auto& c : it.first) chs << emitExpr(c, target);
            out += QString("        %1: %2 = %3;\n")
                       .arg(chs.join(", "),
                            emitExpr(cs.target, target),
                            emitExpr(it.second, target));
        }
        if (cs.defaultExpr)
            out += QString("        default: %1 = %2;\n")
                       .arg(emitExpr(cs.target, target),
                            emitExpr(cs.defaultExpr, target));
        out += "    endcase\n";
        out += "end\n";
    }

    for (auto& inst : m.instances) {
        out += QString("%1 %2 (").arg(inst.moduleName, inst.label);
        QStringList conns;
        for (auto& p : inst.portMap)
            conns << QString(".%1(%2)").arg(p.first, p.second);
        out += conns.join(", ");
        out += ");\n";
    }

    for (const auto& p : m.processes) {
        QString header;
        if (p.clocked && !p.clockName.isEmpty())
            header = sv ? QString("always_ff @(posedge %1)").arg(p.clockName)
                        : QString("always @(posedge %1)").arg(p.clockName);
        else
            header = sv ? "always_comb" : "always @(*)";
        out += header + " begin\n";
        out += emitStmts(p.body, target, 1);
        out += "end\n";
    }

    out += "\nendmodule\n";
    return out;
}