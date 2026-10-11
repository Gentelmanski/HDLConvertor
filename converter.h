#pragma once
#include "diagnostic.h"
#include "expr.h"
#include "ir.h"
#include <QString>

class Converter {
public:
    QString convert(const IRModule& module, HDL source, HDL target,
                    QVector<Diagnostic>& diagnostics) const;

    QString emitExpr(const ExprPtr& e, HDL target) const;

private:
    QString directionVerilog(PortDirection d) const;
    QString directionVhdl(PortDirection d) const;
    QString portTypeVhdl(const IRPort& p) const;

    void addCompatibilityDiagnostics(const IRModule& module, HDL source, HDL target,
                                     QVector<Diagnostic>& diagnostics) const;
};