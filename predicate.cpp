#include "predicate.h"

#include <cctype>
#include <iostream>
#include <string>
#include <unordered_set>

/*


 expression: '(' expression ')' | or_expression 
 or_expression: and_expression ('or' and_expression)*
 and_expression: not_expression ['and' and_expression]
 not_expression: ['not'] comparison

 comparison: term '==' term |
             term '!=' term |
             term 'in' set |
             term 'not' 'in' set

 term: NAME | STRING
 set: '{' '}' | '{' NAME set_tail* '} | '{' STRING set_tail* '}'
 set_tail: ',' NAME | ',' STRING

 
 */


static const std::string EMPTY_STRING;


typedef std::string String;
typedef std::unordered_set<std::string> Set;
typedef std::function<const std::string&(const Variables&)> Term;
typedef std::function<bool(const Variables&)> Comparison;
typedef std::function<bool(const Variables&)> Expression;


template <class Iterator>
Iterator skipSpaces(Iterator it, Iterator end)
{
    while (it != end && isspace(static_cast<unsigned char>(*it))) {
        ++it;
    }
    return it;
}


struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, const char* pattern) const
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

} parsePattern;


// NAME

struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, String* name) const
    {
        Iterator it = skipSpaces(begin, end);
        if (it == end || !isalpha(static_cast<unsigned char>(*it))) {
            return false;
        }
        const Iterator start = it;
        while (it != end && (isalpha(static_cast<unsigned char>(*it)) || isalpha(static_cast<unsigned char>(*it)) || *it == '_')) {
            ++it;
        }
        const Iterator stop = it;
        name->assign(start, stop);
        begin = it;
        return true;
    }
} parseName;


// STRING

struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, String* string) const
    {
        Iterator it = skipSpaces(begin, end);
        if (!parsePattern(it, end, "'")) {
            return false;
        }
        const Iterator start = it;
        while (it != end && *it != '\'') {
            ++it;
        }
        const Iterator stop = it;
        if (!parsePattern(it, end, "'")) {
            return false;
        }
        string->assign(start, stop);
        begin = it;
        return true;
    }
} parseString;


// set: '{' '}' | '{' NAME set_tail* '} | '{' STRING set_tail* '}'
// set_tail: ',' NAME | ',' STRING

struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, Set* set) const
    {
        Iterator it = begin;
        if (!parsePattern(it, end, ",")) {
            return false;
        }
        String string;
        if (!parseString(it, end, &string) && !parseName(it, end, &string)) {
            return false;
        }
        set->insert(std::move(string));
        begin = it;
        return true;
    }
} parseSetTail;


struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, Set* set) const
    {
        set->clear();
        Iterator it = begin;
        if (!parsePattern(it, end, "{")) {
            return false;
        }
        String string;
        if (parseString(it, end, &string) || parseName(it, end, &string)) {
            set->insert(std::move(string));
            while (parseSetTail(it, end, set));
        }
        if (!parsePattern(it, end, "}")) {
            return false;
        }
        begin = it;
        return true;
    }
} parseSet;


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

struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, Term* term) const
    {
        String string;
        if (parseName(begin, end, &string)) {
            *term = VariableTerm{string};
        } else if (parseString(begin, end, &string)) {
            *term = StringTerm{string};
        } else {
            return false;
        }
        return true;
    }
} parseTerm;


// comparison: term '==' term |
//             term '!=' term |
//             term 'in' set |
//             term 'not' 'in' set

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

struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, Comparison* comparison) const
    {
        Iterator it = begin;
        Term left;
        if (!parseTerm(it, end, &left)) {
            return false;
        }
        if (parsePattern(it, end, "==")) {
            Term right;
            if (!parseTerm(it, end, &right)) {
                return false;
            }
            *comparison = EqualComparison{ std::move(left), std::move(right) };

        } else if (parsePattern(it, end, "!=")) {
            Term right;
            if (!parseTerm(it, end, &right)) {
                return false;
            }
            *comparison = NotEqualComparison{ std::move(left), std::move(right) };

        } else if (parsePattern(it, end, "in")) {
            Set right;
            if (!parseSet(it, end, &right)) {
                return false;
            }
            *comparison = InComparison{ std::move(left), std::move(right) };

        } else if (parsePattern(it, end, "not") && parsePattern(it, end, "in")) {
            Set right;
            if (!parseSet(it, end, &right)) {
                return false;
            }
            *comparison = NotInComparison{ std::move(left), std::move(right) };

        } else {
            return false;
        }
        begin = it;
        return true;
    }
} parseComparison;


