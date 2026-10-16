/* basic.c - Extended integer BASIC for AT&T 3b2 (WE32000) / SysV      */
/*                                                                     */
/* Written in strict K&R C: no prototypes, no void, no ANSI-only       */
/* escapes, no mixed declarations.  32-bit int is assumed.             */
/* Compile with:  cc -o basic basic.c                                  */
/*                                                                     */
/* Commands:  RUN LIST [n[-m]] NEW SAVE "f" LOAD "f" BYE               */
/* Statements (':' separates several on one line):                     */
/*   PRINT ? LET INPUT IF/THEN/ELSE GOTO GOSUB RETURN FOR/TO/STEP NEXT */
/*   DIM READ DATA RESTORE REM ' RANDOMIZE CLS END STOP                */
/* Variables: A-Z (integer), A$-Z$ (string, 80 chars), arrays A(n),    */
/*   A$(n) with indexes 0..n (default size 10 if not DIMmed).          */
/* Operators: ^  * / MOD  + -  = < > <= >= <>  NOT AND OR              */
/*   (true is -1, false is 0, AND/OR/NOT are bitwise as in MS BASIC)   */
/* Functions: RND(n) SQR ABS SGN INT LEN(s$) ASC(s$) VAL(s$)           */
/*   CHR$ STR$ LEFT$ RIGHT$ MID$ SPACE$                                */
/* Ctrl-C (interrupt) stops a running program.                         */

#include <stdio.h>
#include <signal.h>

#define MAXLINES    1000
#define LINLEN      256
#define STR_LEN     80
#define NUM_MAX     5000
#define STR_MAX     500
#define DEFDIM      10
#define FORSTK      16
#define SUBSTK      32
#define BIGLINE     1000000

#define OP_EQ 1
#define OP_LT 2
#define OP_GT 3
#define OP_LE 4
#define OP_GE 5
#define OP_NE 6

/* Library functions that do not return int */
char *malloc();
char *calloc();
long time();

/* Our functions that do not return int, or are used as values */
int onint();
char *skip_spaces();
char *get_quoted();
char *find_else();
char *sarr_slot();
int *arr_slot();

/* ==================== STATE ==================== */
static char *program[MAXLINES];
static int linenum_arr[MAXLINES];
static int num_lines = 0;

static int var_int[26];
static char str_val[26][STR_LEN + 1];
static int *arr_ptr[26];
static int arr_n[26];
static char *sarr_ptr[26];
static int sarr_n[26];

static int rnd_seed = 7919;

static int pc = 0;              /* index of current program line     */
static int run_mode = 0;
static int jumped = 0;          /* statement asked for a jump        */
static int jump_pc = 0;
static char *jump_ptr = 0;      /* 0 = start of line                 */
static char *run_ptr = 0;
static char *expr_ptr = 0;
static int err_flag = 0;
static int intr_flag = 0;
static int out_col = 0;
static int conv_ok = 0;

static int for_var[FORSTK];
static int for_end[FORSTK];
static int for_step[FORSTK];
static int for_pc[FORSTK];
static char *for_ptr[FORSTK];
static int for_stk_ptr = 0;

static int sub_pc[SUBSTK];
static char *sub_ptr[SUBSTK];
static int sub_sp = 0;

static int data_pc = 0;
static char *data_ptr = 0;
static char *in_ptr = 0;
static int in_more = 0;

static int lv_l, lv_str, lv_arr, lv_idx;      /* parsed lvalue */
static char get_quoted_buf[STR_LEN + 1];

/* ==================== MAIN ==================== */
main()
{
    char buf[LINLEN];
    char *p;

    signal(SIGINT, onint);
    printf("3b2asic 2026oct16 - Integer Basic for the 3b2 computer.\n");

    for (;;) {
        printf("? ");
        fflush(stdout);
        if (fgets(buf, LINLEN, stdin) == 0) {
            if (intr_flag) {
                intr_flag = 0;
                clearerr(stdin);
                printf("\n");
                continue;
            }
            break;
        }
        intr_flag = 0;
        i_chomp(buf);
        upcase(buf);
        p = skip_spaces(buf);
        if (*p == '\0')
            continue;

        if (is_digit(*p))
            parse_line(p);
        else if (starts_with(p, "RUN"))
            do_run();
        else if (starts_with(p, "LIST"))
            do_list(p + 4);
        else if (starts_with(p, "NEW"))
            do_new();
        else if (starts_with(p, "BYE"))
            break;
        else if (starts_with(p, "SAVE"))
            do_save(p + 4);
        else if (starts_with(p, "LOAD"))
            do_load(p + 4);
        else
            exec_line(p);
    }
    printf("Done\n");
    return 0;
}

onint()
{
    signal(SIGINT, onint);
    intr_flag = 1;
    return 0;
}

/* ==================== ERRORS ==================== */
err_tail()
{
    if (run_mode && pc >= 0 && pc < num_lines)
        printf(" IN %d", linenum_arr[pc]);
    printf("\n");
    out_col = 0;
    return -1;
}

rt_error(msg)
char *msg;
{
    if (err_flag)
        return -1;
    err_flag = 1;
    if (out_col)
        printf("\n");
    printf("?%s", msg);
    return err_tail();
}

rt_errnum(msg, n)
char *msg;
int n;
{
    if (err_flag)
        return -1;
    err_flag = 1;
    if (out_col)
        printf("\n");
    printf("?%s %d", msg, n);
    return err_tail();
}

