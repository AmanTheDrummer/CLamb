# CLamb Lexer

This directory contains the table-driven lexical analyzer for **CLamb**, the reduced-C language proposed for the compiler construction project.

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
- **Keywords:** `if`, `else`, `for`, `while`, `return`, `lamb`, `null`, `print`, `malloc`, `free`
- **Boolean literals:** `true`, `false`
- **Logical operators:** `AND`, `OR`, `NOT`
- **Identifiers:** letters, digits, and `_` after the initial character; the specified digit-led forms such as `2sum` are also accepted
- **Number literals:** integers and floats such as `7` and `3.14`
- **Character literals:** `'a'`
- **String literals:** `"hello world"`
- **Operators:** `+`, `-`, `*`, `/`, `%`, `&`, `=`, `<`, `>`, `!` combinations `==`, `!=`, `<=`, `>=`
- **Punctuators:** `(` `)` `{` `}` `[` `]` `;` `,`
- **Comments:** `//` through the end of the line

Whitespace is ignored. The lexer is table-driven: recognition and error transitions come from the DFA in `clamb_lexer.c`.

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

- `Lexical Specification - CLamb (1).pdf` defines the token rules and DFA behavior.
- `Compiler Construction Project Proposal Aman.pdf` describes the planned CLamb language and its broader parser/compiler features.
- `2_dfa_transition_table.md` documents the implemented DFA in readable table form.

The current project is the lexical analyzer only. Parsing, type checking, code generation, lambda semantics, arrays, and pointer semantics belong to later compiler stages proposed in the project document.
