#include "hoc.h"
#include "y.tab.h"
#include <math.h>

extern double Log(), Log10(), Exp(), Sqrt(), integer(), Atan2(), Rand();

static struct { /* Constants */
  char *name;
  double cval;
} consts[] = {"PI",  3.141592,  "E", 2.71828, "GAMMA", 0.577215, /* Euler */
              "DEG", 57.295779, /* deg/radian */
              "PHI", 1.61803,   /* golden ration */
              0,     0};

static struct { /* Build-ins */
  char *name;
  double (*func)();
} builtins[] = {"sin",   sin,     "cos", cos,  "atan", atan, 
                "atan2", Atan2, /* checks argument */
                "log",   Log,   /* checks argument */
                "log10", Log10, /* checks argument */
                "exp",   Exp,   /* checks argument */
                "sqer",  Sqrt,  /* checks argument */
                "int",   integer, "abs", fabs, "rand", Rand, 0,      0};

void init() /* install constants and build-ins in table */
{
  int i;
  Symbol *s;

  for (i = 0; consts[i].name; i++) {
    install(consts[i].name, CONST, consts[i].cval);
  }
  for (i = 0; builtins[i].name; i++) {
    s = install(builtins[i].name, BLTIN, 0.0);
    s->u.ptr = builtins[i].func;
  }
}