/* ==================== LINE MANAGEMENT ==================== */
parse_line(s)
char *s;
{
    int linenum_val, i, j, len;
    char *p, *newline;

    linenum_val = atoi_custom(s);
    p = skip_spaces(s);
    while (is_digit(*p))
        p++;
    p = skip_spaces(p);

    if (*p == '\0') {
        delete_line(linenum_val);
        return 0;
    }

    len = strlen_custom(p) + 1;
    newline = malloc((unsigned) len);
    if (newline == 0) {
        printf("?OUT OF MEMORY\n");
        return 0;
    }
    strcpy_custom(newline, p);

    for (i = 0; i < num_lines && linenum_arr[i] < linenum_val; i++)
        ;

    if (i < num_lines && linenum_arr[i] == linenum_val) {
        free(program[i]);
        program[i] = newline;
        return 0;
    }

    if (num_lines >= MAXLINES) {
        free(newline);
        printf("?PROGRAM TOO LONG\n");
        return 0;
    }

    for (j = num_lines; j > i; j--) {
        linenum_arr[j] = linenum_arr[j - 1];
        program[j] = program[j - 1];
    }
    linenum_arr[i] = linenum_val;
    program[i] = newline;
    num_lines++;
    return 0;
}

delete_line(n)
int n;
{
    int i, j;

    for (i = 0; i < num_lines && linenum_arr[i] != n; i++)
        ;
    if (i >= num_lines)
        return 0;

    free(program[i]);
    for (j = i; j < num_lines - 1; j++) {
        linenum_arr[j] = linenum_arr[j + 1];
        program[j] = program[j + 1];
    }
    num_lines--;
    program[num_lines] = 0;
    return 0;
}

find_line(target)
int target;
{
    int i;

    for (i = 0; i < num_lines && linenum_arr[i] < target; i++)
        ;
    if (i < num_lines && linenum_arr[i] == target)
        return i;
    return -1;
}

/* ==================== EXECUTION ==================== */
do_run()
{
    clear_vars();
    start_run(0);
    return 0;
}

start_run(idx)
int idx;
{
    sub_sp = 0;
    for_stk_ptr = 0;
    data_pc = 0;
    data_ptr = 0;
    intr_flag = 0;
    run_loop(idx);
    return 0;
}

run_loop(start)
int start;
{
    int r;
    char *p;

    pc = start;
    run_ptr = 0;
    run_mode = 1;

    while (pc >= 0 && pc < num_lines) {
        if (intr_flag)
            break;
        if (run_ptr == 0)
            run_ptr = program[pc];
        expr_ptr = run_ptr;
        jumped = 0;
        r = exec_statement();
        if (r < 0)
            break;
        if (jumped) {
            pc = jump_pc;
            run_ptr = jump_ptr;
            continue;
        }
        p = skip_spaces(expr_ptr);
        if (*p == ':')
            run_ptr = p + 1;
        else if (*p == '\0') {
            pc++;
            run_ptr = 0;
        } else if (starts_with(p, "ELSE"))
            run_ptr = p;
        else {
            rt_error("SYNTAX ERROR");
            break;
        }
    }
    if (intr_flag) {
        intr_flag = 0;
        if (pc >= 0 && pc < num_lines)
            printf("\nBREAK IN %d\n", linenum_arr[pc]);
        else
            printf("\nBREAK\n");
        out_col = 0;
    }
    run_mode = 0;
    return 0;
}

/* Execute a line typed without a line number */
exec_line(s)
char *s;
{
    int r;

    run_mode = 0;
    expr_ptr = s;
    for (;;) {
        jumped = 0;
        r = exec_statement();
        if (r < 0)
            return -1;
        if (jumped) {
            start_run(jump_pc);
            return 0;
        }
        expr_ptr = skip_spaces(expr_ptr);
        if (*expr_ptr == ':') {
            expr_ptr++;
            continue;
        }
        if (*expr_ptr == '\0')
            return 0;
        if (starts_with(expr_ptr, "ELSE"))
            continue;
        return rt_error("SYNTAX ERROR");
    }
}

skip_eol()
{
    while (*expr_ptr)
        expr_ptr++;
    return 0;
}

at_stmt_end(p)
char *p;
{
    return *p == '\0' || *p == ':' || starts_with(p, "ELSE");
}

exec_statement()
{
    int r;
    char *p;

    err_flag = 0;
    expr_ptr = skip_spaces(expr_ptr);
    p = expr_ptr;

    if (*p == '\0' || *p == ':')
        return 0;
    if (*p == '\'' || starts_with(p, "REM") || starts_with(p, "ELSE")
        || starts_with(p, "DATA"))
        return skip_eol();

    if (*p == '?') {
        expr_ptr++;
        r = st_print();
    } else if (starts_with(p, "PRINT")) {
        expr_ptr += 5;
        r = st_print();
    } else if (starts_with(p, "IF")) {
        expr_ptr += 2;
        r = st_if();
    } else if (starts_with(p, "GOTO")) {
        expr_ptr += 4;
        r = st_goto(0);
    } else if (starts_with(p, "GOSUB")) {
        expr_ptr += 5;
        r = st_goto(1);
    } else if (starts_with(p, "RETURN")) {
        expr_ptr += 6;
        r = st_return();
    } else if (starts_with(p, "FOR")) {
        expr_ptr += 3;
        r = st_for();
    } else if (starts_with(p, "NEXT")) {
        expr_ptr += 4;
        r = st_next();
    } else if (starts_with(p, "INPUT")) {
        expr_ptr += 5;
        r = st_input();
    } else if (starts_with(p, "READ")) {
        expr_ptr += 4;
        r = st_read();
    } else if (starts_with(p, "RESTORE")) {
        expr_ptr += 7;
        data_pc = 0;
        data_ptr = 0;
        r = 0;
    } else if (starts_with(p, "DIM")) {
        expr_ptr += 3;
        r = do_dim();
    } else if (starts_with(p, "RANDOMIZE")) {
        expr_ptr += 9;
        r = st_random();
    } else if (starts_with(p, "CLS")) {
        expr_ptr += 3;
        printf("\f");
        out_col = 0;
        r = 0;
    } else if (starts_with(p, "END")) {
        expr_ptr += 3;
        r = -1;
    } else if (starts_with(p, "STOP")) {
        if (run_mode)
            printf("BREAK IN %d\n", linenum_arr[pc]);
        r = -1;
    } else {
        if (starts_with(p, "LET"))
            expr_ptr += 3;
        r = st_let();
    }
    if (err_flag)
        return -1;
    return r;
}

