/* ARCHITECTURE
 * ------------
 * This scanner is driven entirely by a DFA transition table:
 *
 *      Transition table[NUM_STATES][NUM_CLASSES];
 *
 * The scan loop for every token is a single generic loop:
 *
 *      state = S0
 *      loop:
 *          c     = next input character
 *          class = char_class_of[c]              (table lookup, O(1))
 * 
 *          t     = table[state][class]            (table lookup, O(1))
 * 
 *          maybe append c to lexeme buffer         (per t.append)
 *          maybe push c back onto the input        (per t.pushback)
 *          if t.accept: finalize and return token
 *          state = t.next_state
 *
 * No per-character if/else or switch chains are used to *recognize*
 * tokens -- every state transition, consumption decision, and
 * accept/error decision comes from the table. The only lookup tables
 * built with simple loops are:
 *   (a) char_class_of[256]      -- maps a raw byte to a character class
 *   (b) the reserved-word/data-type/boolean/operator lookup tables --
 *       a dictionary lookup once an identifier has already been
 *       *recognized* by the DFA, exactly the way a real compiler front
 *       end (e.g. flex + a keyword hash table) does it.
 *
 * ERROR HANDLING (per the CLamb Lexical Specification, Section 2)
 * ----------------------------------------------------------------
 * Every malformed lexeme (S0 on '.'/other, S3, S4 on letter, S5/S6/S7 at
 * EOF or bad char literal, S11 on lone '!') transitions into S_ERROR, which
 * emits one diagnostic on entry, discards characters, and resyncs on
 * whitespace/EOF or on a recognizable token-start character (which is NOT
 * consumed and is re-lexed from S0).
 *
 * Build:  gcc -std=c11 -Wall -Wextra -o clamb_lexer clamb_lexer.c
 * Run:    ./clamb_lexer source.clamb        (or pipe via stdin)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "clamb_lexer.h"

// Here we are defining the vocabulary of the Clamb Language.
typedef enum {
    CLASS_LETTER = 0, CLASS_DIGIT, 
    CLASS_UNDERSCORE, CLASS_DQUOTE, 
    CLASS_SQUOTE, CLASS_EQUALS, 
    CLASS_LESS, CLASS_GREATER, 
    CLASS_BANG, CLASS_SLASH,
    CLASS_ADDSUB, CLASS_STAR, 
    CLASS_PERCENT, CLASS_AMP,
    CLASS_LPAREN, CLASS_RPAREN, 
    CLASS_LBRACE, CLASS_RBRACE,
    CLASS_LBRACKET, CLASS_RBRACKET, 
    CLASS_SEMI, CLASS_COMMA, 
    CLASS_DOT, CLASS_NEWLINE, 
    CLASS_WS, CLASS_OTHER, 
    CLASS_EOF, NUM_CLASSES
} CharClass;

// Defining the DFA States of the Clamb Language.
// The states are defined in the order they appear in the DFA diagram in the specification.
typedef enum {
    S0 = 0,  /* Start                                    */
    S1,      /* Identifier / reserved-word accumulation  */
    S2,      /* Digit-initial (int / float / digit-ident)*/
    S3,      /* Float dot -- must be followed by a digit */
    S4,      /* Float digits                             */
    S5,      /* String literal body                      */
    S6,      /* Char literal start                       */
    S7,      /* Char literal end (expect closing quote)  */
    S8,      /* Saw '='                                  */
    S9,      /* Saw '<'                                  */
    S10,     /* Saw '>'                                  */
    S11,     /* Saw '!'                                  */
    S12,     /* Saw '/'                                  */
    S13,     /* Comment body                             */
    S_ERROR, /* Panic-mode: discard + resync              */
    NUM_STATES
} State;

