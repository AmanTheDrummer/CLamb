# CLamb — DFA Transition Table

This is the exact table implemented in `table[state][class]` inside `clamb_lexer.c`.
The scanner's main loop performs **only** table lookups per character — no
per-character if/else or switch logic is used to recognize tokens.

## States

| State | Meaning |
|---|---|
| `S0` | Start state |
| `S1` | Identifier / reserved-word lexeme accumulation |
| `S2` | Digit-initial (integer, float, or digit-led identifier) |
| `S3` | Float dot seen — must be followed by a digit |
| `S4` | Float digits accumulation |
| `S5` | String literal body |
| `S6` | Char literal — just saw opening `'` |
| `S7` | Char literal — expecting closing `'` |
| `S8` | Saw `=` |
| `S9` | Saw `<` |
| `S10` | Saw `>` |
| `S11` | Saw `!` |
| `S12` | Saw `/` |
| `S13` | Comment body (after `//`) |
| `S_ERROR` | Panic-mode: discard and resync |

## Reserved-Word Classes

Per the updated CLamb Lexical Specification, keywords are **not** interchangeable, so each reserved word gets its own token class instead of a shared `keyword` class:

| Lexeme | Class |
|---|---|
| `if` | `kw_if` |
| `else` | `kw_else` |
| `for` | `kw_for` |
| `while` | `kw_while` |
| `return` | `kw_return` |
| `lamb` | `kw_lamb` |
| `null` | `kw_null` |
| `print` | `kw_print` |
| `malloc` | `kw_malloc` |
| `free` | `kw_free` |

This only changes which class the S1 reserved-word lookup assigns; the DFA states, transitions, and every other token class are unchanged.

## Character Classes

`LETTER, DIGIT, UNDERSCORE, DQUOTE("), SQUOTE('), EQUALS(=), LESS(<), GREATER(>), BANG(!), SLASH(/), ADDSUB(+ or -), STAR(*), PERCENT(%), AMP(&), LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, SEMI(;), COMMA(,), DOT(.), NEWLINE, WS(space/tab/CR), OTHER, EOF`

## Transition Table

Columns: **Next State** | **Append to lexeme?** | **Pushback (not consumed)?** | **Accept / emits?**

### S0 (Start)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| LETTER | S1 | Y | N | — | — |
| DIGIT | S2 | Y | N | — | — |
| UNDERSCORE | S1 | Y | N | — | — |
| DQUOTE | S5 | Y | N | — | — |
| SQUOTE | S6 | N | N | — | — |
| EQUALS | S8 | Y | N | — | — |
| LESS | S9 | Y | N | — | — |
| GREATER | S10 | Y | N | — | — |
| BANG | S11 | Y | N | — | — |
| SLASH | S12 | Y | N | — | — |
| ADDSUB | S0 | Y | N | **Yes** | `add_op` |
| STAR | S0 | Y | N | **Yes** | `asterisk` |
| PERCENT | S0 | Y | N | **Yes** | `mul_op` |
| AMP | S0 | Y | N | **Yes** | `address_op` |
| LPAREN…COMMA (8 classes) | S0 | Y | N | **Yes** | `l_paren`/`r_paren`/`l_brace`/`r_brace`/`l_bracket`/`r_bracket`/`semicolon`/`comma` (one per class) |
| NEWLINE, WS | S0 | N | N | — | *(skip)* |
| DOT, OTHER | **S_ERROR** | N | N | — | — |
| EOF | S0 | N | N | **Yes** | `EOF` |

### S1 (Identifier)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| LETTER, DIGIT, UNDERSCORE | S1 | Y | N | — | — |
| *default (all other classes)* | S0 | N | **Y** | **Yes** | reserved-word lookup → `data_type` / one of `kw_if`,`kw_else`,`kw_for`,`kw_while`,`kw_return`,`kw_lamb`,`kw_null`,`kw_print`,`kw_malloc`,`kw_free` / `boolean_literal` / `and_op` / `or_op` / `not_op` / else `identifier` |
| EOF | S0 | N | N | **Yes** | (same lookup) |

### S2 (Digit-initial)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| DIGIT | S2 | Y | N | — | — |
| UNDERSCORE, LETTER | S1 | Y | N | — | — *(becomes identifier, e.g. `2sum`)* |
| DOT | S3 | Y | N | — | — |
| *default* | S0 | N | **Y** | **Yes** | `integer_literal` |
| EOF | S0 | N | N | **Yes** | `integer_literal` |

### S3 (Float dot)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| DIGIT | S4 | Y | N | — | — |
| *default (Other)* | **S_ERROR** | N | **Y** | — | — *(malformed float, e.g. `"3."`; diagnostic emitted)* |
| EOF | **S_ERROR** | N | N | — | — *(diagnostic emitted)* |

### S4 (Float digits)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| DIGIT | S4 | Y | N | — | — |
| LETTER | **S_ERROR** | N | **Y** | — | — *(e.g. `"3.14abc"`; diagnostic emitted)* |
| *default (Other, incl. `_`)* | S0 | N | **Y** | **Yes** | `float_literal` |
| EOF | S0 | N | N | **Yes** | `float_literal` |