/* ==================== EXPRESSIONS ==================== */
/* Each ex_ function parses from expr_ptr and returns the value. */
/* Precedence low to high: OR, AND, NOT, compare, + -, * / MOD,  */
/* unary -, ^                                                    */
ex_or()
{
    int v;

    v = ex_and();
    for (;;) {
        expr_ptr = skip_spaces(expr_ptr);
        if (!starts_with(expr_ptr, "OR"))
            break;
        expr_ptr += 2;
        v = v | ex_and();
    }
    return v;
}

ex_and()
{
    int v;

    v = ex_not();
    for (;;) {
        expr_ptr = skip_spaces(expr_ptr);
        if (!starts_with(expr_ptr, "AND"))
            break;
        expr_ptr += 3;
        v = v & ex_not();
    }
    return v;
}

ex_not()
{
    expr_ptr = skip_spaces(expr_ptr);
    if (starts_with(expr_ptr, "NOT")) {
        expr_ptr += 3;
        return ~ex_not();
    }
    return ex_cmp();
}

ex_cmp()
{
    int lhs, rhs, op;

    expr_ptr = skip_spaces(expr_ptr);
    if (is_str_start(expr_ptr))
        return str_compare();
    lhs = ex_add();
    op = get_relop();
    if (op == 0)
        return lhs;
    rhs = ex_add();
    return cmp_result(op, lhs, rhs) ? -1 : 0;
}

get_relop()
{
    char *p;

    p = skip_spaces(expr_ptr);
    if (*p == '<') {
        if (p[1] == '=') {
            expr_ptr = p + 2;
            return OP_LE;
        }
        if (p[1] == '>') {
            expr_ptr = p + 2;
            return OP_NE;
        }
        expr_ptr = p + 1;
        return OP_LT;
    }
    if (*p == '>') {
        if (p[1] == '=') {
            expr_ptr = p + 2;
            return OP_GE;
        }
        if (p[1] == '<') {
            expr_ptr = p + 2;
            return OP_NE;
        }
        expr_ptr = p + 1;
        return OP_GT;
    }
    if (*p == '=') {
        if (p[1] == '<') {
            expr_ptr = p + 2;
            return OP_LE;
        }
        if (p[1] == '>') {
            expr_ptr = p + 2;
            return OP_GE;
        }
        expr_ptr = p + 1;
        return OP_EQ;
    }
    return 0;
}

cmp_result(op, a, b)
int op, a, b;
{
    switch (op) {
    case OP_EQ: return a == b;
    case OP_LT: return a < b;
    case OP_GT: return a > b;
    case OP_LE: return a <= b;
    case OP_GE: return a >= b;
    case OP_NE: return a != b;
    }
    return 0;
}

str_compare()
{
    char a[STR_LEN + 1], b[STR_LEN + 1];
    int op;

    eval_str(a);
    if (err_flag)
        return 0;
    op = get_relop();
    if (op == 0)
        return rt_error("TYPE MISMATCH");
    eval_str(b);
    if (err_flag)
        return 0;
    return cmp_result(op, strcmp_custom(a, b), 0) ? -1 : 0;
}

ex_add()
{
    int v, op;
    char *p;

    v = ex_term();
    for (;;) {
        p = skip_spaces(expr_ptr);
        if (*p != '+' && *p != '-')
            break;
        op = *p;
        expr_ptr = p + 1;
        if (op == '+')
            v = v + ex_term();
        else
            v = v - ex_term();
    }
    return v;
}

ex_term()
{
    int v, r;
    char *p;

    v = ex_unary();
    for (;;) {
        p = skip_spaces(expr_ptr);
        if (*p == '*') {
            expr_ptr = p + 1;
            v = v * ex_unary();
        } else if (*p == '/') {
            expr_ptr = p + 1;
            r = ex_unary();
            if (r == 0)
                return rt_error("DIVISION BY ZERO");
            v = v / r;
        } else if (starts_with(p, "MOD")) {
            expr_ptr = p + 3;
            r = ex_unary();
            if (r == 0)
                return rt_error("DIVISION BY ZERO");
            v = v % r;
        } else
            break;
    }
    return v;
}

ex_unary()
{
    char *p;

    p = skip_spaces(expr_ptr);
    if (*p == '-') {
        expr_ptr = p + 1;
        return -ex_unary();
    }
    if (*p == '+') {
        expr_ptr = p + 1;
        return ex_unary();
    }
    return ex_power();
}

ex_power()
{
    int b, e;
    char *p;

    b = ex_prim();
    p = skip_spaces(expr_ptr);
    if (*p == '^') {
        expr_ptr = p + 1;
        e = ex_unary();
        return ipow(b, e);
    }
    return b;
}

ex_prim()
{
    int v;
    char *p;

    expr_ptr = skip_spaces(expr_ptr);
    p = expr_ptr;
    if (*p == '(') {
        expr_ptr = p + 1;
        v = ex_or();
        if (err_flag || need_rparen() < 0)
            return 0;
        return v;
    }
    if (is_digit(*p)) {
        v = 0;
        while (is_digit(*p)) {
            v = v * 10 + (*p - '0');
            p++;
        }
        expr_ptr = p;
        return v;
    }
    if (is_alpha(*p))
        return ex_ident();
    return rt_error("SYNTAX ERROR");
}

ex_ident()
{
    char name[16];
    char *p, *q;
    int n, l, idx, id;
    int *ip;

    p = expr_ptr;
    n = 0;
    while (is_alpha(*p)) {
        if (n < 15)
            name[n] = *p;
        n++;
        p++;
    }
    if (n > 15)
        n = 15;
    name[n] = '\0';
    if (*p == '$')
        return rt_error("TYPE MISMATCH");
    q = skip_spaces(p);
    if (n > 1) {
        id = func_id(name);
        if (id < 0)
            return rt_error("SYNTAX ERROR");
        if (*q != '(')
            return rt_error("MISSING (");
        expr_ptr = q + 1;
        return call_func(id);
    }
    l = name[0] - 'A';
    if (*q == '(') {
        expr_ptr = q + 1;
        idx = ex_or();
        if (err_flag || need_rparen() < 0)
            return 0;
        ip = arr_slot(l, idx);
        if (ip == 0)
            return 0;
        return *ip;
    }
    expr_ptr = p;
    return var_int[l];
}

