#pragma once
#include <QString>
#include <QVector>
#include <memory>

enum class ExprKind {
    Ident, Literal, Unary, Binary, Index, Slice, Concat, Ternary,
    FunctionCall, Others, Aggregate
};

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct Expr {
    ExprKind kind = ExprKind::Ident;
    QString text;
    QVector<ExprPtr> children;
};

inline ExprPtr mkExpr(ExprKind k) {
    auto e = std::make_shared<Expr>(); e->kind = k; return e;
}
inline ExprPtr mkIdent(const QString& s) {
    auto e = mkExpr(ExprKind::Ident); e->text = s; return e;
}
inline ExprPtr mkLit(const QString& s) {
    auto e = mkExpr(ExprKind::Literal); e->text = s; return e;
}
inline ExprPtr mkUnary(const QString& op, ExprPtr a) {
    auto e = mkExpr(ExprKind::Unary); e->text = op; e->children = {a}; return e;
}
inline ExprPtr mkBinary(const QString& op, ExprPtr a, ExprPtr b) {
    auto e = mkExpr(ExprKind::Binary); e->text = op; e->children = {a, b}; return e;
}
inline ExprPtr mkIndex(ExprPtr a, ExprPtr idx) {
    auto e = mkExpr(ExprKind::Index); e->children = {a, idx}; return e;
}
inline ExprPtr mkSlice(ExprPtr a, ExprPtr hi, ExprPtr lo) {
    auto e = mkExpr(ExprKind::Slice); e->children = {a, hi, lo}; return e;
}
inline ExprPtr mkConcat(const QVector<ExprPtr>& parts) {
    auto e = mkExpr(ExprKind::Concat); e->children = parts; return e;
}
inline ExprPtr mkTernary(ExprPtr c, ExprPtr t, ExprPtr f) {
    auto e = mkExpr(ExprKind::Ternary); e->children = {c, t, f}; return e;
}
inline ExprPtr mkFunc(const QString& name, const QVector<ExprPtr>& args) {
    auto e = mkExpr(ExprKind::FunctionCall); e->text = name; e->children = args; return e;
}
inline ExprPtr mkOthers(const QString& val) {
    auto e = mkExpr(ExprKind::Others); e->text = val; return e;
}