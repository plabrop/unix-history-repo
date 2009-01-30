# $FreeBSD$

echo 1..12

REGRESSION_START($1)

REGRESSION_TEST(`00', `tr abcde 12345 < regress.in')
REGRESSION_TEST(`01', `tr 12345 abcde < regress.in')
REGRESSION_TEST(`02', `tr -d aceg < regress.in')
REGRESSION_TEST(`03', `tr "[[:lower:]]" "[[:upper:]]" < regress.in')
REGRESSION_TEST(`04', `tr "[[:alpha:]]" . < regress.in')
REGRESSION_TEST(`05', `tr "[[:lower:]]" "[[:upper:]]" < regress.in | tr "[[:upper:]]" "[[:lower:]]"')
REGRESSION_TEST(`06', `tr "[[:digit:]]" "?" < regress2.in')
REGRESSION_TEST(`07', `tr "[[:alnum:]]" "#" < regress2.in')
REGRESSION_TEST(`08', `tr "[[:upper:]]" "[[:lower:]]" < regress2.in | tr -d "[^[:alpha:]] "')
REGRESSION_TEST(`09', `printf "\\f\\r\\n" | tr "\\014\\r" "?#"')
REGRESSION_TEST(`0a', `printf "0xdeadbeef\\n" | tr "x[[:xdigit:]]" "?\$"')
REGRESSION_TEST(`0b', `(tr -cd "[[:xdigit:]]" < regress2.in ; echo)')

REGRESSION_END()
