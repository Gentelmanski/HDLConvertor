#pragma once
#include "expr.h"
#include <QString>
#include <QVector>
#include <QPair>
#include <memory>

enum class HDL { VHDL, Verilog, SystemVerilog };
enum class PortDirection { Input, Output, Inout };

struct IRParameter {
    QString name;
    QString defaultValue;
    QString type = "integer";
};

struct IRPort {
    QString name;
    PortDirection direction = PortDirection::Input;
    QString type = "logic";
    QString rangeVhdl;      // "WIDTH-1 downto 0", если параметризовано
    QString rangeVerilog;   // "[WIDTH-1:0]"
    int width = 1;          // запасная ширина (для литеральных диапазонов)
};

struct IRSignal {
    QString name;
    QString type = "logic";
    QString rangeVhdl;
    QString rangeVerilog;
    int width = 1;
};

// ---- Дерево операторов внутри процесса ----
struct IRStmt;
using StmtPtr = std::shared_ptr<IRStmt>;

enum class StmtKind { Assign, If, Case };

struct IRCaseItem {
    QVector<ExprPtr> choices;    // пусто → default
    QVector<StmtPtr> stmts;
};

struct IRStmt {
    StmtKind kind = StmtKind::Assign;

    // Assign
    ExprPtr lhs;
    ExprPtr rhs;
    bool sequential = false;     // true для <=

    // If
    ExprPtr cond;
    QVector<StmtPtr> thenStmts;
    QVector<QPair<ExprPtr, QVector<StmtPtr>>> elsifStmts;
    QVector<StmtPtr> elseStmts;

    // Case
    ExprPtr caseExpr;
    QVector<IRCaseItem> caseItems;
};

struct IRProcess {
    HDL kind = HDL::VHDL;
    bool clocked = false;
    QString clockName;
    bool risingEdge = true;
    QVector<StmtPtr> body;
};

struct IRInstance {
    QString label;
    QString moduleName;
    QVector<QPair<QString, QString>> portMap;  // formal → actual
};

// with/select (или concurrent case) на верхнем уровне архитектуры
struct IRCaseStatement {
    ExprPtr selectExpr;
    ExprPtr target;
    QVector<QPair<QVector<ExprPtr>, ExprPtr>> items;  // choices → value
    ExprPtr defaultExpr;                              // when others
};

struct IRModule {
    QString name;
    QVector<IRParameter> parameters;
    QVector<IRPort> ports;
    QVector<IRSignal> signalList;

    // concurrent assign и with/select
    QVector<QPair<ExprPtr, ExprPtr>> assignments;
    QVector<IRCaseStatement> withSelect;

    QVector<IRInstance> instances;
    QVector<IRProcess> processes;
    QVector<QString> unsupportedConstructs;
};