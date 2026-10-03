# Independent transcription of PDF Section 2 ("DFA Transition Table"), compared cell-by-cell to the table built by the C code.
CL = "LETTER DIGIT UNDERSCORE DQUOTE SQUOTE EQUALS LESS GREATER BANG SLASH ADDSUB STAR PERCENT AMP LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET SEMI COMMA DOT NEWLINE WS OTHER EOF".split()
ST = "S0 S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S_ERROR".split()
PUNCT = "ADDSUB STAR PERCENT AMP LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET SEMI COMMA".split()
START = "LETTER DIGIT UNDERSCORE DQUOTE SQUOTE EQUALS LESS GREATER BANG SLASH".split() + PUNCT
# spec cell = (next_state, accept, pushback)   pushback=None => PDF does not say (we only check next/accept)
ACC_PB = ("S0", True, True)      # "<X>(Accept), not consumed, pushback"
ACC    = ("S0", True, False)     # accept, char consumed
E = "S_ERROR"
spec = {}
def row(st, explicit, other, skip=()):
    for c in CL:
        if c in skip: continue
        spec[(st,c)] = explicit.get(c, other)
# S0 -- Other (= DOT, OTHER) -> S_Error ; EOF not in the PDF (checked separately)
e = {"LETTER":("S1",False,False),"DIGIT":("S2",False,False),"UNDERSCORE":("S1",False,False),"DQUOTE":("S5",False,False),
     "SQUOTE":("S6",False,False),"EQUALS":("S8",False,False),"LESS":("S9",False,False),"GREATER":("S10",False,False),
     "BANG":("S11",False,False),"SLASH":("S12",False,False),"WS":("S0",False,False),"NEWLINE":("S0",False,False)}
e.update({p:ACC for p in PUNCT})
row("S0", e, (E,False,False), skip=("EOF",))
row("S1", {"LETTER":("S1",False,False),"DIGIT":("S1",False,False),"UNDERSCORE":("S1",False,False)}, ACC_PB, skip=("EOF",))
row("S2", {"DIGIT":("S2",False,False),"UNDERSCORE":("S1",False,False),"LETTER":("S1",False,False),"DOT":("S3",False,False)}, ACC_PB, skip=("EOF",))
row("S3", {"DIGIT":("S4",False,False)}, (E,False,None), skip=("EOF",))
row("S4", {"DIGIT":("S4",False,False),"LETTER":(E,False,None)}, ACC_PB, skip=("EOF",))
row("S5", {"DQUOTE":ACC}, ("S5",False,False), skip=("EOF",))
row("S6", {"SQUOTE":(E,False,None)}, ("S7",False,False), skip=("EOF",))
row("S7", {"SQUOTE":ACC}, (E,False,None), skip=("EOF",))
for s in ("S8","S9","S10"): row(s, {"EQUALS":ACC}, ACC_PB, skip=("EOF",))
row("S11", {"EQUALS":ACC}, (E,False,True), skip=("EOF",))
row("S12", {"SLASH":("S13",False,False)}, ACC_PB, skip=("EOF",))
row("S13", {"NEWLINE":("S0",False,False)}, ("S13",False,False), skip=("EOF",))
e = {"WS":("S0",False,False),"NEWLINE":("S0",False,False)}
e.update({c:("S0",False,True) for c in START})
row("S_ERROR", e, (E,False,False), skip=("EOF",))

import subprocess, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
exe = os.path.join(HERE, "_dump_table" + (".exe" if os.name == "nt" else ""))
r = subprocess.run(["gcc", "-std=c11", "-o", exe, os.path.join(HERE, "dump.c")], capture_output=True, text=True, cwd=HERE)
if r.returncode: sys.exit("Could not compile dump.c (needs gcc and clamb_lexer.c in the same folder):\n" + r.stderr)
dump = subprocess.run([exe], capture_output=True, text=True).stdout
code = {}
for l in dump.splitlines():
    s,c,n,a,p,ac,tok = l.split()
    code[(ST[int(s)], CL[int(c)])] = (ST[int(n)], bool(int(ac)), bool(int(p)), int(a), tok)