need_rparen()
{
    expr_ptr = skip_spaces(expr_ptr);
    if (*expr_ptr == ')') {
        expr_ptr++;
        return 0;
    }
    return rt_error("MISSING )");
}

/* ==================== FUNCTIONS ==================== */
func_id(name)
char *name;
{
    if (strcmp_custom(name, "RND") == 0)
        return 0;
    if (strcmp_custom(name, "SQR") == 0)
        return 1;
    if (strcmp_custom(name, "ABS") == 0)
        return 2;
    if (strcmp_custom(name, "SGN") == 0)
        return 3;
    if (strcmp_custom(name, "INT") == 0)
        return 4;
    if (strcmp_custom(name, "LEN") == 0)
        return 5;
    if (strcmp_custom(name, "ASC") == 0)
        return 6;
    if (strcmp_custom(name, "VAL") == 0)
        return 7;
    return -1;
}

/* expr_ptr is just past the '(' */
call_func(id)
int id;
{
    char s[STR_LEN + 1];
    int v;

    if (id >= 5) {
        eval_str(s);
        if (err_flag || need_rparen() < 0)
            return 0;
        if (id == 5)
            return strlen_custom(s);
        if (id == 6)
            return s[0] & 0xFF;
        return scan_int(s);
    }
    v = ex_or();
    if (err_flag || need_rparen() < 0)
        return 0;
    switch (id) {
    case 0: return rnd_func(v);
    case 1: return sqrt_custom(v);
    case 2: return abs_custom(v);
    case 3: return sgn_custom(v);
    case 4: return v;
    }
    return 0;
}

rnd_func(n)
int n;
{
    if (n < 1) {
        rt_error("ILLEGAL FUNCTION CALL");
        return 0;
    }
    rnd_seed = (rnd_seed * 1103515245 + 12345) & 0x7FFFFFFF;
    return ((rnd_seed >> 8) % n) + 1;
}

