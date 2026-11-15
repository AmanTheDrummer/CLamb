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

Scan a source file:

```text
./clamb_lexer.exe 4_test_input.clamb
```

Run the lexer interactively through standard input:

```text
./clamb_lexer.exe
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
python run_tests.py
```

The test suite covers valid tokens, comments, whitespace, standard input, malformed literals, error recovery, diagnostic line numbers, oversized lexemes, and missing files.

Verify the DFA table against the independent transcription of the lexical specification:

```text
python verify_table.py
```

A successful verification reports zero transition and token-class mismatches.

Example inputs and expected diagnostics are included in:

- `4_test_input.clamb`
- `error_test_input.clamb`
- `error_test_diagnostics.txt`

## Specification references

- `Lexical Specification - CLamb.pdf` defines the token rules and DFA behavior.
- `Revised Grammar & Parser - CLamb.pdf` defines the reduced LL(k) grammar.
- `Compiler Construction Project Proposal Aman.pdf` describes the planned CLamb language and its broader parser/compiler features.
- `2_dfa_transition_table.md` documents the implemented DFA in readable table form.

Type checking, code generation, and the semantics of lambdas belong to later compiler stages proposed in the project document.

## Parser

Build the parser together with the lexer implementation:

```text
gcc -std=c11 -Wall -Wextra -DCLAMB_LEXER_NO_MAIN -o clamb_parser clamb_parser.c clamb_lexer.c
```

Parse a source file or read from standard input:

```text
./clamb_parser source.clamb
./clamb_parser
```

The parser consumes the lexer's `Token` API from `clamb_lexer.h`. Syntax errors
include the line, expected construct, and found token. Lexical errors are
reported by the lexer and cause the overall parse to fail.

Run the parser integration tests. They build the parser in a temporary
directory and exercise the valid and invalid fixtures, a lexical-error case,
and focused cases for the revised grammar (including top-level-only function
declarations, recursive functions, lambda expressions, control flow, chained
calls, rejection of pointers and arrays, LL(k) nonterminal/function-count
parity, and the `else_opt` k=2 lookahead):

```text
python parser_tests.py
```

The recorded output is in `parser_test_results.txt`. The grammar audit is in
`parser_grammar_review.md`.

The PDF lists `4_test_input_3.clamb` and `4_test_input_5.clamb` as valid
fixtures, but their current contents use arrays and pointers. The reduced
grammar excludes these forms, so the integration tests verify that both are
rejected.

The lexer remains independently buildable with the command above in the lexer
section.