bad = 0; checked = 0
for (s,c),(n,ac,pb) in sorted(spec.items()):
    cn,cac,cpb,capp,tok = code[(s,c)]
    checked += 1
    if cn!=n or cac!=ac or (pb is not None and cpb!=pb):
        bad += 1; print("MISMATCH", s, c, "spec=",(n,ac,pb), "code=",(cn,cac,cpb))
print(f"cells compared against PDF: {checked}, mismatches: {bad}")
# reserved-word classes the S1 lookup must resolve to (data-driven, not part
# of the DFA table itself, but worth checking against the running binary)
import subprocess as _sp
BIN_FOR_KW = None
for cand in ("clamb_lexer","clamb_lexer.exe","lexer","lexer.exe"):
    import os as _os
    if _os.path.isfile(_os.path.join(HERE, cand)):
        BIN_FOR_KW = _os.path.join(HERE, cand); break
if BIN_FOR_KW:
    kw = "if else for while return lamb null print malloc free"
    out = _sp.run([BIN_FOR_KW], input=kw, capture_output=True, text=True).stdout
    got = [l.split(',')[0].strip('<> ') for l in out.splitlines() if l.startswith('<')]
    want = ["kw_"+w for w in kw.split()]
    print("keyword classes:", "OK, all distinct per-keyword classes" if got==want else f"MISMATCH got={got} want={want}")
else:
    print("keyword classes: skipped (no compiled lexer found next to verify_table.py)")

# token classes the PDF assigns to accepting cells
tok_spec = {("S0","ADDSUB"):"add_op",("S0","STAR"):"asterisk",("S0","PERCENT"):"mul_op",("S0","AMP"):"address_op",
 ("S0","LPAREN"):"l_paren",("S0","RPAREN"):"r_paren",("S0","LBRACE"):"l_brace",("S0","RBRACE"):"r_brace",
 ("S0","LBRACKET"):"l_bracket",("S0","RBRACKET"):"r_bracket",("S0","SEMI"):"semicolon",("S0","COMMA"):"comma",
 ("S1","OTHER"):"dynamic",("S2","OTHER"):"integer_literal",("S4","OTHER"):"float_literal",("S5","DQUOTE"):"string_literal",
 ("S7","SQUOTE"):"char_literal",("S8","EQUALS"):"eq_op",("S8","OTHER"):"assignment_op",("S9","EQUALS"):"le_op",
 ("S9","OTHER"):"lt_op",("S10","EQUALS"):"ge_op",("S10","OTHER"):"gt_op",("S11","EQUALS"):"ne_op",
 ("S12","OTHER"):"mul_op"}
tb = 0
for k,v in tok_spec.items():
    if code[k][4]!=v: tb+=1; print("TOKEN MISMATCH",k,"spec",v,"code",code[k][4])
print(f"accepting-cell token classes checked: {len(tok_spec)}, mismatches: {tb}")
# append flags the token-set text implies: opening ' dropped, closing ' dropped, quotes of strings kept
print("lexeme conventions:", "S0 ' dropped" if code[("S0","SQUOTE")][3]==0 else "BAD",
      "| S0 \" kept" if code[("S0","DQUOTE")][3]==1 else "| BAD",
      "| S5 closing \" kept" if code[("S5","DQUOTE")][3]==1 else "| BAD",
      "| S7 closing ' dropped" if code[("S7","SQUOTE")][3]==0 else "| BAD")
# S_ERROR must be unreachable-from-nowhere: which states can enter it
print("states with a transition INTO S_ERROR:", sorted({s for (s,c),v in code.items() if v[0]=="S_ERROR" and s!="S_ERROR"}, key=lambda x:int(x[1:]) ))
# no cell in the whole table emits an error token any more
print("EOF row (PDF silent):", {s: (code[(s,'EOF')][0], code[(s,'EOF')][1], code[(s,'EOF')][4]) for s in ST})
import sys; sys.exit(1 if bad or tb else 0)