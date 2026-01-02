#include "hoc.h"
#include "y.tab.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static Datum *stack = NULL; /* malloc・reallocで確保したスタック領域全体の先頭アドレスを指す */
static Datum *stackp; /* next free spot on stack */
static size_t stack_size = 0; /* malloc・reallocで確保した容量を記録 */

Inst *prog = NULL;  /* malloc・reallocで確保したスタック領域全体の先頭アドレスを指す */
Inst *progp; /* next free spot for code generation */
static size_t prog_size = 0; /* malloc・reallocで確保した容量を記録 */

Inst *pc;

void initcode(void) /* initialize for code generation */
{
  if (stack == NULL) {
    stack_size = 1;
    stack = (Datum *)malloc(stack_size * sizeof(Datum));
  }
  stackp = stack; /* stackが空なので先頭のアドレスを代入 */

  if (prog == NULL) {
    prog_size = 1;
    prog = (Inst *)malloc(prog_size * sizeof(Inst));
  }
  progp = prog; /* stackが空なので先頭のアドレスを代入 */
}

void push(Datum d) /* push d onto stack */
{
  if(stackp >= stack + stack_size){
    size_t offset = stackp - stack; /* 既にスタックに積まれている要素数 */
    // stack_size *= 2;
    stack_size++;
    Datum *new_stack = (Datum *)realloc(stack, stack_size * sizeof(Datum));
    if (new_stack == NULL) {
      execerror("stack overflow", (char *) 0);
    }
    stack = new_stack; /* realloc後にアドレスが変わる可能性があるのでコピー */
    stackp = stack + offset;
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
  if(progp >= prog + prog_size) {
    size_t offset = progp - prog;
    prog_size *= 2;
    Inst *new_prog = (Inst *)realloc(prog, prog_size * sizeof(Inst));
    if (new_prog == NULL) {
      execerror("program too big", (char *) 0);
    }
    prog = new_prog;
    progp = prog + offset;
  }
  Inst *oprogp = progp; /* 命令を書き込む前のポインタを記録 */
  *progp++ = f; /* 命令を書き込んでポインタを進める */
  return oprogp; /* 命令を書き込んだ位置を返す */
}

void execute(Inst *p) /* run the machine */
{
  for(pc = p; *pc != STOP;){
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

void eval(void) /* evaluate variable on stack */
{
  Datum d;
  d = pop();
  if (d.sym->type == UNDEF){
    execerror("undefined variable", d.sym->name);
  }
  d.val = d.sym->u.val;
  push(d);
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

void cleanup(void)
{
  if (stack != NULL) {
    free(stack);
    stack = NULL;
  }
  if (prog != NULL){
    free(prog);
    prog = NULL;
  }
}
    