// Final labels given to a completed chunk of text.
// These are the "token classes" that the parser will see.
const char *clamb_token_type_name(TokenType t) {
    switch (t) { /* purely cosmetic: printable label for output, NOT
                    used anywhere for recognition/dispatch */
        case TOK_DATA_TYPE:       return "data_type";
        case TOK_KW_IF:           return "kw_if";
        case TOK_KW_ELSE:         return "kw_else";
        case TOK_KW_FOR:          return "kw_for";
        case TOK_KW_WHILE:        return "kw_while";
        case TOK_KW_RETURN:       return "kw_return";
        case TOK_KW_LAMB:         return "kw_lamb";
        case TOK_KW_NULL:         return "kw_null";
        case TOK_KW_PRINT:        return "kw_print";
        case TOK_KW_MALLOC:       return "kw_malloc";
        case TOK_KW_FREE:         return "kw_free";
        case TOK_IDENTIFIER:      return "identifier";
        case TOK_INTEGER_LITERAL: return "integer_literal";
        case TOK_FLOAT_LITERAL:   return "float_literal";
        case TOK_CHAR_LITERAL:    return "char_literal";
        case TOK_STRING_LITERAL:  return "string_literal";
        case TOK_BOOLEAN_LITERAL: return "boolean_literal";
        case TOK_ADD_OP:          return "add_op";
        case TOK_MUL_OP:          return "mul_op";
        case TOK_ASTERISK:        return "asterisk";
        case TOK_AND_OP:          return "and_op";
        case TOK_OR_OP:           return "or_op";
        case TOK_NOT_OP:          return "not_op";
        case TOK_EQ_OP:           return "eq_op";
        case TOK_NE_OP:           return "ne_op";
        case TOK_LT_OP:           return "lt_op";
        case TOK_LE_OP:           return "le_op";
        case TOK_GT_OP:           return "gt_op";
        case TOK_GE_OP:           return "ge_op";
        case TOK_ASSIGNMENT_OP:   return "assignment_op";
        case TOK_ADDRESS_OP:      return "address_op";
        case TOK_L_PAREN:         return "l_paren";
        case TOK_R_PAREN:         return "r_paren";
        case TOK_L_BRACE:         return "l_brace";
        case TOK_R_BRACE:         return "r_brace";
        case TOK_L_BRACKET:       return "l_bracket";
        case TOK_R_BRACKET:       return "r_bracket";
        case TOK_SEMICOLON:       return "semicolon";
        case TOK_COMMA:           return "comma";
        case TOK_EOF:             return "EOF";
        default:                  return "UNKNOWN";
    }
}

// Reserved keywords, data types, boolean literals, and logical operators in the CLamb language.
// Per the updated CLamb Lexical Specification, every keyword is now its own
// token class (kw_if, kw_else, ...) rather than a single shared "keyword"
// class -- keywords are not interchangeable, so each gets a distinct class.
typedef struct { const char *lexeme; TokenType type; } KeywordEntry;
static const KeywordEntry keyword_table[] = {
    {"if",     TOK_KW_IF},
    {"else",   TOK_KW_ELSE},
    {"for",    TOK_KW_FOR},
    {"while",  TOK_KW_WHILE},
    {"return", TOK_KW_RETURN},
    {"lamb",   TOK_KW_LAMB},
    {"null",   TOK_KW_NULL},
    {"print",  TOK_KW_PRINT},
    {"malloc", TOK_KW_MALLOC},
    {"free",   TOK_KW_FREE},
};
static const int num_keyword_entries = (int)(sizeof(keyword_table) / sizeof(keyword_table[0]));

static const char *data_types[]  = {"int","float","char","bool",
                                     "string","void","auto"};
static const int num_data_types  = 7;

static const char *booleans[]    = {"true","false"};
static const int num_booleans    = 2;

static int in_set(const char *lexeme, const char **set, int n) {
    for (int i = 0; i < n; i++) if (strcmp(lexeme, set[i]) == 0) return 1;
    return 0;
}

static TokenType classify_identifier(const char *lexeme) {
    for (int i = 0; i < num_keyword_entries; i++)
        if (strcmp(lexeme, keyword_table[i].lexeme) == 0) return keyword_table[i].type;
    if (in_set(lexeme, data_types, num_data_types))   return TOK_DATA_TYPE;
    if (in_set(lexeme, booleans, num_booleans))       return TOK_BOOLEAN_LITERAL;
    if (strcmp(lexeme, "AND") == 0) return TOK_AND_OP;
    if (strcmp(lexeme, "OR") == 0) return TOK_OR_OP;
    if (strcmp(lexeme, "NOT") == 0) return TOK_NOT_OP;
    return TOK_IDENTIFIER;
}