// not_expression: 'not' not_expression | comparison | '(' expression ')'


template <class Iterator>
bool parseExpressionFwd(Iterator& begin, Iterator end, Expression* expression);


struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, Expression* expression) const
    {
        Iterator it = begin;
        bool isNegative = false;
        while (parsePattern(it, end, "not")) {
            isNegative = !isNegative;
        }
        if (!parseComparison(it, end, expression) &&
            !(parsePattern(it, end, "(") && parseExpressionFwd(it, end, expression) && parsePattern(it, end, ")"))) {
            return false;
        }
        if (isNegative) {
            *expression = std::not1(std::move(*expression));
        }
        begin = it;
        return true;
    }
} parseNotExpression;


// and_expression: not_expression ['and' not_expression]

struct AndExpression {
    Expression left, right;
    bool operator() (const Variables& variables) const
    {
        return left(variables) && right(variables);
    }
};

struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, Expression* expression) const
    {
        Iterator it = begin;
        if (!parseNotExpression(it, end, expression)) {
            return false;
        }
        if (parsePattern(it, end, "and")) {
            Expression rightExpression;
            if (!(*this)(it, end, &rightExpression)) {
                return false;
            }
            *expression = AndExpression{ std::move(*expression), std::move(rightExpression) };
        } 
        begin = it;
        return true;
    }
} parseAndExpression;


// or_expression: and_expression ('or' and_expression)*

struct OrExpression {
    Expression left, right;
    bool operator() (const Variables& variables) const
    {
        return left(variables) || right(variables);
    }
};

struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, Expression* expression) const
    {
        Iterator it = begin;
        if (!parseAndExpression(it, end, expression)) {
            return false;
        }
        if (parsePattern(it, end, "or")) {
            Expression rightExpression;
            if (!(*this)(it, end, &rightExpression)) {
                return false;
            }
            *expression = OrExpression{ std::move(*expression), std::move(rightExpression) };
        }
        begin = it;
        return true;
    }
} parseOrExpression;


// expression: or_expression 

struct {
    template <class Iterator>
    bool operator() (Iterator& begin, Iterator end, Expression* expression) const
    {
        return parseOrExpression(begin, end, expression);
    }
} parseExpression;


template <class Iterator>
bool parseExpressionFwd(Iterator& begin, Iterator end, Expression* expression)
{
    return parseExpression(begin, end, expression);
}


template <class ResultType, class Parser>
void accept(const std::string& input, const Parser& parser)
{
    ResultType result;
    auto it = input.begin();
    if (!parser(it, input.end(), &result) || input.end() != skipSpaces(it, input.end())) {
        std::cerr << "Fail: '" << input << "'" << std::endl;
    }
}


const char* exec(const std::string& expression, const Variables& variables)
{
    Expression result;
    auto it = expression.begin();
    if (!parseExpression(it, expression.end(), &result) || expression.end() != skipSpaces(it, expression.end())) {
        return "None";
    }
    return result(variables) ? "True" : "False";
}



int main()
{
    accept<std::string>("hello_world", parseName);
    accept<std::string>(" hello_world ", parseName);

    accept<std::string>("'hello_world ' ", parseString);
    accept<std::string>("' hello_world' ", parseString);

    accept<std::unordered_set<std::string> >("{ 'a', 'b' , 'c' }", parseSet);
    accept<std::unordered_set<std::string> >("{ 'a' }", parseSet);
    accept<std::unordered_set<std::string> >("{ }", parseSet);
    accept<std::unordered_set<std::string> >("{ street, locality, country, 'ab cd' }", parseSet);

    accept<Term>(" xxx", parseTerm);
    accept<Term>("'xxx1'", parseTerm);
    accept<String>("'xxx2'", parseString);

    accept<Comparison>(" 'xxx' in { a, b, 'c' } ", parseComparison);

    accept<Expression>(" not 'xxx' not in { a, b, 'c' } ", parseNotExpression);

    accept<Expression>(" not 'xxx' not in { a, b, 'c' } and xx!='adsf'", parseAndExpression);

    accept<Expression>(" not 'xxx' not in { a, b, 'c' } and xx!='adsf' or not (xx == 'uu')", parseExpression);

    std::cout << exec("kind in {street, district, locality} and country == 'TR'", {
        {"kind", "street"},
        {"country", "TR"}
    }) << '\n';

    return 0;
}
