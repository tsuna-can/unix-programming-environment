#include "hoc.h"
#include "y.tab.h"
#include <math.h>
#include <stdio.h>

#define NSTACK 256
static Datum stack[NSTACK]; /* the stack */
static Datum *stackp; /* next free spot on stack */

#define NPROG 2000
Inst prog[NPROG]; /* the machine */
Inst *progp; /* next free spot for code generation */
Inst *pc; /* program counter during execution */

void initcode(void) /* initialize for code generation */
{
  stackp = stack;
  progp = prog;
}

void push(Datum d) /* push d onto stack */
{
  if(stackp >= &stack[NSTACK]){
    execerror("stack overflow", (char *) 0);
  }
  *stackp++ = d;
}

Datum pop(void) /* pop and return top elem from stack */
{
  if (stackp <= stack){
    execerror("stack underflow", (char *) 0);
  }
  return *--stackp;
}

/* popは戻り値がDatumのためcode2の引数にできない
 * ラッパーを定義してそれをcode2の引数とする */
void popstack(void) /* pop and discard top elem */
{ 
  pop();
}

Inst *code(Inst f) /* install one instruction or operand */
{
  Inst *oprogp = progp;
  if(progp >= &prog[NPROG]){
    execerror("program too big", (char *) 0);
  }
  *progp++ = f;
  return oprogp;
}

void execute(Inst *p) /* run the machine */
{
  for(pc = p; *pc != STOP; ){
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

