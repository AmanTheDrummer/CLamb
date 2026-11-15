import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
CC = os.environ.get("CC", "gcc")

VALID_PROGRAMS = {
    "valid_core_scopes_recursion_and_conversions": """\
int global = global + 1;
int recurse(int n) {
    if (n == 0) { return 0; }
    return recurse(n - 1);
}
float widen(int p) {
    float result = p;
    { int result = 2; { bool ok = result < p + 1 + 2; if (ok) { print(result); } } }
    return p;
}
int main() {
    int value = recurse(3);
    string text = 'x';
    float number = value;
    int arithmetic = (value * 2) / 2 % 3;
    bool logical = true AND NOT false OR false;
    if (true) { { bool ready = number >= 0.0; print(text, ready); } }
    while (false) { arithmetic = arithmetic - 1; }
    for (; logical; ) { logical = false; }
    return value;
}
""",
    "valid_lambda_auto_and_null": """\
string label = 'a';
string joined = 'b' + label;
bool null_equal = null == null;
int global_value = 7;
auto increment = lamb(int value) { return value + 1; };
auto read_global = lamb() { return global_value; };
auto action = lamb() { print("done"); };
float promote(float value) { return value; }
int main() {
    int value = increment(4);
    float result = promote(value);
    action();
    print(joined, result);
    return value + read_global();
}
""",
}

INVALID_PROGRAMS = {
    "undeclared_identifier": (
        "int main() { return missing; }\n", "undeclared identifier 'missing'"),
    "duplicate_declaration_same_scope": (
        "int main() { int value; float value; return 0; }\n", "duplicate declaration of 'value'"),
    "incompatible_assignment": (
        "int main() { bool ready = 1; return 0; }\n", "cannot initialize bool with int"),
    "incompatible_assignment_statement": (
        "int main() { bool ready; ready = 1; return 0; }\n", "cannot assign int to bool"),
    "non_lvalue_assignment_target": (
        "int main() { (1 + 2) = 3; return 0; }\n", "assignment target is not a variable"),
    "incompatible_operator": (
        "int main() { return true + 1; }\n", "operator '+' is not defined"),
    "floating_remainder_rejected": (
        "float main() { return 3.0 % 2; }\n", "operator '%' is not defined"),
    "wrong_argument_count": (
        "int f(int value) { return value; } int main() { return f(); }\n",
        "expects 1 argument(s), but received 0"),
    "wrong_argument_type": (
        "int f(bool value) { return 0; } int main() { return f(1); }\n",
        "argument 1 has type int; expected bool"),
    "invalid_return_type": (
        "int main() { return \"wrong\"; }\n", "cannot return string from a function returning int"),
    "void_return_must_be_empty": (
        "void noop() {} void use() { return noop(); }\n", "void function cannot return a value"),
    "non_boolean_condition": (
        "int main() { if (1) {} return 0; }\n", "if condition must have type bool"),
    "void_for_condition_rejected": (
        "void noop() {} int use() { for (; noop(); ) {} return 0; }\n",
        "for condition must have type bool"),
    "calling_variable": (
        "int main() { int value = 1; return value(); }\n", "called expression is not a function"),
    "parameter_local_collision": (
        "int f(int value) { int value; return 0; }\n", "duplicate declaration of 'value'"),
    "forward_variable_use": (
        "int main() { return later; }\n", "undeclared identifier 'later'"),
    "forward_function_call": (
        "int first() { return second(); } int second() { return 2; }\n",
        "undeclared identifier 'second'"),
    "undeclared_callee_skips_signature_checks": (
        "int main() { return missing(true); }\n", "undeclared identifier 'missing'"),
    "overloading_rejected": (
        "int f() { return 0; } float f() { return 0.0; }\n",
        "duplicate declaration of 'f'"),
    "noncapturing_lambda": (
        "int f() { int local = 1; auto fn = lamb() { return local; }; return 0; }\n",
        "undeclared identifier 'local'"),
    "uninitialized_auto": (
        "int main() { auto value; return 0; }\n", "auto variable 'value' requires an initializer"),
    "null_auto_initializer": (
        "int main() { auto value = null; return 0; }\n",
        "auto initializer must have a non-void, non-null value"),
    "lambda_return_inference_conflict": (
        "int main() { auto fn = lamb(bool flag) { if (flag) { return 1; } else { return \"x\"; } }; return 0; }\n",
        "lambda return type string conflicts with earlier return type int"),
    "void_variable_rejected": (
        "void value;\n", "variable 'value' cannot have type void"),
    "auto_function_return_rejected": (
        "auto f() { return 1; }\n", "function 'f' cannot have return type auto"),
    "void_parameter_rejected": (
        "int f(void value) { return 0; }\n", "parameter 'value' cannot have type void"),
    "auto_parameter_rejected": (
        "int f(auto value) { return 0; }\n", "parameter 'value' cannot have type auto"),
}

