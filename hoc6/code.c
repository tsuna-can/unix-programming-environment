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

Inst *progbase = prog; /* start of current subprogram */
int returning; /* 1 if return stmt seen */

typedef struct Frame { /* proc/func call stack frame */
  Symbol *sp; /* symbol table entry */
  Inst *retpc; /* where to resume after return */
  Datum *argn; /* nth argument on stack */
  int nargs; /* number of arguments */
} Frame;

#define NFRAME 100
Frame frame[NFRAME];
Frame *fp; /* frame pointer */

Inst *pc;

/* 命令オペランドタイプを示す定数 マシンの表示に使用する */
#define OP_NONE 0 /* オペランドなし add, mul など */
#define OP_SYMBOL 1 /* シンボル（変数・定数）を1つもつ */
#define OP_BLTIN 2 /* 組み込み関数ポインタをもつ */
#define OP_ADDRS 3 /* 複数のアドレスを利用するもの if, while など */

/* マシンのデバック表示をするか */
static int trace_enabled = 0;

static int break_flag = 0;
static int continue_flag =0;

static struct {
  Inst func;
  const char *name;
  int op_type;
  int noperands; /* 命令の後に続くオペランドのスロット数 */
} inst_table[] = {
  {constpush, "constpush", OP_SYMBOL, 1},
  {varpush, "varpush", OP_SYMBOL, 1},
  {add, "add", OP_NONE, 0},
  {sub, "sub", OP_NONE, 0},
  {mul, "mul", OP_NONE, 0},
  {divide, "divide", OP_NONE, 0},
  {negate, "negate", OP_NONE, 0},
  {power, "power", OP_NONE, 0},
  {eval, "eval", OP_NONE, 0},
  {assign, "assign", OP_NONE, 0},
  {addeq, "addeq", OP_NONE, 0},
  {subeq, "subeq", OP_NONE, 0},
  {muleq, "muleq", OP_NONE, 0},
  {diveq, "diveq", OP_NONE, 0},
  {pre_increment, "pre_increment", OP_NONE, 0},
  {post_increment, "post_increment", OP_NONE, 0},
  {pre_decrement, "pre_decrement", OP_NONE, 0},
  {post_decrement, "post_decrement", OP_NONE, 0},
  {print, "print", OP_NONE, 0},
  {prexpr, "prexpr", OP_NONE, 0},
  {popstack, "popstack", OP_NONE, 0},
  {bltin, "bltin", OP_BLTIN, 1},
  {gt, "gt", OP_NONE, 0},
  {lt, "lt", OP_NONE, 0},
  {eq, "eq", OP_NONE, 0},
  {ge, "ge", OP_NONE, 0},
  {le, "le", OP_NONE, 0},
  {ne, "ne", OP_NONE, 0},
  {and, "and", OP_NONE, 0},
  {or, "or", OP_NONE, 0},
  {not, "not", OP_NONE, 0},
  {whilecode, "whilecode", OP_ADDRS, 2},
  {forcode, "forcode", OP_ADDRS, 6},
  {ifcode, "ifcode", OP_ADDRS, 3},
  {andcode, "andcode", OP_ADDRS, 2},
  {STOP, "STOP", OP_NONE, 0},
  {NULL, NULL, 0, 0}  /* Sentinel */
};

/* 命令名検索 */
static const char* lookup_inst_name(Inst func, int *op_type, int *noperands) {
  int i;
  /* STOPはNULLポインタなので先にチェック */
  if (func == STOP) {
    *op_type = OP_NONE;
    *noperands = 0;
    return "STOP";
  }
  for (i = 0; inst_table[i].name != NULL; i++) { /* センチネルまでループ */
    if (inst_table[i].func == func) {
      *op_type = inst_table[i].op_type;
      *noperands = inst_table[i].noperands;
      return inst_table[i].name;
    }
  }
  *op_type = OP_NONE;
  *noperands = 0;
  return "UNKNOWN";
}