static CharClass char_class_of[256];

// Character classification table initialization.
// This table maps every possible byte value (0-255) to a character class
static void init_char_classes(void) {
    for (int i = 0; i < 256; i++) char_class_of[i] = CLASS_OTHER;
    for (int c = 'a'; c <= 'z'; c++) char_class_of[c] = CLASS_LETTER;
    for (int c = 'A'; c <= 'Z'; c++) char_class_of[c] = CLASS_LETTER;
    for (int c = '0'; c <= '9'; c++) char_class_of[c] = CLASS_DIGIT;
    char_class_of[(unsigned char)'_']  = CLASS_UNDERSCORE;
    char_class_of[(unsigned char)'"']  = CLASS_DQUOTE;
    char_class_of[(unsigned char)'\''] = CLASS_SQUOTE;
    char_class_of[(unsigned char)'=']  = CLASS_EQUALS;
    char_class_of[(unsigned char)'<']  = CLASS_LESS;
    char_class_of[(unsigned char)'>']  = CLASS_GREATER;
    char_class_of[(unsigned char)'!']  = CLASS_BANG;
    char_class_of[(unsigned char)'/']  = CLASS_SLASH;
    char_class_of[(unsigned char)'+']  = CLASS_ADDSUB;
    char_class_of[(unsigned char)'-']  = CLASS_ADDSUB;
    char_class_of[(unsigned char)'*']  = CLASS_STAR;
    char_class_of[(unsigned char)'%']  = CLASS_PERCENT;
    char_class_of[(unsigned char)'&']  = CLASS_AMP;
    char_class_of[(unsigned char)'(']  = CLASS_LPAREN;
    char_class_of[(unsigned char)')']  = CLASS_RPAREN;
    char_class_of[(unsigned char)'{']  = CLASS_LBRACE;
    char_class_of[(unsigned char)'}']  = CLASS_RBRACE;
    char_class_of[(unsigned char)'[']  = CLASS_LBRACKET;
    char_class_of[(unsigned char)']']  = CLASS_RBRACKET;
    char_class_of[(unsigned char)';']  = CLASS_SEMI;
    char_class_of[(unsigned char)',']  = CLASS_COMMA;
    char_class_of[(unsigned char)'.']  = CLASS_DOT;
    char_class_of[(unsigned char)'\n'] = CLASS_NEWLINE;
    char_class_of[(unsigned char)' ']  = CLASS_WS;
    char_class_of[(unsigned char)'\t'] = CLASS_WS;
    char_class_of[(unsigned char)'\r'] = CLASS_WS;
}

static CharClass class_of(int c) {
    if (c == EOF) return CLASS_EOF;
    return char_class_of[(unsigned char)c];
}

/* ================================================================== */
/*  The DFA transition table itself                                    */
/* ================================================================== */

// Defining the structure of a transition in the DFA transition table.
typedef struct {
    State     next_state;
    int       append;    /* 1 = write this char into the lexeme buffer   */
    int       pushback;  /* 1 = char is NOT consumed; re-fed next call   */
    int       accept;    /* 1 = this transition finalizes a token        */
    TokenType type;       /* meaningful only when accept == 1             */
} Transition;

// Creating the DFA transition table for the Clamb Language.
static Transition table[NUM_STATES][NUM_CLASSES];

// the following two functions are used to fill the transition table with the appropriate
// transitions for each state and character class. The set() function sets a single transition
// for a given state and character class, while the set_row() function fills an entire row
// of the transition table with the same transition, allowing callers to override only the
// exceptional classes afterward. This is a direct encoding of the spec's own "Other = any
// character not explicitly matched" convention.
static void set(State s, CharClass c, State next, int append, int pushback,
                 int accept, TokenType type) {
    table[s][c].next_state = next;
    table[s][c].append     = append;
    table[s][c].pushback   = pushback;
    table[s][c].accept     = accept;
    table[s][c].type       = type;
}

