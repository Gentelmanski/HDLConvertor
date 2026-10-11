#pragma once
#include "diagnostic.h"
#include "expr.h"
#include "ir.h"
#include <QString>

class Converter {
public:
    QString convert(const IRModule& module, HDL source, HDL target,
                    QVector<Diagnostic>& diagnostics) const;

private:
    QString emitExpr(const ExprPtr& e, HDL target, HDL source) const;
    QString emitTarget(const ExprPtr& e, HDL target, HDL source) const;
    QString portTypeVhdl(const IRPort& p) const;
    QString portTypeVerilog(const IRPort& p, bool systemVerilog) const;
    QString directionVerilog(PortDirection d) const;
    QString directionVhdl(PortDirection d) const;
    QString canonicalTypeForVerilog(const IRPort& p, bool systemVerilog) const;

    void addCompatibilityDiagnostics(const IRModule& module, HDL source, HDL target,
                                     QVector<Diagnostic>& diagnostics) const;
};