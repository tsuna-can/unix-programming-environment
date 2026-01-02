%{
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include "hoc.h"
#define code2(c1,c2) code(c1); code(c2);
#define code3(c1,c2,c3) code(c1); code(c2); code(c3);

int yylex(void);
void yyerror(const char *s);
void warning(const char *s, const char *t);
void execerror(const char *s, const char *t);
void fpecatch(int sig);
void init(void);
void initcode(void);
void execute(Inst *p);
%}

%union{
  Symbol *sym;  /* symbol table pointer */
  Inst *inst; /* machine instruction */
}
%token <sym> NUMBER VAR BLTIN UNDEF
%right '='
%left '+' '-'
%left '*' '/' '%'
%left UNARYPLUS UNARYMINUS
%right '^'  /* exponentiation */
%%

list:  /* nothing */
    | list '\n'
    | list asgn '\n' { code2(popstack, STOP); return 1; }
    | list expr '\n' { code2(print, STOP); return 1; }
    | list error '\n' { yyerrok; }
    ;
asgn: VAR '=' expr {
      code3(varpush, (Inst)$1, assign);
    }
    ;
expr: NUMBER { code2(constpush, (Inst)$1); }
    | VAR { code3(varpush, (Inst)$1, eval); }
    | asgn
    | BLTIN '(' expr ')' { code2(bltin, (Inst)$1->u.ptr); }
    | '(' expr ')'
    | expr '+' expr { code(add); }
    | expr '-' expr { code(sub); }
    | expr '*' expr { code(mul); }
    | expr '/' expr { code(divide); }
    | expr '^' expr { code(power); }
    | '-' expr %prec UNARYMINUS { code(negate); }
    ;
%%

/* end of grammar */

char *progname;
int lineno = 1;
jmp_buf begin;
int main(int argc, char *argv[])
{
  progname = argv[0];
  init();
  setjmp(begin);
  signal(SIGFPE, fpecatch);
  for (initcode(); yyparse(); initcode()) {
    execute(prog);
  }
  cleanup();
  return 0;
}

int yylex(void)
{
  int c;

  while ((c = getchar()) == ' ' || c == '\t') {
    /* 空白とタブをスキップ（何もしない） */
  }
  if (c == EOF) {
    return 0;
  }
  if (isalpha(c)) { /* alphabet */
    Symbol *s;
    char sbuf[100], *p = sbuf;
    do {
      *p++ = c;
    } while ((c=getchar()) != EOF && isalnum(c));
    ungetc(c, stdin);
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
  if (c == '.' || isdigit(c)) { /* number */
    double d;
    ungetc(c, stdin);
    scanf("%lf", &d);
    yylval.sym = install("", NUMBER, d);
    return NUMBER;
  }
  if (c == '\n') {
    lineno++;
    return '\n';
  }
  return c;
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