sqrt_custom(n)
int n;
{
    int x, y;

    if (n < 0) {
        rt_error("ILLEGAL FUNCTION CALL");
        return 0;
    }
    if (n < 2)
        return n;
    x = n;
    /* y = n / 2 + 1; */
    y = n / 2 + n % 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

abs_custom(n)
int n;
{
    return n < 0 ? -n : n;
}

sgn_custom(n)
int n;
{
    return n > 0 ? 1 : (n < 0 ? -1 : 0);
}

ipow(b, e)
int b, e;
{
    int r;

    if (e < 0)
        return (b == 1) ? 1 : 0;
    r = 1;
    while (e > 0) {
        if (e & 1)
            r = r * b;
        b = b * b;
        e = e >> 1;
    }
    return r;
}

/* ==================== STRING EXPRESSIONS ==================== */
is_str_start(p)
char *p;
{
    p = skip_spaces(p);
    if (*p == '"')
        return 1;
    if (is_alpha(*p)) {
        while (is_alpha(*p))
            p++;
        if (*p == '$')
            return 1;
    }
    return 0;
}

/* string expression: terms joined by '+', result in dst */
eval_str(dst)
char *dst;
{
    char tmp[STR_LEN + 1];

    str_term(dst);
    for (;;) {
        if (err_flag)
            break;
        expr_ptr = skip_spaces(expr_ptr);
        if (*expr_ptr != '+')
            break;
        expr_ptr++;
        str_term(tmp);
        strcat_custom(dst, tmp);
    }
    return 0;
}

str_term(dst)
char *dst;
{
    char name[16];
    char tmp[STR_LEN + 1];
    char *p, *cp;
    int n, l, i, a, b, len;

    expr_ptr = skip_spaces(expr_ptr);
    p = expr_ptr;
    dst[0] = '\0';

    if (*p == '"') {
        p++;
        n = 0;
        while (*p && *p != '"') {
            if (n < STR_LEN)
                dst[n++] = *p;
            p++;
        }
        dst[n] = '\0';
        if (*p == '"')
            p++;
        expr_ptr = p;
        return 0;
    }
    if (!is_alpha(*p))
        return rt_error("TYPE MISMATCH");

    n = 0;
    while (is_alpha(*p)) {
        if (n < 15)
            name[n] = *p;
        n++;
        p++;
    }
    if (n > 15)
        n = 15;
    name[n] = '\0';
    if (*p != '$')
        return rt_error("TYPE MISMATCH");
    p++;

    if (n == 1) {
        l = name[0] - 'A';
        expr_ptr = skip_spaces(p);
        if (*expr_ptr == '(') {
            expr_ptr++;
            i = ex_or();
            if (err_flag || need_rparen() < 0)
                return -1;
            cp = sarr_slot(l, i);
            if (cp == 0)
                return -1;
            str_copyn(dst, cp, STR_LEN);
        } else {
            expr_ptr = p;
            str_copyn(dst, str_val[l], STR_LEN);
        }
        return 0;
    }

    /* string function */
    expr_ptr = skip_spaces(p);
    if (*expr_ptr != '(')
        return rt_error("MISSING (");
    expr_ptr++;

    if (strcmp_custom(name, "CHR") == 0) {
        a = ex_or();
        if (err_flag || need_rparen() < 0)
            return -1;
        dst[0] = a & 0xFF;
        dst[1] = '\0';
        return 0;
    }
    if (strcmp_custom(name, "STR") == 0) {
        a = ex_or();
        if (err_flag || need_rparen() < 0)
            return -1;
        int_to_str(tmp, a);
        str_copyn(dst, tmp, STR_LEN);
        return 0;
    }
    if (strcmp_custom(name, "SPACE") == 0) {
        a = ex_or();
        if (err_flag || need_rparen() < 0)
            return -1;
        if (a < 0 || a > STR_LEN)
            return rt_error("ILLEGAL FUNCTION CALL");
        for (i = 0; i < a; i++)
            dst[i] = ' ';
        dst[a] = '\0';
        return 0;
    }
    if (strcmp_custom(name, "LEFT") == 0 ||
        strcmp_custom(name, "RIGHT") == 0 ||
        strcmp_custom(name, "MID") == 0) {
        eval_str(tmp);
        if (err_flag)
            return -1;
        expr_ptr = skip_spaces(expr_ptr);
        if (*expr_ptr != ',')
            return rt_error("SYNTAX ERROR");
        expr_ptr++;
        a = ex_or();
        if (err_flag)
            return -1;
        len = strlen_custom(tmp);
        if (name[0] == 'M') {
            b = BIGLINE;
            expr_ptr = skip_spaces(expr_ptr);
            if (*expr_ptr == ',') {
                expr_ptr++;
                b = ex_or();
                if (err_flag)
                    return -1;
            }
            if (err_flag || need_rparen() < 0)
                return -1;
            if (a < 1 || b < 0)
                return rt_error("ILLEGAL FUNCTION CALL");
            if (a > len)
                dst[0] = '\0';
            else
                str_copyn(dst, tmp + a - 1, b);
            return 0;
        }
        if (need_rparen() < 0)
            return -1;
        if (a < 0)
            return rt_error("ILLEGAL FUNCTION CALL");
        if (a > len)
            a = len;
        if (name[0] == 'L')
            str_copyn(dst, tmp, a);
        else
            str_copyn(dst, tmp + len - a, a);
        return 0;
    }
    return rt_error("SYNTAX ERROR");
}

str_copyn(dst, src, n)
char *dst, *src;
int n;
{
    int i;

    if (n > STR_LEN)
        n = STR_LEN;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
    return i;
}

/* ==================== VARIABLES AND ARRAYS ==================== */
/* Parse a variable or array element used as an assignment target. */
/* Result is left in lv_l, lv_str, lv_arr, lv_idx.                 */
parse_lval()
{
    char *p;

    p = skip_spaces(expr_ptr);
    if (!is_alpha(*p))
        return rt_error("SYNTAX ERROR");
    lv_l = *p - 'A';
    p++;
    if (is_alpha(*p))
        return rt_error("SYNTAX ERROR");
    lv_str = 0;
    if (*p == '$') {
        lv_str = 1;
        p++;
    }
    lv_arr = 0;
    p = skip_spaces(p);
    if (*p == '(') {
        expr_ptr = p + 1;
        lv_idx = ex_or();
        if (err_flag || need_rparen() < 0)
            return -1;
        lv_arr = 1;
    } else
        expr_ptr = p;
    return 0;
}

store_num(v)
int v;
{
    int *ip;

    if (lv_str)
        return rt_error("TYPE MISMATCH");
    if (lv_arr) {
        ip = arr_slot(lv_l, lv_idx);
        if (ip == 0)
            return -1;
        *ip = v;
    } else
        var_int[lv_l] = v;
    return 0;
}

store_str(s)
char *s;
{
    char *cp;

    if (!lv_str)
        return rt_error("TYPE MISMATCH");
    if (lv_arr) {
        cp = sarr_slot(lv_l, lv_idx);
        if (cp == 0)
            return -1;
    } else
        cp = str_val[lv_l];
    str_copyn(cp, s, STR_LEN);
    return 0;
}

int *
arr_slot(l, i)
int l, i;
{
    if (arr_ptr[l] == 0) {
        if (dim_array(l, DEFDIM, 0) < 0)
            return 0;
    }
    if (i < 0 || i > arr_n[l]) {
        rt_error("SUBSCRIPT OUT OF RANGE");
        return 0;
    }
    return &arr_ptr[l][i];
}

char *
sarr_slot(l, i)
int l, i;
{
    if (sarr_ptr[l] == 0) {
        if (dim_array(l, DEFDIM, 1) < 0)
            return 0;
    }
    if (i < 0 || i > sarr_n[l]) {
        rt_error("SUBSCRIPT OUT OF RANGE");
        return 0;
    }
    return sarr_ptr[l] + i * (STR_LEN + 1);
}

dim_array(l, n, isstr)
int l, n, isstr;
{
    if (n < 0 || n > (isstr ? STR_MAX : NUM_MAX))
        return rt_error("BAD SUBSCRIPT");
    if (isstr) {
        if (sarr_ptr[l])
            free(sarr_ptr[l]);
        sarr_ptr[l] = calloc((unsigned) (n + 1), (unsigned) (STR_LEN + 1));
        if (sarr_ptr[l] == 0)
            return rt_error("OUT OF MEMORY");
        sarr_n[l] = n;
    } else {
        if (arr_ptr[l])
            free((char *) arr_ptr[l]);
        arr_ptr[l] = (int *) calloc((unsigned) (n + 1), (unsigned) sizeof(int));
        if (arr_ptr[l] == 0)
            return rt_error("OUT OF MEMORY");
        arr_n[l] = n;
    }
    return 0;
}

do_dim()
{
    int l, n, isstr;

    for (;;) {
        expr_ptr = skip_spaces(expr_ptr);
        if (!is_alpha(*expr_ptr))
            return rt_error("SYNTAX ERROR");
        l = *expr_ptr - 'A';
        expr_ptr++;
        isstr = 0;
        if (*expr_ptr == '$') {
            isstr = 1;
            expr_ptr++;
        }
        expr_ptr = skip_spaces(expr_ptr);
        if (*expr_ptr != '(')
            return rt_error("SYNTAX ERROR");
        expr_ptr++;
        n = ex_or();
        if (err_flag || need_rparen() < 0)
            return -1;
        if (dim_array(l, n, isstr) < 0)
            return -1;
        expr_ptr = skip_spaces(expr_ptr);
        if (*expr_ptr == ',') {
            expr_ptr++;
            continue;
        }
        break;
    }
    return 0;
}

clear_vars()
{
    int i;

    for (i = 0; i < 26; i++) {
        var_int[i] = 0;
        str_val[i][0] = '\0';
        if (arr_ptr[i])
            free((char *) arr_ptr[i]);
        arr_ptr[i] = 0;
        arr_n[i] = 0;
        if (sarr_ptr[i])
            free(sarr_ptr[i]);
        sarr_ptr[i] = 0;
        sarr_n[i] = 0;
    }
    return 0;
}

/* ==================== STATEMENTS ==================== */
st_let()
{
    char s[STR_LEN + 1];
    int v;

    if (parse_lval() < 0)
        return -1;
    expr_ptr = skip_spaces(expr_ptr);
    if (*expr_ptr != '=')
        return rt_error("SYNTAX ERROR");
    expr_ptr++;
    if (lv_str) {
        eval_str(s);
        if (err_flag)
            return -1;
        return store_str(s);
    }
    v = ex_or();
    if (err_flag)
        return -1;
    return store_num(v);
}

out_ch(c)
int c;
{
    putchar(c);
    if (c == '\n')
        out_col = 0;
    else
        out_col++;
    return 0;
}

out_str(s)
char *s;
{
    int c;

    while (*s) {
        c = *s++;
        out_ch(c);
    }
    return 0;
}

st_print()
{
    char s[STR_LEN + 1];
    char nbuf[16];
    char *before;
    int v, nl;

    nl = 1;
    for (;;) {
        expr_ptr = skip_spaces(expr_ptr);
        if (at_stmt_end(expr_ptr))
            break;
        if (*expr_ptr == ';') {
            expr_ptr++;
            nl = 0;
            continue;
        }
        if (*expr_ptr == ',') {
            expr_ptr++;
            do
                out_ch(' ');
            while (out_col % 14 != 0);
            nl = 0;
            continue;
        }
        before = expr_ptr;
        if (is_str_start(expr_ptr)) {
            eval_str(s);
            if (err_flag)
                return -1;
            out_str(s);
        } else {
            v = ex_or();
            if (err_flag)
                return -1;
            if (expr_ptr == before)
                return rt_error("SYNTAX ERROR");
            int_to_str(nbuf, v);
            out_str(nbuf);
        }
        nl = 1;
    }
    if (nl)
        out_ch('\n');
    return 0;
}

/* find an ELSE outside of quotes */
char *
find_else(p)
char *p;
{
    int inq;

    inq = 0;
    for (; *p; p++) {
        if (*p == '"')
            inq = !inq;
        else if (!inq && *p == 'E' && starts_with(p, "ELSE"))
            return p;
    }
    return 0;
}

then_part()
{
    int n;

    expr_ptr = skip_spaces(expr_ptr);
    if (is_digit(*expr_ptr)) {
        n = atoi_custom(expr_ptr);
        while (is_digit(*expr_ptr))
            expr_ptr++;
        return do_jump(n, 0);
    }
    return exec_statement();
}

st_if()
{
    int cond;
    char *e;

    cond = ex_or();
    if (err_flag)
        return -1;
    expr_ptr = skip_spaces(expr_ptr);
    if (starts_with(expr_ptr, "THEN"))
        expr_ptr += 4;
    else if (!starts_with(expr_ptr, "GOTO"))
        return rt_error("SYNTAX ERROR");

    if (cond)
        return then_part();
    e = find_else(expr_ptr);
    if (e == 0)
        return skip_eol();
    expr_ptr = e + 4;
    return then_part();
}

st_goto(sub)
int sub;
{
    int n;

    n = ex_or();
    if (err_flag)
        return -1;
    return do_jump(n, sub);
}

do_jump(n, sub)
int n, sub;
{
    int idx;

    idx = find_line(n);
    if (idx < 0)
        return rt_errnum("UNDEFINED LINE", n);
    if (sub) {
        if (!run_mode)
            return rt_error("ILLEGAL DIRECT");
        if (sub_sp >= SUBSTK)
            return rt_error("GOSUB NESTING TOO DEEP");
        sub_pc[sub_sp] = pc;
        sub_ptr[sub_sp] = skip_spaces(expr_ptr);
        sub_sp++;
    }
    jumped = 1;
    jump_pc = idx;
    jump_ptr = 0;
    return 0;
}

st_return()
{
    if (!run_mode)
        return rt_error("ILLEGAL DIRECT");
    if (sub_sp <= 0)
        return rt_error("RETURN WITHOUT GOSUB");
    sub_sp--;
    jumped = 1;
    jump_pc = sub_pc[sub_sp];
    jump_ptr = sub_ptr[sub_sp];
    return 0;
}

st_for()
{
    int l, start, end, step, i;

    if (!run_mode)
        return rt_error("ILLEGAL DIRECT");
    expr_ptr = skip_spaces(expr_ptr);
    if (!is_alpha(*expr_ptr) || is_alpha(expr_ptr[1]))
        return rt_error("SYNTAX ERROR");
    l = *expr_ptr - 'A';
    expr_ptr++;
    expr_ptr = skip_spaces(expr_ptr);
    if (*expr_ptr != '=')
        return rt_error("SYNTAX ERROR");
    expr_ptr++;
    start = ex_or();
    if (err_flag)
        return -1;
    expr_ptr = skip_spaces(expr_ptr);
    if (!starts_with(expr_ptr, "TO"))
        return rt_error("SYNTAX ERROR");
    expr_ptr += 2;
    end = ex_or();
    if (err_flag)
        return -1;
    step = 1;
    expr_ptr = skip_spaces(expr_ptr);
    if (starts_with(expr_ptr, "STEP")) {
        expr_ptr += 4;
        step = ex_or();
        if (err_flag)
            return -1;
    }
    var_int[l] = start;

    /* re-entering a loop on the same variable discards it and inner loops */
    for (i = 0; i < for_stk_ptr; i++) {
        if (for_var[i] == l) {
            for_stk_ptr = i;
            break;
        }
    }
    if (for_stk_ptr >= FORSTK)
        return rt_error("FOR NESTING TOO DEEP");
    for_var[for_stk_ptr] = l;
    for_end[for_stk_ptr] = end;
    for_step[for_stk_ptr] = step;
    for_pc[for_stk_ptr] = pc;
    for_ptr[for_stk_ptr] = skip_spaces(expr_ptr);
    for_stk_ptr++;
    return 0;
}

st_next()
{
    int v, idx, step, cont;

    if (!run_mode)
        return rt_error("ILLEGAL DIRECT");
    for (;;) {
        expr_ptr = skip_spaces(expr_ptr);
        v = -1;
        if (is_alpha(*expr_ptr)) {
            v = *expr_ptr - 'A';
            expr_ptr++;
        }
        idx = for_stk_ptr - 1;
        if (v >= 0) {
            while (idx >= 0 && for_var[idx] != v)
                idx--;
        }
        if (idx < 0)
            return rt_error("NEXT WITHOUT FOR");
        for_stk_ptr = idx + 1;
        v = for_var[idx];
        step = for_step[idx];
        var_int[v] += step;
        if (step > 0)
            cont = var_int[v] <= for_end[idx];
        else
            cont = var_int[v] >= for_end[idx];
        if (cont) {
            jumped = 1;
            jump_pc = for_pc[idx];
            jump_ptr = for_ptr[idx];
            return 0;
        }
        for_stk_ptr = idx;
        expr_ptr = skip_spaces(expr_ptr);
        if (*expr_ptr == ',') {
            expr_ptr++;
            continue;
        }
        return 0;
    }
}

st_random()
{
    expr_ptr = skip_spaces(expr_ptr);
    if (at_stmt_end(expr_ptr))
        rnd_seed = (int) (time((long *) 0) & 0x7FFFFFFF);
    else
        rnd_seed = ex_or();
    return 0;
}

/* INPUT ["prompt";|,] var[,var...] */
st_input()
{
    char line[LINLEN];
    char pr[STR_LEN + 1];
    char s[STR_LEN + 1];
    char *start;
    int n, v, first, bad, showq;

    pr[0] = '\0';
    showq = 1;
    expr_ptr = skip_spaces(expr_ptr);
    if (*expr_ptr == '"') {
        expr_ptr++;
        n = 0;
        while (*expr_ptr && *expr_ptr != '"') {
            if (n < STR_LEN)
                pr[n++] = *expr_ptr;
            expr_ptr++;
        }
        pr[n] = '\0';
        if (*expr_ptr == '"')
            expr_ptr++;
        expr_ptr = skip_spaces(expr_ptr);
        if (*expr_ptr == ';')
            expr_ptr++;
        else if (*expr_ptr == ',') {
            expr_ptr++;
            showq = 0;
        }
    }
    start = expr_ptr;

    for (;;) {
        expr_ptr = start;
        printf("%s", pr);
        if (showq)
            printf("? ");
        line[0] = '\0';
        in_ptr = line;
        in_more = 0;
        first = 1;
        bad = 0;
        for (;;) {
            if (parse_lval() < 0)
                return -1;
            if (first || !in_more) {
                if (!first)
                    printf("?? ");
                fflush(stdout);
                if (fgets(line, LINLEN, stdin) == 0) {
                    if (intr_flag)
                        clearerr(stdin);
                    return -1;
                }
                i_chomp(line);
                in_ptr = line;
            }
            first = 0;
            get_field(s);
            if (lv_str) {
                if (store_str(s) < 0)
                    return -1;
            } else {
                if (s[0] == '\0') {
                    v = 0;
                    conv_ok = 1;
                } else
                    v = scan_int(s);
                if (!conv_ok) {
                    bad = 1;
                    break;
                }
                if (store_num(v) < 0)
                    return -1;
            }
            expr_ptr = skip_spaces(expr_ptr);
            if (*expr_ptr == ',') {
                expr_ptr++;
                continue;
            }
            break;
        }
        if (!bad)
            break;
        printf("?REDO FROM START\n");
    }
    out_col = 0;
    return 0;
}

/* take the next comma-separated field from in_ptr (INPUT and DATA) */
get_field(dst)
char *dst;
{
    int n;
    char *p;

    p = skip_spaces(in_ptr);
    n = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (n < STR_LEN)
                dst[n++] = *p;
            p++;
        }
        if (*p == '"')
            p++;
        p = skip_spaces(p);
    } else {
        while (*p && *p != ',') {
            if (n < STR_LEN)
                dst[n++] = *p;
            p++;
        }
        while (n > 0 && dst[n - 1] == ' ')
            n--;
    }
    dst[n] = '\0';
    if (*p == ',') {
        p++;
        in_more = 1;
    } else
        in_more = 0;
    in_ptr = p;
    return 0;
}

