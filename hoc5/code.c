#include "hoc.h"
#include "y.tab.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define NSTACK 256
static Datum stack[NSTACK]; /* the stack */
static Datum *stackp; /* next free spot on stack */

#define NPROG 2000
Inst prog[NPROG]; /* the machine */
Inst *progp; /* next free spot for code generation */

Inst *pc;

/* 命令オペランドタイプを示す定数 マシンの表示に使用する */
#define OP_NONE 0 /* オペランドなし add, mul など */
#define OP_SYMBOL 1 /* シンボル（変数・定数）を1つもつ */
#define OP_BLTIN 2 /* 組み込み関数ポインタをもつ */
#define OP_ADDRS 3 /* 複数のアドレスを利用するもの if, while など */

/* マシンのデバック表示をするか */
static int trace_enabled = 1;

static struct {
  Inst func;
  const char *name;
  int op_type;
} inst_table[] = {
  {constpush, "constpush", OP_SYMBOL},
  {varpush, "varpush", OP_SYMBOL},
  {add, "add", OP_NONE},
  {sub, "sub", OP_NONE},
  {mul, "mul", OP_NONE},
  {divide, "divide", OP_NONE},
  {negate, "negate", OP_NONE},
  {power, "power", OP_NONE},
  {eval, "eval", OP_NONE},
  {assign, "assign", OP_NONE},
  {addeq, "addeq", OP_NONE},
  {subeq, "subeq", OP_NONE},
  {muleq, "muleq", OP_NONE},
  {diveq, "diveq", OP_NONE},
  {print, "print", OP_NONE},
  {prexpr, "prexpr", OP_NONE},
  {popstack, "popstack", OP_NONE},
  {bltin, "bltin", OP_BLTIN},
  {gt, "gt", OP_NONE},
  {lt, "lt", OP_NONE},
  {eq, "eq", OP_NONE},
  {ge, "ge", OP_NONE},
  {le, "le", OP_NONE},
  {ne, "ne", OP_NONE},
  {and, "and", OP_NONE},
  {or, "or", OP_NONE},
  {not, "not", OP_NONE},
  {whilecode, "whilecode", OP_ADDRS},
  {ifcode, "ifcode", OP_ADDRS},
  {STOP, "STOP", OP_NONE},
  {NULL, NULL, 0}  /* Sentinel */
};

/* 命令名検索 */
static const char* lookup_inst_name(Inst func, int *op_type) {
  int i;
  for (i = 0; inst_table[i].func != NULL; i++) { /* センチネルまでループ */
    if (inst_table[i].func == func) {
      *op_type = inst_table[i].op_type;
      return inst_table[i].name;
    }
  }
  *op_type = OP_NONE;
  return "UNKNOWN";
}

/* マシンの情報を表示 */
static void trace_instructon(Inst *pc_current) {
  int op_type;
  const char *name = lookup_inst_name(*pc_current, &op_type);
  long offset = pc_current - prog;
  int i;

  fprintf(stderr, "[%04ld] %-12s", offset, name); /* 0埋めして4桁で表示、幅12文字 */

  /* op_typeに応じて出力 */
  switch(op_type){
    case OP_SYMBOL: {
      Symbol *sym = (Symbol *)(*(pc_current + 1));
      fprintf(stderr, " sym='%s' val=%.8g", sym->name, sym->u.val);
      break;
    }
    case OP_BLTIN: {
      void *func = (void *)(*(pc_current + 1));
      fprintf(stderr, "func=%p", func);
      break;
    }
    case OP_ADDRS: {
      Inst **addr1 = (Inst **)(pc_current + 1);
      Inst **addr2 = (Inst **)(pc_current + 2);
      Inst **addr3 = (Inst **)(pc_current + 3);
      fprintf(stderr, " [%ld,%ld,%ld]", 
              *addr1 ? *addr1 - prog : -1, /* アドレスが有効ならオフセット、無効なら-1 */
              *addr2 ? *addr2 - prog : -1,
              *addr3 ? *addr3 - prog : -1
              );
      /* アドレススロットの詳細も表示 */
      fprintf(stderr, "\n");
      fprintf(stderr, "[%04ld]   <addr1>    -> %ld\n", offset+1, *addr1 ? *addr1 - prog : -1);
      fprintf(stderr, "[%04ld]   <addr2>    -> %ld\n", offset+2, *addr2 ? *addr2 - prog : -1);
      fprintf(stderr, "[%04ld]   <addr3>    -> %ld", offset+3, *addr3 ? *addr3 - prog : -1);
      break;
    }
    case OP_NONE:
    default:
      break;
  }
  fprintf(stderr, "\n");
}

