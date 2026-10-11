#pragma once

#include "expr.h"
#include <QString>
#include <QVector>

enum class HDL { VHDL, Verilog, SystemVerilog };
enum class PortDirection { Input, Output, Inout };

struct IRPort {
    QString name;
    PortDirection direction = PortDirection::Input;
    QString type = "logic";     // std_logic / std_logic_vector / wire / reg / logic / integer / ...
    int width = 1;
};

struct IRSignal {
    QString name;
    QString type = "logic";
    int width = 1;
};

struct IRAssignment {
    ExprPtr lhs;
    ExprPtr rhs;
    bool sequential = false;    // true — внутри process/always
};

struct IRProcess {
    HDL kind = HDL::VHDL;       // VHDL process или Verilog always*
    bool clocked = false;       // rising_edge / posedge
    QString clockName;
    QVector<IRAssignment> assignments;
};

struct IRModule {
    QString name;
    QVector<IRPort> ports;
    QVector<IRSignal> signalList;
    QVector<IRAssignment> assignments;    // concurrent
    QVector<IRProcess> processes;
    QVector<QString> unsupportedConstructs;
};