/* returns -1 when no DATA is left */
read_datum(dst)
char *dst;
{
    char *p;

    for (;;) {
        if (data_ptr == 0) {
            while (data_pc < num_lines) {
                p = skip_spaces(program[data_pc]);
                if (starts_with(p, "DATA")) {
                    data_ptr = p + 4;
                    break;
                }
                data_pc++;
            }
            if (data_ptr == 0)
                return -1;
        }
        if (*skip_spaces(data_ptr) == '\0') {
            data_ptr = 0;
            data_pc++;
            continue;
        }
        in_ptr = data_ptr;
        get_field(dst);
        if (in_more)
            data_ptr = in_ptr;
        else {
            data_ptr = 0;
            data_pc++;
        }
        return 0;
    }
}

st_read()
{
    char s[STR_LEN + 1];
    int v;

    for (;;) {
        if (parse_lval() < 0)
            return -1;
        if (read_datum(s) < 0)
            return rt_error("OUT OF DATA");
        if (lv_str) {
            if (store_str(s) < 0)
                return -1;
        } else {
            if (s[0] == '\0') {
                v = 0;
                conv_ok = 1;
            } else
                v = scan_int(s);
            if (!conv_ok)
                return rt_error("TYPE MISMATCH");
            if (store_num(v) < 0)
                return -1;
        }
        expr_ptr = skip_spaces(expr_ptr);
        if (*expr_ptr == ',') {
            expr_ptr++;
            continue;
        }
        break;
    }
    return 0;
}

