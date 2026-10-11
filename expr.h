#pragma once
#include <QString>
#include <QVector>
#include <memory>

enum class ExprKind {
    Ident, Literal, Unary, Binary, Index, Slice, Concat, Ternary
};

struct Expr;
using ExprPtr = std::shared_ptr<Expr>;

struct Expr {
    ExprKind kind = ExprKind::Ident;
    QString text;                 // идентификатор, литерал или канонический оператор
    QVector<ExprPtr> children;
};

inline ExprPtr mkIdent(const QString& s) {
    auto e = std::make_shared<Expr>(); e->kind = ExprKind::Ident; e->text = s; return e;
}
inline ExprPtr mkLit(const QString& s) {
    auto e = std::make_shared<Expr>(); e->kind = ExprKind::Literal; e->text = s; return e;
}
inline ExprPtr mkUnary(const QString& op, ExprPtr a) {
    auto e = std::make_shared<Expr>(); e->kind = ExprKind::Unary;
    e->text = op; e->children = {a}; return e;
}
inline ExprPtr mkBinary(const QString& op, ExprPtr a, ExprPtr b) {
    auto e = std::make_shared<Expr>(); e->kind = ExprKind::Binary;
    e->text = op; e->children = {a, b}; return e;
}
inline ExprPtr mkIndex(ExprPtr a, ExprPtr idx) {
    auto e = std::make_shared<Expr>(); e->kind = ExprKind::Index;
    e->children = {a, idx}; return e;
}
inline ExprPtr mkSlice(ExprPtr a, ExprPtr hi, ExprPtr lo) {
    auto e = std::make_shared<Expr>(); e->kind = ExprKind::Slice;
    e->children = {a, hi, lo}; return e;
}
inline ExprPtr mkConcat(const QVector<ExprPtr>& parts) {
    auto e = std::make_shared<Expr>(); e->kind = ExprKind::Concat;
    e->children = parts; return e;
}
inline ExprPtr mkTernary(ExprPtr c, ExprPtr t, ExprPtr f) {
    auto e = std::make_shared<Expr>(); e->kind = ExprKind::Ternary;
    e->children = {c, t, f}; return e;
}