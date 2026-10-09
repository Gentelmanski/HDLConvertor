#pragma once

#include "diagnostic.h"
#include "ir.h"
#include <QString>

class Converter
{
public:
    QString convert(const IRModule& module,
                    HDL source,
                    HDL target,
                    QVector<Diagnostic>& diagnostics) const;

private:
    QString portTypeVhdl(const IRPort& p) const;
    QString portTypeVerilog(const IRPort& p) const;
    QString directionVerilog(PortDirection d) const;
    QString directionVhdl(PortDirection d) const;

    void addCompatibilityDiagnostics(const IRModule& module,
                                     HDL source,
                                     HDL target,
                                     QVector<Diagnostic>& diagnostics) const;
};
