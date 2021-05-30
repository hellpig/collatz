/* ******************************************

Confirming Collatz conjecture up to a certain start n.

Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0
For example, let's start at 9...
  9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work. This gives a 128-bit integer!
Compile using something like...
  gcc -O3 collatzSieveless_repeatedKsteps.c

I use __builtin_ctzll(), which should be at least for 64-bit integers.
Note that I use the "long long" function strtoull() when reading in the arguments.

Sieves of size 2^k are used, where k can be very large!
Storage drive not used.
Minimal RAM used to store the 2^k2 sieve.

k < 81 must be true



Here is a great read for some basic optimizations...
  https://en.wikipedia.org/wiki/Collatz_conjecture#Optimizations
Read it, or else the following won't make sense!

I am using the idea of this paper...
  http://www.ijnc.org/index.php/ijnc/article/download/135/144

Here is a brilliant algorithm that I use to make k2 sieve...
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

Breaking numbers into this form is useful for forming exclusion rules
  and for doing k steps at a time!



If n ever becomes less than start n, move on to testing next n.
This works if we test *all* not-excluded numbers n>1.



This code tests all n = A*2^k + B where...
 - B is not excluded by a 2^k sieve
 - n is not excluded by a 3^2 sieve
 - aStart <= A < aEnd
You are free to set k yourself!





A nice optimization is to not run n%3==2 numbers because n = 3*N + 2 always
  follows the already tested 2*N + 1 number (note that 2*N+1 is all odd numbers).
  It is important that 2*N + 1 is less than 3*N + 2. This is called a 3^1 sieve.
A better sieve is to also exclude n%9==4 numbers. This is a 3^2 sieve.
  Any n = 9*N + 4 number always follows the already tested 8*N + 3.
There are tons of these rules, but almost all exclusions occur with the above rules.
  For example, all mod 3^9 rules will block only a few percent more n than a 3^2 sieve,
  but you won't gain because checking against these rules takes time.



To speed up the code, I loop over all aStart <= a < aEnd for each b value
  before moving to the next b.
This is best called interlacing. This is faster for many reasons!
This allows the first k steps to occur all at once with minimal calculation!




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
If the code gets stuck on a certain number,
  you may also have disproved the conjecture!





This code requires two arguments...
  ./a.out  task_id0  task_id

Starting at 0, increase task_id by 1 each run until its max
  ( 2^(k - TASK_SIZE) - 1 )
Then, reset task_id and increase task_id0 by 1 (starting at 0).
Don't skip!

Here is a neat way to run this code...
  seq -f %1.0f 0 1048575 | parallel -P 2 ./a.out 0 |tee -a log.txt &
To pick up where you left off, change the start (and stop) value of seq.
k, TASK_SIZE, and TASK_SIZE0 should not change between runs.
Change the -P argument of parallel to run more CPU threads at a time!

For each task_id0, 9 * 2 ^ TASK_SIZE0 numbers will be tested,
  but only after each task_id is run from 0 to ( 2^(k - TASK_SIZE) - 1 )
Why the 9? I thought it might help my GPU code, but it only does EXTREMELY SLIGHTLY.
Feel free to get rid of the 9 when aStart and aSteps are defined in the code.
You'll also want to get rid of the division by 9 when this host program
  checks if task_id0 will cause overflow.
If task_id0 > 0, you'll also want to fix the line...
  aMod = 0



274133054632352106267 will overflow 128 bits and, for not-crazy k2, should be
  noticed by this code!
This number is over 64-bits, so it cannot be made into an integer literal.
To test 128-bit overflow and using printf() in kernel2,
calculate the following using Python 3, then put it as your task_id0...
  task_id0 = 274133054632352106267 // (9 << TASK_SIZE0)
Then set the following as your TASK_ID...
  remainder = (274133054632352106267 % (9 << TASK_SIZE0))
  task_id = (remainder % (1 << k)) // (1 << TASK_SIZE)




(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/time.h>
struct timeval tv1, tv2;



/*
  The following variables are in log2
  TASK_SIZE0 - k should ideally be at least 16 for k=51 and deltaN_max = 222
  TASK_SIZE0 - k should be larger for larger k or for larger deltaN_max
  TASK_SIZE should at least be 10 to give each process something to run!
  Due to the limits of strtoull(), k - TASK_SIZE < 64
  9 * 2^(TASK_SIZE0 + TASK_SIZE - k) numbers will be run by each process
*/
const int k = 51;
const int TASK_SIZE0 = 67;       // 9 * 2^TASK_SIZE0 numbers will be run total
const int TASK_SIZE = 20;        // TASK_SIZE <= k



