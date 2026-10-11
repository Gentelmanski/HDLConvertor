#include "converter.h"

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
    if (p.width <= 1) return "std_logic";
    return QString("std_logic_vector(%1 downto 0)").arg(p.width - 1);
}
QString Converter::portTypeVerilog(const IRPort& p, bool sv) const {
    // Сохраняем reg, если исходный был reg или sequential
    QString kind = sv ? "logic" : "wire";
    if (!sv) {
        if (p.direction == PortDirection::Output &&
            p.type.compare("reg", Qt::CaseInsensitive) == 0)
            kind = "reg";
        else if (p.type.compare("reg", Qt::CaseInsensitive) == 0)
            kind = "reg";
        else if (p.type.compare("logic", Qt::CaseInsensitive) == 0 ||
                 p.type.compare("bit",   Qt::CaseInsensitive) == 0)
            kind = "wire";
    }
    if (p.width <= 1) return kind;
    return QString("%1 [%2:0]").arg(kind).arg(p.width - 1);
}

// ---- Выражения ----

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
        if (canon=="ABS")  return "abs";
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
        if (canon=="CONCAT") return "&";
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

QString Converter::emitExpr(const ExprPtr& e, HDL target, HDL /*source*/) const {
    if (!e) return QString();
    switch (e->kind) {
    case ExprKind::Ident:   return e->text;
    case ExprKind::Literal: {
        // VHDL bit-string literal x"FF" —> Verilog 'hFF
        if (target != HDL::VHDL && e->text.size() > 1 && e->text[1] == '"') {
            const QChar base = e->text[0].toLower();
            QString digits = e->text.mid(2);
            if (digits.endsWith('"')) digits.chop(1);
            if (base=='x') return "'h" + digits;
            if (base=='b') return "'b" + digits;
            if (base=='o') return "'o" + digits;
            if (base=='d') return "'d" + digits;
        }
        // Verilog based literal —> VHDL bit-string
        if (target == HDL::VHDL && e->text.contains('\'')) {
            int q = e->text.indexOf('\'');
            QString bits = e->text.left(q);
            QString rest = e->text.mid(q+1);
            QChar base = rest.isEmpty() ? 'd' : rest[0].toLower();
            if (base=='s' && rest.size()>1) { base = rest[1].toLower(); rest = rest.mid(1); }
            QString digits = rest.mid(1);
            if (base=='h') return "x\"" + digits + "\"";
            if (base=='b') return "b\"" + digits + "\"";
            if (base=='o') return "o\"" + digits + "\"";
            return digits;
        }
        return e->text;
    }
    case ExprKind::Unary: {
        QString op = opFor(target, e->text);
        QString operand = emitExpr(e->children[0], target, target);
        // VHDL: not a, abs a — префиксный
        return QString("%1 %2").arg(op, operand);
    }
    case ExprKind::Binary: {
        if (e->text == "CONCAT" && target != HDL::VHDL) {
            QStringList parts;
            for (auto& ch : e->children) parts << emitExpr(ch, target, target);
            return "{" + parts.join(", ") + "}";
        }
        QString op = opFor(target, e->text);
        return QString("(%1 %2 %3)")
            .arg(emitExpr(e->children[0], target, target), op,
                 emitExpr(e->children[1], target, target));
    }
    case ExprKind::Index: {
        return QString("%1(%2)").arg(emitExpr(e->children[0], target, target),
                                     emitExpr(e->children[1], target, target));
    }
    case ExprKind::Slice: {
        const QString a = emitExpr(e->children[0], target, target);
        const QString hi = emitExpr(e->children[1], target, target);
        const QString lo = emitExpr(e->children[2], target, target);
        if (target == HDL::VHDL)
            return QString("%1(%2 downto %3)").arg(a, hi, lo);
        return QString("%1[%2:%3]").arg(a, hi, lo);
    }
    case ExprKind::Concat: {
        QStringList parts;
        for (auto& ch : e->children) parts << emitExpr(ch, target, target);
        if (target == HDL::VHDL) return parts.join(" & ");
        return "{" + parts.join(", ") + "}";
    }
    case ExprKind::Ternary: {
        const QString c = emitExpr(e->children[0], target, target);
        const QString tv = emitExpr(e->children[1], target, target);
        const QString fv = emitExpr(e->children[2], target, target);
        if (target == HDL::VHDL) {
            // VHDL не имеет ?: — эмулируем через when/else нельзя внутри выражения,
            // поэтому оставляем диагностическое предупреждение, а сам примитив выражаем
            // через and/or (a and b) or (not a and c).
            return QString("((%1 and %2) or (not %3 and %4))").arg(c, tv, c, fv);
        }
        return QString("(%1 ? %2 : %3)").arg(c, tv, fv);
    }
    }
    return QString();
}

QString Converter::emitTarget(const ExprPtr& e, HDL target, HDL source) const {
    return emitExpr(e, target, source);
}

// ---- Диагностика ----

