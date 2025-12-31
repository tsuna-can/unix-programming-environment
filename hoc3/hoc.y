%{
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include "hoc.h"
extern double Pow();

int yylex(void);
void yyerror(const char *s);
void warning(const char *s, const char *t);
void execerror(const char *s, const char *t);
void fpecatch(int sig);
void init(void);
%}

%union{
  double val;  /* actual type */
  Symbol *sym;  /* symbol table pointer */
}
%token <val> NUMBER
%token <sym> VAR BLTIN UNDEF CONST NEW_CONST
%type <val> expr asgn 
%right '='
%left '+' '-'
%left '*' '/' '%'
%left UNARYPLUS UNARYMINUS
%right '^'  /* exponentiation */
%%

list:  /* nothing */
    | list '\n'
    | list asgn '\n'
    | list expr '\n' { printf("\t%.8g\n",$2); }
    | list error '\n' { yyerrok; }
    ;
asgn: VAR '=' expr { 
       $$ = $1->u.val=$3; 
       $1->type = VAR; 
      }
    ;
asgn: CONST '=' expr {
      execerror("cannot reassign to CONST ", $1->name);
      }
    ;
asgn: NEW_CONST '=' expr { 
       $$ = $1->u.val=$3; 
       $1->type = CONST; 
      }
    ;
expr: NUMBER
    | VAR { 
        if ($1->type == UNDEF) {
          execerror("undefined variable", $1 ->name); 
        }
        $$ = $1->u.val;
      }
    | CONST { $$ = $1->u.val; }
    | asgn
    | BLTIN '(' ')' { $$ = (*($1->u.ptr))(); }
    | BLTIN '(' expr ')' { $$ = (*($1->u.ptr))($3); }
    | BLTIN '(' expr ',' expr ')' { $$ = (*($1->u.ptr))($3,$5); }
    | expr '+' expr { $$ = $1 + $3; }
    | expr '-' expr { $$ = $1 - $3; }
    | expr '*' expr { $$ = $1 * $3; }
    | expr '/' expr { 
        if ($3 == 0.0 ) {
          execerror("division by zero", "");
        }
        $$ = $1 / $3;
      }
    | expr '^' expr { $$ = Pow($1, $3); }
    | expr '%' expr { $$ = fmod($1, $3); }
    | '(' expr ')' { $$ = $2; }
    | '+' expr %prec UNARYPLUS { $$ = +$2; }
    | '-' expr %prec UNARYMINUS { $$ = -$2; }
    ;
%%

char *progname;
int lineno = 1;
jmp_buf begin;
int main(int argc, char *argv[])
{
  progname = argv[0];
  init();
  setjmp(begin);
  signal(SIGFPE, fpecatch);
  yyparse();
}

int is_upper_only(char *s){
  for (; *s; s++){
    if (isalpha(*s) && !isupper(*s)){
      return 0;
     }
   }
  return 1;
}

int yylex(void)
{
  int c;

  while ((c = getchar()) == ' ' || c == '\t')
    ;
  if (c == EOF) {
    return 0;
  }
  if (c == '!') {  /* シェルエスケープ処理*/
    char cmd[256];  /* シェルコマンドを格納するバッファ */
    int i = 0;
    while ((c = getchar()) != '\n' && c != EOF && i < 255) {
      cmd[i++] = c;
    }
    cmd[i] = '\0';  /* 文字列の終端を示すヌル文字を追加 */
    system(cmd);  /* コマンド実行 */
    lineno++;
    return '\n';
  }
  if (isalpha(c)) {
    Symbol *s;
    char sbuf[100], *p = sbuf;
    do {
      *p++ = c;
    } while ((c=getchar()) != EOF && isalnum(c));
    ungetc(c, stdin);
    *p = '\0';
    if ((s=lookup(sbuf)) == 0){
      s = install(sbuf, UNDEF, 0.0);
    }
    yylval.sym = s;
    if (s->type == UNDEF) {
      return is_upper_only(sbuf) ? NEW_CONST : VAR;
    }
    return s->type;
  }
  if (c == '.' || isdigit(c)) {
    ungetc(c, stdin);
    scanf("%lf", &yylval.val);
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
  if(t)
    fprintf(stderr, "%s", t);
  fprintf(stderr, " near line %d\n", lineno);
}