void initcode(void) /* initialize for code generation */
{
  stackp = stack; /* stackが空なので先頭のアドレスを代入 */
  progp = prog; /* progが空なので先頭のアドレスを代入 */
}

void push(Datum d) /* push d onto stack */
{
  if(stackp >= &stack[NSTACK]){
    execerror("stack overflow", (char *) 0);
  }
  *stackp++ = d; /* スタックに値を追加して、ポインタを進める */
}

Datum pop(void) /* pop and return top elem from stack */
{
  if (stackp <= stack){
    execerror("stack underflow", (char *) 0);
  }
  return *--stackp; /* ポインタを戻して、スタックから値を取り出す */
}

/* popは戻り値がDatumのためcode2の引数にできない
 * ラッパーを定義してそれをcode2の引数とする */
void popstack(void) /* pop and discard top elem */
{ 
  pop();
}

Inst *code(Inst f) /* install one instruction or operand */
{
  if(progp >= &prog[NPROG]) {
    execerror("stack overflow", (char *) 0);
  }
  Inst *oprogp = progp; /* 命令を書き込む前のポインタを記録 */
  *progp++ = f; /* 命令を書き込んでポインタを進める */
  return oprogp; /* 命令を書き込んだ位置を返す */
}

void execute(Inst *p) /* run the machine */
{
  for(pc = p; *pc != STOP;){
    if (trace_enabled) {
      trace_instructon(pc); /* マシンを表示 */
    }
    /* 
     * Inst f = *pc;
     * pc++;
     * f();
     */
    (*(*pc++))();
  }
}

void constpush(void) /* push constant onto stack */
{
  Datum d;
  d.val = ((Symbol *)*pc++)->u.val;
  push(d);
}

void varpush(void) /* push variable onto stack */
{
  Datum d;
  d.sym = (Symbol *)(*pc++);
  push(d);
}

void add(void) /* add top two elem on stack */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val += d2.val;
  push(d1);
}

void sub(void) /* subtract top two elem on stack */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val -= d2.val;
  push(d1);
}

void mul(void) /* multiply top to elem on stack */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val *= d2.val;
  push(d1);
}

void divide(void)
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  if (d2.val == 0.0){
    execerror("division by zero", (char *) 0);
  }
  d1.val /= d2.val;
  push(d1);
}

void negate(void) /* negate top of stack */
{
  Datum d;
  d = pop();
  d.val = -d.val;
  push(d);
}

void power(void) /* power */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = pow(d1.val, d2.val);
  push(d1);
}

void eval(void) /* 変数シンボルを実際の値に変換する */
{
  Datum d;
  d = pop(); /* スタックから変数シンボルを取得 */
  if (d.sym->type == UNDEF){
    execerror("undefined variable", d.sym->name);
  }
  d.val = d.sym->u.val; /* シンボルから値を取り出す */
  push(d); /* 値をスタックにpush */
}

void assign(void) /* assign top value to next value */
{
  Datum d1, d2;
  d1 = pop();
  d2 = pop();
  if (d1.sym->type != VAR && d1.sym->type != UNDEF){
    execerror("assignment to non-variable", d1.sym->name);
  }
  d1.sym->u.val = d2.val;
  d1.sym->type =  VAR;
  push(d2);
}

