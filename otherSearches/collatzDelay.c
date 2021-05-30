/* ******************************************

Assume a random walk, calculate the expected "delay" as defined here
  http://www.ericr.nl/wondrous/index.html#part3
Except I define (3*n + 1)/2 as a single step,
  though just switch out a single line of code to make it 2 steps.
Also, calculate the standard deviation.




Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0
For example, let's start at 9...
  9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1

Compile and run using something like...
  gcc -O3 collatzDelay.c
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



(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>   // for sqrt()




// larger than 1023 causes 2^maxSteps to overflow a double
const int maxSteps = 1023;




int main(int argc, char *argv[]) {

  double log2_N = 40.0;
  if(argc > 1) {
    log2_N = strtod(argv[1], NULL);
  }



  // cList[index] gives the number of paths with increases = index
  double *cList = malloc(sizeof(double)*(maxSteps+1));
  double countOfRemovals;
  double c;
  int increasesOfRemovals;

  double *check = malloc(sizeof(double)*maxSteps);
  double *averageDelay = malloc(sizeof(double)*maxSteps);
  double *averageDelaySquared = malloc(sizeof(double)*maxSteps);

  double log2_3 = 1.5849625007211563;   // log2(3)




    double twoToStep = 1.0;

    // initialize cList[]
    cList[0] = 1.0;
    for (size_t i=1; i < maxSteps + 1; i++) cList[i] = 0.0;

    for (int step=1; step <= maxSteps; step++) {

      twoToStep *= 2.0;

      for (int i = step - 1; i >= 0; i--) {   // i is increases done so far

        c = cList[i];    // count of paths that have i increases

        cList[i+1] += c;   // for the increasing steps

        if ( log2_3 * i < step - log2_N ) {
          countOfRemovals = c;
          increasesOfRemovals = 0;
          //increasesOfRemovals = i;   // if (3*n + 1)/2 is 2 steps
          cList[i] = 0;
          break;
        }

        if ( i == 0) countOfRemovals = 0.0;

      }
      check[step - 1] = countOfRemovals / twoToStep;
      averageDelay[step - 1] = countOfRemovals * (step + increasesOfRemovals) / twoToStep;
      averageDelaySquared[step - 1] = countOfRemovals * (step + increasesOfRemovals) * (step + increasesOfRemovals) / twoToStep;
      printf(" %f\n", averageDelaySquared[step - 1]);

    }

    // sum backwards for minimal roundoff error
    // Ideally, I should sort before summing.
    double checkTotal = 0;
    double averageDelayTotal = 0;
    double averageDelaySquaredTotal = 0;
    for (int i = maxSteps - 1; i >= 0; i--) checkTotal += check[i];
    for (int i = maxSteps - 1; i >= 0; i--) averageDelayTotal += averageDelay[i];
    for (int i = maxSteps - 1; i >= 0; i--) averageDelaySquaredTotal += averageDelaySquared[i];
    printf("    check = %f\n", checkTotal);       // should be 1
    printf("    expected delay is %f log2(N)\n", averageDelayTotal / log2_N);
    printf("    standard deviation is %f sqrt(log2(N))\n", sqrt(averageDelaySquaredTotal - averageDelayTotal * averageDelayTotal) / sqrt(log2_N));
    printf("    log2(N) = %f\n", log2_N);





  return 0;
}
