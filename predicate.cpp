#include "predicate.h"

#include <cctype>
#include <iostream>
#include <string>
#include <unordered_set>

namespace {

/*
    expression: or_expression
    or_expression: and_expression ['or' or_expression]
    and_expression: not_expression ['and' and_expression]
    not_expression: 'not' not_expression | comparison | '(' expression ')'
    comparison: term '==' term |
                term '!=' term |
                term 'in' set |
                term 'not' 'in' set |
                term
    set: '{' '}' | '{' NAME set_tail* '} | '{' STRING set_tail* '}'
    set_tail: ',' NAME | ',' STRING
    term: NAME | STRING
 */

static const std::string EMPTY_STRING;

typedef std::string String;
typedef std::unordered_set<std::string> Set;
typedef std::function<const std::string&(const Variables&)> Term;
typedef std::function<bool(const Variables&)> Expression;


template <class Iterator>
Iterator skipSpaces(Iterator it, Iterator end)
{
    while (it != end && isspace(static_cast<unsigned char>(*it))) {
        ++it;
    }
    return it;
}

template <class Iterator>
bool parsePattern(Iterator& begin, Iterator end, const char* pattern)
{
    Iterator it = skipSpaces(begin, end);
    while (*pattern != '\0' && it != end && *it == *pattern) {
        ++pattern;
        ++it;
    }
    if (*pattern == '\0') {
        begin = it;
        return true;
    } else {
        return false;
    }
}


// NAME

template <class Iterator>
bool parseName(Iterator& begin, Iterator end, String* name)
{
    Iterator it = skipSpaces(begin, end);
    if (it == end || !isalpha(static_cast<unsigned char>(*it))) {
        return false;
    }
    const Iterator start = it;
    while (it != end && (isalnum(static_cast<unsigned char>(*it)) || *it == '_')) {
        ++it;
    }
    const Iterator stop = it;
    name->assign(start, stop);
    begin = it;
    return true;
}


// STRING

template <class Iterator>
bool parseString(Iterator& begin, Iterator end, String* string)
{
    Iterator it = skipSpaces(begin, end);
    if (parsePattern(it, end, "'")) {
        const Iterator start = it;
        while (it != end && *it != '\'') {
            ++it;
        }
        if (it == end) {
            return false;
        }
        string->assign(start, it);
        ++it;

    } else {
        const Iterator start = it;
        while (it != end && (isalnum(static_cast<unsigned char>(*it)) || *it == '_')) {
            ++it;
        }
        if (it == start) {
            return false;
        }
        string->assign(start, it);
    }
    begin = it;
    return true;
}


// set: '{' '}' | '{' STRING set_tail* '}'
// set_tail: ',' STRING

template <class Iterator>
bool parseSetTail(Iterator& begin, Iterator end, Set* set)
{
    Iterator it = begin;
    if (!parsePattern(it, end, ",")) {
        return false;
    }
    String string;
    if (!parseString(it, end, &string)) {
        return false;
    }
    set->insert(std::move(string));
    begin = it;
    return true;
}

template <class Iterator>
bool parseSet(Iterator& begin, Iterator end, Set* set)
{
    set->clear();
    Iterator it = begin;
    if (!parsePattern(it, end, "{")) {
        return false;
    }
    String string;
    if (parseString(it, end, &string)) {
        set->insert(std::move(string));
        while (parseSetTail(it, end, set));
    }
    if (!parsePattern(it, end, "}")) {
        return false;
    }
    begin = it;
    return true;
}


// term: NAME | STRING

template <class Map>
const typename Map::mapped_type& getWithDef(const Map& map, const typename Map::key_type& key, const typename Map::mapped_type& defaultValue)
{
    const auto it = map.find(key);
    if (it == map.end()) {
        return defaultValue;
    } else {
        return it->second;
    }
}


struct StringTerm {
    String value;
    const String& operator() (const Variables&) const
    {
        return value;
    }
};

struct VariableTerm {
    String name;
    const String& operator() (const Variables& variables) const
    {
        return getWithDef(variables, name, EMPTY_STRING);
    }
};

template <class Iterator>
Term parseTerm(Iterator& begin, Iterator end)
{
    String string;
    if (parseName(begin, end, &string)) {
        return VariableTerm{string};
    } else if (parseString(begin, end, &string)) {
        return StringTerm{string};
    } else {
        return {};
    }
}


// comparison: term '==' term |
//             term '!=' term |
//             term 'in' set |
//             term 'not' 'in' set |
//             term

struct EqualComparison {
    Term left, right;
    bool operator() (const Variables& variables) const { return left(variables) == right(variables); }
};

struct NotEqualComparison {
    Term left, right;
    bool operator() (const Variables& variables) const { return left(variables) != right(variables); }
};

struct InComparison {
    Term left;
    Set set;
    bool operator() (const Variables& variables) const { return set.count(left(variables)) > 0; }
};

struct NotInComparison {
    Term left;
    Set set;
    bool operator() (const Variables& variables) const { return set.count(left(variables)) == 0; }
};

struct NotEmpty {
    Term term;
    bool operator() (const Variables& variables) const { return !term(variables).empty(); }
};

template <class Iterator>
Expression parseComparison(Iterator& it, Iterator end)
{
    const Iterator oldIt = it;
    if (Term left = parseTerm(it, end)) {
        if (parsePattern(it, end, "==")) {
            if (Term right = parseTerm(it, end)) {
                return EqualComparison{ std::move(left), std::move(right) };
            }

        } else if (parsePattern(it, end, "!=")) {
            if (Term right = parseTerm(it, end)) {
                return NotEqualComparison{ std::move(left), std::move(right) };
            }

        } else if (parsePattern(it, end, "in")) {
            Set right;
            if (parseSet(it, end, &right)) {
                return InComparison{ std::move(left), std::move(right) };;
            }

        } else if (parsePattern(it, end, "not") && parsePattern(it, end, "in")) {
            Set right;
            if (parseSet(it, end, &right)) {
                return NotInComparison{ std::move(left), std::move(right) };;
            }
        } else {
            return NotEmpty{ std::move(left) };
        }
    }
    it = oldIt;
    return {};
}


template <class Iterator>
Expression parseExpression(Iterator& begin, Iterator end);


// not_expression: 'not' not_expression | comparison | '(' expression ')'

template <class Iterator>
Expression parseNotExpression(Iterator& it, Iterator end)
{
    const Iterator oldIt = it;
    bool isNegative = false;
    while (parsePattern(it, end, "not")) {
        isNegative = !isNegative;
    }
    Expression expression;
    if ( (expression = parseComparison(it, end)) ||
         (parsePattern(it, end, "(") && (expression = parseExpression(it, end)) && parsePattern(it, end, ")")) )
    {
        if (isNegative) {
            return std::not1(std::move(expression));
        } else {
            return std::move(expression);
        }
    }
    it = oldIt;
    return {};
}


// and_expression: not_expression ['and' and_expression]

struct AndExpression {
    Expression left, right;
    bool operator() (const Variables& variables) const { return left(variables) && right(variables); }
};

template <class Iterator>
Expression parseAndExpression(Iterator& it, Iterator end)
{
    const Iterator oldIt = it;
    if (Expression left = parseNotExpression(it, end)) {
        if (parsePattern(it, end, "and")) {
            if (Expression right = parseAndExpression(it, end)) {
                return AndExpression{ std::move(left), std::move(right) };
            }
        } else {
            return std::move(left);
        }
    }
    it = oldIt;
    return {};
}


// or_expression: and_expression ['or' or_expression]

struct OrExpression {
    Expression left, right;
    bool operator() (const Variables& variables) const { return left(variables) || right(variables); }
};

template <class Iterator>
Expression parseOrExpression(Iterator& it, Iterator end)
{
    const Iterator oldIt = it;
    if (Expression left = parseAndExpression(it, end)) {
        if (parsePattern(it, end, "or")) {
            if (Expression right = parseOrExpression(it, end)) {
                return OrExpression{ std::move(left), std::move(right) };
            }
        } else {
            return std::move(left);
        }
    }
    it = oldIt;
    return {};
}


// expression: or_expression

template <class Iterator>
Expression parseExpression(Iterator& it, Iterator end)
{
    return parseOrExpression(it, end);
}

} // namespace