void addeq()
{
  Datum d1, d2;
  d1 = pop();
  d2 = pop();
  if (d1.sym->type != VAR){
    execerror("cannot use += on undefined variable", d1.sym->name);
  }
  d2.val = d1.sym->u.val + d2.val;
  d1.sym->u.val = d2.val; // 加算代入される変数の値を更新
  push(d2);
}

void subeq()
{
  Datum d1, d2;
  d1 = pop();
  d2 = pop();
  if (d1.sym->type != VAR){
    execerror("cannot use += on undefined variable", d1.sym->name);
  }
  d2.val = d1.sym->u.val - d2.val;
  d1.sym->u.val = d2.val;
  push(d2);
}

void muleq()
{
  Datum d1, d2;
  d1 = pop();
  d2 = pop();
  if (d1.sym->type != VAR){
    execerror("cannot use += on undefined variable", d1.sym->name);
  }
  d2.val = d1.sym->u.val * d2.val;
  d1.sym->u.val = d2.val;
  push(d2);
}

void diveq()
{
  Datum d1, d2;
  d1 = pop();
  d2 = pop();
  if (d1.sym->type != VAR){
    execerror("cannot use += on undefined variable", d1.sym->name);
  }
  if (d2.val == 0.0){
    execerror("division by zero", (char *) 0);
  }
  d2.val = d1.sym->u.val / d2.val;
  d1.sym->u.val = d2.val;
  push(d2);
}

void print(void) /* pop top value from stack, print it */
{
  Datum d;
  d = pop();
  printf("\t%.8g\n", d.val);
}

void bltin(void) /* evaluate built-in on top of stack */
{
  Datum d;
  d = pop();
  d.val = (*(double (*)())(*pc++))(d.val);
  push(d);
}

void le() /* less than or qeual to */
{ 
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = (double)(d1.val <= d2.val);
  push(d1);
}

void ge() /* greater than or qeual to */
{ 
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = (double)(d1.val >= d2.val);
  push(d1);
}

void gt() /* greater than */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = (double)(d1.val > d2.val);
  push(d1);
}

void lt() /* less than */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = (double)(d1.val < d2.val);
  push(d1);
}

void eq() /* equale to */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = (double)(d1.val == d2.val);
  push(d1);
}

void ne() /* not equale */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = (double)(d1.val != d2.val);
  push(d1);
}

void and() /* and */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = (double)(d1.val && d2.val);
  push(d1);
}

void or() /* or */
{
  Datum d1, d2;
  d2 = pop();
  d1 = pop();
  d1.val = (double)(d1.val || d2.val);
  push(d1);
}

void not() /* not */
{
  Datum d;
  d = pop();
  d.val = (double)(!d.val);
  push(d);
}

void whilecode()
{
  Datum d;
  Inst *savepc = pc; /* loop body */
  execute(savepc+2); /* condition */
  d = pop();
  while (d.val){
    execute(*((Inst **)(savepc))); /* body */
    execute(savepc+2);
    d = pop();
  }
  pc = *((Inst **)(savepc+1)); /* next statement */
}

void ifcode()
{
  /*
  [n]   ifcode命令
  [n+1] then部へのポインタ
  [n+2] else部へのポインタ
  [n+3] 次の文へのポインタ
  [n+4] 条件式コード
   */
  Datum d;
  Inst *savepc = pc; /* executeでインクリメント済みなのでこれがthen部分を指している */
  execute(savepc+3); /* if文の条件式を実行 */
  d = pop(); /* 条件式の結果を取得 */
  if (d.val) {
    execute(*((Inst **)(savepc))); /* then部分を実行 */
  }
  else if (*((Inst **)(savepc+1))) { /* else part? */
    execute(*((Inst **)(savepc+1))); /* else部分を実行 */
  }
  pc = *((Inst **)(savepc+2)); /* next stmt */
}

void prexpr() /* print numeric value */
{
  Datum d;
  d = pop();
  printf("%.8g\n", d.val);
}

