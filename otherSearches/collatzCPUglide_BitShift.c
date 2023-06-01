/* ******************************************

Find a rolling average of the "glide" as defined here
  http://www.ericr.nl/wondrous/index.html#part2
Except I define a step as a bit shift as defined here
  https://rdcu.be/b5nn1
I will average the glides in each consecutive 2^k block of numbers,
  so it isn't a true rolling average.



Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0
For example, let's start at 9...
  9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work. This gives a 128-bit integer!
Compile and run using something like...
  gcc -O3 collatzCPUglide_BitShift.c
  ./a.out [taskID] >> log.txt &
To run many processes but only a few at a time, do something like...
  seq 0 7 | parallel -P 2 ./a.out |tee -a log.txt
The above example will run 8 processes (tasksID 0 to 7) at most 2 at a time.
Numbers taskID*2^k + 1 to (taskID + 1)*2^k will be run.

I use the following to speed up my code...
  __builtin_ctzll(n)
On my computer, an "unsigned long long" is only 64 bits (not 128 bits), so I limit the length
  of the lookup tables for powers of 3, and I do the following for alpha and beta...
            if ((uint64_t)n == 0) alpha = 64;
            else alpha = __builtin_ctzll(n);
If that didn't make sense yet, don't worry about it. I just put it here to be with similar info.
I also use the "long long" function strtoull when reading in the taskID argument.

To have a well defined right bit shift (>>), use an unsigned type for n.
__uint128_t has a max of 2^128 - 1.
I do lots of bitwise operations in this code!



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


/* 2^k numbers will be run */
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

  const __uint128_t UINTmax = -1;                   // trick to get all bits to be 1


  // 3^c = 2^(log2(3)*c) = 2^(1.585*c),
  //    so c=80 is the max to fit in 128-bit numbers.
  // Note that c3[0] = 3^0
  const int lenC3 = 81;  // for 128-bit numbers


  __uint128_t n, nStart;
  int j, alpha, beta;




  // calculate lookup table for c3, which is 3^c
  __uint128_t* c3 = (__uint128_t*)malloc(lenC3*sizeof(__uint128_t));
  c3[0] = 1;
  for (j=1; j<lenC3; j++) { c3[j] = c3[j-1]*3; }


  // defining max values is very important to detect overflow
  // calculate lookup table for maxNs
  __uint128_t* maxNs = (__uint128_t*)malloc(lenC3*sizeof(__uint128_t));
  for (j=0; j<lenC3; j++) { maxNs[j] = UINTmax / c3[j]; }



  gettimeofday(&tv1, NULL);    // start timer


  ////////////////////////////////////////////////////////////////
  //////// do it
  ////////////////////////////////////////////////////////////////

  __uint128_t stepCount = 0;

  // take care of even numbers right away
  stepCount += (uint64_t)1<<(k-1);

  // take care of nStart % 4 == 1 numbers right away;
  stepCount += (uint64_t)1<<(k-1);

  // do nStart % 4 == 3
  for ( nStart = (taskID<<k) + 3; nStart < ((taskID+1)<<k); nStart += 4) {

    n = nStart;

    while (1) {             // go until overflow or n < nStart

      stepCount += 2;

      n++;
      if ((uint64_t)n == 0) alpha = 64 + __builtin_ctzll(n>>64);
      else alpha = __builtin_ctzll(n);
      n >>= alpha;
      if ( alpha >= lenC3 || n > maxNs[alpha] ) {
        printf("Overflow! nStart = ");
        print128(nStart);
        return 0;
      }
      n *= c3[alpha];   // 3^c from lookup table

      n--;
      if ((uint64_t)n == 0) beta = 64 + __builtin_ctzll(n>>64);
      else beta = __builtin_ctzll(n);
      n >>= beta;
      if (n < nStart) break;

    }

  }

  printf("    taskID = ");
  print128(taskID);

  printf("  total step count = ");
  print128(stepCount);

  printf("  average glide = %e\n", (double)stepCount / (double)((uint64_t)1<<k));

  gettimeofday(&tv2, NULL);
  printf("    Running numbers took %e seconds\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec)); 



  // free memory (cuz why not?)
  free(maxNs);
  free(c3);
  return 0;
}
