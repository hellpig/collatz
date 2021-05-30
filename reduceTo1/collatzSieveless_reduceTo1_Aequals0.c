/* ******************************************

Reduce to 1 and find max steps.
This is for the A=0 case (to be run before you can use a 2^k sieve).
Note that the n=2 record of 1 step won't be found due to my 3^2 sieve.

I count (3*n+1)/2 as 1 step.
If you want to count it as 2 steps, there are 4 lines of code
  to uncomment or switch to.



Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0
For example, let's start at 9...
  9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work. This gives a 128-bit integer!
Compile using something like...
  gcc -O3 collatzSieveless_reduceTo1_Aequals0.c

I use __builtin_ctzll(), which should be at least for 64-bit integers.
Note that I use the "long long" function strtoull() when reading in the arguments.

Storage drive not used.
Minimal RAM used to store the 2^k2 lookup table.




Here is a great read for some basic optimizations...
  https://en.wikipedia.org/wiki/Collatz_conjecture#Optimizations
Read it, or else the following won't make sense!

I am using the idea of this paper...
  http://www.ijnc.org/index.php/ijnc/article/download/135/144

Here is a brilliant algorithm that I use to make k2 lookup table...
  https://github.com/xbarin02/collatz/blob/master/doc/ALGORITHM.md
  https://rdcu.be/b5nn1



In binary form, break n into A and B, first choosing k>1...
  B is the k least significant bits.
  A is the rest of the bits.
For k=7...
  n = 0bAAAABBBBBBB
For example...
  643 = 0b1010000011
gives, for k=7...
  A = 0b101 = 5
  B = 0b0000011 = 3
  5*2^7 + 3 = 643

Breaking numbers into this form is useful for doing multiple steps at a time!






A nice optimization is to not run n%3==2 numbers because n = 3*N + 2 always
  follows the already tested 2*N + 1 number (note that 2*N+1 is all odd numbers).
  It is important that 2*N + 1 is less than 3*N + 2. This is called a 3^1 sieve.
A better sieve is to also exclude n%9==4 numbers. This is a 3^2 sieve.
  Any n = 9*N + 4 number always follows the already tested 8*N + 3.
There are tons of these rules, but almost all exclusions occur with the above rules.
  For example, all mod 3^9 rules will block only a few percent more n than a 3^2 sieve,
  but you won't gain because checking against these rules takes time.




To have a well defined right bit shift (>>), use an unsigned type for n.
__uint128_t has a max of 2^128 - 1.
I do lots of bitwise operations in this code!



For this paragraph, I will use a double slash, //, for integer division.
  (3*n+1) / 2 = 3*(n//2) + 2
since n//2 = (n-1)/2 since n is odd.
It is better to use 3*(n//2) + 2 because you calculate n//2 THEN multiply by 3
allowing n to be twice as large without causing an overflow.
This is a minor improvement worth doing!
(Though David Barina's algorithm no longer requires this calculation!)



If overflow is reached, you may have found a number that
  could disprove the conjecture! Overflow must be carefully checked.
If the code reaches the step limit,
  you may also have disproved the conjecture!





This code requires an argument...
  ./a.out task_id

Here is a neat way to run this code...
  seq -f %1.0f 0 7 | parallel -P 2 ./a.out |tee -a log.txt &
To pick up where you left off, change the start (and stop) value of seq.
TASK_SIZE should not change between runs.
Change the -P argument of parallel to run more CPU threads at a time!




274133054632352106267 will overflow 128 bits and, for not-crazy k2, should be
  noticed by this code!
This number is over 64-bits, so it cannot be made into an integer literal.
To test 128-bit overflow...
  task_id = 274133054632352106267 // (1 << TASK_SIZE)




(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/time.h>
struct timeval tv1, tv2;

#define min(a,b) (((a)<(b))?(a):(b))



/*
  The following variables are in log2
  TASK_SIZE should at least be 10 to give each process something to run!
*/
const int TASK_SIZE = 33;



/*
  2^k2 is lookup table size for doing k2 steps at once
  k2 < 37
*/
const int k2 = 15;





/*
  Where to start the search for max steps.
  Put your old result here
*/
int stepsMax = 762;






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

  if( argc < 2 ) {
    printf("Too few arguments. Aborting.\n");
    return 0;
  }

  uint64_t task_id  = (uint64_t)strtoull(argv[1], NULL, 10);

  printf("task_id = ");
  print128(task_id);
  printf("TASK_SIZE = ");
  print128(TASK_SIZE);
  printf("  k2 = %i\n", k2);
  fflush(stdout);



  int j;
  const __uint128_t UINTmax = -1;                   // trick to get all bits to be 1





  // 3^c = 2^(log2(3)*c) = 2^(1.585*c),
  //    so c=80 is the max to fit in 128-bit numbers.
  // Note that c3[0] = 3^0
  //const int lenC3 = 81;  // for 128-bit numbers
  const int lenC3 = 65;




  __uint128_t n, nStart;
  int nMod;





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
  //////// test integers that aren't excluded by 3^2 sieve
  ////////////////////////////////////////////////////////////////

  __uint128_t bStart = ( (__uint128_t)1 << TASK_SIZE )*task_id;
  __uint128_t bEnd = bStart + ( (__uint128_t)1 << TASK_SIZE );

  if (bStart == 0) bStart = 1;

  for (nStart = bStart; nStart < bEnd; nStart++) {


      // a trick for modNum == 9 to get nStart%9 faster
      uint64_t r = 0;
      r += (uint64_t)(nStart)        & 0xfffffffffffffff;
      r += (uint64_t)(nStart >>  60) & 0xfffffffffffffff;
      r += (uint64_t)(nStart >> 120);
      nMod = r%9;



      if (nMod == 2 || nMod == 4 || nMod == 5 || nMod == 8) {   // modNum = 9
        continue;
      }



      n = nStart;

      int steps = 0;


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
                  break;
              }
              n += newB;

              steps += k2;
              //steps += k2 + (int)newC;   // if (3*n + 1)/2 is 2 steps

              if (steps > 1000000) {     // must be checked so that steps doesn't overflow
                  printf("Steps limit reached! nStart = ");
                  print128(nStart);
                  break;
              }

      }

      // will crash if steps limit was reached or if overflow, but who cares?
      steps += delayk2[n];

      if (steps > stepsMax) {
            stepsMax = steps;
            printf(" steps = %i found. nStart = ", steps);
            print128(nStart);
      }



  }


  gettimeofday(&tv2, NULL);
  printf("  %e seconds\n\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec)); 

  // free memory (cuz why not?)
  free(maxNs);
  free(c3);
  free(arrayk2);
  free(delayk2);
  return 0;
}
