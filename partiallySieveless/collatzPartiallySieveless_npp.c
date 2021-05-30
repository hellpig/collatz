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
  gcc -O3 collatzPartiallySieveless_npp.c

I use the following to speed up my code...
  __builtin_ctzll(n)
On my computer, an "unsigned long long" is only 64 bits (not 128 bits), so I limit the length
  of the lookup tables for powers of 3, and I do the following for alpha and beta...
            if ((uint64_t)n == 0) alpha = 64;
            else alpha = __builtin_ctzll(n);
If that didn't make sense yet, don't worry about it. I just put it here to be with similar info.
I also use the "long long" function strtoull when reading in the arguments.

Currently loads in a sieve file, which must must match the k1 value set in this code.



Here is a great read for some basic optimizations...
  https://en.wikipedia.org/wiki/Collatz_conjecture#Optimizations
Read it, or else the following won't make sense!
I don't actually create lookup tables for c and d,
  but understanding the above link is crucial to understanding the rules
  that make up my 2^k sieve and 3^2 sieve.



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




Here is a brilliant algorithm that I use (by David Barina)...
  https://github.com/xbarin02/collatz/blob/master/doc/ALGORITHM.md
  https://rdcu.be/b5nn1



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



Sieves of size 2^k are used, where k can be very large!
Essentially no RAM is used.

k < 81 must be true.



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



55247846101001863167 will overflow 128 bits.
This number is over 64-bits, so it cannot be made into an integer literal.
To test 128-bit overflow,
calculate the following using Python 3, then put it as your task_id0...
  task_id0 = 55247846101001863167 // (9 << TASK_SIZE0)
Then set the following as your task_id...
  remainder = (55247846101001863167 % (9 << TASK_SIZE0))
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
  TASK_SIZE0 - k should ideally be at least 10
  TASK_SIZE should at least be 10 to give each process something to run!
     (minimum TASK_SIZE is 8)
  Due to the limits of strtoull(), k - TASK_SIZE < 64
  9 * 2^(TASK_SIZE0 + TASK_SIZE - k) numbers will be run by each process
*/
const int k = 51;
const int TASK_SIZE0 = 67;       // 9 * 2^TASK_SIZE0 numbers will be run total
const int TASK_SIZE = 18;        // TASK_SIZE <= k







// For the 2^k1 sieve that speeds up the calculation of the 2^k sieve...
//   TASK_SIZE <= k1 <= k
// The only con of a larger k1 is that the sieve file is harder to create and store,
//   but, once you have the sieve file, use it!
// Especially good values are 32, 35, 37, 40, ...

const int k1 = 37;

const char file[10] = "sieve37";






// 2^8 sieve
const uint8_t sieveSmall[16] = {27, 31, 47, 71, 91, 103, 111, 127, 155, 159, 167, 191, 231, 239, 251, 255};




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
  printf("k = %i\n", k);
  printf("k1 = %i\n", k1);
  fflush(stdout);




  const __uint128_t k2 = (__uint128_t)1 << k;       // 2^k
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
  const int lenC3 = 81;  // for 128-bit numbers

