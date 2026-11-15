# Revised CLamb Grammar and Parser

The parser implements the final right-recursive LL(k) grammar in
`Revised Grammar & Parser - CLamb.pdf`. It has one `parse_*` function for
each of the 38 nonterminals in that production list. The parser support
functions, lookahead, and program entry point are not grammar productions.

## Program structure and declarations

- A program contains zero or more top-level declarations followed by EOF.
- A declaration is a variable declaration or a function declaration.
- Declaration heads contain a data type and identifier only. Variables may
  have an initializer or neither; function declarations have a parameter list
  and a brace-delimited body.
- Parameters are data-type/identifier pairs. Function declarations are only
  allowed at the top level.
- Blocks contain zero or more statements, including local variable
  declarations and nested blocks.

Pointers, array declarations/indexing, address-of, `malloc`, and `free` are
not part of the revised grammar. The lexer treats `&`, `[` and `]` as invalid
characters; `malloc` and `free` are ordinary identifiers.

## Statements and expressions

- Statement forms are variable declarations, blocks, expression/empty
  statements, `if`/`else`, `while`, `for`, `return`, and `print`.
- `else_opt` uses k=2 lookahead for `(kw_else, l_brace)` or
  `(kw_else, kw_if)`. Other lookahead pairs select epsilon unless the first
  token is `kw_else`, in which case the parser reports a malformed `else`.
- Expressions follow the grammar's precedence levels: right-associative
  assignment, logical OR, logical AND, equality, relational, additive,
  multiplicative, unary, and postfix/primary expressions.
- Unary operators are `logical_not` and `add_op`; `asterisk` is supported as
  a multiplicative operator only.
- Postfix calls may be chained. Expression lists may be empty in call and
  `print` argument positions. Expression statements require a trailing
  semicolon.
- Lambdas use the same parameter-list and block forms as documented.

The parser implements syntax only; it does not validate types, assignment
targets, or other semantic constraints.

## Lexer token mapping

- `logical_and`, `logical_or`, and `logical_not` map to the corresponding
  logical token types.
- Equality and relational terminals map to `relational_eq`,
  `relational_noteq`, `relational_less`, `relational_lesseq`,
  `relational_greater`, and `relational_greatereq`.
- `mul_op` and `asterisk` are both accepted as multiplicative operators.
- `add_op` supplies both additive and unary `+`/`-` operators.

## Verification coverage

`parser_tests.py` builds the parser in a temporary directory and checks valid,
invalid, and lexical-error cases. It exercises declarations, lambda bodies,
control flow, empty `for` clauses, chained function calls, and grammar
rejections for nested function declarations, top-level statements, malformed
parameter lists, invalid `else` continuations, pointers, arrays, indexing,
and address-of. It also checks one parser function per LL(k) nonterminal and
that `else_opt` uses the second lookahead token.

The PDF lists `4_test_input_3.clamb` and `4_test_input_5.clamb` as valid
fixtures, but their current contents use arrays and pointers excluded by the
revised grammar. The integration tests therefore expect these legacy fixtures
to be rejected.
