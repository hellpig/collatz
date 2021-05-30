/* ******************************************

Find a rolling average of the "delay" as defined here
  http://www.ericr.nl/wondrous/index.html#part3
Except I define (3*n + 1)/2 as a single step,
  though 4 lines can be uncommented or switched to make it 2 steps.
I will average the delays in each consecutive 2^k block of numbers



Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0
For example, let's start at 9...
  9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work. This gives a 128-bit integer!
Compile and run using something like...
  gcc -O3 collatzCPUdelay.c
  ./a.out [log2(N)] [taskID] >> log.txt &
Numbers N to N + 2^k - 1 will be run for taskID=0,
  so log2(N) >= 1.0 must be true.
The next taskID runs the next 2^k numbers.
The argument log2(N) will be converted to a double,
  then 2^log2(N) will be converted to an integer.
To run many processes but only a few at a time, do something like...
  seq 0 7 | parallel -P 2 ./a.out 50 |tee -a log.txt
The above example will run 8 processes (tasksID 0 to 7) at most 2 at a time.




(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <sys/time.h>
struct timeval tv1, tv2;

#define min(a,b) (((a)<(b))?(a):(b))


/*
  2^k numbers will be run
  k=30 will take around 3 minutes for a log2(N) just before overflow starts to happen
*/
const int k = 30;



/*
  2^k2 is lookup table size for doing k2 steps at once
  k2 < 37
*/
const int k2 = 15;




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

  double log2_N = 40.0;
  if(argc > 1) {
    log2_N = strtod(argv[1], NULL);
  }
  int taskID = 0;
  if(argc > 2) {
    taskID = strtol(argv[2], NULL, 10);
  }





  const __uint128_t UINTmax = -1;       // trick to get all bits to be 1
  const int lenC3 = 65;

  // calculate lookup table for c3, which is 3^c
  __uint128_t* c3 = (__uint128_t*)malloc(lenC3*sizeof(__uint128_t));
  c3[0] = 1;
  for (int j=1; j<lenC3; j++) { c3[j] = c3[j-1]*3; }

  // defining max values is very important to detect overflow
  // calculate lookup table for maxNs
  __uint128_t* maxNs = (__uint128_t*)malloc(lenC3*sizeof(__uint128_t));
  for (int j=0; j<lenC3; j++) { maxNs[j] = UINTmax / c3[j]; }



  __uint128_t n, nStart, N;

  N = pow(2.0, log2_N);
  N += ((__uint128_t)1 << k) * taskID;
  printf("    log2(N) = %f\n", log2_N);
  printf("    taskID = %lu\n", (unsigned long)taskID);
  printf("    N + taskID * 2^k = ");
  print128(N);




  gettimeofday(&tv1, NULL);    // start timer







  ////////////////////////////////////////////////////////////////
  //////// create arrayk2[] and delayk2[] for the 2^k2 lookup tables
  ////////////////////////////////////////////////////////////////


  uint64_t *arrayk2 = malloc(sizeof(uint64_t) * ((size_t)1 << k2));
  int *delayk2 = malloc(sizeof(int) * ((size_t)1 << k2));

  for (size_t index = 0; index < ((size_t)1 << k2); ++index) {

    uint64_t L = index;   // index is the initial L
    size_t Salpha = 0;   // sum of alpha
    int reducedTo1 = 0;
    if (L == 0) goto next;

    int R = k2;   // counter
    size_t alpha, beta;

    do {
        L++;
        do {
            if ((uint64_t)L == 0) alpha = 64;  // __builtin_ctzll(0) is undefined
            else alpha = __builtin_ctzll(L);

            alpha = min(alpha, (size_t)R);
            R -= alpha;
            L >>= alpha;
            L *= c3[alpha];
            Salpha += alpha;
            if (R == 0) {
              L--;
              goto next;
            }
        } while (!(L & 1));
        L--;
        do {
            if ((uint64_t)L == 0) beta = 64;  // __builtin_ctzll(0) is undefined
            else beta = __builtin_ctzll(L);

            beta = min(beta, (size_t)R);
            R -= beta;
            L >>= beta;
            if (L == 1 && reducedTo1 == 0) reducedTo1 = k2 - R;
            //if (L == 1 && reducedTo1 == 0) reducedTo1 = k2 - R + Salpha;     // if (3*n + 1)/2 is 2 steps
            if (R == 0) goto next;
        } while (!(L & 1));
    } while (1);

next:

    /* stores both L and Salpha */
    arrayk2[index] = L + ((uint64_t)Salpha << 58);

    size_t Ssteps = 0;
    if (L == 0) goto finish;
    if (reducedTo1 > 0) {
      if (index == 1) delayk2[1] = 0;
      else delayk2[index] = reducedTo1;
      continue;
    }

    do {
        L++;
        do {
            if ((uint64_t)L == 0) alpha = 64;  // __builtin_ctzll(0) is undefined
            else alpha = __builtin_ctzll(L);

            L >>= alpha;
            L *= c3[alpha];
            //Salpha += alpha;   // if (3*n + 1)/2 is 2 steps
            Ssteps += alpha;
        } while (!(L & 1));
        L--;
        do {
            if ((uint64_t)L == 0) beta = 64;  // __builtin_ctzll(0) is undefined
            else beta = __builtin_ctzll(L);

            L >>= beta;
            Ssteps += beta;
            if (L == 1) goto finish;
        } while (!(L & 1));
    } while (1);

finish:

    delayk2[index] = k2 + Ssteps;
    //delayk2[index] = k2 + Ssteps + Salpha;  // if (3*n + 1)/2 is 2 steps

  }









  ////////////////////////////////////////////////////////////////
  //////// do it
  ////////////////////////////////////////////////////////////////

  __uint128_t steps = 0;

  for ( nStart = N; nStart < N + ((__uint128_t)1<<k); nStart++) {

    n = nStart;

    /* do k2 steps at a time */
    while ( (n >> k2) > 0 ) {

        size_t index = n & ( ((uint64_t)1<<k2) - 1 );  // get lowest k2 bits of n
        __uint128_t newB = (__uint128_t)arrayk2[index];
        size_t newC = newB >> 58;    // just 6 bits gives c
        newB &= 0x3ffffffffffffff;   // rest of bits gives b

        /* find the new n */
        //n = (n >> k2)*c3[newC] + newB;
        n >>= k2;
        if (n > maxNs[newC]) {
            printf("Overflow! nStart = ");
            print128(nStart);
            break;
        }
        n *= c3[newC];
        if (n > UINTmax - newB) {
            printf("Overflow! nStart = ");
            print128(nStart);
            return 0;
        }
        n += newB;

        steps += k2;
        //steps += k2 + (int)newC;   // if (3*n + 1)/2 is 2 steps

    }

    steps += delayk2[n];

  }



  printf("  total step count = ");
  print128(steps);

  printf("  average delay = %f log2(N)\n", (double)steps / (double)((uint64_t)1<<k) / log2_N );

  gettimeofday(&tv2, NULL);
  printf("    Running numbers took %e seconds\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));




  return 0;
}
