/* ******************************************

Assume a random walk, calculate the expected "delay" as defined here
  http://www.ericr.nl/wondrous/index.html#part3
Except I define (3*n + 1)/2 as a single step,
  though just uncomment a single line of code to make it 2 steps.
Also, calculate the standard deviation.




Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0
For example, let's start at 9...
  9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1

Run using something like...
  ./a.out [log2(N)]
The argument log2(N)...
  will be converted to a double.
  log2(N) > 0
  should be much smaller than maxSteps if you want accurate results



I will approximate that N has reduced to 1 when...
  3^increases < 2^step / N
which is the same as...
  log2(3) * increases < step - log2(N)



The expected delay depends logarithmically on N, so...
  expectedDelay = f(N) log2(N)
A goal of this code is to find the number f(N).

The standard deviation depends on sqrt(log2(N)), so...
  expectedDelay = g(N) sqrt(log2(N))
A goal of this code is to find the number g(N).



GMP manual...
  https://gmplib.org/manual/Floating_002dpoint-Functions
  https://gmplib.org/manual/Float-Internals
I use GMP until I get the quotient of two huge numbers.
Then I use type double

On macOS, I used MacPorts to do...
  sudo port install gmp
which let me compile via...
  gcc -O3 collatzDelayGMP.c -I/opt/local/include/ -L/opt/local/lib/ -lgmp

On Linux, I needed to do...
  sudo apt install libgmp-dev
which let me compile via...
  gcc -O3 collatzDelayGMP.c -lgmp -lm

On Windows (using Cygwin with gcc-core and libgmp-devel)...
  gcc -O3 collatzDelayGMP.c -lgmp -lm



(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>   // for sqrt()

#include <gmp.h>

#include <sys/time.h>
struct timeval tv1, tv2;

#define min(a,b) (((a)<(b))?(a):(b))



/*
  If less than 1024, this GMP code is overkill!
  This number should be many times larger than log2_N
  Run time is proportional to the square of this number
*/
const int maxSteps = 100000;




int main(int argc, char *argv[]) {

  double log2_N = 100.0;
  if(argc > 1) {
    log2_N = strtod(argv[1], NULL);
  }



  double log2_3 = 1.5849625007211563;   // log2(3)

  // the max number of increases that can affect the calculation
  int iMax = (maxSteps - log2_N) / log2_3;
  if (iMax <= 0) {
    printf(" maxSteps needs to be much larger! Aborting.\n");
    return 0;
  }




  mpf_set_default_prec((mp_bitcnt_t)64);

  // cList[index] gives the number of paths with increases = index
  mpf_t *cList = malloc(sizeof(mpf_t)*(iMax+2));
  mpf_t twoToStep;
  mpf_t quotient;

  for (size_t i=0; i < iMax + 2; i++) mpf_init(cList[i]);
  mpf_init(twoToStep);
  mpf_init(quotient);

  double quotientDouble;





  int increasesOfRemovals = 0;

  double *check = malloc(sizeof(double)*(iMax + 1));
  double *averageDelay = malloc(sizeof(double)*(iMax + 1));
  double *averageDelaySquared = malloc(sizeof(double)*(iMax + 1));

  // initialize to zero
  for (size_t i=0; i <= iMax; i++) check[i] = 0.0;
  for (size_t i=0; i <= iMax; i++) averageDelay[i] = 0.0;
  for (size_t i=0; i <= iMax; i++) averageDelaySquared[i] = 0.0;





  gettimeofday(&tv1, NULL);    // start timer





    int iMin = 0;

    mpf_set_ui(twoToStep, 1);

    // reset cList[]
    mpf_set_ui(cList[0], 1);
    //for (size_t i=1; i < iMax + 2; i++) mpf_set_ui(cList[i], 0);

    for (int step=1; step <= maxSteps; step++) {

      mpf_mul_2exp(twoToStep, twoToStep, (mp_bitcnt_t)1);

      for (int i = min(iMax, step - 1); i >= iMin; i--) {   // i is increases done so far

        mpf_add(cList[i+1], cList[i+1], cList[i]);   // for the increasing steps

        if ( log2_3 * i < step - log2_N ) {

          // cList[i] is binomial coefficient: steps choose i
          mpf_div(quotient, cList[i], twoToStep);
          quotientDouble = mpf_get_d(quotient);
          mpf_clear(cList[i]);

          //increasesOfRemovals = i;   // if (3*n + 1)/2 is 2 steps

          check[iMin] = quotientDouble;
          averageDelay[iMin] = quotientDouble * (step + increasesOfRemovals);
          averageDelaySquared[iMin] = quotientDouble * (step + increasesOfRemovals) * (step + increasesOfRemovals);
          printf(" %f\n", averageDelaySquared[iMin]);

          if ( quotientDouble == (double)0.0 && step < 2*i ) {
            printf(" Step = %i is going to be the final step\n", step);
            step = maxSteps + 1;
          }

          iMin++;

        }

      }


    }

    // Sum backwards for minimal roundoff error
    // Ideally, I should sort before summing.
    double checkTotal = 0;
    double averageDelayTotal = 0;
    double averageDelaySquaredTotal = 0;
    for (int i = iMax; i >= 0; i--) checkTotal += check[i];
    for (int i = iMax; i >= 0; i--) averageDelayTotal += averageDelay[i];
    for (int i = iMax; i >= 0; i--) averageDelaySquaredTotal += averageDelaySquared[i];
    printf("    check = %f\n", checkTotal);       // should be 1
    printf("    expected delay is %f log2(N)\n", averageDelayTotal / log2_N);
    printf("    standard deviation is %f sqrt(log2(N))\n", sqrt(averageDelaySquaredTotal - averageDelayTotal * averageDelayTotal) / sqrt(log2_N));
    printf("    log2(N) = %f\n", log2_N);





  gettimeofday(&tv2, NULL);
  printf("    Running numbers took %e seconds\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec)); 




  return 0;
}
