# CLamb Semantic Implementation

## Integration

`clamb_parser.c` performs semantic actions as it commits to productions in its deterministic LL(2) recursive-descent parse. Expression results carry a `SemType *` and an l-value flag upward. The active return context and symbol scope are carried on parser-side context stacks. There is no speculative parse or backtracking. The function signature is inserted after `)` and before parsing its body, allowing self-recursion but not calls to later functions.

Action points are implemented at declaration parsing (`parse_variable` and `parse_decl_tail`), parameter insertion (`insert_parameters`), scope entry/exit in block and control parsers, identifier lookup and literal synthesis in `parse_primary_expr`, operator checks in precedence parsers, assignment checking in `parse_assign_expr`, call validation in `parse_postfix_expr`, and condition/return/print checks in their statement parsers. Synthesized expression types are produced before a parent checks them; declared and expected-return types are available before parsing initializers/bodies; each operator tail receives the accumulated left type from the preceding expression. This is the one-pass evaluation order.

The parser collects a parameter list before entering the function/lambda body. On `{`, it enters the body scope and inserts those parameters before parsing statements. Ordinary brace blocks enter child scopes; lambda scopes have no enclosing local parent, but lookups still fall back to global scope. Function parameters and declarations in the outermost function body share a scope, so a local cannot reuse a parameter name. Inner blocks may shadow names.

Explicitly typed variables are inserted before their initializer is checked, so self-reference such as `int x = x + 1;` resolves. `auto` variables are inserted after initializer analysis; they require a non-void, non-null initializer. An erroneous initializer still leaves an explicitly typed declaration in its scope. An `auto` declaration with an erroneous or unusable initializer is entered with the error type to prevent cascades.

## Compatibility

Implicit conversions are applied during checking; explicit casts are not in the grammar.

| Context/operator | Accepted types | Result |
| --- | --- | --- |
| Assignment, initialization, argument, return | Same type; `int` to `float`; `char` to `string` | Target type |
| Function/lambda assignment | Exact parameter and return signature | Function type |
| `+` | Numeric operands, or `char`/`string` pairs | Promoted numeric type, or `string` |
| `-`, `*`, `/` | `int` and/or `float` | `float` if either operand is `float`, otherwise `int` |
| `%` | `int`, `int` | `int` |
| `<`, `>`, `<=`, `>=` | Numeric operands | `bool` |
| `==`, `!=` | Same type or a permitted widening pair; `null` with `null` | `bool` |
| `AND`, `OR`, `NOT` | `bool` operands | `bool` |
| Unary `+`, `-` | `int` or `float` | Operand type |

Overloading is rejected as a duplicate declaration. Return statements are checked against the declared return type; `void` requires an empty return, and valid widening conversions are accepted. Missing-return-path analysis is out of scope. Lambdas are non-capturing for locals, with typed parameters and a return type inferred from their returns; no return defaults to `void`. `null` is only accepted for equality/inequality when both operands are null.

## Scope and Symbols

Each symbol stores its name, kind (`VAR`, `PARAM`, or `FUNC`), type, scope ID, declaration line and column, and a link in its scope table. Function types store parameter types and a return type. Each scope has a parent link; lookup searches the current scope outward, and isolated lambdas additionally search the global scope. Insertion checks duplicates only in the current scope.

## Diagnostics and Recovery

Diagnostics are categorized as `[Lexical Error]`, `[Syntax Error]`, or `[Semantic Error]` and include one-based line and column. Semantic messages are buffered and printed only if the complete input has no lexical or syntax error. Semantic errors do not stop parsing. The dedicated error type propagates through operators and assignments without secondary type-mismatch messages. Undeclared identifiers are not inserted. An undeclared callee yields the error type and is not checked against a signature; a declared call with wrong arguments reports the mismatch but retains its declared return type.

Build and run:

```text
gcc -std=c11 -Wall -Wextra -DCLAMB_LEXER_NO_MAIN -o clamb_parser clamb_parser.c clamb_semantic.c clamb_lexer.c
./clamb_parser source.clamb
python parser_tests.py
python semantic_tests.py
```

## Test Coverage

`semantic_valid_test.clamb` and `semantic_valid_test_2.clamb` are the two runnable valid programs. `semantic_invalid_test.clamb` demonstrates the required semantic error categories, and `semantic_multi_error_test.clamb` is the separate recovery example. `semantic_tests.py` runs all four files as part of its regression suite. It also checks shadowing, parameter/local collisions, declaration-before-use, recursion, forward-call rejection, self-reference, overloading rejection, widening conversions, lambda scope/inference, null comparisons, and suppression of semantic messages after lexical or syntax failure. Each emitted semantic diagnostic is checked for its source location.

The valid scope/recursion/conversion program exercises at least three nested scope levels, function parameters and calls, initialized declarations, and control flow. A representative attribute/action trace for that program is:

| Parser event | Attribute/action effect |
| --- | --- |
| Parse `float widen(int p)` | `widen` signature enters scope 0 before its body; return context is `float` |
| Consume function `{` | Enter scope 1; insert parameter `p:int` |
| Parse `float result = p` | Declare `result:float`; resolve `p:int`; assignment compatibility widens `int` to `float` |
| Enter nested `{ ... { ... } ... }` | Enter scopes 2 and 3; declarations there are isolated from scope 1 and are removed from lookup when each block closes |
| Parse `if (ok)` | Synthesized condition type is `bool`; check succeeds; its braced body enters another child scope |
| Parse `return p` | Inherited expected return type is `float`; synthesized expression type is `int`; return check accepts widening |
| Parse `recurse(3)` in `main` | Resolve the earlier function signature; argument type `int` matches; call synthesizes the function return type |
| Close function/body braces | Exit each scope to its parent; inner shadowed declarations no longer participate in lookup |

The expression attribute accumulator follows the left-associative additive chain: `p:int + 1:int + 2:int` passes `int` to the next additive tail after each operand, so the following operand is checked against that inherited type. Each child expression is synthesized before its type is passed to the tail, so dependencies flow left-to-right.
