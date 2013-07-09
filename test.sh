#!/bin/sh

# Comparison
## ==
./predicate "x == y" \
    "x = 0, y = 0" \
    "x = 0, y = 1" \
    "x = 1, y = 0" \
    "x = 1, y = 1"

## !=
./predicate "x == y" \
    "x = 0, y = 0" \
    "x = 0, y = 1" \
    "x = 1, y = 0" \
    "x = 1, y = 1"

## in
./predicate "x in {a, b}" \
    "x = a" \
    "x = b" \
    "x = c"

## in
./predicate "x not in {a, b}" \
    "x = a" \
    "x = b" \
    "x = c"

## x
./predicate "x" \
    "x = ''" \
    "x = 0" \
    "x = 1"


# (x)
./predicate "(x)" \
    "x = ''" \
    "x = 0" \
    "x = 1"


# not
./predicate "not x" \
    "x = ''" \
    "x = 0" \
    "x = 1"


# and
./predicate "x and y" \
    "x = '', y = ''" \
    "x = '', y = 1" \
    "x = 1, y = ''" \
    "x = 1, y = 1"


# or
./predicate "x or y" \
    "x = '', y = ''" \
    "x = '', y = 1" \
    "x = 1, y = ''" \
    "x = 1, y = 1"


# A free form
./predicate "kind in {'street', 'district'} and 'TR' == country or fallback" \
    "kind=locality, country=RU" \
    "kind=locality, country=TR" \
    "kind=district, country=RU" \
    "kind=district, country=TR" \
    "kind=street, country=RU" \
    "kind=street, country=TR" \
    "kind=locality, country=TR, fallback=1" \
    "kind=district, country=RU, fallback=1" \
    "kind=district, country=TR, fallback=1" \
    "kind=street, country=RU, fallback=1" \
    "kind=street, country=TR, fallback=1" \
    "fallback=1"
