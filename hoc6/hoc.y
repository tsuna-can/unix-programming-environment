%{
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "hoc.h"
#define code2(c1,c2) code(c1); code(c2);
#define code3(c1,c2,c3) code(c1); code(c2); code(c3);
#define code6(c1,c2,c3,c4,c5,c6) code(c1); code(c2); code(c3); code(c4); code(c5); code(c6);

int yylex(void);
void yyerror(const char *s);
void warning(const char *s, const char *t);
void execerror(const char *s, const char *t);
void fpecatch(int sig);
void init(void);
void initcode(void);
void execute(Inst *p);
int follow(int expect, int ifyes, int ifno);
void defnonly(char *s);
int indef;
char *infile; /* input file name */
FILE *fin; /* inptu file pointer */
char **gargv; /* global argument list */
int gargc;
int c; /* global for use by warning() */
int backslash(int c);
int moreinput(void);
void run(void);
%}

%union{
  Symbol *sym;  /* symbol table pointer */
  Inst *inst; /* machine instruction */
  int narg; /* number of arguments */
}
%token <sym> NUMBER PRINT VAR BLTIN UNDEF WHILE IF ELSE FOR BREAK CONTINUE STRING /* 終端記号 */
%token <sym> FUNCTION PROCEDURE RETURN FUNC PROC READ
%token <narg> ARG
%type <inst> stmt asgn expr prlist stmtlist cond while if begin end and or for forcond optional break continue ternary /* 非終端記号 */
%type <sym> procname
%type <narg> arglist
%right '=' ADDEQ SUBEQ MULEQ DIVEQ INCREMENT DECREMENT
%right '?' ':'
%left AND OR
%left GT GE LT LE EQ NE
%left '+' '-'
%left '*' '/' '%'
%left UNARYPLUS UNARYMINUS NOT
%right '^'
%%

list:  /* nothing */
    | list '\n'
    | list defn '\n'
    | list asgn '\n' { code2(popstack, STOP); return 1; }
    | list stmt '\n' { code(STOP); return 1; }
    | list expr '\n' { code2(print, STOP); return 1; }
    | list error '\n' { yyerrok; }
    ;
asgn: VAR '=' expr {
      $$ = $3;
      code3(varpush, (Inst)$1, assign);
    }
    | ARG '=' expr {
      defnonly("$");
      code2(argassign, (Inst)$1);
      $$=$3;
    }
    | VAR ADDEQ expr {
      $$ = $3;
      code3(varpush, (Inst)$1, addeq);
    }
    | VAR SUBEQ expr {
      $$ = $3;
      code3(varpush, (Inst)$1, subeq);
    }
    | VAR MULEQ expr {
      $$ = $3;
      code3(varpush, (Inst)$1, muleq);
    }
    | VAR DIVEQ expr {
      $$ = $3;
      code3(varpush, (Inst)$1, diveq);
    }
    | INCREMENT VAR {
      $$ = code3(varpush, (Inst)$2, pre_increment);
    }
    | VAR INCREMENT {
      $$ = code3(varpush, (Inst)$1, post_increment);
    }
    | DECREMENT VAR {
      $$ = code3(varpush, (Inst)$2, pre_decrement);
    }
    | VAR DECREMENT {
      $$ = code3(varpush, (Inst)$1, post_decrement);
    }
    ;
stmt: expr { code(popstack); }
    | RETURN {
      defnonly("return");
      code(procret);
    }
    | RETURN expr {
      defnonly("return");
      $$=$2;
      code(funcret);
    }
    | PROCEDURE begin '(' arglist ')' {
      $$=$2;
      code3(call, (Inst)$1, (Inst)$4);
    }
    | PRINT prlist {
      $$=$2;
    }
    | break
    | continue
    | PRINT expr {
      code(prexpr);
      $$ = $2;
    }
    | for '(' optional ';' end forcond ';' end optional ')' end stmt end {
      ($1)[1] = (Inst)$3; /* 初期化式 */
      ($1)[2] = (Inst)$6; /* 条件式 */
      ($1)[3] = (Inst)$9; /* 更新式 */
      ($1)[4] = (Inst)$12; /* ループ内部の文 */
      ($1)[5] = (Inst)$13; /* 次の文 */
    }
    | while cond stmt end {
      ($1)[1] = (Inst)$3; /* body of loop */
      ($1)[2] = (Inst)$4; /* end, if cond fails */
    }
    | if cond stmt end { /* else-less if */
      ($1)[1] = (Inst)$3; /* then part */
      ($1)[3] = (Inst)$4; /* end, if cond fails */
    }
    | if cond stmt end ELSE stmt end { /* if with else */
      ($1)[1] = (Inst)$3; /* then part */
      ($1)[2] = (Inst)$6; /* else part */
      ($1)[3] = (Inst)$7; /* end, if cond fails */
    }
    | '{' stmtlist '}' {
      $$ = $2;
    }
    ;
