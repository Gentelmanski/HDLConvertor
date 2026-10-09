#include "converter.h"

QString Converter::directionVerilog(PortDirection d) const
{
    switch (d) {
    case PortDirection::Input: return "input";
    case PortDirection::Output: return "output";
    case PortDirection::Inout: return "inout";
    }
    return "input";
}

QString Converter::directionVhdl(PortDirection d) const
{
    switch (d) {
    case PortDirection::Input: return "in";
    case PortDirection::Output: return "out";
    case PortDirection::Inout: return "inout";
    }
    return "in";
}

QString Converter::portTypeVhdl(const IRPort& p) const
{
    if (p.width <= 1)
        return "std_logic";

    return QString("std_logic_vector(%1 downto 0)")
        .arg(p.width - 1);
}

QString Converter::portTypeVerilog(const IRPort& p) const
{
    if (p.width <= 1)
        return "logic";

    return QString("logic [%1:0]").arg(p.width - 1);
}

void Converter::addCompatibilityDiagnostics(
    const IRModule& module,
    HDL source,
    HDL target,
    QVector<Diagnostic>& diagnostics) const
{
    Q_UNUSED(source);

    if (!module.unsupportedConstructs.isEmpty()) {
        for (const QString& text : module.unsupportedConstructs) {
            diagnostics.append({
                Severity::Warning, 0, 0,
                "Ограничение преобразования: " + text
            });
        }
    }

    if (target == HDL::VHDL) {
        diagnostics.append({
            Severity::Info, 0, 0,
            "Генерация VHDL выполняется через ограниченное переносимое IR."
        });
    }

    if (target == HDL::SystemVerilog) {
        diagnostics.append({
            Severity::Info, 0, 0,
            "SystemVerilog поддерживается как расширение Verilog-совместимого генератора."
        });
    }
}

QString Converter::convert(const IRModule& module,
                           HDL source,
                           HDL target,
                           QVector<Diagnostic>& diagnostics) const
{
    if (source == target) {
        diagnostics.append({
            Severity::Warning, 0, 0,
            "Исходный и целевой языки совпадают."
        });
        return QString();
    }

    addCompatibilityDiagnostics(module, source, target, diagnostics);

    QString out;

    if (target == HDL::VHDL) {
        out += "library ieee;\n";
        out += "use ieee.std_logic_1164.all;\n\n";
        out += QString("entity %1 is\n").arg(module.name);
        out += "    port(\n";

        for (int i = 0; i < module.ports.size(); ++i) {
            const auto& p = module.ports[i];
            out += QString("        %1 : %2 %3")
                       .arg(p.name)
                       .arg(directionVhdl(p.direction))
                       .arg(portTypeVhdl(p));

            if (i + 1 < module.ports.size())
                out += ";";
            out += "\n";
        }

        out += "    );\n";
        out += QString("end entity %1;\n\n").arg(module.name);
        out += QString("architecture rtl of %1 is\n").arg(module.name);

        for (const auto& signal : module.signalList) {
            if (signal.width <= 1) {
                out += QString("    signal %1 : std_logic;\n").arg(signal.name);
            } else {
                out += QString("    signal %1 : std_logic_vector(%2 downto 0);\n")
                .arg(signal.name)
                    .arg(signal.width - 1);
            }
        }

        out += "begin\n";

        for (const auto& a : module.assignments) {
            out += QString("    %1 <= %2;\n").arg(a.lhs, a.rhs);
        }

        out += "end architecture rtl;\n";
        return out;
    }

    const bool systemVerilog = target == HDL::SystemVerilog;

    if (systemVerilog)
        out += "// Generated SystemVerilog\n";
    else
        out += "// Generated Verilog-2001\n";

    out += QString("module %1 (\n").arg(module.name);

    for (int i = 0; i < module.ports.size(); ++i) {
        const auto& p = module.ports[i];

        out += QString("    %1 %2 %3")
                   .arg(directionVerilog(p.direction))
                   .arg(systemVerilog ? "logic" : "wire")
                   .arg(p.width > 1
                            ? QString("[%1:0] %2")
                                  .arg(p.width - 1).arg(p.name)
                            : p.name);

        if (i + 1 < module.ports.size())
            out += ",";
        out += "\n";
    }

    out += ");\n\n";

    for (const auto& signal : module.signalList) {
        const QString kind = systemVerilog ? "logic" : "wire";
        if (signal.width <= 1) {
            out += QString("%1 %2;\n").arg(kind, signal.name);
        } else {
            out += QString("%1 [%2:0] %3;\n")
            .arg(kind)
                .arg(signal.width - 1)
                .arg(signal.name);
        }
    }

    if (!module.signalList.isEmpty())
        out += "\n";

    for (const auto& a : module.assignments) {
        out += QString("assign %1 = %2;\n").arg(a.lhs, a.rhs);
    }

    out += "\nendmodule\n";
    return out;
}
