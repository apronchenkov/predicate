#pragma once

#include <functional>
#include <string>
#include <unordered_map>


typedef std::unordered_map<std::string, std::string> Variables;


typedef std::function<bool(const Variables&)> Predicate;


Predicate parsePredicate(const std::string& predicate);
