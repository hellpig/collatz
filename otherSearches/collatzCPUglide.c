/* ******************************************

Find a rolling average of the "glide" as defined here
  http://www.ericr.nl/wondrous/index.html#part2
Except I define (3*n + 1)/2 as a single step,
  though uncomment a couple lines of code to call it 2 steps.
I will average the glides in each consecutive 2^k block of numbers



Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0
For example, let's start at 9...
  9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work. This gives a 128-bit integer!
Compile and run using something like...
  gcc -O3 collatzCPUglide.c
  ./a.out [taskID] >> log.txt &
To run many processes but only a few at a time, do something like...
  seq 0 7 | parallel -P 2 ./a.out |tee -a log.txt
The above example will run 8 processes (tasksID 0 to 7) at most 2 at a time.
Numbers taskID*2^k + 1 to (taskID + 1)*2^k will be run.

I use the "long long" function strtoull when reading in the taskID argument.



I define the glide of 1 as 2. Instead of making this definition, you may instead 
think of this code as running...
  taskID*2^k + 2 to (taskID + 1)*2^k + 1



(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/time.h>
struct timeval tv1, tv2;



/* must at least be 2 */
const int k = 30;



// Prints __uint128_t numbers since printf("%llu\n", x) doesn't work
//   since "long long" is only 64-bit in gcc.
// This function works for any non-negative integer less than 128 bits.
void print128(__uint128_t n) {
  char a[40] = { '\0' };
  char *p = a + 39;
  if (n==0) { *--p = (char)('0'); }
  else { for (; n != 0; n /= 10) *--p = (char)('0' + n % 10); }
  printf("%s\n", p);
  fflush(stdout);
}




int main(int argc, char *argv[]) {

  __uint128_t taskID = 0;
  if(argc > 1) {
    taskID = (__uint128_t)strtoull(argv[1], NULL, 10);
  }

  const __uint128_t UINTmax = -1;       // trick to get all bits to be 1
  const __uint128_t nMax = 2*(UINTmax/3)-1;


  __uint128_t n, nStart;
  int j;



  gettimeofday(&tv1, NULL);    // start timer


  ////////////////////////////////////////////////////////////////
  //////// do it
  ////////////////////////////////////////////////////////////////

  __uint128_t stepCount = 0;

  // take care of even numbers right away
  stepCount += (uint64_t)1<<(k-1);

  // take care of nStart % 4 == 1 numbers right away;
  stepCount += (uint64_t)1<<(k-1);
  //stepCount += (uint64_t)1<<(k-2);   // to give (3*n + 1)/2 an extra count

  // do nStart % 4 == 3
  for ( nStart = (taskID<<k) + 3; nStart < ((taskID+1)<<k); nStart += 4) {

    n = nStart;

    while (1) {         // go until overflow or n < nStart

      stepCount += 1;

      if (n & 1) {        // bitwise test for odd
        if (n > nMax) {
          printf(" Overflow!\n");
          return 0;
        }
        n = 3*(n/2) + 2;  // note that n is odd
        //stepCount += 1;   // to give (3*n + 1)/2 an extra count
      } else {
        n >>= 1;
        if (n < nStart) break;
      }

    }

  }




  printf("    taskID = ");
  print128(taskID);

  printf("  total step count = ");
  print128(stepCount);

  printf("  average glide = %f\n", (double)stepCount / (double)((uint64_t)1<<k));

  gettimeofday(&tv2, NULL);
  printf("    Running numbers took %e seconds\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec)); 




  return 0;
}
