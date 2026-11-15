# CLamb Lexer and Parser

This directory contains the table-driven lexical analyzer and recursive-descent syntax analyzer for **CLamb**, the reduced-C language proposed for the compiler construction project.

The lexer reads CLamb source code and prints one token per line. It recognizes the token categories in the lexical specification and reports malformed lexemes on standard error.

## Requirements

- GCC with C11 support
- Python 3.8 or newer for the test scripts

## Build

From this directory, compile the lexer with:

```text
gcc -std=c11 -Wall -Wextra -o clamb_lexer clamb_lexer.c
```

On Windows, this creates `clamb_lexer.exe`.

## Run

Scan a source file (the fixtures are in `parser tests`):

```text
.\clamb_lexer.exe "parser tests\4_test_input_2.clamb"
```

Run the lexer interactively through standard input:

```text
.\clamb_lexer.exe
```

Then type CLamb source and press `Ctrl+Z`, followed by Enter, to signal end-of-file in Windows PowerShell.

The output has this form:

```text
TOKEN CLASS        LEXEME
----------------------------------------
<data_type       , int>
<identifier      , total>
<assignment_op   , =>
<integer_literal, 7>
<semicolon       , ;>
```

Diagnostics are written to standard error. A malformed input reports a lexical error and the scanner attempts to continue at the next recognizable token.

## Recognized tokens

- **Data types:** `int`, `float`, `char`, `bool`, `string`, `void`, `auto`
- **Keywords:** `if`, `else`, `for`, `while`, `return`, `lamb`, `null`, `print`
- **Boolean literals:** `true`, `false`
- **Logical operators:** `AND`, `OR`, `NOT` are tokenized as `logical_and`, `logical_or`, and `logical_not`
- **Identifiers:** letters, digits, and `_` after the initial character; the specified digit-led forms such as `2sum` are also accepted
- **Number literals:** integers and floats such as `7` and `3.14`
- **Character literals:** `'a'`
- **String literals:** `"hello world"`
- **Operators:** `+`/`-` (`add_op`), `*` (`asterisk`), `/`/`%` (`mul_op`), `=` (`assignment_op`), and comparisons `==`, `!=`, `<`, `>`, `<=`, `>=` (`relational_eq`, `relational_noteq`, `relational_less`, `relational_greater`, `relational_lesseq`, `relational_greatereq`)
- **Punctuators:** `(` `)` `{` `}` `;` `,`
- **Comments:** `//` through the end of the line

Whitespace is ignored. The lexer is table-driven: recognition and error transitions come from the DFA in `clamb_lexer.c`.

Pointers, arrays, address-of, and memory-management keywords are outside the
revised language. `&`, `[` and `]` are reported as unrecognized characters;
`malloc` and `free` are ordinary identifiers.

## Tests

Run the lexer behavior tests:

```text
python "parser tests\run_tests.py"
```

The test suite covers valid tokens, comments, whitespace, standard input, malformed literals, error recovery, diagnostic line numbers, oversized lexemes, and missing files.

Verify the DFA table against the independent transcription of the lexical specification:

```text
python "parser tests\verify_table.py"
```

A successful verification reports zero transition and token-class mismatches.

Parser/lexer test inputs are in `parser tests`; expected diagnostics are in:

- `parser tests\4_test_input_2.clamb` through `parser tests\4_test_input_6.clamb`
- `parser tests\error_test_input.clamb`
- `error_test_diagnostics.txt`

## Specification references

- `documentation\Lexical Specification - CLamb.pdf` defines the token rules and DFA behavior.
- `documentation\Revised Grammar & Parser - CLamb.pdf` defines the reduced LL(k) grammar.
- `documentation\Compiler Construction Project Proposal Aman.pdf` describes the planned CLamb language and its broader parser/compiler features.
- `2_dfa_transition_table.md` documents the implemented DFA in readable table form.

Code generation remains outside this milestone. The parser now performs semantic
type checking, scope resolution, function-call validation, and the documented
non-capturing lambda checks.

## Parser

Build the parser together with the lexer implementation:

```text
gcc -std=c11 -Wall -Wextra -DCLAMB_LEXER_NO_MAIN -o clamb_parser clamb_parser.c clamb_semantic.c clamb_lexer.c
```

Parse a source file or read from standard input. Quote the source path because
the test folders contain spaces:

```text
.\clamb_parser.exe "parser tests\parser_valid_1.clamb"
.\clamb_parser.exe "semantic tests\semantic_valid_test.clamb"
.\clamb_parser.exe
```

The parser consumes the lexer’s `Token` API from `clamb_lexer.h` and runs
semantic actions from `clamb_semantic.c` during recursive descent. It maintains
nested symbol scopes, checks expression and statement types and function calls,
and reports lexical, syntax, and semantic errors with line and column. Normal
mode requires both syntactic and semantic validity. `--syntax-only` is available
for grammar-focused parser tests.

Run the parser integration tests. They build the parser in a temporary
directory and exercise the grammar in syntax-only mode, including top-level-only
function declarations, recursive functions, lambda syntax, control flow,
chained calls, rejection of pointers and arrays, semantic parser entry points,
and the `else_opt` k=2 lookahead:

```text
python "parser tests\parser_tests.py"
python "semantic tests\semantic_tests.py"
```

`semantic_tests.py` checks valid programs, applicable semantic error categories,
the documented declaration/scope/function decisions, conversions, and
independent-error recovery.

The semantic fixtures are in `semantic tests`:

- `semantic_valid_test.clamb` and `semantic_valid_test_2.clamb`: nested scopes,
	type-changing shadowing, scope restoration after block exit, calls, control
	flow, and the supported `int` to `float` and `char` to `string` conversions.
- `semantic_invalid_test.clamb`: the required semantic error categories, an
	out-of-scope identifier, and a local that redeclares a function parameter.
- `semantic_multi_error_test.clamb`: several independent semantic errors in one
	syntactically valid program, including a type-narrowing initializer.

Run the analyzer on one fixture at a time, for example:

```text
.\clamb_parser.exe "semantic tests\semantic_valid_test.clamb"
.\clamb_parser.exe "semantic tests\semantic_invalid_test.clamb"
```

Valid input prints `Semantic analysis successful.`; invalid input prints
line-and-column semantic diagnostics and exits with a nonzero status.

The grammar audit is in `parser_grammar_review.md`; implementation decisions
and semantic rules are summarized in `semantic_implementation.md`.

The grammar PDF lists `4_test_input_3.clamb` and `4_test_input_5.clamb` as valid
fixtures, but their current contents use arrays and pointers. The reduced
grammar excludes these forms, so the integration tests verify that both are
rejected.

The lexer remains independently buildable with the command above in the lexer
section.