/*
  For a smaller 2^k2 sieve to do k2 steps at a time after the initial k steps
  3 < k2 < 37
  Will use more than 2^(k2 + 3) bytes of RAM
  For my CPU, 11 is the best because it fits in cache
*/
const int k2 = 11;





__uint128_t deltaN_max = 222;     // don't let deltaN be larger than this







// Code will test aStart <= a < aStart + aSteps
// Set this to 2^0 = 1 to have get an idea of how long processing the sieve takes.
// Ideally, I like to set this to at least 2^10.
const __uint128_t aSteps = (__uint128_t)9 << (TASK_SIZE0 - k);



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

  if( argc < 3 ) {
    printf("Too few arguments. Aborting.\n");
    return 0;
  }

  uint64_t task_id0 = (uint64_t)strtoull(argv[1], NULL, 10);

  uint64_t task_id  = (uint64_t)strtoull(argv[2], NULL, 10);

  uint64_t maxTaskID = ((uint64_t)1 << (k - TASK_SIZE));
  if ( task_id >= maxTaskID ) {
    printf("Aborting. task_id must be less than ");
    print128( maxTaskID );
    return 0;
  }

  printf("task_id0 = ");
  print128(task_id0);
  printf("task_id = ");
  print128(task_id);
  printf("task_id must be less than ");
  print128(maxTaskID);
  printf("TASK_SIZE = ");
  print128(TASK_SIZE);
  printf("TASK_SIZE0 = ");
  print128(TASK_SIZE0);
  fflush(stdout);




  int j;
  __uint128_t deltaN;

  // lookup deltaN (else calculate it)
  if (k<=5) deltaN = 0;
  else if (k<=18) deltaN = 1;
  else if (k<=24) deltaN = 6;
  else if (k<=27) deltaN = 12;
  else if (k<=29) deltaN = 25;
  else if (k<=32) deltaN = 34;
  else if (k<=33) deltaN = 37;
  else if (k<=35) deltaN = 46;
  else if (k<=37) deltaN = 88;
  else if (k<=40) deltaN = 120;
  else if (k<=43) deltaN = 208;
  else if (k<=45) deltaN = 222;
  else if (k<=46) deltaN = 5231;  // needs experimental reduction
  else if (k<=47) deltaN = 6015;  // needs experimental reduction
  else {
    int minC = 0.6309297535714574371 * k + 1.0;  // add 1 to get ceiling
    double minC3 = 1.0;     // 3^minC
    for (j=0; j<minC; j++) minC3 *= 3.0;
    double deltaNtemp = 0.0;
    for (j=0; j<minC; j++) deltaNtemp = (3.0 * deltaNtemp + 1.0) / 2.0;
    deltaNtemp = deltaNtemp * (((__uint128_t)1<<k) - ((__uint128_t)1<<minC)) / minC3;
    deltaN = deltaNtemp + 1.0;    // add 1 to get ceiling
  }

  if ( deltaN > deltaN_max ) deltaN = deltaN_max;

  printf("  k = %i\n", k);
  printf("  deltaN = ");
  print128(deltaN);
  printf("  k2 = %i\n", k2);
  fflush(stdout);




  const __uint128_t kk = (__uint128_t)1 << k;       // 2^k
  const __uint128_t UINTmax = -1;                   // trick to get all bits to be 1


  if ( task_id0 > ( (UINTmax / 9) >> (TASK_SIZE0 - k) ) - 1 ) {
    printf("Error: Overflow!\n");
    return 0;
  }
  const __uint128_t aStart = (__uint128_t)task_id0*9 << (TASK_SIZE0 - k);
  const __uint128_t aEnd = aStart + aSteps;



  // 3 or 9 (the actual code must be changed to match, so keep it at 9)
  const int modNum = 9;

  // 3^c = 2^(log2(3)*c) = 2^(1.585*c),
  //    so c=80 is the max to fit in 128-bit numbers.
  // Note that c3[0] = 3^0
  //const int lenC3 = 81;  // for 128-bit numbers
  const int lenC3 = k+1;




  __uint128_t n, nStart, a, m;
  int alpha, aMod, nMod, bMod;


  //const int kkMod = kk % modNum;
  //const int kkMod = (k & 1) ? 2 : 1 ;   // trick for modNum == 3
  uint64_t r = 0;    // trick for modNum == 9
  r += (uint64_t)(kk)        & 0xfffffffffffffff;
  r += (uint64_t)(kk >>  60) & 0xfffffffffffffff;
  r += (uint64_t)(kk >> 120);
  const int kkMod = r%9;



  // calculate lookup table for c3, which is 3^c
  __uint128_t* c3 = (__uint128_t*)malloc(lenC3*sizeof(__uint128_t));
  c3[0] = 1;
  for (j=1; j<lenC3; j++) { c3[j] = c3[j-1]*3; }


  // defining max values is very important to detect overflow
  // calculate lookup table for maxNs
  __uint128_t* maxNs = (__uint128_t*)malloc(lenC3*sizeof(__uint128_t));
  for (j=0; j<lenC3; j++) { maxNs[j] = UINTmax / c3[j]; }


  // Test a for overflow
  // Note that after k steps for B = 2^k - 1, B will become 3^k - 1
  //   and A*2^k will become A*3^k
  const __uint128_t maxA = (UINTmax - c3[k] + 1) / c3[k];     // max a
  if (aEnd - 1 > maxA) {
    printf("Error: a*2^k + (2^k - 1) will overflow after k steps!\n");
    return 0;
  }





  gettimeofday(&tv1, NULL);    // start timer






  ////////////////////////////////////////////////////////////////
  //////// create arrayk2[] for the 2^k2 sieve
  ////////////////////////////////////////////////////////////////