/*
  // Note that __builtin_ctzll(0) is undefined. It doesn't always return 64 like I would wish.
  printf("  __builtin_ctzll(0) = %i\n", __builtin_ctzll(0));   // is 64 ?
  printf("  __builtin_ctzll((__uint128_t)1 << 64) = %i\n", __builtin_ctzll((__uint128_t)1 << 64));  // is 64 ?
  printf("  __builtin_ctzll((__uint128_t)1 << 63) = %i\n", __builtin_ctzll((__uint128_t)1 << 63));  // is 63 ?
  printf("  casting 2^64 to 64-bit = %llu\n", (uint64_t)((__uint128_t)1 << 64) );  // better be 0 !
*/




  __uint128_t n, nStart, a;
  int alpha, aMod, nMod, bMod, j;


  //const int k2Mod = k2 % modNum;
  //const int k2Mod = (k & 1) ? 2 : 1 ;   // trick for modNum == 3
  uint64_t r = 0;    // trick for modNum == 9
  r += (uint64_t)(k2)        & 0xfffffffffffffff;
  r += (uint64_t)(k2 >>  60) & 0xfffffffffffffff;
  r += (uint64_t)(k2 >> 120);
  const int k2Mod = r%9;



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

  __uint128_t bStart = ( (__uint128_t)1 << TASK_SIZE )*task_id;

  __uint128_t countB = 0;    // to count the numbers that need testing in segment of 2^k sieve






  /* open the 2^k1 sieve file */

  FILE* fp;
  size_t file_size;

  // I will load only a single chunk of 2^k1 sieve into RAM at a time
  fp = fopen(file, "rb");
  const uint64_t bufferBytes = (uint64_t)1 << 13;    // 2^13 bytes = 8 kiB
  uint16_t* data = (uint16_t*)malloc(bufferBytes);

  // Check file size
  // Bytes in sieve file are 2^(k1 - 7)
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  if ( file_size != ((size_t)1 << (k1 - 7)) ) {
    printf("  error: wrong sieve file!\n");
    return 0;
  }

  /*
    Seek to necessary part of the file
    Note that ((((uint64_t)1 << k1) - 1) & bStart) equals bStart % ((uint64_t)1 << k1)
  */
  //rewind(fp);
  fseek(fp, ((((uint64_t)1 << k1) - 1) & bStart) >> 7, SEEK_SET);

  // for handling the buffer...
  uint64_t bufferStepMax = bufferBytes >> 1;   // since each pattern is 2 bytes
  uint64_t bufferStep = bufferStepMax;
  uint16_t bytes;    // the current 2 bytes




  ////////////////////////////////////////////////////////////////
  //////// test integers that aren't excluded by certain rules
  ////////////////////////////////////////////////////////////////

  for (uint64_t pattern = 0; pattern < ((uint64_t)1 << (TASK_SIZE - 8)); pattern++) {

    // get bytes
    if (bufferStep >= bufferStepMax) {    // do we need to refresh buffer?
      fread(data, sizeof(uint16_t), bufferBytes / sizeof(uint16_t), fp);
      bufferStep = 0;
    }
    bytes = data[bufferStep];
    bufferStep++;

    for (int bit = 0; bit < 16; bit ++) {      // loop over 16 bits in pattern

      // allow the 2^k1 sieve to help!
      if ( !( bytes & (1 << bit) ) ) continue;
      //countB++;



      __uint128_t b = bStart + pattern * 256 + sieveSmall[bit];

      int go = 1;          // acts as a boolean
      __uint128_t bb = b;  // will become fk(b)
      int c = 0;           // number of increases experienced when calculated fk(b)

      // check to see if 2^k*a + b is reduced in no more than k steps
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
      if ( go == 0 ) continue;




      // Try another method to rule out the b...
      //   Starting at b-1, compare the final b and c after k steps
      // This will take more time, and will only rule out a few more b values

      __uint128_t bm = b - 1;
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
        continue;
        //print128(b);
        break;
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

          nMod = (aMod * k2Mod + bMod) % modNum;

          // iterate before the following possible continue
          aMod++;
          if (aMod == modNum) aMod = 0;

          //if (nMod == 2) continue;    // modNum = 3
          if (nMod == 2 || nMod == 4 || nMod == 5 || nMod == 8) {   // modNum = 9
            continue;
          }




          nStart = (a<<k) + b;

          n = a*c3[c] + bb;



          if (!(n&1)) goto even;

          while (1) {             // go until overflow or n < nStart

            n++;
            if ((uint64_t)n == 0) alpha = 64;
            else alpha = __builtin_ctzll(n);
            n >>= alpha;
            //if ( alpha >= lenC3 || n > maxNs[alpha] ) {
            if ( n > maxNs[alpha] ) {
              printf("Overflow! nStart = ");
              print128(nStart);
              break;
            }
            n *= c3[alpha];   // 3^c from lookup table
            n--;

even:
            if ((uint64_t)n == 0) n >>= 64;
            else n >>= __builtin_ctzll(n);
            if (n < nStart) break;

          }

      }

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
  free(data);
  fclose(fp);
  return 0;
}