void Converter::addCompatibilityDiagnostics(const IRModule& m, HDL /*s*/,
                                            HDL t, QVector<Diagnostic>& d) const {
    for (const QString& w : m.unsupportedConstructs)
        d.append({Severity::Warning, 0, 0, "Ограничение преобразования: " + w});

    if (t == HDL::VHDL)
        d.append({Severity::Info, 0, 0,
                  "Генерация VHDL выполняется через ограниченное переносимое IR."});
    if (t == HDL::SystemVerilog)
        d.append({Severity::Info, 0, 0,
                  "SystemVerilog поддерживается как расширение Verilog-совместимого генератора."});

    for (const auto& p : m.processes) {
        if (p.clocked)
            d.append({Severity::Info, 0, 0,
                      QString("Обнаружен тактируемый процесс по сигналу '%1'.")
                          .arg(p.clockName)});
    }
}

// ---- Основной вывод ----

QString Converter::convert(const IRModule& m, HDL source, HDL target,
                           QVector<Diagnostic>& d) const
{
    if (source == target) {
        d.append({Severity::Warning, 0, 0, "Исходный и целевой языки совпадают."});
        return QString();
    }
    // Verilog <-> SystemVerilog: тривиальная (по IR совпадают), предупреждаем.
    if ((source == HDL::Verilog && target == HDL::SystemVerilog) ||
        (source == HDL::SystemVerilog && target == HDL::Verilog)) {
        d.append({Severity::Info, 0, 0,
                  "Verilog ↔ SystemVerilog: IR общий, различия только в ключевых словах."});
    }

    addCompatibilityDiagnostics(m, source, target, d);

    QString out;

    if (target == HDL::VHDL) {
        out += "library ieee;\n";
        out += "use ieee.std_logic_1164.all;\n\n";
        out += QString("entity %1 is\n").arg(m.name);
        if (!m.ports.isEmpty()) {
            out += "    port(\n";
            for (int i = 0; i < m.ports.size(); ++i) {
                const auto& p = m.ports[i];
                out += QString("        %1 : %2 %3")
                           .arg(p.name, directionVhdl(p.direction), portTypeVhdl(p));
                if (i+1 < m.ports.size()) out += ";";
                out += "\n";
            }
            out += "    );\n";
        }
        out += QString("end entity %1;\n\n").arg(m.name);
        out += QString("architecture rtl of %1 is\n").arg(m.name);

        for (const auto& s : m.signalList) {
            if (s.width <= 1)
                out += QString("    signal %1 : std_logic;\n").arg(s.name);
            else
                out += QString("    signal %1 : std_logic_vector(%2 downto 0);\n")
                           .arg(s.name).arg(s.width - 1);
        }
        out += "begin\n";

        for (const auto& a : m.assignments) {
            out += QString("    %1 <= %2;\n")
            .arg(emitTarget(a.lhs, HDL::VHDL, source),
                 emitTarget(a.rhs, HDL::VHDL, source));
        }

        for (const auto& p : m.processes) {
            if (p.clocked && !p.clockName.isEmpty())
                out += QString("    process(%1)\n").arg(p.clockName);
            else
                out += "    process\n";
            out += "    begin\n";
            for (const auto& a : p.assignments) {
                out += QString("        %1 <= %2;\n")
                .arg(emitTarget(a.lhs, HDL::VHDL, source),
                     emitTarget(a.rhs, HDL::VHDL, source));
            }
            out += "    end process;\n";
        }

        out += "end architecture rtl;\n";
        return out;
    }

    const bool sv = (target == HDL::SystemVerilog);
    out += sv ? "// Generated SystemVerilog\n" : "// Generated Verilog-2001\n";
    out += QString("module %1 (\n").arg(m.name);

    for (int i = 0; i < m.ports.size(); ++i) {
        const auto& p = m.ports[i];
        out += QString("    %1 %2 %3")
                   .arg(directionVerilog(p.direction),
                        portTypeVerilog(p, sv),
                        p.name);
        if (i+1 < m.ports.size()) out += ",";
        out += "\n";
    }
    if (!m.ports.isEmpty()) out += ");\n\n";
    else out += ");\n\n";

    for (const auto& s : m.signalList) {
        QString kind = sv ? "logic" : "wire";
        if (!sv && (s.type.compare("reg", Qt::CaseInsensitive) == 0))
            kind = "reg";
        else if (sv && s.type.compare("reg", Qt::CaseInsensitive) == 0)
            kind = "logic";
        if (s.width <= 1)
            out += QString("%1 %2;\n").arg(kind, s.name);
        else
            out += QString("%1 [%2:0] %3;\n").arg(kind).arg(s.width-1).arg(s.name);
    }
    if (!m.signalList.isEmpty()) out += "\n";

    for (const auto& a : m.assignments) {
        out += QString("assign %1 = %2;\n")
        .arg(emitTarget(a.lhs, target, source),
             emitTarget(a.rhs, target, source));
    }

    for (const auto& p : m.processes) {
        QString header;
        if (p.clocked && !p.clockName.isEmpty())
            header = sv ? "always_ff @(posedge " + p.clockName + ")"
                        : "always @(posedge " + p.clockName + ")";
        else
            header = sv ? "always_comb" : "always @(*)";
        out += header + " begin\n";
        for (const auto& a : p.assignments) {
            const QString op = p.clocked ? "<=" : "=";
            out += QString("    %1 %2 %3;\n")
                       .arg(emitTarget(a.lhs, target, source), op,
                            emitTarget(a.rhs, target, source));
        }
        out += "end\n";
    }

    out += "\nendmodule\n";
    return out;
}