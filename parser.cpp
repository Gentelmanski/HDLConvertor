#include "parser.h"

#include <QRegularExpression>

namespace {

bool isDirection(const QString& text)
{
    const QString v = text.toLower();
    return v == "input" || v == "output" || v == "inout";
}

PortDirection toDirection(const QString& text)
{
    const QString v = text.toLower();
    if (v == "output")
        return PortDirection::Output;
    if (v == "inout")
        return PortDirection::Inout;
    return PortDirection::Input;
}

bool isTypeKeyword(const QString& text)
{
    const QString v = text.toLower();
    return v == "wire" || v == "reg" || v == "logic" || v == "bit"
           || v == "signed" || v == "integer" || v == "int";
}

}

QString Parser::joinTokens(const QVector<Token>& tokens, int from, int to) const
{
    QString result;
    for (int i = from; i < to && i < tokens.size(); ++i) {
        if (!result.isEmpty())
            result += ' ';
        result += tokens[i].text;
    }
    return result;
}

int Parser::findToken(const QVector<Token>& tokens,
                      const QString& value,
                      int start) const
{
    for (int i = start; i < tokens.size(); ++i) {
        if (tokens[i].text.compare(value, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

bool Parser::parse(const QString& source,
                   HDL sourceLanguage,
                   IRModule& module,
                   QVector<Diagnostic>& diagnostics) const
{
    module = IRModule{};

    Lexer lexer;
    const QVector<Token> tokens = lexer.tokenize(source, diagnostics);

    bool ok = false;
    if (sourceLanguage == HDL::VHDL)
        ok = parseVhdl(tokens, module, diagnostics);
    else
        ok = parseVerilog(tokens, module, diagnostics);

    for (const auto& d : diagnostics) {
        if (d.severity == Severity::Error)
            return false;
    }

    return ok;
}

bool Parser::parseVhdl(const QVector<Token>& tokens,
                       IRModule& module,
                       QVector<Diagnostic>& diagnostics) const
{
    const int entity = findToken(tokens, "entity", 0);
    if (entity < 0) {
        diagnostics.append({
            Severity::Error, 1, 1,
            "Не найдено ключевое слово VHDL 'entity'."
        });
        return false;
    }

    if (entity + 1 >= tokens.size()
        || tokens[entity + 1].type == TokenType::EndOfFile) {
        diagnostics.append({
            Severity::Error, tokens[entity].line, tokens[entity].column,
            "После 'entity' ожидается имя сущности."
        });
        return false;
    }

    module.name = tokens[entity + 1].text;

    const int port = findToken(tokens, "port", entity + 2);
    if (port >= 0) {
        const int open = findToken(tokens, "(", port + 1);
        const int close = open >= 0 ? findToken(tokens, ")", open + 1) : -1;

        if (open < 0 || close < 0) {
            diagnostics.append({
                Severity::Error,
                tokens[port].line,
                tokens[port].column,
                "Некорректное объявление port: ожидаются '(' и ')'."
            });
            return false;
        }

        int i = open + 1;
        while (i < close) {
            // Skip separators.
            if (tokens[i].text == ";" || tokens[i].text == ",") {
                ++i;
                continue;
            }

            if (tokens[i].type != TokenType::Identifier) {
                diagnostics.append({
                    Severity::Error,
                    tokens[i].line,
                    tokens[i].column,
                    "Ожидается имя VHDL-порта."
                });
                return false;
            }

            QStringList names;
            names << tokens[i].text;
            ++i;

            while (i < close && tokens[i].text == ",") {
                ++i;
                if (i >= close || tokens[i].type != TokenType::Identifier) {
                    diagnostics.append({
                        Severity::Error,
                        i < close ? tokens[i].line : tokens[close].line,
                        i < close ? tokens[i].column : tokens[close].column,
                        "После ',' ожидается имя порта."
                    });
                    return false;
                }
                names << tokens[i].text;
                ++i;
            }

            if (i >= close || tokens[i].text != ":") {
                diagnostics.append({
                    Severity::Error,
                    i > 0 ? tokens[i - 1].line : tokens[open].line,
                    i > 0 ? tokens[i - 1].column : tokens[open].column,
                    "Ожидается ':' после имени порта."
                });
                return false;
            }
            ++i;

            if (i >= close) {
                diagnostics.append({
                    Severity::Error,
                    tokens[close].line,
                    tokens[close].column,
                    "После ':' ожидается направление порта."
                });
                return false;
            }

            PortDirection direction;
            const QString dir = tokens[i].text.toLower();
            if (dir == "in")
                direction = PortDirection::Input;
            else if (dir == "out")
                direction = PortDirection::Output;
            else if (dir == "inout")
                direction = PortDirection::Inout;
            else {
                diagnostics.append({
                    Severity::Error,
                    tokens[i].line,
                    tokens[i].column,
                    "Неизвестное направление порта: " + tokens[i].text
                });
                return false;
            }
            ++i;

            if (i >= close) {
                diagnostics.append({
                    Severity::Error,
                    tokens[close].line,
                    tokens[close].column,
                    "После направления порта ожидается тип."
                });
                return false;
            }

            const QString type = tokens[i].text;
            ++i;
            int width = 1;

            if (i < close && tokens[i].text == "(") {
                const int rangeOpen = i;
                ++i;
                QString range;
                while (i < close && tokens[i].text != ")") {
                    range += tokens[i].text;
                    ++i;
                }

                if (i >= close) {
                    diagnostics.append({
                        Severity::Error,
                        tokens[rangeOpen].line,
                        tokens[rangeOpen].column,
                        "Незакрытый диапазон типа VHDL."
                    });
                    return false;
                }
                ++i;

                QRegularExpression re(
                    R"((\d+)\s*(downto|to)\s*(\d+))",
                    QRegularExpression::CaseInsensitiveOption);
                const auto match = re.match(range);
                if (match.hasMatch()) {
                    const int a = match.captured(1).toInt();
                    const int b = match.captured(3).toInt();
                    width = qAbs(a - b) + 1;
                } else {
                    module.unsupportedConstructs.append(
                        "Нестандартный диапазон VHDL-порта: " + range);
                }
            }

            while (i < close && tokens[i].text != ";")
                ++i;
            if (i < close)
                ++i;

            for (const QString& name : names)
                module.ports.append({name, direction, type, width});
        }
    }

    // Internal VHDL signals.
    for (int i = 0; i + 2 < tokens.size(); ++i) {
        if (tokens[i].text.compare("signal", Qt::CaseInsensitive) != 0)
            continue;

        ++i;
        QStringList names;
        while (i < tokens.size() && tokens[i].text != ":") {
            if (tokens[i].type == TokenType::Identifier)
                names << tokens[i].text;
            ++i;
        }
        if (i >= tokens.size())
            break;
        ++i;

        if (i >= tokens.size())
            break;

        const QString type = tokens[i].text;
        ++i;
        int width = 1;

        if (i < tokens.size() && tokens[i].text == "(") {
            ++i;
            QString range;
            while (i < tokens.size() && tokens[i].text != ")") {
                range += tokens[i].text;
                ++i;
            }
            if (i < tokens.size())
                ++i;
            QRegularExpression re(
                R"((\d+)\s*(downto|to)\s*(\d+))",
                QRegularExpression::CaseInsensitiveOption);
            const auto match = re.match(range);
            if (match.hasMatch())
                width = qAbs(match.captured(1).toInt()
                             - match.captured(3).toInt()) + 1;
        }

        while (i < tokens.size() && tokens[i].text != ";")
            ++i;

        for (const QString& name : names)
            module.signalList.append({name, type, width});
    }

    // Simple concurrent assignments: a <= b;
    for (int i = 0; i + 2 < tokens.size(); ++i) {
        if ((tokens[i].type == TokenType::Identifier
             || tokens[i].type == TokenType::Keyword)
            && tokens[i + 1].text == "<=") {
            QString rhs;
            int j = i + 2;
            while (j < tokens.size() && tokens[j].text != ";") {
                if (!rhs.isEmpty())
                    rhs += ' ';
                rhs += tokens[j].text;
                ++j;
            }
            if (j < tokens.size())
                module.assignments.append({tokens[i].text, rhs, false});
        }
    }

    if (findToken(tokens, "process", 0) >= 0) {
        module.unsupportedConstructs.append(
            "VHDL process распознан, но его тело пока не преобразуется в IR-процесс.");
        const int processToken = findToken(tokens, "process", 0);
        diagnostics.append({
            Severity::Warning,
            tokens[processToken].line,
            tokens[processToken].column,
            "VHDL process поддерживается частично."
        });
    }

    if (findToken(tokens, "generic", 0) >= 0) {
        module.unsupportedConstructs.append(
            "generic VHDL распознан, но параметры пока не хранятся в IR.");
        const int genericToken = findToken(tokens, "generic", 0);
        diagnostics.append({
            Severity::Warning,
            tokens[genericToken].line,
            tokens[genericToken].column,
            "VHDL generic поддерживается частично."
        });
    }

    return true;
}

bool Parser::parseVerilog(const QVector<Token>& tokens,
                          IRModule& module,
                          QVector<Diagnostic>& diagnostics) const
{
    const int moduleToken = findToken(tokens, "module", 0);
    if (moduleToken < 0) {
        diagnostics.append({
            Severity::Error, 1, 1,
            "Не найдено ключевое слово 'module'."
        });
        return false;
    }

    if (moduleToken + 1 >= tokens.size()
        || tokens[moduleToken + 1].type == TokenType::EndOfFile) {
        diagnostics.append({
            Severity::Error,
            tokens[moduleToken].line,
            tokens[moduleToken].column,
            "После 'module' ожидается имя модуля."
        });
        return false;
    }

    module.name = tokens[moduleToken + 1].text;

    int search = moduleToken + 2;

    // SystemVerilog/Verilog parameter list: module name #( ... ) ( ... );
    if (search < tokens.size() && tokens[search].text == "#") {
        const int paramOpen = search + 1;
        if (paramOpen >= tokens.size() || tokens[paramOpen].text != "(") {
            diagnostics.append({
                Severity::Error,
                tokens[search].line,
                tokens[search].column,
                "После '#' ожидается список параметров '(...)'."
            });
            return false;
        }

        const int paramClose = findToken(tokens, ")", paramOpen + 1);
        if (paramClose < 0) {
            diagnostics.append({
                Severity::Error,
                tokens[paramOpen].line,
                tokens[paramOpen].column,
                "Незакрытый список параметров модуля."
            });
            return false;
        }

        module.unsupportedConstructs.append(
            "Параметры module #( ... ) распознаны, но пока не хранятся в IR.");
        diagnostics.append({
            Severity::Warning,
            tokens[search].line,
            tokens[search].column,
            "Параметры module поддерживаются частично."
        });

        search = paramClose + 1;
    }

    const int open = findToken(tokens, "(", search);
    const int close = open >= 0 ? findToken(tokens, ")", open + 1) : -1;

    if (open < 0 || close < 0) {
        diagnostics.append({
            Severity::Error,
            tokens[moduleToken].line,
            tokens[moduleToken].column,
            "Не найден список портов модуля '(...)'."
        });
        return false;
    }

    int i = open + 1;
    while (i < close) {
        if (tokens[i].text == ",") {
            ++i;
            continue;
        }

        if (!isDirection(tokens[i].text)) {
            ++i;
            continue;
        }

        const PortDirection dir = toDirection(tokens[i].text);
        ++i;

        QString type = "wire";
        if (i < close && isTypeKeyword(tokens[i].text)) {
            type = tokens[i].text;
            ++i;
        }

        int width = 1;
        if (i < close && tokens[i].text == "[") {
            const int rangeOpen = i;
            ++i;

            const QString left = i < close ? tokens[i].text : QString();
            ++i;
            if (i < close && tokens[i].text == ":")
                ++i;
            const QString right = i < close ? tokens[i].text : QString();
            ++i;

            if (i >= close || tokens[i].text != "]") {
                diagnostics.append({
                    Severity::Error,
                    tokens[rangeOpen].line,
                    tokens[rangeOpen].column,
                    "Незакрытый диапазон порта '[...]'."
                });
                return false;
            }
            ++i;

            bool okLeft = false;
            bool okRight = false;
            const int a = left.toInt(&okLeft);
            const int b = right.toInt(&okRight);
            if (okLeft && okRight)
                width = qAbs(a - b) + 1;
            else
                module.unsupportedConstructs.append(
                    "Параметризованный диапазон порта: [" + left + ":" + right + "]");
        }

        if (i >= close || tokens[i].type == TokenType::EndOfFile) {
            diagnostics.append({
                Severity::Error,
                tokens[close].line,
                tokens[close].column,
                "После направления/типа ожидается имя порта."
            });
            return false;
        }

        if (tokens[i].type != TokenType::Identifier
            && tokens[i].type != TokenType::Keyword) {
            diagnostics.append({
                Severity::Error,
                tokens[i].line,
                tokens[i].column,
                "Ожидается имя порта."
            });
            return false;
        }

        module.ports.append({tokens[i].text, dir, type, width});
        ++i;

        // ANSI-style declaration may continue with another name after comma.
        // For the starter parser each declaration is treated separately.
    }

    // Internal wire/reg/logic declarations.
    for (int k = close + 1; k + 1 < tokens.size(); ++k) {
        const QString keyword = tokens[k].text.toLower();
        if (keyword != "wire" && keyword != "reg" && keyword != "logic")
            continue;

        QString type = tokens[k].text;
        int p = k + 1;
        int width = 1;

        if (p < tokens.size() && tokens[p].text == "[") {
            ++p;
            const QString left = p < tokens.size() ? tokens[p].text : QString();
            ++p;
            if (p < tokens.size() && tokens[p].text == ":")
                ++p;
            const QString right = p < tokens.size() ? tokens[p].text : QString();
            ++p;
            if (p < tokens.size() && tokens[p].text == "]")
                ++p;

            bool okLeft = false;
            bool okRight = false;
            const int a = left.toInt(&okLeft);
            const int b = right.toInt(&okRight);
            if (okLeft && okRight)
                width = qAbs(a - b) + 1;
        }

        if (p < tokens.size()
            && tokens[p].type != TokenType::EndOfFile
            && tokens[p].text != ";") {
            module.signalList.append({tokens[p].text, type, width});
        }
    }

    // Continuous assignments.
    for (int k = 0; k + 2 < tokens.size(); ++k) {
        if (tokens[k].text.compare("assign", Qt::CaseInsensitive) == 0
            && k + 2 < tokens.size()
            && tokens[k + 2].text == "=") {
            const QString lhs = tokens[k + 1].text;
            QString rhs;
            int j = k + 3;
            while (j < tokens.size() && tokens[j].text != ";") {
                if (!rhs.isEmpty())
                    rhs += ' ';
                rhs += tokens[j].text;
                ++j;
            }

            if (j >= tokens.size()) {
                diagnostics.append({
                    Severity::Error,
                    tokens[k].line,
                    tokens[k].column,
                    "У assign отсутствует ';'."
                });
                return false;
            }

            module.assignments.append({lhs, rhs, false});
        }
    }

    // Sequential/behavioral blocks are recognized but intentionally not lowered
    // into the small IR yet. This is a warning, not a lexer/syntax error.
    const int always = findToken(tokens, "always", 0);
    const int alwaysFf = findToken(tokens, "always_ff", 0);
    const int alwaysComb = findToken(tokens, "always_comb", 0);

    if (always >= 0 || alwaysFf >= 0 || alwaysComb >= 0) {
        int first = always;
        if (first < 0 || (alwaysFf >= 0 && tokens[alwaysFf].line < tokens[first].line))
            first = alwaysFf;
        if (first < 0 || (alwaysComb >= 0 && tokens[alwaysComb].line < tokens[first].line))
            first = alwaysComb;

        module.unsupportedConstructs.append(
            "always/always_ff/always_comb распознаны, но их тело пока не преобразуется в IR.");
        diagnostics.append({
            Severity::Warning,
            tokens[first].line,
            tokens[first].column,
            "always/always_ff/always_comb поддерживаются частично."
        });
    }

    return true;
}
