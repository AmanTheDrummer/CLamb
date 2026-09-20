import subprocess, re, sys, os, tempfile
import shutil
HERE = os.path.dirname(os.path.abspath(__file__))
def find_bin():
    """Usage: python run_tests.py [path-to-compiled-lexer]   (or set CLAMB_BIN)"""
    cands = [sys.argv[1]] if len(sys.argv) > 1 else []
    if os.environ.get("CLAMB_BIN"): cands.append(os.environ["CLAMB_BIN"])
    for d in (HERE, os.getcwd()):
        for n in ("clamb_lexer.exe", "clamb_lexer", "lexer.exe", "lexer"):
            cands.append(os.path.join(d, n))
    for c in cands:
        if os.path.isfile(c): return os.path.abspath(c)
    sys.exit("Compiled lexer not found. Build it first, e.g.:\n"
             "    gcc -std=c11 -Wall -Wextra -o clamb_lexer clamb_lexer.c\n"
             "then run:  python run_tests.py clamb_lexer")
BIN = find_bin()
print("Testing binary:", BIN, "\n")
def run(src, stdin=False):
    if stdin:
        p = subprocess.run([BIN], input=src.encode('latin-1'), capture_output=True)
    else:
        with tempfile.NamedTemporaryFile('wb', suffix='.clamb', delete=False) as f:
            f.write(src.encode('latin-1')); n = f.name
        p = subprocess.run([BIN, n], capture_output=True); os.unlink(n)
    text = p.stdout.decode('latin-1').replace('\r\n', '\n').split('\n',2)[2]  # Windows text-mode stdout uses CRLF
    toks = [(m.group(1), m.group(2)) for m in re.finditer(r'^<(\w+)\s*, (.*?)>$', text, re.M|re.S)]
    errs = [l for l in p.stderr.decode('latin-1').splitlines() if l.startswith('Lexical Error')]
    other_err = [l for l in p.stderr.decode('latin-1').splitlines() if l and not l.startswith('Lexical Error')]
    return toks, errs, other_err, p.returncode

