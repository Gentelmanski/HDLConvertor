#pragma once

#include <QString>
#include <QVector>

enum class HDL
{
    VHDL,
    Verilog,
    SystemVerilog
};

enum class PortDirection
{
    Input,
    Output,
    Inout
};

struct IRPort
{
    QString name;
    PortDirection direction = PortDirection::Input;
    QString type;
    int width = 1;
};

struct IRSignal
{
    QString name;
    QString type;
    int width = 1;
};

struct IRAssignment
{
    QString lhs;
    QString rhs;
    bool sequential = false;
};

struct IRProcess
{
    QVector<QString> sensitivityList;
    QString condition;
    QVector<IRAssignment> assignments;
};

struct IRModule
{
    QString name;
    QVector<IRPort> ports;
    QVector<IRSignal> signalList;
    QVector<IRAssignment> assignments;
    QVector<IRProcess> processes;

    QVector<QString> unsupportedConstructs;
};
