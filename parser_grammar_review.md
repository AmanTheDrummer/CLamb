# CLamb Grammar and Parser Review

## Result

Comparison against `Grammar & Parser - CLamb.pdf` confirms that the parser
follows its LL(1) productions, including the recursive postfix tail. A chained
call/index expression is included in the valid parser fixtures. The PDF's
`FIRST_STMT` includes `FIRST_EXPR`; the parser's expression-start check covers
those terminals, including unary starters. The FIRST/FOLLOW entries present in
the PDF match the productions, but six statement nonterminals are still missing
from its table; their sets are listed below.

## Missing FIRST/FOLLOW Rows

All six nonterminals below have `FOLLOW = FIRST_STMT ∪ {r_brace}`, since each
is an alternative of `<Statement>` in `<StmtList>`.

| Nonterminal | FIRST |
| --- | --- |
| `<ExprStmt>` | `FIRST_EXPR ∪ {semicolon}` |
| `<WhileStmt>` | `{kw_while}` |
| `<ForStmt>` | `{kw_for}` |
| `<ReturnStmt>` | `{kw_return}` |
| `<PrintStmt>` | `{kw_print}` |
| `<FreeStmt>` | `{kw_free}` |

The parser already dispatches on these terminals and accepts expression and
empty statements as specified, so no parser change is required for these rows.

## Terminal Mapping

- Grammar terminals `logical_and`, `logical_or`, and `logical_not` map to lexer
  token types `TOK_AND_OP`, `TOK_OR_OP`, and `TOK_NOT_OP`.
- `relational_eq`, `relational_noteq`, `relational_less`, `relational_lesseq`,
  `relational_greater`, and `relational_greatereq` map respectively to
  `TOK_EQ_OP`, `TOK_NE_OP`, `TOK_LT_OP`, `TOK_LE_OP`, `TOK_GT_OP`, and
  `TOK_GE_OP`.
- Grammar `<MulOp>` includes both `mul_op` and `asterisk`; the parser accepts
  `TOK_MUL_OP` and `TOK_ASTERISK` at the multiplicative level.

## Scope Preserved from the PDF

- A program is a list of declarations only. Top-level statements are rejected;
  `4_test_input_2.clamb` includes top-level statements and is not a valid program
  under the submitted grammar.
- Declarations inside blocks use the unified declaration production from the
  LL(1) version, which also permits nested function declarations as the PDF
  notes.
- `<PostfixTail>` is recursive, so a postfix expression may have any sequence
  of call and index suffixes. The parser consumes suffixes repeatedly, including
  mixed chains such as `a[0](x)[1]`.
- Statements include expression statements and empty statements. The parser
  distinguishes these with `FIRST_EXPR` and `semicolon`, as specified.
- The assignment production permits any logical-OR expression on the left of
  `=`. Restricting assignments to assignable locations (such as identifiers or
  indexed expressions) is a semantic constraint not enforced by this grammar.

The parser follows the submitted syntax rather than silently broadening these
rules. Syntax diagnostics include the source line, expected construct, and
actual token. Lexical errors are also fatal to the parser result even when
error recovery allows the remaining token sequence to parse.