I,K,D,B,L = 'identifier','keyword','data_type','boolean_literal','logical_op'
tests = [
 ("relational ops", "a<=b>=c==d!=e<f>g=h", [(I,'a'),('relational_op','<='),(I,'b'),('relational_op','>='),(I,'c'),('relational_op','=='),(I,'d'),('relational_op','!='),(I,'e'),('relational_op','<'),(I,'f'),('relational_op','>'),(I,'g'),('assignment_op','='),(I,'h')], 0),
 ("punctuators+ops", "(){}[];,+-*%&/", [('l_paren','('),('r_paren',')'),('l_brace','{'),('r_brace','}'),('l_bracket','['),('r_bracket',']'),('semicolon',';'),('comma',','),('add_op','+'),('add_op','-'),('asterisk','*'),('mul_op','%'),('address_op','&'),('mul_op','/')], 0),
 ("comment at EOF no newline", "x=a//c", [(I,'x'),('assignment_op','='),(I,'a')], 0),
 ("comment only", "//only a comment", [], 0),
 ("comment then next line", "//c\nx", [(I,'x')], 0),
 ("identifiers/numbers", "2sum _temp my_var __ 007 3.14 0.5 9_9", [(I,'2sum'),(I,'_temp'),(I,'my_var'),(I,'__'),('integer_literal','007'),('float_literal','3.14'),('float_literal','0.5'),(I,'9_9')], 0),
 ("all keywords/types/bools/logical", "if else for while return lamb null print malloc free int float char bool string void auto true false AND OR NOT and Int",
   [(K,w) for w in "if else for while return lamb null print malloc free".split()]+[(D,w) for w in "int float char bool string void auto".split()]+[(B,'true'),(B,'false')]+[(L,'AND'),(L,'OR'),(L,'NOT'),(I,'and'),(I,'Int')], 0),
 ("keyword prefixes are identifiers", "iffy printx truex ANDY intx", [(I,'iffy'),(I,'printx'),(I,'truex'),(I,'ANDY'),(I,'intx')], 0),
 ("strings", '"a b // c" "" "x"', [('string_literal','"a b // c"'),('string_literal','""'),('string_literal','"x"')], 0),
 ("multiline string", '"l1\nl2"', [('string_literal','"l1\nl2"')], 0),
 ("chars", "'x' '1' ' ' ';'", [('char_literal','x'),('char_literal','1'),('char_literal',' '),('char_literal',';')], 0),
 ("unary-ish +-", "a+-b", [(I,'a'),('add_op','+'),('add_op','-'),(I,'b')], 0),
 ("slash vs comment", "a/b //c\n/ d", [(I,'a'),('mul_op','/'),(I,'b'),('mul_op','/'),(I,'d')], 0),
 ("CRLF + tabs", "x\t=\t1;\r\ny=2;\r\n", [(I,'x'),('assignment_op','='),('integer_literal','1'),('semicolon',';'),(I,'y'),('assignment_op','='),('integer_literal','2'),('semicolon',';')], 0),
 ("empty input", "", [], 0),
 ("whitespace only", " \n\t\r\n", [], 0),
 # --- error cases (PDF S_Error behaviour) ---
 ("S3: '3.' then ;", "3. ;", [('semicolon',';')], 1),
 ("S3: '3.' at EOF", "3.", [], 1),
 ("S3: '3.;' pushback keeps ;", "3.;", [('semicolon',';')], 1),
 ("S4: 3.14abc", "3.14abc", [(I,'abc')], 1),
 ("S4: underscore is Other -> float ok", "3.14_x", [('float_literal','3.14'),(I,'_x')], 0),
 ("float then dot", "3.14.15", [('float_literal','3.14'),('integer_literal','15')], 1),
 ("S6: empty char ''", "'' ;", [('semicolon',';')], 1),
 ("S6: lone ' at EOF", "'", [], 1),
 ("S7: 'ab'", "'ab' ;", [(I,'b'),('semicolon',';')], 2),
 ("S7: 'a at EOF", "'a", [], 1),
 ("S11: lone !", "a!b", [(I,'a'),(I,'b')], 1),
 ("S11: ! at EOF", "!", [], 1),
 ("S11: != still ok", "a!=b", [(I,'a'),('relational_op','!='),(I,'b')], 0),
 ("S5: unterminated string", '"abc', [], 1),
 ("S5: unterminated string swallows rest of file (spec: any char stays in S5)", 'x "abc\ny', [(I,'x')], 1),
 ("S0: stray @", "@ y", [(I,'y')], 1),
 ("S_ERROR: junk run = 1 diagnostic", "@#$ x", [(I,'x')], 1),
 ("S_ERROR: junk glued to ident resyncs", "@#$x", [(I,'x')], 1),
 ("S_ERROR: resync on punctuator w/o eating it", "@#;", [('semicolon',';')], 1),
 ("stray dot", "x . y", [(I,'x'),(I,'y')], 1),
 ("dot glued", "a.b", [(I,'a'),(I,'b')], 1),
 ("non-ASCII UTF-8 bytes", "x \xc3\xa9 y", [(I,'x'),(I,'y')], 1),
 ("non-ASCII glued after ident", "x\xc3\xa9", [(I,'x')], 1),
 ("two separate errors", "@ a # b", [(I,'a'),(I,'b')], 2),
]
fail = 0
for name, src, exp, nerr in tests:
    toks, errs, other, rc = run(src)
    ok = (toks == exp and len(errs) == nerr and not other and rc == 0)
    print(("PASS " if ok else "FAIL ") + name)
    if not ok:
        fail += 1
        print("   input   :", repr(src)); print("   expected:", exp, nerr, "errors")
        print("   got     :", toks, len(errs), "errors", errs, other, "rc", rc)

# line numbers in diagnostics
toks, errs, other, rc = run("int a;\nint b;\n  @\nint c = 3.;\n")
ok = len(errs)==2 and 'line 3' in errs[0] and 'line 4' in errs[1]
print(("PASS " if ok else "FAIL ")+"diagnostic line numbers", errs); fail += (not ok)

# multi-line unterminated string: one diagnostic line, reported at the line the string STARTED
toks, errs, other, rc = run('ok\n"abc\ndef\n')
ok = len(errs)==1 and 'line 2' in errs[0] and '\\n' in errs[0] and not other
print(("PASS " if ok else "FAIL ")+"unterminated string diag: start line + escaped newlines", errs, other); fail += (not ok)

# stdin mode
toks, errs, other, rc = run("int x;", stdin=True)
ok = toks == [(D,'int'),(I,'x'),('semicolon',';')]
print(("PASS " if ok else "FAIL ")+"stdin mode"); fail += (not ok)

# very long identifier / string: must not crash (ASAN) ; lexeme is capped
toks, errs, other, rc = run("a"*1000 + " " + '"' + "s"*1000 + '"')
ok = rc==0 and not other and len(toks)==2 and toks[0][0]==I and len(toks[0][1])==255
print(("PASS " if ok else "FAIL ")+"oversized lexeme: no overflow, truncated to 255", [len(t[1]) for t in toks], other[:1]); fail += (not ok)

# missing file
p = subprocess.run([BIN, "/nonexistent.clamb"], capture_output=True)
ok = p.returncode==1 and b"cannot open" in p.stderr
print(("PASS " if ok else "FAIL ")+"missing file -> exit 1"); fail += (not ok)

print("\nTOTAL FAILURES:", fail); sys.exit(1 if fail else 0)