/* ==================== SAVE/LOAD/LIST/NEW ==================== */
char *
get_quoted(s)
char *s;
{
    int i;

    i = 0;
    s = skip_spaces(s);
    if (*s != '"') {
        get_quoted_buf[0] = '\0';
        return get_quoted_buf;
    }
    s++;
    while (*s && *s != '"') {
        if (i < STR_LEN)
            get_quoted_buf[i++] = *s;
        s++;
    }
    get_quoted_buf[i] = '\0';
    return get_quoted_buf;
}

do_save(arg)
char *arg;
{
    FILE *f;
    char name[STR_LEN + 1];
    int i;

    strcpy_custom(name, get_quoted(arg));
    if (name[0] == '\0') {
        printf("?NEED QUOTED NAME\n");
        return 0;
    }
    f = fopen(name, "w");
    if (f == 0) {
        printf("?CANNOT OPEN %s\n", name);
        return 0;
    }
    for (i = 0; i < num_lines; i++)
        fprintf(f, "%d %s\n", linenum_arr[i], program[i]);
    fclose(f);
    printf("SAVED\n");
    return 0;
}

do_load(arg)
char *arg;
{
    FILE *f;
    char name[STR_LEN + 1];
    char buf[LINLEN];
    char *p;

    strcpy_custom(name, get_quoted(arg));
    if (name[0] == '\0') {
        printf("?NEED QUOTED NAME\n");
        return 0;
    }
    f = fopen(name, "r");
    if (f == 0) {
        printf("?CANNOT OPEN %s\n", name);
        return 0;
    }
    do_new();
    while (fgets(buf, LINLEN, f) != 0) {
        i_chomp(buf);
        upcase(buf);
        p = skip_spaces(buf);
        if (is_digit(*p))
            parse_line(p);
    }
    fclose(f);
    printf("LOADED\n");
    return 0;
}

