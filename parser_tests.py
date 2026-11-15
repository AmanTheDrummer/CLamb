import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time


HERE = Path(__file__).resolve().parent
CC = os.environ.get("CC", "gcc")
VALID = ("parser_valid_1.clamb", "parser_valid_2.clamb")
INVALID = ("parser_invalid_1.clamb", "parser_invalid_2.clamb", "parser_invalid_3.clamb")
LEXICAL_INVALID = "parser_invalid_lexical.clamb"
PDF_FIXTURE_CASES = (
    ("4_test_input_4.clamb", "4_test_input_4.clamb", False),
    ("legacy 4_test_input_3.clamb uses removed pointers and arrays",
     "4_test_input_3.clamb", False),
    ("legacy 4_test_input_5.clamb uses removed arrays and pointers",
     "4_test_input_5.clamb", False),
    ("4_test_input_6.clamb", "4_test_input_6.clamb", False),
)
NONTERMINALS = (
    "program_start", "decl", "decl_tail", "var_init",
    "param_list", "param_tail", "statement", "var_decl", "expr_stmt",
    "if", "else_opt", "while", "for", "return", "print",
    "expr_opt", "expr", "assign_expr", "assign_tail", "or_expr",
    "or_tail", "and_expr", "and_tail", "eq_expr", "eq_tail",
    "relational_expr", "relational_tail", "additive_expr",
    "additive_tail", "mul_expr", "mul_tail", "unary_expr",
    "postfix_expr", "postfix_tail", "primary_expr", "expr_list",
    "nonempty_expr_list", "expr_list_tail",
)

GRAMMAR_CASES = (
    ("empty program", "", True),
    (
        "declarations, lambdas, control flow, and chained calls",
        """int global;
int calculate(int data, int limit) {
    auto apply = lamb(int value) {
        if (value > 0) {
            return value;
        } else if (value == 0) {
            return 0;
        } else {
            return -value;
        }
    };
    for (; limit > 0; limit = limit - 1) {
        data = global = apply(limit);
    }
    while (limit < 0) {
        limit = limit + 1;
    }
    print();
    return apply(limit)(global);
}
""",
        True,
    ),
    (
        "recursive functions and nested call expressions",
        """int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}
int main() {
    int sequence = 10;
    auto element = lamb(int items, int index) {
        return items + index;
    };
    int value = element(sequence, 5);
    if (value != null) {
        print("value:", value);
    }
    return fibonacci(6);
}
""",
        True,
    ),
    ("empty for clauses", "int main() { for (;;) {} return 0; }", True),
    ("asterisk, division, and remainder are multiplicative", "int main() { return 2 * 3 / 1 % 2; }", True),
    (
        "chained call expression statement requires a semicolon",
        "int main() { b(e=4)(v+f); }",
        True,
    ),
    (
        "chained call expression statement without a semicolon",
        "int main() { b(e=4)(v+f) }",
        False,
    ),
    (
        "nested function declaration is not a statement",
        "int main() { int nested() {} return 0; }",
        False,
    ),
    (
        "else must be followed by a block or if",
        "int main() { if (true) {} else value; }",
        False,
    ),
    ("top-level expression is not a declaration", "int value; value = 1;", False),
    ("trailing parameter comma", "int f(int value,) {}", False),
    ("pointer declarator is not in the grammar", "int *value;", False),
    ("pointer dereference is not in the grammar", "int main() { return *value; }", False),
    ("array declarator is not in the grammar", "int values[3];", False),
    ("index expression is not in the grammar", "int main() { return values[0]; }", False),
    ("address-of operator is not in the grammar", "int main() { return &value; }", False),
)

def remove_test_binary(path):
    for attempt in range(10):
        try:
            path.unlink(missing_ok=True)
            return
        except PermissionError:
            if os.name != "nt" or attempt == 9:
                raise
            time.sleep(0.1)


def main():
    failures = 0
    parser_source = (HERE / "clamb_parser.c").read_text(encoding="utf-8")
    parser_functions = tuple(
        re.findall(r"static void parse_([a-z_]+)\(void\) \{", parser_source)
    )
    count_ok = parser_functions == NONTERMINALS
    print(("PASS " if count_ok else "FAIL ")
          + f"one parser function per LL(k) nonterminal ({len(parser_functions)}/{len(NONTERMINALS)})")
    if not count_ok:
        print("  functions:", parser_functions)
        failures += 1

    else_opt_uses_two_tokens = (
        "if (lookahead2.type == TOK_L_BRACE)" in parser_source
        and "} else if (lookahead2.type == TOK_KW_IF)" in parser_source
    )
    print(("PASS " if else_opt_uses_two_tokens else "FAIL ")
          + "else_opt uses k=2 lookahead")
    if not else_opt_uses_two_tokens:
        failures += 1

    with tempfile.TemporaryDirectory(prefix="clamb-parser-tests-") as temp_dir:
        parser = Path(temp_dir) / ("clamb_parser.exe" if os.name == "nt" else "clamb_parser")
        build = subprocess.run(
            [CC, "-std=c11", "-Wall", "-Wextra", "-DCLAMB_LEXER_NO_MAIN",
             "-o", str(parser), str(HERE / "clamb_parser.c"), str(HERE / "clamb_lexer.c")],
            capture_output=True, text=True,
        )
        if build.returncode:
            sys.stderr.write(build.stderr)
            return 1

        cases = [(name, HERE / name, True, "Parse successful.") for name in VALID]
        cases += [(name, HERE / name, False, "Syntax Error at line") for name in INVALID]
        cases.append((LEXICAL_INVALID, HERE / LEXICAL_INVALID, False, "Parse rejected:"))
        cases += [
            (label, HERE / filename, should_pass,
             "Parse successful." if should_pass else "Syntax Error at line")
            for label, filename, should_pass in PDF_FIXTURE_CASES
        ]

        for index, (name, source, should_pass) in enumerate(GRAMMAR_CASES):
            source_path = Path(temp_dir) / f"grammar_case_{index}.clamb"
            source_path.write_text(source, encoding="ascii")
            if should_pass:
                expected_text = "Parse successful."
            elif name == "address-of operator is not in the grammar":
                expected_text = "Parse rejected:"
            else:
                expected_text = "Syntax Error at line"
            cases.append((name, source_path, should_pass, expected_text))

        for name, source, should_pass, expected_text in cases:
            result = subprocess.run(
                [str(parser), str(source)], capture_output=True, text=True,
            )
            passed = (result.returncode == 0) == should_pass
            output = result.stdout + result.stderr
            passed = passed and expected_text in output
            print(("PASS " if passed else "FAIL ") + name)
            for line in output.splitlines():
                print("  " + line)
            if not passed:
                failures += 1
                print("  exit:", result.returncode)

        remove_test_binary(parser)

    print("\nTOTAL FAILURES:", failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())