// Fill every cell of a state's row with the same transition, so callers
// only need to override the exceptional classes afterward -- this is a
// direct encoding of the spec's own "Other = any character not
// explicitly matched" convention.
static void set_row(State s, State next, int append, int pushback,
                     int accept, TokenType type) {
    for (int c = 0; c < NUM_CLASSES; c++)
        set(s, (CharClass)c, next, append, pushback, accept, type);
}

static void init_transition_table(void) {
    // Safety-net default for every cell: treat as an unrecognized
    // character and enter panic-mode error recovery. Every row below
    // overrides the classes that state actually cares about.
    for (int s = 0; s < NUM_STATES; s++)
        set_row((State)s, S_ERROR, 0, 0, 0, TOK_NONE);

    /* ---------------- S0 : Start ---------------- */
    set(S0, CLASS_LETTER,     S1,  1, 0, 0, TOK_NONE);
    set(S0, CLASS_DIGIT,      S2,  1, 0, 0, TOK_NONE);
    set(S0, CLASS_UNDERSCORE, S1,  1, 0, 0, TOK_NONE);
    set(S0, CLASS_DQUOTE,     S5,  1, 0, 0, TOK_NONE); /* opening " kept   */
    set(S0, CLASS_SQUOTE,     S6,  0, 0, 0, TOK_NONE); /* opening ' dropped*/
    set(S0, CLASS_EQUALS,     S8,  1, 0, 0, TOK_NONE);
    set(S0, CLASS_LESS,       S9,  1, 0, 0, TOK_NONE);
    set(S0, CLASS_GREATER,    S10, 1, 0, 0, TOK_NONE);
    set(S0, CLASS_BANG,       S11, 1, 0, 0, TOK_NONE);
    set(S0, CLASS_SLASH,      S12, 1, 0, 0, TOK_NONE);
    set(S0, CLASS_ADDSUB,     S0,  1, 0, 1, TOK_ADD_OP);
    set(S0, CLASS_STAR,       S0,  1, 0, 1, TOK_ASTERISK);
    set(S0, CLASS_PERCENT,    S0,  1, 0, 1, TOK_MUL_OP);
    set(S0, CLASS_AMP,        S0,  1, 0, 1, TOK_ADDRESS_OP);
    set(S0, CLASS_LPAREN,     S0,  1, 0, 1, TOK_L_PAREN);
    set(S0, CLASS_RPAREN,     S0,  1, 0, 1, TOK_R_PAREN);
    set(S0, CLASS_LBRACE,     S0,  1, 0, 1, TOK_L_BRACE);
    set(S0, CLASS_RBRACE,     S0,  1, 0, 1, TOK_R_BRACE);
    set(S0, CLASS_LBRACKET,   S0,  1, 0, 1, TOK_L_BRACKET);
    set(S0, CLASS_RBRACKET,   S0,  1, 0, 1, TOK_R_BRACKET);
    set(S0, CLASS_SEMI,       S0,  1, 0, 1, TOK_SEMICOLON);
    set(S0, CLASS_COMMA,      S0,  1, 0, 1, TOK_COMMA);
    set(S0, CLASS_NEWLINE,    S0,  0, 0, 0, TOK_NONE);  /* skip           */
    set(S0, CLASS_WS,         S0,  0, 0, 0, TOK_NONE);  /* skip           */
    set(S0, CLASS_EOF,        S0,  0, 0, 1, TOK_EOF);
    /* CLASS_DOT and CLASS_OTHER keep the S_ERROR default (unrecognized) */

    /* ---------------- S1 : Identifier ---------------- */
    set_row(S1, S0, 0, 1, 1, TOK_DYNAMIC_IDENT);   /* default: end ident  */
    set(S1, CLASS_LETTER,     S1, 1, 0, 0, TOK_NONE);
    set(S1, CLASS_DIGIT,      S1, 1, 0, 0, TOK_NONE);
    set(S1, CLASS_UNDERSCORE, S1, 1, 0, 0, TOK_NONE);
    set(S1, CLASS_EOF,        S0, 0, 0, 1, TOK_DYNAMIC_IDENT);

    /* ---------------- S2 : Digit-initial ---------------- */
    set_row(S2, S0, 0, 1, 1, TOK_INTEGER_LITERAL); /* default: end int    */
    set(S2, CLASS_DIGIT,      S2, 1, 0, 0, TOK_NONE);
    set(S2, CLASS_UNDERSCORE, S1, 1, 0, 0, TOK_NONE); /* -> identifier    */
    set(S2, CLASS_LETTER,     S1, 1, 0, 0, TOK_NONE); /* -> identifier    */
    set(S2, CLASS_DOT,        S3, 1, 0, 0, TOK_NONE); /* -> possible float*/
    set(S2, CLASS_EOF,        S0, 0, 0, 1, TOK_INTEGER_LITERAL);

    /* ---------------- S3 : Float dot (must see a digit) ---------------- */
    set_row(S3, S_ERROR, 0, 1, 0, TOK_NONE); /* Other -> S_Error, pushback  */
    set(S3, CLASS_DIGIT, S4, 1, 0, 0, TOK_NONE);
    set(S3, CLASS_EOF,   S_ERROR, 0, 0, 0, TOK_NONE);

    /* ---------------- S4 : Float digits ---------------- */
    set_row(S4, S0, 0, 1, 1, TOK_FLOAT_LITERAL);   /* default: end float  */
    set(S4, CLASS_DIGIT,      S4, 1, 0, 0, TOK_NONE);
    set(S4, CLASS_LETTER,     S_ERROR, 0, 1, 0, TOK_NONE); /* "3.14abc"   */
    set(S4, CLASS_EOF,        S0, 0, 0, 1, TOK_FLOAT_LITERAL);

    /* ---------------- S5 : String literal body ---------------- */
    set_row(S5, S5, 1, 0, 0, TOK_NONE);            /* default: stay, absorb */
    set(S5, CLASS_DQUOTE, S0, 1, 0, 1, TOK_STRING_LITERAL); /* closing " kept */
    set(S5, CLASS_EOF,    S_ERROR, 0, 0, 0, TOK_NONE);       /* unterminated  */

    /* ---------------- S6 : Char literal start ---------------- */
    set_row(S6, S7, 1, 0, 0, TOK_NONE);            /* default: any char -> S7 */
    set(S6, CLASS_SQUOTE, S_ERROR, 0, 0, 0, TOK_NONE); /* empty literal '' (consumed) */
    set(S6, CLASS_EOF,    S_ERROR, 0, 0, 0, TOK_NONE); /* unterminated            */

    /* ---------------- S7 : Char literal end ---------------- */
    set_row(S7, S_ERROR, 0, 1, 0, TOK_NONE);       /* Other -> S_Error, pushback */
    set(S7, CLASS_SQUOTE, S0, 0, 0, 1, TOK_CHAR_LITERAL); /* closing ' dropped */
    set(S7, CLASS_EOF,    S_ERROR, 0, 0, 0, TOK_NONE);

    /* ---------------- S8 : saw '=' ---------------- */
    set_row(S8, S0, 0, 1, 1, TOK_ASSIGNMENT_OP);   /* default: lone '='   */
    set(S8, CLASS_EQUALS, S0, 1, 0, 1, TOK_EQ_OP); /* "=="        */
    set(S8, CLASS_EOF,    S0, 0, 0, 1, TOK_ASSIGNMENT_OP);

    /* ---------------- S9 : saw '<' ---------------- */
    set_row(S9, S0, 0, 1, 1, TOK_LT_OP);   /* default: lone '<'   */
    set(S9, CLASS_EQUALS, S0, 1, 0, 1, TOK_LE_OP); /* "<="        */
    set(S9, CLASS_EOF,    S0, 0, 0, 1, TOK_LT_OP);

    /* ---------------- S10 : saw '>' ---------------- */
    set_row(S10, S0, 0, 1, 1, TOK_GT_OP);  /* default: lone '>'   */
    set(S10, CLASS_EQUALS, S0, 1, 0, 1, TOK_GE_OP); /* ">="       */
    set(S10, CLASS_EOF,    S0, 0, 0, 1, TOK_GT_OP);

    /* ---------------- S11 : saw '!' ---------------- */
    set_row(S11, S_ERROR, 0, 1, 0, TOK_NONE);      /* lone '!': S_Error, pushback */
    set(S11, CLASS_EQUALS, S0, 1, 0, 1, TOK_NE_OP); /* "!="       */
    set(S11, CLASS_EOF,    S_ERROR, 0, 0, 0, TOK_NONE);

    /* ---------------- S12 : saw '/' ---------------- */
    set_row(S12, S0, 0, 1, 1, TOK_MUL_OP);         /* default: division   */
    set(S12, CLASS_SLASH, S13, 0, 0, 0, TOK_NONE); /* "//" starts comment */
    set(S12, CLASS_EOF,   S0,  0, 0, 1, TOK_MUL_OP);

    /* ---------------- S13 : Comment body ---------------- */
    set_row(S13, S13, 0, 0, 0, TOK_NONE);          /* default: discard    */
    set(S13, CLASS_NEWLINE, S0, 0, 0, 0, TOK_NONE); /* comment ends       */
    set(S13, CLASS_EOF,     S0, 0, 0, 0, TOK_NONE); /* comment ends at EOF*/

    /* ---------------- S_ERROR : panic-mode recovery ---------------- */
    set_row(S_ERROR, S_ERROR, 0, 0, 0, TOK_NONE);  /* default: keep discarding */
    set(S_ERROR, CLASS_WS,      S0, 0, 0, 0, TOK_NONE); /* resync, discard   */
    set(S_ERROR, CLASS_NEWLINE, S0, 0, 0, 0, TOK_NONE); /* resync, discard   */
    set(S_ERROR, CLASS_EOF,     S0, 0, 0, 0, TOK_NONE); /* resync at EOF     */
    /* Recognizable token-start classes: resync WITHOUT discarding them   */
    CharClass restart_classes[] = {
        CLASS_LETTER, CLASS_DIGIT, CLASS_UNDERSCORE, CLASS_DQUOTE, CLASS_SQUOTE,
        CLASS_EQUALS, CLASS_LESS, CLASS_GREATER, CLASS_BANG, CLASS_SLASH,
        CLASS_ADDSUB, CLASS_STAR, CLASS_PERCENT, CLASS_AMP,
        CLASS_LPAREN, CLASS_RPAREN, CLASS_LBRACE, CLASS_RBRACE,
        CLASS_LBRACKET, CLASS_RBRACKET, CLASS_SEMI, CLASS_COMMA
    };
    for (size_t i = 0; i < sizeof(restart_classes)/sizeof(restart_classes[0]); i++)
        set(S_ERROR, restart_classes[i], S0, 0, 1, 0, TOK_NONE);
    /* CLASS_DOT and CLASS_OTHER keep the "stay in S_ERROR, discard" default,
       since a bare '.' is not itself a valid CLamb token start either. */
}