do_list(s)
char *s;
{
    int i, lo, hi;

    lo = 0;
    hi = BIGLINE;
    s = skip_spaces(s);
    if (is_digit(*s)) {
        lo = atoi_custom(s);
        hi = lo;
        while (is_digit(*s))
            s++;
        s = skip_spaces(s);
        if (*s == '-') {
            s = skip_spaces(s + 1);
            hi = is_digit(*s) ? atoi_custom(s) : BIGLINE;
        }
    } else if (*s == '-') {
        s = skip_spaces(s + 1);
        hi = atoi_custom(s);
    }
    for (i = 0; i < num_lines; i++) {
        if (intr_flag)
            break;
        if (linenum_arr[i] >= lo && linenum_arr[i] <= hi)
            printf("%d %s\n", linenum_arr[i], program[i]);
    }
    intr_flag = 0;
    return 0;
}

do_new()
{
    int i;

    for (i = 0; i < num_lines; i++) {
        if (program[i])
            free(program[i]);
        program[i] = 0;
    }
    num_lines = 0;
    clear_vars();
    return 0;
}

/* ==================== UTILITIES ==================== */
i_chomp(s)
char *s;
{
    while (*s && *s != '\n')
        s++;
    *s = '\0';
    return 0;
}

/* upper-case everything outside of double quotes */
upcase(s)
char *s;
{
    int inq;

    inq = 0;
    for (; *s; s++) {
        if (*s == '"')
            inq = !inq;
        else if (!inq && *s >= 'a' && *s <= 'z')
            *s = *s - 'a' + 'A';
    }
    return 0;
}

starts_with(s, kw)
char *s, *kw;
{
    while (*kw) {
        if (*s != *kw)
            return 0;
        s++;
        kw++;
    }
    return 1;
}

char *
skip_spaces(s)
char *s;
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

is_digit(c)
int c;
{
    return c >= '0' && c <= '9';
}

is_alpha(c)
int c;
{
    return c >= 'A' && c <= 'Z';
}

atoi_custom(s)
char *s;
{
    int n;

    n = 0;
    while (is_digit(*s)) {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

/* signed integer; sets conv_ok if the whole string was a number */
scan_int(s)
char *s;
{
    int n, neg;
    char *p;

    p = skip_spaces(s);
    neg = 0;
    if (*p == '-') {
        neg = 1;
        p++;
    } else if (*p == '+')
        p++;
    conv_ok = 0;
    n = 0;
    while (is_digit(*p)) {
        n = n * 10 + (*p - '0');
        conv_ok = 1;
        p++;
    }
    p = skip_spaces(p);
    if (*p != '\0')
        conv_ok = 0;
    return neg ? -n : n;
}

strlen_custom(s)
char *s;
{
    int n;

    n = 0;
    while (*s++)
        n++;
    return n;
}

strcmp_custom(s1, s2)
char *s1, *s2;
{
    while (*s1 == *s2) {
        if (*s1 == '\0')
            return 0;
        s1++;
        s2++;
    }
    return (*s1 & 0xFF) - (*s2 & 0xFF);
}

strcpy_custom(dest, src)
char *dest, *src;
{
    while (*src)
        *dest++ = *src++;
    *dest = '\0';
    return 0;
}

strcat_custom(dest, src)
char *dest, *src;
{
    int d;

    d = strlen_custom(dest);
    while (*src && d < STR_LEN)
        dest[d++] = *src++;
    dest[d] = '\0';
    return 0;
}

int_to_str(buf, val)
char *buf;
int val;
{
    char tmp[16];

    int n, k, neg;
    unsigned u;
    u = 0;

    n = 0;
    k = 0;
    neg = 0;



/*
    if (val < 0) {
        neg = 1;
        val = -val;
    }
    if (val == 0)
        tmp[n++] = '0';
    else
        while (val > 0) {
            tmp[n++] = (val % 10) + '0';
            val /= 10;
        }
*/
    u = (unsigned) val;
    if (val < 0) {
        neg = 1;
        u = ~u + 1;
    }
    if (u == 0)
        tmp[n++] = '0';
    else
        while (u > 0) {
            tmp[n++] = (u % 10) + '0';
            u /= 10;
        }

    if (neg)
        buf[k++] = '-';
    while (n > 0)
        buf[k++] = tmp[--n];
    buf[k] = '\0';
    return k;
}