Predicate parsePredicate(const std::string& predicate)
{
    auto it = predicate.begin();
    if (Predicate result = parseExpression(it, predicate.end())) {
        if (predicate.end() == skipSpaces(it, predicate.end())) {
            return result;
        }
    }
    return {};
}


// variable: NAME=STRING
// variables: variable (',' variable)*

template <class Iterator>
bool parseVariable(Iterator& it, Iterator end, Variables* variables)
{
    const Iterator oldIt = it;
    String name, value;
    if (parseName(it, end, &name) && parsePattern(it, end, "=") && parseString(it, end, &value)) {
        (*variables)[name] = value;
        return true;
    }
    it = oldIt;
    return false;
}


template <class Iterator>
bool parseVariables(Iterator& begin, Iterator end, Variables* variables)
{
    variables->clear();
    Iterator it = begin;
    do {
        if (!parseVariable(it, end, variables)) {
            return false;
        }
    } while (parsePattern(it, end, ","));
    begin = it;
    return true;
}

bool parseVariables(const std::string& input, Variables* variables)
{
    variables->clear();
    auto it = input.begin();
    return parseVariables(it, input.end(), variables) && input.end() == skipSpaces(it, input.end());
}



int main(int argc, const char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: predicate [variable_set ...]\n\n";
        return -1;
    }

    const Predicate predicate = parsePredicate(argv[1]);
    if (!predicate) {
        std::cerr << "Unable to parse predicate: " << argv[1] << std::endl;
        return -1;
    }

    for (int index = 2; index < argc; ++index) {
        Variables variables;
        if (parseVariables(argv[index], &variables)) {
            std::cout << argv[1] << ": " << argv[index] << ": " << (predicate(variables) ? "True" : "False") << std::endl;
        } else {
            std::cerr << "Unable to parse variable_set: " << argv[index] << std::endl;
        }
    }
    return 0;
}
