/* ******************************************

Assume a random walk, calculate the average "glide" as defined here
  http://www.ericr.nl/wondrous/index.html#part2
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
  gcc -O3 collatzGlide.c
  ./a.out




I will assume (approximate?) that a number has reduced when
  3^increases < 2^step
which is the same as...
  log2(3) * increases < step



To get the 9.477955 in section 2.1 here
  https://chamberland.math.grinnell.edu/papers/3x_survey_eng.pdf
edit my code...
 (1) set cList[1] to 1, not cList[0]
 (2) "int step=1;" to become "int step=2;"
 (3) uncomment out the line to make (3*n+1)/2 be two steps
This number is regarding *only odd* starting values.



(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <math.h>   // for sqrt()



// larger than 1023 causes 2^maxSteps to overflow a double
const int maxSteps = 1023;




int main(void) {


  // cList[index] gives the number of paths with increases = index
  double *cList = malloc(sizeof(double)*(maxSteps+1));
  double countOfRemovals;
  double c;
  int increasesOfRemovals;

  // initialize cList[]
  for (size_t i=0; i < maxSteps + 1; i++) cList[i] = 0.0;
  cList[0] = 1.0;

  double *check = malloc(sizeof(double)*maxSteps);
  double *averageGlide = malloc(sizeof(double)*maxSteps);
  double *averageGlideSquared = malloc(sizeof(double)*maxSteps);

  double log2_3 = 1.5849625007211563;
  double twoToStep = 1.0;




  /* let's do it! */
  for (int step=1; step <= maxSteps; step++) {

    twoToStep *= 2.0;

    for (int i = step - 1; i >= 0; i--) {   // i is increases done so far

      c = cList[i];    // count of paths that have i increases

      cList[i+1] += c;   // for the increasing steps

      if ( log2_3 * i < step ) {
        countOfRemovals = c;
        increasesOfRemovals = 0;
        //increasesOfRemovals = i;   // if (3*n + 1)/2 is 2 steps
        cList[i] = 0;
        break;
      }

    }
    check[step - 1] = countOfRemovals / twoToStep;
    averageGlide[step - 1] = countOfRemovals * (step + increasesOfRemovals) / twoToStep;
    averageGlideSquared[step - 1] = countOfRemovals * (step + increasesOfRemovals) * (step + increasesOfRemovals) / twoToStep;
    //printf(" %f\n", averageGlideSquared[step - 1]);

  }



  // sum backwards for minimal roundoff error
  // Ideally, I should sort before summing.
  double checkTotal = 0;
  double averageGlideTotal = 0;
  double averageGlideSquaredTotal = 0;
  for (int i = maxSteps - 1; i >= 0; i--) checkTotal += check[i];
  for (int i = maxSteps - 1; i >= 0; i--) averageGlideTotal += averageGlide[i];
  for (int i = maxSteps - 1; i >= 0; i--) averageGlideSquaredTotal += averageGlideSquared[i];
  printf("    check = %f\n", checkTotal);       // should be 1
  printf("    average glide = %f\n", averageGlideTotal);
  printf("    standard deviation = %f\n", sqrt(averageGlideSquaredTotal - averageGlideTotal * averageGlideTotal));




  return 0;
}