cond: '(' expr ')' {
      code(STOP);
      $$ = $2;
    }
    ;
while: WHILE {
      $$ = code3(whilecode, STOP, STOP);
    }
    ;
for: FOR {
      $$ = code6(forcode, STOP, STOP, STOP, STOP, STOP);
    }
    ;
optional: expr
    | /* nothing */ { $$ = progp; }
    ;
forcond: expr
    | /* nothing */ { $$ = code(push1); }
    ;
if: IF {
      $$ = code(ifcode);
      code3(STOP, STOP, STOP);
    }
    ;
and: AND {
      $$ = code(andcode);
      code2(STOP, STOP);
    }
    ;
or: OR {
      $$ = code(orcode);
      code2(STOP, STOP);
    }
    ;
ternary: '?'  {
       $$ = code(ternarycode);
       code3(STOP, STOP, STOP);
    }
    ;
break: BREAK {
      $$ = code(breakcode);
    }
    ;
continue: CONTINUE {
      $$ = code(continuecode);
    }
    ;
end: /* nothing */ {
      code(STOP);
      $$ = progp;
    }
    ;
stmtlist: /* nothing */ { $$ = progp; }
    | stmtlist '\n'
    | stmtlist stmt
    ;
expr: NUMBER { 
      $$ = code2(constpush, (Inst)$1); 
    }
    | VAR { 
      $$ = code3(varpush, (Inst)$1, eval); 
    }
    | ARG {
      defnonly("$");
      $$=code2(arg, (Inst)$1);
    }
    | asgn
    | FUNCTION begin '(' arglist ')' {
      $$=$2;
      code3(call, (Inst)$1, (Inst)$4);
    }
    | READ '(' VAR ')' {
      $$=code2(varread, (Inst)$3);
    }
    | BLTIN '(' expr ')' {
      $$ = $3;
      code2(bltin, (Inst)$1->u.ptr); 
    }
    | '(' expr ')' { $$ = $2; }
    | expr '+' expr { code(add); }
    | expr '-' expr { code(sub); }
    | expr '*' expr { code(mul); }
    | expr '/' expr { code(divide); }
    | expr '^' expr { code(power); }
    | '-' expr %prec UNARYMINUS { 
      $$ = $2;
      code(negate); 
    }
    | expr GT expr { code(gt); }
    | expr GE expr { code(ge); }
    | expr LT expr { code(lt); }
    | expr LE expr { code(le); }
    | expr EQ expr { code(eq); }
    | expr NE expr { code(ne); }
    | expr and expr end {
      ($2)[1] = (Inst) $3; /* 右辺部分 */
      ($2)[2] = (Inst) progp; /* 次の命令 */
    }
    | expr or expr end {
      ($2)[1] = (Inst) $3; /* 右辺部分 */
      ($2)[2] = (Inst) progp; /* 次の命令 */
    }
    | expr ternary expr end ':' expr end {
      ($2)[1] = (Inst) $3; /* then部分 */
      ($2)[2] = (Inst) $6; /* else命令 */
      ($2)[3] = (Inst) progp; /* 次の命令 */
    }
    | NOT expr {
      $$ = $2;
      code(not);
    }
    ;
begin: /* nothing */ {
       $$=progp;
    }
    ;
prlist: expr { code(prexpr); }
    | STRING {
      $$=code2(prstr, (Inst)$1);
    }
    | prlist ',' expr {
      code(prexpr);
    }
    | prlist ',' STRING {
      code2(prstr, (Inst)$3);
    }
    ;
defn: FUNC procname { $2->type=FUNCTION; indef=1; } '(' ')' stmt {
      code(procret);
      define($2);
      indef=0;
    }
    | PROC procname { $2->type=PROCEDURE; indef=1; } '(' ')' stmt {
      code(procret);
      define($2);
      indef=0;
    }
    ;
procname: VAR
    | FUNCTION
    | PROCEDURE
    ;
arglist: /* nothing */ { $$ = 0; }
    | expr { $$ = 1; }
    | arglist ',' expr { $$ = $1 + 1; }
    ;
%%

/** 
* $$ = $1は省略できる
**/

/* end of grammar */

char *progname;
int lineno = 1;
jmp_buf begin;

int main(int argc, char *argv[])
{
  progname = argv[0];
  if (argc == 1) { /* fake an argument list */
    static char *stdinonly[] = { "-" };
    gargv = stdinonly;
    gargc = 1;
  } else {
    gargv = argv + 1;
    gargc = argc - 1;
  }
  init();
  while (moreinput()) {
    run();
  }
  return 0;
}