/* 1命令分の情報を表示 */
static void print_inst(Inst *p) {
  int op_type, nops;
  const char *name = lookup_inst_name(*p, &op_type, &nops);
  long offset = p - prog;
  int i;

  fprintf(stderr, "[%04ld] %-16s", offset, name);

  switch(op_type){
    case OP_SYMBOL: {
      Symbol *sym = (Symbol *)(*(p + 1));
      fprintf(stderr, " sym='%s' val=%.8g", sym->name, sym->u.val);
      break;
    }
    case OP_BLTIN: {
      void *func = (void *)(*(p + 1));
      fprintf(stderr, " func=%p", func);
      break;
    }
    case OP_ADDRS: {
      for (i = 1; i <= nops; i++) {
        Inst *addr = (Inst *)(*(p + i));
        fprintf(stderr, " [%ld]", addr ? addr - prog : -1L);
      }
      fprintf(stderr, "\n");
      for (i = 1; i <= nops; i++) {
        Inst *addr = (Inst *)(*(p + i));
        fprintf(stderr, "[%04ld]   <addr%d>      -> %ld\n", offset+i, i, addr ? addr - prog : -1L);
      }
      return; /* 改行済みなのでここで返す */
    }
    case OP_NONE:
    default:
      break;
  }
  fprintf(stderr, "\n");
}

/* マシンの全命令を表示 */
void dump_code(void) {
  Inst *p;
  int op_type, nops;

  fprintf(stderr, "--- program dump ---\n");
  for (p = prog; p < progp; ) {
    lookup_inst_name(*p, &op_type, &nops);
    print_inst(p);
    p += 1 + nops;
  }
  fprintf(stderr, "--- end dump ---\n");
}

void initcode(void) /* initialize for code generation */
{
  progp = progbase;
  stackp = stack; /* stackが空なので先頭のアドレスを代入 */
  fp = frame;
  returning = 0;
  // progp = prog; /* progが空なので先頭のアドレスを代入 */
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
  for(pc = p; *pc != STOP && !returning; ){
    if (trace_enabled) {
      print_inst(pc); /* マシンを表示 */
    }
    if (break_flag) {
      fprintf(stderr, "break at [%ld]\n", pc - prog);
      return;
    } 
    if (continue_flag) {
      fprintf(stderr, "continue at [%ld]\n", pc - prog);
      return;
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
    execerror("cannot use -= on undefined variable", d1.sym->name);
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
    execerror("cannot use *= on undefined variable", d1.sym->name);
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
    execerror("cannot use /= on undefined variable", d1.sym->name);
  }
  if (d2.val == 0.0){
    execerror("division by zero", (char *) 0);
  }
  d2.val = d1.sym->u.val / d2.val;
  d1.sym->u.val = d2.val;
  push(d2);
}

void pre_increment()
{
  Datum d1;
  d1 = pop();
  if (d1.sym->type != VAR){
    execerror("cannot use ++ on undefined variable", d1.sym->name);
  }
  d1.sym->u.val += 1;
  Datum d2 = {.val = d1.sym->u.val};
  push(d2);
}

void post_increment()
{
  Datum d1;
  d1 = pop();
  if (d1.sym->type != VAR){
    execerror("cannot use ++ on undefined variable", d1.sym->name);
  }
  Datum d2 = {.val = d1.sym->u.val};
  push(d2);
  d1.sym->u.val += 1;
}

void pre_decrement()
{
  Datum d1;
  d1 = pop();
  if (d1.sym->type != VAR){
    execerror("cannot use -- on undefined variable", d1.sym->name);
  }
  d1.sym->u.val -= 1;
  Datum d2 = {.val = d1.sym->u.val};
  push(d2);
}

void post_decrement()
{
  Datum d1;
  d1 = pop();
  if (d1.sym->type != VAR){
    execerror("cannot use -- on undefined variable", d1.sym->name);
  }
  Datum d2 = {.val = d1.sym->u.val};
  push(d2);
  d1.sym->u.val -= 1;
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
    if (returning) break;
    execute(savepc+2);
    d = pop();
  }
  if (!returning) {
    pc = *((Inst **)(savepc+1)); /* next statement */
  }
}

void breakcode(){
  break_flag = 1;
}

void continuecode(){
  continue_flag = 1;
}

void forcode()
{
  /*
  [n]   forcode命令
  [n+1] 初期化式
  [n+2] 条件式
  [n+3] 更新式
  [n+4] ループの内部
  [n+5] 次の文
   */
  Datum d;
  Inst *savepc = pc;
  execute(*((Inst **)(savepc))); /* 初期化式の実行 */
  execute(*((Inst **)(savepc+1))); /* 条件式の実行 */
  d = pop();
  while (d.val){
    execute(*((Inst **)(savepc+3))); /* body */
    if (break_flag){
      break;
    }
    if (continue_flag){
      continue_flag = 0;
    }
    execute(*((Inst **)(savepc+2))); /* 更新式の実行 */
    execute(*((Inst **)(savepc+1))); /* 条件式の実行 */
    d = pop();
  }
  break_flag = 0; /* breakフラグをリセット */
  pc = *((Inst **)(savepc+4)); /* next statement */
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
  if (!returning){
    pc = *((Inst **)(savepc+2)); /* next stmt */
  }
}

