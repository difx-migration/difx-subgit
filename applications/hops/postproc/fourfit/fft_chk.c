/*
 * $Id: fft_chk.c 377 2011-06-27 20:23:33Z gbc $
 *
 * Program to verify that FFT1 is correct and to find its normalization.
 */

/*
 * Include code to be checked.
 */
#include <stdio.h>
#include <math.h>
#include "mk4_data.h"
#include "type_comp.h"

extern int FFT1(complex In[MAXMAX*2], int NN, int ISign,
		complex Out[MAXMAX*2], int rev);

#include "fft1.c"

/*
 * MAXMAX is 2048 from include/mk4_data.h
 * ISign == 1 for forward xform, -1 for inverse.
 * dc component is in 0
 * smallest positive freq in element 1
 * smallest negative freq in element NN-1
 * rev = 1 reorders the output
 * NN is 2^n for some n
 */

/*
 * Test code follows.
 */
#include <stdlib.h>

int verb;
double tol;


static void dump(complex *a, complex *c, complex *b, int nn, int errs)
{
    printf("%4d.re: %22.18f %22.18f %+.5f %d\n",
	nn, a[nn].re, c[nn].re, (a[nn].re - b[nn].re)/tol, errs);
    printf("%4d.im: %22.18f %22.18f %+.5f %d\n",
	nn, a[nn].im, c[nn].im, (a[nn].im - b[nn].im)/tol, errs);
}

static int compare(complex *a, complex *c, complex *b, int nn)
{
    int errs = 0, kk = 0;
    while (nn-- > 0) {
	if (fabs(a[kk].re - b[kk].re) > tol) errs ++;
	if (fabs(a[kk].im - b[kk].im) > tol) errs ++;
	if (verb>1) dump(a, c, b, kk, errs);
	kk++;
    }
    return(errs);
}

static void rand_junk(complex *d, int nn)
{
    int	kk = 0;
    while (nn-- > 0) {
	d[kk].re = (double)random()/(double)RAND_MAX - 0.5;
	d[kk].im = (double)random()/(double)RAND_MAX - 0.5;
	kk++;
    }
}

static int dothedance(complex junk[2*MAXMAX], int nn)
{
    static complex kunj[2*MAXMAX], komp[2*MAXMAX];
    int kk;
    (void)FFT1(junk, nn,  1, kunj, 0);
    (void)FFT1(kunj, nn, -1, komp, 0);
    /* implicit inverse multiple by NN missing */
    for (kk = 0; kk < nn; kk++) {
	komp[kk].re /= (double)nn;
	komp[kk].im /= (double)nn;
    }
    return(compare(junk, kunj, komp, nn));
}

static int random_test(int nn)
{
    static complex junk[2*MAXMAX];
    if (verb>0) printf("random_test(input,output,diff/tol,errs)\n");
    rand_junk(junk, nn);
    return(dothedance(junk, nn));
}

static int sin_test(int nn)
{
    static complex sinus[2*MAXMAX];
    int ii;
    if (verb>0) printf("sin_test(input,output,diff/tol,errs)\n");
    for (ii = 0; ii < nn; ii++) {
	sinus[ii].re =  1.0 * sin(2.0*M_PI*(double)ii/(double)nn);
	sinus[ii].re += 0.5 * sin(4.0*M_PI*(double)ii/(double)nn);
	sinus[ii].im = 0;
    }
    return(dothedance(sinus, nn));
}

static int cos_test(int nn)
{
    static complex sinus[2*MAXMAX];
    int ii;
    if (verb>0) printf("cos_test(input,output,diff/tol,errs)\n");
    for (ii = 0; ii < nn; ii++) {
	sinus[ii].re =  1.0 * cos(2.0*M_PI*(double)ii/(double)nn);
	sinus[ii].re += 0.5 * cos(4.0*M_PI*(double)ii/(double)nn);
	sinus[ii].im = 0;
    }
    return(dothedance(sinus, nn));
}

int main(int argc, char **argv)
{
    int err = 0, nn;
    char *tv = getenv("testverb");
    char *tl = getenv("tolerance");

    verb = (tv) ? atoi(tv) : 0;
    tol = (tl) ? atof(tl) : 1e-14;

    nn = (argc > 1) ? atoi(argv[1]) : 16;

    err += random_test(nn);
    err += sin_test(nn);
    err += cos_test(nn);

    if (err) printf("%d of %d checks failed, tolerance %g\n", err, 3*nn, tol);
    return(err);
}

/*
 * eof
 */
