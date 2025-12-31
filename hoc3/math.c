#include <errno.h>
#include <math.h>
#include <stdlib.h>

extern int errno;
double errcheck();
void execerror(const char *s, const char *t);

double Log(double x) { return errcheck(log(x), "log"); }

double Log10(double x) { return errcheck(log10(x), "log10"); }

double Exp(double x) { return errcheck(exp(x), "exp"); }

double Sqrt(double x) { return errcheck(sqrt(x), "sqrt"); }

double Pow(double x, double y) { return errcheck(pow(x, y), "exponentiation"); }

double integer(double x) { return (double)(long)x; }

double Atan2(double x, double y) { return errcheck(atan2(y, x), "atan2"); }

double Rand() { return (double)rand()/RAND_MAX; }

 double errcheck(double d, char *s) /* check result of library call */
{
  if (errno == EDOM) {
    errno = 0;
    execerror(s, "argument out of domain");
  } else if (errno == ERANGE) {
    errno = 0;
    execerror(s, "result out of domain");
  }
  return d;
}