void andcode()
{
  /*
  [n]   andcode命令
  [n+1] 右辺へのポインタ
  [n+2] 次の命令へのポインタ
   */
  Datum d1, d2, d3;
  Inst *savepc = pc; /* executeでインクリメント済みなのでこれが右辺を指している */
  d1 = pop(); /* 左辺の結果を取得 */
  if (d1.val) {
    execute(*((Inst **)(savepc))); /* 右辺を実行 */
    d2 = pop(); /* 右辺の結果を取得 */
    if (d2.val) {
      d3.val = 1;
    } else {
      d3.val = 0;
    }
  } else {    
    d3.val = 0;
  }
  push(d3);
  pc = *((Inst **)(savepc+1)); /* next stmt */
}

void orcode()
{
  /*
  [n]   orcode命令
  [n+1] 右辺へのポインタ
  [n+2] 次の命令へのポインタ
   */
  Datum d1, d2, d3;
  Inst *savepc = pc; /* executeでインクリメント済みなのでこれが右辺を指している */
  d1 = pop(); /* 左辺の結果を取得 */
  if (d1.val) {
    d3.val = 1;
  } else {    
    execute(*((Inst **)(savepc))); /* 右辺を実行 */
    d2 = pop(); /* 右辺の結果を取得 */
    if (d2.val) {
      d3.val = 1;
    } else {
      d3.val = 0;
    }
  }
  push(d3);
  pc = *((Inst **)(savepc+1)); /* next stmt */
}

void prexpr() /* print numeric value */
{
  Datum d;
  d = pop();
  printf("%.8g\n", d.val);
}

void push1()
{
  Datum d;
  d.val = 1;
  push(d);
}

void define(Symbol *sp) /* put func/proc in symbol table */
{
  sp->u.defn = (Inst)progbase; /* start of code */
  progbase = progp; /* next code starts here */
}

void call() /* call a function */
{
  Symbol *sp = (Symbol *)pc[0]; /* symbol table entry */
  /* for function */
  if (fp++ >= &frame[NFRAME-1]) {
    execerror(sp->name, "call nested too deeply");
  }
  fp->sp = sp;
  fp->nargs = (int)pc[1];
  fp->retpc = pc +2;
  fp->argn = stackp -1; /* last argument */
  execute((Inst *)sp->u.defn);
  returning = 0;
}

void funcret() /* return from a function */
{
  Datum d;
  if (fp->sp->type == PROCEDURE) {
    execerror(fp->sp->name, "(proc) returns value");
  }
  d = pop(); /* preserve function return value */
  ret();
  push(d);
}

void procret() /* return from a procedure */
{
  if(fp->sp->type == FUNCTION) {
    execerror(fp->sp->name, "(func) returns no value");
  }
  ret();
}

void ret() /* common return from a func or proc */
{
  int i;
  for (i=0; i< fp->nargs; i++) {
    pop(); /* pop arguments */
  }
  pc = (Inst *)fp->retpc;
  --fp;
  returning = 1;
}

double *getarg() /* return pointer to argument */
{
  int nargs = (int) *pc++;
  if (nargs > fp->nargs) {
    execerror(fp->sp->name, "not enough arguments");
  }
  return &fp->argn[nargs - fp->nargs].val;
}

void arg() /*push argument onto stack */
{
  Datum d;
  d.val = *getarg();
  push(d);
}

void argassign() /* store top of stack argument */
{
  Datum d;
  d = pop();
  push(d); /* leave value on stack */
  *getarg() = d.val;
}

void prstr() /* print string value */
{
  printf("%s", (char *) *pc++);
}

void preexpr() /* print numeric value */
{
  Datum d;
  d = pop();
  printf("%.8g ", d.val);
}

void varread() /* read into variable */
{
  Datum d;
  extern FILE *fin;
  Symbol *var = (Symbol *) *pc++;
Again:
  switch(fscanf(fin, "%1f", &var->u.val)) {
    case EOF:
      if (moreinput())
        goto Again;
      d.val = var->u.val = 0.0;
      break;
    case 0:
      execerror("non-number read into" , var->name);
      break;
    default:
      d.val = 1.0;
      break;
  }
  var->type = VAR;
  push(d);
}