int moreinput()
{
  if (gargc-- <= 0){
    return 0;
  }
  if (fin && fin != stdin) {
    fclose(fin);
  }
  infile = *gargv++;
  lineno = 1;
  if (strcmp(infile, "-") == 0 ){
    fin = stdin;
    infile = 0;
  } else if (( fin=fopen(infile, "r")) == NULL) {
    fprintf(stderr, "%s: can't open %s\n", progname, infile);
    return moreinput();
  }
  return 1;
}

void run ()
{
  setjmp(begin);
  signal(SIGFPE, fpecatch);
  for (initcode(); yyparse(); initcode()) {
    dump_code();
    execute(progbase);
  }
}

int c;

int yylex(void)
{
  while ((c = getc(fin)) == ' ' || c == '\t') {
    /* 空白とタブをスキップ（何もしない） */
  }
  if (c == EOF) {
    return 0;
  }
  if (c == '.' || isdigit(c)) { /* number */
    double d;
    ungetc(c, fin);
    fscanf(fin, "%lf", &d);
    yylval.sym = install("", NUMBER, d);
    return NUMBER;
  }
  if (isalpha(c)) { /* alphabet */
    Symbol *s;
    char sbuf[100], *p = sbuf;
    do {
      if (p >= sbuf + sizeof(sbuf) -1) {
        *p = '\0';
        execerror("name too long" , sbuf);
      }
      *p++ = c;
    } while ((c=getc(fin)) != EOF && isalnum(c));
    ungetc(c, fin);
    *p = '\0';
    if ((s=lookup(sbuf)) == 0) {
      s = install(sbuf, UNDEF, 0.0);
    }
    yylval.sym = s;
    if (s->type == UNDEF) {
      return VAR;
    }
    return s->type;
  }
  if ( c == '$') { /* argument? */ 
    int n = 0;
    while (isdigit(c=getc(fin))) {
      n = 10 * n + c - '0';
    }
    ungetc(c, fin);
    if (n == 0) {
      execerror("strange $...", (char*)0);
    }
    yylval.narg = n;
    return ARG;
  }
  if ( c == '"') { /* quoted string */
    char sbuf[100], *p, *emalloc();
    for ( p = sbuf; (c=getc(fin)) != '"'; p++) {
      if (c == '\n' || c == EOF) {
        execerror("missing quote", "");
      }
      if (p >= sbuf + sizeof(sbuf) -1 ) {
        *p = '\0';
        execerror("string too long" , sbuf);
      }
      *p = backslash(c);
    }
    *p = 0;
    yylval.sym = (Symbol *)emalloc(strlen(sbuf)+1);
    strcpy(yylval.sym, sbuf);
    return STRING;
  }
  switch (c) {
    case '>': return follow('=', GE, GT);
    case '<': return follow('=', LE, LT);
    case '=': return follow('=', EQ, '=');
    case '!': return follow('=', NE, NOT);
    case '|': return follow('|', OR, '|');
    case '&': return follow('&', AND, '&');
    case '+': return follow('=', ADDEQ, follow('+', INCREMENT, '+'));
    case '-': return follow('=', SUBEQ, follow('-', DECREMENT, '-'));
    case '*': return follow('=', MULEQ, '*');
    case '/': return follow('=', DIVEQ, '/');
    case '\n': lineno++; return '\n';
    default: return c;
  }
  if (c == '\n') { /* return */
    lineno++;
    return '\n';
  }
  return c;
}

int follow(int expect, int ifyes, int ifno) /* look after for >=, etc... */
{
  int c = getc(fin);
  if (c == expect){
    return ifyes;
  }
  ungetc(c, fin);
  return ifno;
}

void execerror(const char *s, const char *t)
{
  warning(s,t);
  longjmp(begin, 0);
}

void fpecatch(int sig)
{
  execerror("floating point exception", (char *) 0);
}

void yyerror(const char *s)
{
  warning(s, (char *) 0);
}

void warning(const char *s, const char *t)
{
  fprintf(stderr, "%s: %s", progname, s);
  if(t){
    fprintf(stderr, "%s", t);
  }
  fprintf(stderr, " near line %d\n", lineno);
}

void defnonly(char *s) /* warn if illegal definition */
{
  if (!indef){
    execerror(s, "used outside definition");
  }
}

int backslash(int c) /* get next char with \'s interpreted */ 
{
  char *index(); /* `strchar()' in some system */
  static char transtab[] = "b\bf\fn\nr\rt\t";
  if ( c != '\\' ) return c;
  c = getc(fin);
  if ( islower(c) && index(transtab, c)) return index(transtab, c)[1];
  return c;
}