/* ================================================================== */
/*  Input stream handling                                              */
/* ================================================================== */
static FILE *src;
static int line_number = 1;
static int lexical_error_count;

// Reads the next character from the input stream, updating the line number if a newline is encountered.
static int next_char(void) {
    int c = fgetc(src);
    if (c == '\n') line_number++;
    return c;
}

// basically its the lookahed function that pushes back the character to the input stream
static void push_back(int c) {
    if (c == EOF) return;
    if (c == '\n') line_number--;
    ungetc(c, src);
}

/* ================================================================== */
/*  Error message text (diagnostics only -- not part of recognition)   */
/* ================================================================== */
/* Print a lexeme for diagnostics, escaping newlines/tabs so a multi-line
   malformed literal stays on one diagnostic line. */
static void esc(const char *lx, char *out, size_t n) {
    size_t j = 0;
    for (; *lx && j + 3 < n; lx++) {
        if (*lx == '\n')      { out[j++] = '\\'; out[j++] = 'n'; }
        else if (*lx == '\t') { out[j++] = '\\'; out[j++] = 't'; }
        else if (*lx == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
        else                  out[j++] = *lx;
    }
    out[j] = '\0';
}

// Prints a diagnostic message for a malformed lexeme, based on the state it was in when the error was detected.
// These are case specific hence handled using switch cases.
static void report_error(State from, const char *raw_lexeme, int c, int at_line) {
    lexical_error_count++;
    char lexeme[2 * MAX_LEXEME + 4];
    esc(raw_lexeme, lexeme, sizeof lexeme);
    switch (from) {
        case S3:
            fprintf(stderr, "Lexical Error (line %d): malformed float literal '%s' "
                    "(digit required after '.')\n", at_line, lexeme);
            break;
        case S4:
            fprintf(stderr, "Lexical Error (line %d): invalid float literal '%s' "
                    "followed by a letter\n", at_line, lexeme);
            break;
        case S5:
            fprintf(stderr, "Lexical Error (line %d): unterminated string literal '%s'\n",
                    at_line, lexeme);
            break;
        case S6:
            fprintf(stderr, "Lexical Error (line %d): empty or unterminated char "
                    "literal\n", at_line);
            break;
        case S7:
            fprintf(stderr, "Lexical Error (line %d): malformed/unterminated char "
                    "literal '%s'\n", at_line, lexeme);
            break;
        case S11:
            fprintf(stderr, "Lexical Error (line %d): '!' is not a valid standalone "
                    "token (expected '!=')\n", at_line);
            break;
        default: /* S0: a single unrecognized character */
            if (c >= 32 && c < 127)
                fprintf(stderr, "Lexical Error (line %d): unrecognized character '%c'\n",
                        at_line, c);
            else
                fprintf(stderr, "Lexical Error (line %d): unrecognized character "
                        "(byte 0x%02X)\n", at_line, (unsigned)c & 0xFF);
    }
}

// This is the core engine.
// Reads a new token from input stream, builds, recognizes, and returns the token structure.
// Uses the DFA transition table to drive the recognition process.
void clamb_lexer_init(FILE *input) {
    init_char_classes();
    init_transition_table();
    src = input;
    line_number = 1;
    lexical_error_count = 0;
}

int clamb_lexer_error_count(void) {
    return lexical_error_count;
}

Token clamb_lexer_next_token(void) {
    State state = S0;
    char lexeme[MAX_LEXEME];
    int idx = 0;
    int start_line = line_number; /* line where the current token began */

    for (;;) {
        int c = next_char();
        CharClass cls = class_of(c);
        Transition t = table[state][cls];
        if (state == S0) start_line = line_number; /* bookkeeping for diagnostics */

        if (t.append && idx < MAX_LEXEME - 1) {
            lexeme[idx++] = (char)c;
        }
        if (t.pushback) {
            push_back(c);
        }

        /* Entering S_ERROR from any other state: emit ONE diagnostic
           (spec: "emit error diagnostic once on entry"). Tokens are NOT
           emitted for malformed lexemes; S_ERROR discards until resync. */
        if (t.next_state == S_ERROR && state != S_ERROR) {
            lexeme[idx] = '\0';
            report_error(state, lexeme, c, start_line);
        }

        if (t.accept) {
            lexeme[idx] = '\0';
            Token tok;
            tok.type = (t.type == TOK_DYNAMIC_IDENT)
                       ? classify_identifier(lexeme)
                       : t.type;
            strncpy(tok.lexeme, lexeme, MAX_LEXEME - 1);
            tok.lexeme[MAX_LEXEME - 1] = '\0';
            tok.line = start_line;
            return tok;
        }

        if (t.next_state == S0) idx = 0; /* buffer resets whenever we
                                             return to S0 (natural end,
                                             comment end, or error resync) */
        state = t.next_state;
    }
}


#ifndef CLAMB_LEXER_NO_MAIN
// Reads a file and tokenizes it, printing each token class and lexeme to stdout.
int main(int argc, char **argv) {
    if (argc < 2) {
        src = stdin;
    } else {
        src = fopen(argv[1], "r");
        if (!src) {
            fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
            return 1;
        }
    }
    clamb_lexer_init(src);

    printf("%-18s %s\n", "TOKEN CLASS", "LEXEME");
    printf("----------------------------------------\n");

    Token tok;
    do {
        tok = clamb_lexer_next_token();
        if (tok.type != TOK_EOF) {
            printf("<%-16s, %s>\n", clamb_token_type_name(tok.type), tok.lexeme);
        }
    } while (tok.type != TOK_EOF);

    if (src != stdin) fclose(src);
    return 0;
}
#endif