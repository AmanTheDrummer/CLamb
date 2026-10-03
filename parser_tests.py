import os
from pathlib import Path
import subprocess
import sys


HERE = Path(__file__).resolve().parent
CC = os.environ.get("CC", "gcc")
VALID = ("parser_valid_1.clamb", "parser_valid_2.clamb")
INVALID = ("parser_invalid_1.clamb", "parser_invalid_2.clamb", "parser_invalid_3.clamb")
LEXICAL_INVALID = "parser_invalid_lexical.clamb"


def main():
    failures = 0
    parser = HERE / ("clamb_parser.exe" if os.name == "nt" else "clamb_parser")
    build = subprocess.run(
        [CC, "-std=c11", "-Wall", "-Wextra", "-DCLAMB_LEXER_NO_MAIN",
         "-o", str(parser), str(HERE / "clamb_parser.c"), str(HERE / "clamb_lexer.c")],
        capture_output=True, text=True,
    )
    if build.returncode:
        sys.stderr.write(build.stderr)
        return 1

    cases = [(name, True, "Parse successful.") for name in VALID]
    cases += [(name, False, "Syntax Error at line") for name in INVALID]
    cases.append((LEXICAL_INVALID, False, "Parse rejected:"))

    for name, should_pass, expected_text in cases:
        result = subprocess.run(
            [str(parser), str(HERE / name)], capture_output=True, text=True,
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

    print("\nTOTAL FAILURES:", failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())