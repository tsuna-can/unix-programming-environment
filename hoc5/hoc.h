typedef struct Symbol { /* Symbol table entry */
  char *name;
  short type; /* VAR, BLTIN, UNDEF */
  union {
    double val;      /* if VAR */
    double (*ptr)(); /* if BLTIN */
  } u;
  struct Symbol *next; /* to link to another */
} Symbol;

extern Symbol *install(char *s, int t, double d);
extern Symbol *lookup(char *s);

typedef union Datum { /* interpreter stack type */
  double val;
  Symbol *sym;
} Datum;
extern Datum pop();

typedef void (*Inst)(); /* machine instruction (voidを返す関数へのポインタ) */
#define STOP (Inst) 0 /* 0をInst型にキャスト NULLポインタとして利用 */

extern Inst prog[];
extern Inst *progp;
extern Inst *code(Inst f); /* 関数ポインタを引き数に取り、関数ポインタへのポインタを返す */
extern void eval(void), add(void), sub(void), mul(void), divide(void), negate(void), power(void);
extern void assign(void), bltin(void), varpush(void), constpush(void), print(void), popstack(void);
extern void prexpr();
extern void gt(void), lt(void), eq(void), ge(void), le(void), ne(void), and(void), or(void), not(void);
extern void ifcode(void), whilecode(void);

extern void execerror(const char *s, const char *t);

extern void push(Datum d);
extern void initcode(void);
extern void execute(Inst *p);

