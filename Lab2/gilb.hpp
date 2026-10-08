#pragma once
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace gilb {
struct Token { std::string text; int line; };
struct Branch { int line; std::string kind; int level; };
struct Result {
    int operators = 0, absolute = 0, nesting = 0;
    std::vector<Branch> branches;
    double relative() const { return operators ? double(absolute) / operators : 0.0; }
};
inline bool oneOf(const std::string& s, const std::vector<std::string>& values) {
    return std::find(values.begin(), values.end(), s) != values.end();
}
inline void fail(int line, const std::string& message) {
    throw std::runtime_error("Строка " + std::to_string(line) + ": " + message);
}
// Лексер скрывает содержимое строк и комментариев от структурного парсера.
// Переводы строк сохраняются, поскольку в Groovy можно не писать точку с запятой.
inline std::vector<Token> lex(const std::string& source) {
    std::vector<Token> out;
    size_t i = 0;
    int line = 1;
    auto add = [&](std::string s) { out.push_back({s, line}); };
    auto starts = [&](const std::string& s) { return source.compare(i, s.size(), s) == 0; };
    if (source.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3;
    while (i < source.size()) {
        char c = source[i];
        if (c == '\r' || c == ' ' || c == '\t') { ++i; continue; }
        if (c == '\n') { add("\n"); ++line; ++i; continue; }
        if (c == '\\' && i + 1 < source.size() &&
            (source[i + 1] == '\n' || source[i + 1] == '\r')) {
            ++i;
            if (source[i] == '\r') ++i;
            if (i < source.size() && source[i] == '\n') { ++i; ++line; }
            continue;
        }
        if (starts("//") || (i == 0 && starts("#!"))) {
            while (i < source.size() && source[i] != '\n') ++i;
            continue;
        }
        if (starts("/*")) {
            int begin = line;
            i += 2;
            while (i < source.size() && !starts("*/")) {
                if (source[i] == '\n') { add("\n"); ++line; }
                ++i;
            }
            if (i == source.size()) fail(begin, "не закрыт комментарий.");
            i += 2;
            continue;
        }
        bool dollarSlash = starts("$/");
        bool slash = false;
        if (c == '/' && !starts("/=")) {
            size_t n = out.size();
            while (n && out[n - 1].text == "\n") --n;
            slash = !n || oneOf(out[n - 1].text,
                {"=", "(", "[", ",", ":", "return", "=~", "==~", "in", "==", "!="});
        }
        if (c == '\'' || c == '"' || dollarSlash || slash) {
            int begin = line;
            std::string end;
            bool triple = false;
            if (dollarSlash) { end = "/$"; i += 2; }
            else if (slash) { end = "/"; ++i; }
            else {
                triple = source.compare(i, 3, std::string(3, c)) == 0;
                end = std::string(triple ? 3 : 1, c);
                i += end.size();
            }
            bool closed = false;
            while (i < source.size()) {
                if (dollarSlash && (starts("$$") || starts("$/"))) { i += 2; continue; }
                if (!dollarSlash && source[i] == '\\') {
                    if (i + 1 < source.size() && source[i + 1] == '\n') ++line;
                    i += std::min(size_t(2), source.size() - i);
                    continue;
                }
                if (starts(end)) { i += end.size(); closed = true; break; }
                if ((c == '"' || slash || dollarSlash) && source.compare(i, 2, "$" "{") == 0)
                    fail(line, "выражения интерполяции не поддерживаются; используйте конкатенацию.");
                if (source[i] == '\n') {
                    if (!triple && !slash && !dollarSlash)
                        fail(begin, "обычная строка не закрыта до конца строки.");
                    ++line;
                }
                ++i;
            }
            if (!closed) fail(begin, "не закрыт строковый литерал.");
            out.push_back({"<literal>", begin});
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$' ||
            static_cast<unsigned char>(c) >= 128) {
            size_t begin = i++;
            while (i < source.size()) {
                unsigned char ch = static_cast<unsigned char>(source[i]);
                if (!std::isalnum(ch) && ch != '_' && ch != '$' && ch < 128) break;
                ++i;
            }
            add(source.substr(begin, i - begin));
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t begin = i++;
            while (i < source.size() &&
                (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            add(source.substr(begin, i - begin));
            continue;
        }
        bool found = false;
        for (const auto& op : std::vector<std::string>{
            "==~", ">>>", "**=", "<=>", "...", "==", "!=", "<=", ">=", "&&", "||",
            "++", "--", "+=", "-=", "*=", "/=", "%=", "<<", ">>", "**", "..",
            "?.", "?:", "?[", "=~", "->", "&=", "|=", "^="}) {
            if (starts(op)) { add(op); i += op.size(); found = true; break; }
        }
        if (!found) { add(std::string(1, c)); ++i; }
    }
    out.push_back({"<eof>", line});
    return out;
}

class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;
    Result result;
    int loops = 0, switches = 0;
    const Token& current() const { return tokens[pos]; }
    bool is(const std::string& s) const { return current().text == s; }
    void lines() { while (is("\n")) ++pos; }
    void separators() { while (is("\n") || is(";")) ++pos; }
    void expect(const std::string& s) {
        if (!is(s)) fail(current().line, "ожидалось '" + s + "', получено '" + current().text + "'.");
        ++pos;
    }
    void branch(const Token& token, int level) {
        ++result.absolute;
        result.nesting = std::max(result.nesting, level);
        result.branches.push_back({token.line, token.text == "do" ? "do-while" : token.text, level});
    }
    static bool continuation(const std::string& s) {
        return oneOf(s, {"=", "+", "-", "*", "/", "%", "&&", "||", "==", "!=", "<", ">",
            "<=", ">=", "+=", "-=", "*=", "/=", "%=", "&", "|", "^", "<<", ">>",
            "**", ".", "?.", ",", "in", "=~", "==~", "..", "<=>"});
    }
    void checkExpressionToken() {
        if (oneOf(current().text, {"?", "?:", "?[", "->", "{", "}", "if", "else",
            "for", "while", "do", "switch", "case", "default", "try", "catch"}))
            fail(current().line, "неподдерживаемая конструкция в выражении: '" +
                current().text + "'. См. ограничения в README.md.");
    }
    // Выражения не вычисляются; парные скобки проверяются рекурсивно.
    void group(const std::string& opening) {
        // Копия нужна, поскольку вызывающий код может передать tokens[pos].text.
        std::string closing = opening == "(" ? ")" : "]";
        expect(opening);
        while (!is(closing)) {
            if (is("<eof>")) fail(current().line, "не закрыта скобка.");
            if (is("(") || is("[")) { group(current().text); continue; }
            if (is(")") || is("]")) fail(current().line, "несогласованные скобки.");
            checkExpressionToken();
            ++pos;
        }
        ++pos;
    }
    void condition() {
        lines();
        if (!is("(")) fail(current().line, "после управляющего слова требуются круглые скобки.");
        size_t begin = pos;
        group("(");
        bool any = false;
        for (size_t j = begin + 1; j + 1 < pos; ++j)
            if (tokens[j].text != "\n") any = true;
        if (!any) fail(tokens[begin].line, "пустое условие.");
    }
    bool method() {
        if (!oneOf(current().text, {"def", "void", "int", "long", "double", "float",
                "boolean", "String", "Object", "Integer", "List", "Map", "static", "public", "private"}))
            return false;
        size_t start = pos, p = pos;
        while (p < tokens.size() && tokens[p].text != "(") {
            if (oneOf(tokens[p].text, {"=", ";", "\n", "{", "}", "<eof>"})) return false;
            ++p;
        }
        if (p >= tokens.size() || p - start < 2) return false;
        pos = p;
        group("(");
        lines();
        if (!is("{")) { pos = start; return false; }
        int savedLoops = loops, savedSwitches = switches;
        loops = switches = 0;
        block(0); // Вложенность тела каждой подпрограммы начинается с нуля.
        loops = savedLoops;
        switches = savedSwitches;
        return true;
    }
    void block(int depth) {
        expect("{");
        separators();
        while (!is("}")) {
            if (is("<eof>")) fail(current().line, "не закрыт блок { ... }.");
            statement(depth, false);
            separators();
        }
        expect("}");
    }
    void body(int depth) {
        lines();
        if (is("<eof>") || is("}")) fail(current().line, "отсутствует тело оператора.");
        statement(depth, false);
    }
    void selection(int depth) {
        ++result.operators;
        condition();
        lines();
        expect("{");
        // Предварительный проход учитывает только метки текущего switch.
        int number = 0, braces = 0;
        for (size_t p = pos; p < tokens.size(); ++p) {
            const auto& s = tokens[p].text;
            if (s == "}") { if (braces == 0) break; --braces; }
            else if (s == "{") ++braces;
            else if (s == "case" && braces == 0) ++number;
        }
        ++switches;
        int ordinal = 0, armDepth = depth;
        bool arm = false, defaultSeen = false;
        separators();
        while (!is("}")) {
            if (is("<eof>")) fail(current().line, "не закрыт switch.");
            if (is("case")) {
                Token label = current();
                ++pos;
                bool value = false;
                while (!is(":")) {
                    if (oneOf(current().text, {"<eof>", "}", "case", "default", ";"}))
                        fail(current().line, "после case требуется значение и двоеточие.");
                    if (is("(") || is("[")) { group(current().text); value = true; }
                    else { checkExpressionToken(); if (!is("\n")) value = true; ++pos; }
                }
                if (!value) fail(label.line, "пустая метка case.");
                ++pos;
                ++ordinal;
                branch(label, depth + ordinal - 1);
                armDepth = depth + ordinal;
                arm = true;
            } else if (is("default")) {
                if (defaultSeen) fail(current().line, "повторная ветка default.");
                defaultSeen = true;
                ++pos;
                lines();
                expect(":");
                // Нового условия нет; тело default находится внутри последнего else.
                armDepth = depth + number;
                arm = true;
            } else {
                if (!arm) fail(current().line, "оператор switch должен начинаться с case или default.");
                statement(armDepth, false);
            }
            separators();
        }
        expect("}");
        --switches;
    }
    void simple() {
        Token start = current();
        if (is("break") && !loops && !switches) fail(start.line, "break вне цикла или switch.");
        if (is("continue") && !loops) fail(start.line, "continue вне цикла.");
        bool consumed = false;
        std::string previous;
        while (!is(";") && !is("}") && !is("<eof>")) {
            if (is("\n")) {
                size_t next = pos;
                while (tokens[next].text == "\n") ++next;
                if (!continuation(previous) && tokens[next].text != "." && tokens[next].text != "?.")
                    break;
                ++pos;
                continue;
            }
            if (is("(") || is("[")) { group(current().text); previous = ")"; }
            else {
                if (is(")") || is("]")) fail(current().line, "лишняя закрывающая скобка.");
                checkExpressionToken();
                previous = current().text;
                ++pos;
            }
            consumed = true;
        }
        if (!consumed) fail(current().line, "ожидался оператор.");
        if (continuation(previous)) fail(start.line, "выражение не завершено.");
        ++result.operators;
        if (is(";")) ++pos;
    }
    void statement(int depth, bool allowMethod) {
        lines();
        if (is(";")) { ++pos; return; }
        if (is("{")) { block(depth); return; }
        if (allowMethod && method()) return;
        Token token = current();
        if (is("if")) {
            ++pos;
            ++result.operators;
            branch(token, depth);
            condition();
            body(depth + 1);
            lines();
            if (is("else")) { ++pos; body(depth + 1); }
        } else if (is("for") || is("while")) {
            ++pos;
            ++result.operators;
            branch(token, depth);
            condition();
            ++loops;
            body(depth + 1);
            --loops;
        } else if (is("do")) {
            ++pos;
            ++result.operators;
            branch(token, depth);
            ++loops;
            body(depth + 1);
            --loops;
            lines();
            expect("while");
            condition();
            if (is(";")) ++pos;
        } else if (is("switch")) {
            ++pos;
            selection(depth);
        } else if (oneOf(token.text, {"else", "case", "default", "}", "catch", "finally"})) {
            fail(token.line, "неожиданное '" + token.text + "'.");
        } else if (oneOf(token.text, {"class", "interface", "enum", "trait", "try",
                "synchronized", "package", "import", "@", "<eof>"})) {
            fail(token.line, "конструкция '" + token.text + "' не входит в поддерживаемый учебный синтаксис.");
        } else simple();
    }
public:
    explicit Parser(const std::string& source) : tokens(lex(source)) {}
    Result run() {
        separators();
        while (!is("<eof>")) { statement(0, true); separators(); }
        return result;
    }
};
inline Result analyze(const std::string& source) { return Parser(source).run(); }
} // namespace gilb