MULTI_ERROR_PROGRAM = """\
int main() {
    bool first = missing + 1;
    bool after = first;
    int second = true;
    int third = another_missing;
    return "bad";
}
"""

INVALID_SYNTAX_AFTER_SEMANTIC_ERROR = "int main() { int value = missing; return 0 }\n"
INVALID_LEXICAL_AFTER_SEMANTIC_ERROR = "int main() { int value = missing; @ return 0; }\n"


def main():
    failures = 0
    with tempfile.TemporaryDirectory(prefix="clamb-semantic-tests-") as temp_dir:
        parser = Path(temp_dir) / ("clamb_parser.exe" if os.name == "nt" else "clamb_parser")
        build = subprocess.run(
            [CC, "-std=c11", "-Wall", "-Wextra", "-DCLAMB_LEXER_NO_MAIN",
             "-o", str(parser), str(ROOT / "clamb_parser.c"),
             str(ROOT / "clamb_semantic.c"), str(ROOT / "clamb_lexer.c")],
            capture_output=True, text=True,
        )
        if build.returncode:
            sys.stderr.write(build.stderr)
            return 1

        cases = [(name, source, True, None) for name, source in VALID_PROGRAMS.items()]
        cases += [(name, source, False, diagnostic)
                  for name, (source, diagnostic) in INVALID_PROGRAMS.items()]
        cases.append(("independent_errors_without_cascades", MULTI_ERROR_PROGRAM, False, None))

        for index, (name, source, should_pass, expected) in enumerate(cases):
            source_path = Path(temp_dir) / f"semantic_case_{index}.clamb"
            source_path.write_text(source, encoding="ascii")
            result = subprocess.run([str(parser), str(source_path)], capture_output=True, text=True)
            output = result.stdout + result.stderr
            diagnostics = [line for line in output.splitlines() if line.startswith("[Semantic Error]")]
            passed = (result.returncode == 0) == should_pass
            if should_pass:
                passed = passed and "Semantic analysis successful." in output
            elif expected:
                passed = passed and any(expected in line for line in diagnostics)
            if diagnostics:
                passed = passed and all(re.search(r"line \d+, column \d+", line) for line in diagnostics)
            if name == "independent_errors_without_cascades":
                passed = passed and len(diagnostics) == 4
                passed = passed and any("undeclared identifier 'missing'" in line for line in diagnostics)
                passed = passed and not any("operator '+'" in line for line in diagnostics)
                passed = passed and any("cannot initialize int with bool" in line for line in diagnostics)
                passed = passed and any("cannot return string" in line for line in diagnostics)
            print(("PASS " if passed else "FAIL ") + name)
            for line in output.splitlines():
                print("  " + line)
            if not passed:
                failures += 1
                print("  exit:", result.returncode)

        fixture_cases = (
            ("semantic_valid_test.clamb", True, None),
            ("semantic_valid_test_2.clamb", True, None),
            ("semantic_invalid_test.clamb", False, (
                "undeclared identifier", "duplicate declaration", "cannot initialize",
                "cannot assign", "operator '+'", "expects 1 argument(s)",
                "argument 1 has type", "cannot return string", "if condition",
                "while condition", "for condition", "not a function",
                "undeclared identifier 'hidden'",
                "duplicate declaration of 'parameter'",
                "undeclared identifier 'missing_for_cascade'",
            )),
            ("semantic_multi_error_test.clamb", False, (
                "undeclared identifier 'missing'", "cannot initialize bool with int",
                "cannot initialize int with float", "cannot return string",
            )),
        )
        for filename, should_pass, expected_messages in fixture_cases:
            result = subprocess.run([str(parser), str(HERE / filename)],
                                    capture_output=True, text=True)
            output = result.stdout + result.stderr
            diagnostics = [line for line in output.splitlines()
                           if line.startswith("[Semantic Error]")]
            passed = (result.returncode == 0) == should_pass
            if should_pass:
                passed = passed and "Semantic analysis successful." in output
            else:
                passed = passed and "[Syntax Error]" not in output
                passed = passed and "[Lexical Error]" not in output
                passed = passed and all(any(message in line for line in diagnostics)
                                        for message in expected_messages)
            if filename == "semantic_invalid_test.clamb":
                fixture_source = (HERE / filename).read_text(encoding="utf-8").splitlines()
                cascade_lines = (
                    (fixture_source.index("    int from_undeclared = missing_for_cascade + true;") + 1,
                     "undeclared identifier 'missing_for_cascade'"),
                    (fixture_source.index("    int from_invalid_operator = true + 1;") + 1,
                     "operator '+' is not defined"),
                    (fixture_source.index("    return from_undeclared + from_invalid_operator;") + 1,
                     None),
                )
                for line_number, expected_message in cascade_lines:
                    line_diagnostics = [line for line in diagnostics
                                        if f"line {line_number}," in line]
                    if expected_message is None:
                        passed = passed and not line_diagnostics
                    else:
                        passed = passed and len(line_diagnostics) == 1
                        passed = passed and expected_message in line_diagnostics[0]
            if filename == "semantic_multi_error_test.clamb":
                passed = passed and len(diagnostics) == 4
                passed = passed and not any("operator '+'" in line for line in diagnostics)
                passed = passed and not any("operator '*'" in line for line in diagnostics)
            print(("PASS " if passed else "FAIL ") + f"fixture {filename}")
            for line in output.splitlines():
                print("  " + line)
            if not passed:
                failures += 1

        syntax_path = Path(temp_dir) / "syntax_after_semantic_error.clamb"
        syntax_path.write_text(INVALID_SYNTAX_AFTER_SEMANTIC_ERROR, encoding="ascii")
        syntax_result = subprocess.run([str(parser), str(syntax_path)], capture_output=True, text=True)
        syntax_output = syntax_result.stdout + syntax_result.stderr
        syntax_ok = (syntax_result.returncode != 0 and "[Syntax Error]" in syntax_output
                     and "[Semantic Error]" not in syntax_output)
        print(("PASS " if syntax_ok else "FAIL ") + "syntax failure suppresses semantic diagnostics")
        for line in syntax_output.splitlines():
            print("  " + line)
        if not syntax_ok:
            failures += 1

        lexical_path = Path(temp_dir) / "lexical_after_semantic_error.clamb"
        lexical_path.write_text(INVALID_LEXICAL_AFTER_SEMANTIC_ERROR, encoding="ascii")
        lexical_result = subprocess.run([str(parser), str(lexical_path)], capture_output=True, text=True)
        lexical_output = lexical_result.stdout + lexical_result.stderr
        lexical_ok = (lexical_result.returncode != 0 and "[Lexical Error]" in lexical_output
                      and "[Semantic Error]" not in lexical_output)
        print(("PASS " if lexical_ok else "FAIL ") + "lexical failure suppresses semantic diagnostics")
        for line in lexical_output.splitlines():
            print("  " + line)
        if not lexical_ok:
            failures += 1

        parser.unlink(missing_ok=True)

    print("\nTOTAL FAILURES:", failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
