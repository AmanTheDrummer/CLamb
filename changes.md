# Changes

- Added `clamb_lexer.h` to share token definitions and expose the lexer stream API.
- Updated `clamb_lexer.c` to provide token line numbers, lexical-error counts, and an optional standalone CLI for parser integration.
- Added `clamb_parser.c` as a recursive-descent parser consuming tokens directly from the lexer.
- Split the merged logical and relational operator classes into exact token types (`and_op`, `or_op`, `not_op`, `eq_op`, `ne_op`, `lt_op`, `le_op`, `gt_op`, `ge_op`) to match the updated LL(1)-friendly specification.
- Updated `dump.c` to use the public lexer token-name function.
- Added `parser_grammar_review.md` documenting the PDF-to-parser production and terminal mapping, grammar scope, and FIRST/FOLLOW table limitations.
- Added two valid parser fixtures and four invalid fixtures, including syntax and lexical errors.
- Updated `clamb_parser.c` to consume recursive call and index suffixes from `<PostfixTail>`; added a mixed chained-suffix regression case.
- Added `parser_tests.py` to build the parser and test valid and invalid inputs with diagnostics.
- Added `parser_test_results.txt` with the parser test output.
- Updated `README.md` with parser build, run, and test instructions.
- Left the supplied grammar PDF unchanged; mapped its logical and comparison terminal names to the lexer's distinct token types in the parser review.