#define min(a,b) (((a)<(b))?(a):(b))

  uint64_t *arrayk2 = malloc(sizeof(uint64_t) * ((size_t)1 << k2));

  for (size_t index = 0; index < ((size_t)1 << k2); ++index) {

    uint64_t L = index;   // index is the initial L
    size_t Salpha = 0;   // sum of alpha
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
            if (R == 0) goto next;
        } while (!(L & 1));
    } while (1);

next:

    /* stores both L and Salpha */
    arrayk2[index] = L + ((uint64_t)Salpha << 58);

  }





  ////////////////////////////////////////////////////////////////
  //////// test integers that aren't excluded by certain rules
  ////////////////////////////////////////////////////////////////

  __uint128_t bStart = ( (__uint128_t)1 << TASK_SIZE )*task_id + 3;
  __uint128_t bEnd = bStart + ( (__uint128_t)1 << TASK_SIZE );

  __uint128_t countB = 0;

  for (__uint128_t b = bStart; b < bEnd; b += 4) {

      int go = 1;          // acts as a boolean
      __uint128_t bb = b;  // will become fk(b)
      int c = 0;           // number of increases experienced when calculated fk(b)

      // check to see if 2^k*N + b is reduced in no more than k steps
      for (j=0; j<k; j++) {   // step
        if (bb & 1) {         // bitwise test for odd
          bb = 3*(bb/2) + 2;  // note that bb is odd
          c++;
        } else {
          bb >>= 1;
          if (bb < b) {      // if 2^k*N + b is reduced to a*N + bb
            go = 0;
            break;
          }
        }
      }

      // if go=1, use another method to try to get go=0
      if (go) {

        __uint128_t lenList = ((deltaN+1) < (b-1)) ? (deltaN+1) : (b-1) ;   // get min(deltaN+1, b-1)
        for(m=1; m<lenList; m++) {    // loop over lists

          __uint128_t bm = b - m;
          int cm = 0;

          // take k steps to get bm and cm
          for(j=0; j<k; j++) {
            if (bm & 1) {
              bm = 3*(bm/2) + 2;
              cm++;
            } else {
              bm >>= 1;
            }
          }

          // check bm and cm against bb and c
          if ( bm == bb && cm == c ) {
            go = 0;
            //print128(b);
            break;
          }
      
        }
      }

      if ( go == 0 ) {
        continue;
      }
      countB++;

      aMod = 0;

      // a trick for modNum == 9 to get b%9 faster
      uint64_t r = 0;
      r += (uint64_t)(b)        & 0xfffffffffffffff;
      r += (uint64_t)(b >>  60) & 0xfffffffffffffff;
      r += (uint64_t)(b >> 120);
      bMod = r%9;

      for (a=aStart; a<aEnd; a++) {    // loop over a before next b (interlacing for speed)



          // do a 3^2 sieve

          nMod = (aMod * kkMod + bMod) % modNum;

          // iterate before the following possible continue
          aMod++;
          if (aMod == modNum) aMod = 0;

          //if (nMod == 2) continue;    // modNum = 3
          if (nMod == 2 || nMod == 4 || nMod == 5 || nMod == 8) {   // modNum = 9
            continue;
          }




          nStart = (a<<k) + b;

          n = a*c3[c] + bb;


          /* do k2 steps at a time */
          do {
              size_t index = n & ( ((uint64_t)1<<k2) - 1 );  // get lowest k2 bits of n
              uint64_t newB = arrayk2[index];
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

              if (n < nStart) break;
          } while (1);

      }

  }

  printf("  Numbers in sieve segment that needed testing = ");
  print128(countB);

  gettimeofday(&tv2, NULL);
  printf("  %e seconds\n\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec)); 

  // free memory (cuz why not?)
  free(maxNs);
  free(c3);
  free(arrayk2);
  return 0;
}