### S5 (String literal body)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| *default (any char)* | S5 | Y | N | — | — |
| DQUOTE | S0 | Y | N | **Yes** | `string_literal` (quotes included) |
| EOF | **S_ERROR** | N | N | — | — *(unterminated string; diagnostic emitted)* |

### S6 (Char literal start)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| *default (any char)* | S7 | Y | N | — | — |
| SQUOTE (immediate) | **S_ERROR** | N | N | — | — *(empty char literal `''`; the 2nd `'` is consumed; diagnostic emitted)* |
| EOF | **S_ERROR** | N | N | — | — *(unterminated; diagnostic emitted)* |

### S7 (Char literal end)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| SQUOTE | S0 | N | N | **Yes** | `char_literal` (quote dropped) |
| *default (Other)* | **S_ERROR** | N | **Y** | — | — *(malformed/unterminated; diagnostic emitted)* |
| EOF | **S_ERROR** | N | N | — | — *(diagnostic emitted)* |

### S8 (saw `=`)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| EQUALS | S0 | Y | N | **Yes** | `eq_op` (`==`) |
| *default* | S0 | N | **Y** | **Yes** | `assignment_op` (`=`) |
| EOF | S0 | N | N | **Yes** | `assignment_op` |

### S9 (saw `<`)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| EQUALS | S0 | Y | N | **Yes** | `le_op` (`<=`) |
| *default* | S0 | N | **Y** | **Yes** | `lt_op` (`<`) |
| EOF | S0 | N | N | **Yes** | `lt_op` |

### S10 (saw `>`)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| EQUALS | S0 | Y | N | **Yes** | `ge_op` (`>=`) |
| *default* | S0 | N | **Y** | **Yes** | `gt_op` (`>`) |
| EOF | S0 | N | N | **Yes** | `gt_op` |

### S11 (saw `!`)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| EQUALS | S0 | Y | N | **Yes** | `ne_op` (`!=`) |
| *default (Other)* | **S_ERROR** | N | **Y** | — | — *(lone `!` invalid; diagnostic emitted)* |
| EOF | **S_ERROR** | N | N | — | — *(diagnostic emitted)* |

### S12 (saw `/`)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| SLASH | S13 | N | N | — | — *(comment starts)* |
| *default* | S0 | N | **Y** | **Yes** | `mul_op` (`/`) |
| EOF | S0 | N | N | **Yes** | `mul_op` |

### S13 (Comment body)
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| *default* | S13 | N | N | — | — |
| NEWLINE | S0 | N | N | — | *(comment discarded, no token)* |
| EOF | S0 | N | N | — | *(comment discarded, no token)* |

### S_ERROR (panic-mode recovery)
On entry from any state the scanner emits **one** diagnostic (to stderr) naming the
malformed lexeme and line; no token is added to the token stream.
| Class | Next | Append | Pushback | Accept | Token |
|---|---|---|---|---|---|
| *default (still garbage)* | S_ERROR | N | N | — | — |
| WS, NEWLINE, EOF | S0 | N | N | — | *(resync, discard the whitespace)* |
| any recognizable token-start class (letter, digit, underscore, quotes, operator/punctuator characters) | S0 | N | **Y** | — | *(resync, re-lex this character fresh from S0)* |

Note: `DOT` and generic `OTHER` characters are *not* treated as "recognizable
start" classes inside `S_ERROR`, since a bare `.` is not a valid CLamb token
start either — it stays in panic-mode discard until something genuinely
recognizable (or whitespace/EOF) appears.

## Error / Dead States

- **`S_ERROR`** is the sole dead/error state. Per the CLamb Lexical Specification it is
  non-accepting: it emits one diagnostic on entry, discards characters, and resumes
  normal tokenizing on whitespace/newline/EOF, or (without consuming the character)
  on any recognizable token-start character.
- States with a transition into `S_ERROR`:
  - `S0` on `DOT` / `OTHER` (unrecognized character, consumed);
  - `S3` on Other/EOF (`3.` — digit required after the dot; offending char pushed back);
  - `S4` on `LETTER` (`3.14abc`; letter pushed back). Note `_` is *Other* here, so
    `3.14_x` is the float `3.14` followed by the identifier `_x`;
  - `S5` on EOF (unterminated string);
  - `S6` on `'` (empty char literal `''`, consumed) and on EOF;
  - `S7` on Other/EOF (`'ab'`, unterminated char; offending char pushed back);
  - `S11` on Other/EOF (lone `!`; pushed back, per the specification).
- Because the offending character is pushed back where the specification is silent,
  the token that follows a malformed lexeme is still lexed correctly (e.g. `3.;`
  reports the malformed float and still emits `<semicolon, ;>`).
- EOF handling is not shown in the specification's table; the implementation adds
  an `EOF` column so accepting states finalize their token at end of input.