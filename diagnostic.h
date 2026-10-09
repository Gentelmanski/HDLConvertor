#pragma once

#include <QString>

enum class Severity
{
    Info,
    Warning,
    Error
};

struct Diagnostic
{
    Severity severity = Severity::Info;
    int line = 0;
    int column = 0;
    QString message;
};

inline QString severityToString(Severity s)
{
    switch (s) {
    case Severity::Info: return "INFO";
    case Severity::Warning: return "WARNING";
    case Severity::Error: return "ERROR";
    }
    return "INFO";
}
