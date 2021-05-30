/* ******************************************

Find deltaN distribution, count numbers to be tested,
  and find unique patterns for 2^k sieve.
collatzCPU.c can experimentally search for deltaN too (and count numbers to be tested)!
Patterns are compressed 4-to-1 by only looking at n%4==3.
Practically no RAM is used.

Compile and run via something like...
  clang -O3 collatzSieve2toK_FindPatterns.c
  time ./a.out >> log.txt

Something like the following can eventually be useful
after putting all k < 40 into log.txt...
  sort log.txt | uniq > logSorted.txt
  sort log40.txt > log40Sorted.txt
  comm -23 logSorted.txt log40Sorted.txt
Ideally nothing will be output!
But, if there is output, just combine it with log40!
Will the following return anything?
  comm -23 log40Sorted.txt logSorted.txt

If you want to run k > 40 (but less than 81), change all the
  uint64_t
to
  __uint128_t
The idea is that when 2^40 - 1 increases 40 times, you get approximately 3^40,
  which is less than 2^64. But 3^41 is larger than 2^64 and could cause overflow.
You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work.



I learn that...

Patterns of length 2^3 bits can be referenced by 2 bits (since 3 and 7 give 4 unique patterns).
Patterns of length 2^4 bits can be referenced by 3 bits (since 7, 11, and 15 give 6 unique patterns).
  Note that 7 and 11 cannot occur at the same time, so the two combined give 3 possibilities.
Patterns of length 2^5 bits can be referenced by 4 bits (since 7, 15, 27, and 31 give 16 unique patterns).
Patterns of length 2^6 bits can be referenced by 6 bits (since I never get more than 50 unique patterns; 53 cumulative starting at k=6).
Patterns of length 2^7 bits can be referenced by 9 bits (or maybe more depending on k).
Patterns of length 2^8 bits can be referenced by 14 bits (or maybe more depending on k).
Patterns of length 2^9 bits can be referenced by 18 bits (or maybe more depending on k).





Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0

This code will analyze a sieve of size 2^k.
If n = 2^k*N + i always reduces after no more than k steps for all N,
  there is no need to ever test these numbers.
This code tests all 0 <= i < 2^k to see if this reduction occurs.
Define fm(n) as m applications of f. If m=4, fm(n) = f(f(f(f(n)))).
Define a and b as fm(n) = a*N + b.
For m<=k, it is then true that b = fm(i) and a = 2^(k-m)*3^c,
  where c is how many increases were encountered.
For m<=k, if b <= i, a < 2^k must also be true (if N>0).
This code checks for the situation that b==i and i>1
  because, if N=0, this would be a non-trivial cycle!



In addition to collatzSieve2toK_basicReduction.c, this file does an extra search.
For n0 belonging to the numbers less than 2^k that remain to be excluded,
  see if n0 joins the path of any numbers n0 - deltaN <= n < n0
  in the same j<=k steps having undergone the same c increases.
It can be shown that...
    deltaN <= 2^(k - minC)
The above is derived using minC > k / log2(3) being the lowest c that does not
  give already-ruled-out n0. You want to think about the lowest c since
  it gives you the greatest deltaN. After k steps with c increases...
    n0 -> 3^c / 2^k * n0 + something
  where something does not depend on n0 (only the order of increases and decreases).
  Then define...
    deltaSomething = (something2 from k-c decreases then c increases)
                    - (something1 from c increases then k-c decreases)
  where it is easiest to calculate these somethings using n0=0.
  Anyway...
    deltaN <= deltaSomething / (3^c / 2^k) <= (3/2)^c / (3^c / 2^k)
    deltaN <= 2^(k-c) <= 2^(k-minc)
  Here is some Python code for calculating a something2...
----
n = 0.0
for i in range(c):
  n = (3*n+1) / 2
something2 = n     # something2 < (3/2)^c
----
  To get the something1, divide something2 by 2^(k-c).
Using the same derivation idea above, you may find a tighter limit...
    deltaN <= something2 * (2^k - 2^minC) / 3^minC
I bet a clever person could restrict deltaN more since deltaN must be an integer.
  In fact, I succeeded! Here are some of the results that take no more
  than a minute to find...
    k = 36: deltaN <= 383
    k = 37: deltaN <= 383
    k = 38: deltaN <= 671
    k = 39: deltaN <= 679
    k = 40: deltaN <= 679
  I still needed to run these k values to get the actual experimental deltaN,
    but the above numbers GREATLY speed this up!
I bet I could speed up the code by only searching the deltaN for the current step.
  That is, don't use the same deltaN for each of the j <= k steps.
  But whatever.

For any N >= 0, the following will be excluded if 2^k includes what multiplies N...
  64 N + 15         (deltaN = 1)
  128 N + 95        (deltaN = 1)
  256 N + 63        (deltaN = 1)
  512 N + 383       (deltaN = 1)
  1024 N + 255      (deltaN = 1)
  2048 N + 1535     (deltaN = 1)
  4096 N + 1023     (deltaN = 1)
  8192 N + 6143     (deltaN = 1)
  16384 N + 4095    (deltaN = 1)
  32768 N + 24575   (deltaN = 1)
  65536 N + 16383   (deltaN = 1)
  131072 N + 98303  (deltaN = 1)
  262144 N + 65535  (deltaN = 1)
  262144 N + 121471 (deltaN = 1)
  262144 N + 183295 (deltaN = 1)
  262144 N + 190207 (deltaN = 1)
  262144 N + 199423 (deltaN = 1)
  262144 N + 236031 (deltaN = 1)
  262144 N + 242175 (deltaN = 1)
  524288 N + 2815   (deltaN = 6)
  524288 N + 45567  (deltaN = 4)
  524288 N + 201151 (deltaN = 4)
  524288 N + 255743 (deltaN = 1)
  524288 N + 257727 (deltaN = 4)
  524288 N + 264959 (deltaN = 1)
  524288 N + 296959 (deltaN = 1)
  524288 N + 301567 (deltaN = 1)
  524288 N + 307711 (deltaN = 1)
  524288 N + 390271 (deltaN = 4)
  524288 N + 393215 (deltaN = 1)
  524288 N + 449151 (deltaN = 1)
  524288 N + 506879 (deltaN = 1)
  524288 N + 510975 (deltaN = 1)
     ...

Here is a nice Python 3 script to get jth step of n...
----
n = 45563
for i in range(19):
  if (n%2==0):
    n = n//2
  else:
    n = 3*(n//2) + 2

print(n)
----



For this paragraph, I will use a double slash, //, for integer division.
  (3*n+1) / 2 = 3*(n//2) + 2
since
  n//2 = (n-1)/2
since n is odd.
It is generally better to use 3*(n//2) + 2 because you calculate n//2 THEN
multiply by 3 allowing n to be twice as large without causing an overflow.
This is a minor improvement worth doing!




(c) 2020 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/time.h>
struct timeval tv1, tv2;


//  k = 36 could take an hour to run
const int k = 32;



/*
  The following is only used if code sets deltaN to be larger than this
  If you are only interested in deltaN=1 sieves, set this to 1
*/
const uint64_t deltaN_max = 100000000;



// Find patterns of size 2^K, where K <= k.
// 2 <= K <= 9 is hardcoded into the analysis.
const int K = 8;





// Prints __uint128_t numbers since printf("%llu\n", x) doesn't work
//   since "long long" is only 64-bit in gcc.
// This function works for any non-negative integer less than 128 bits.
void print128(__uint128_t n) {
  char a[40] = { '\0' };
  char *p = a + 39;
  if (n==0) { *--p = (char)('0'); }
  else { for (; n != 0; n /= 10) *--p = (char)('0' + n % 10); }
  printf("%s", p);
}



// prints all m bits, where m <= 128
void printBinary128(__uint128_t n, int m) {
  __uint128_t mask = (__uint128_t)1 << (m-1);
  for (int i=0; i<m; i++) {
    if (n & mask) { printf("1"); }
    else { printf("0"); }
    mask >>= 1;
  }
  printf("\n");
  fflush(stdout);
}




__uint128_t* patterns;

// Binary search
// But, if not found, return location of where it should go.
// I found it useful to, by hand, run through {0,1,2,3,4,5,6,7,8,9}
//   to look for 2.5 and 3.5. Also try -0.5 and 9.5.
// For an empty patterns[] array, l=0 and r=-1, so it works!
int32_t binarySearch(int32_t l, int32_t r, __uint128_t bytes) {

  if (r >= l) {
    int mid = l + (r - l) / 2;
    if (patterns[mid] == bytes) return mid;
    if (patterns[mid] > bytes) return binarySearch(l, mid - 1, bytes);
    return binarySearch(mid + 1, r, bytes);
  }

  return l; 
}




int main(void) {

  int j;
  uint64_t b, b0, count, m, deltaN, lenList, n0, maxM;
  int temp;  // acts as a boolean for various things


  // for doing stuff for finding unique patterns
  __uint128_t aa;
  int32_t bb, nn, mm, oo;



  const uint64_t k2  = (uint64_t)1 << k;   // 2^k
  const int32_t  bits  = (uint64_t)1 << (K-2);   // bits needed for each pattern



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
  else if (k<=41) deltaN = 1215;  // needs experimental reduction
  else {
    int minC = 0.6309297535714574371 * k + 1.0;  // add 1 to get ceiling
    double minC3 = 1.0;     // 3^minC
    for (j=0; j<minC; j++) minC3 *= 3.0;
    double deltaNtemp = 0.0;
    for (j=0; j<minC; j++) deltaNtemp = (3.0 * deltaNtemp + 1.0) / 2.0;
    deltaNtemp = deltaNtemp * (((uint64_t)1<<k) - ((uint64_t)1<<minC)) / minC3;
    deltaN = deltaNtemp + 1.0;    // add 1 to get ceiling
  }

  if (deltaN > deltaN_max) deltaN = deltaN_max;

  printf("  k = %i\n", k);
  printf("  deltaN = ");
  print128(deltaN);
  printf("\n");
  fflush(stdout);

  // in the following lists, the 0th element is for n0
  uint64_t* nList = (uint64_t*)malloc((deltaN+1)*sizeof(uint64_t));
  int* cList = (int*)malloc((deltaN+1)*sizeof(int));



  /* for finding the deltaN distribution */
  uint64_t *deltaNcounts = malloc(sizeof(uint64_t) * (deltaN+1));
  for (size_t i=0; i<(deltaN+1); i++) { deltaNcounts[i] = 0; }




  // Array of unique patterns.
  // I can prove that length = 2^11 is for sure enough for K=7.
  // I can prove that length = 2^16 is for sure enough for K=8.
  // I can prove that length = 2^31 is for sure enough for K=9.
  int32_t length;
  if (K == 9) length = 1048576;   // 16 MiB of RAM for 128-bit int (enough ???)
  else length = 32768;            // 0.5 MiB of RAM for 128-bit int
  patterns = (__uint128_t*)malloc(length*sizeof(__uint128_t));
  for (int32_t i=0; i<length; i++) { patterns[i] = 0; }



  // start timing
  gettimeofday(&tv1, NULL);



  //// Look for unique patterns, find max deltaN, and
  ////   count numbers that need testing.
  //// Only look at n%4 == 3 bits, so 1/4 of the bits.

  count = 0;
  maxM = 0;  // see if deltaN is ever really reached
  bb = 0;    // counter for indexing bits of pattern
  nn = 0;    // counter for patterns array
  aa = 0;    // __uint128_t that stores the pattern
  for (b0=3; b0<k2; b0 += 4) {

    temp = 1;

    // check to see if if 2^k*N + b0 is reduced in no more than k steps
    b = b0;
    for (j=1; j<=k; j++) {  // step
      if (b & 1) {          // bitwise test for odd
        b = 3*(b/2) + 2;    // note that b is odd
      } else {
        b >>= 1;
        if (b <= b0) {
          if (b==b0 && b0>1) {printf("  wow!\n");}
          temp = 0;
          break;
        }
      }
    }

    // if temp=1, use another method to try to get temp=0
    if (temp) {
      n0 = b0;   // it helps my brain to rename it
      lenList = ((deltaN+1) < (n0-1)) ? (deltaN+1) : (n0-1) ;   // get min(deltaN+1, n0-1)
      for(m=0; m<lenList; m++) nList[m] = n0-m;    // initialize nList
      for(m=0; m<lenList; m++) cList[m] = 0;       // initialize cList

      for(m=0; m<lenList; m++) {      // loop over lists

        for(j=1; j<=k; j++) {         // steps
          if (nList[m] & 1) {                 // bitwise test for odd
            nList[m] = 3*(nList[m]/2) + 2;    // note that nList[m] is odd
            cList[m]++;
          } else {
            nList[m] >>= 1;
          }
        }

        // check against 0th element
        if ( m>0 && nList[m]==nList[0] && cList[m]==cList[0] ) {
/*
            if (j<=19 && n0%64!=15 && n0%128!=95 && n0%256!=63 && n0%512!=383 && n0%1024!=255 && n0%2048!=1535 && n0%4096!=1023 && n0%8192!=6143 && n0%16384!=4095 && n0%32768!=24575 && n0%65536!=16383 && n0%131072!=98303 && n0%262144!=65535 && n0%262144!=183295 && n0%262144!=190207 && n0%262144!=199423 && n0%262144!=236031 && n0%262144!=242175 && n0%262144!=121471 && n0%524288!=2815 && n0%524288!=45567 && n0%524288!=201151 && n0%524288!=255743 && n0%524288!=257727 && n0%524288!=264959 && n0%524288!=296959 && n0%524288!=301567 && n0%524288!=307711 && n0%524288!=390271 && n0%524288!=393215 && n0%524288!=449151 && n0%524288!=506879 && n0%524288!=510975){
              printf("  oh no!\n");
            }
*/

            if(m>maxM){
              maxM = m;
            }

            deltaNcounts[m]++;

            temp = 0;
            break;
        }

      }
    }

    if (temp) {
      aa += (__uint128_t)1 << bb;
      count++;
    }

    bb++;
    if (bb == bits) {

/*
      // add aa to patterns if not already in patterns (linear search; unsorted)
      temp = 1;
      for (mm=0; mm<nn; mm++) {
        if (aa == patterns[mm]) {
          temp = 0;
          break;
        }
      }
      if (temp) {
        patterns[nn] = aa;
        nn++;
      }
*/
      // add aa to patterns if not already in patterns (binary search; sorted)
      mm = binarySearch(0, nn-1, aa);
      if (patterns[mm] != aa) {       // add into a sorted patterns[]
        for (oo = nn; oo > mm; oo--)  // shift things over by 1 spot
          patterns[oo] = patterns[oo - 1];
        patterns[mm] = aa;
        nn++;
      }

      bb = 0;
      aa = 0;
    }

  }




  gettimeofday(&tv2, NULL);
  printf("  Elapsed wall time is %e seconds\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec) ); 




  const uint64_t K2    = (uint64_t)1 << K;       // bit-shift trick to get 2^K
  const uint64_t len2  = (uint64_t)1 << (k-K);   // number of chunks


  print128(count);
  printf(" out of ");
  print128(k2);
  printf(" need testing, so %f\n", (double)count / (double)k2);
  if (maxM > 0) {
    printf(" max deltaN = ");
    print128(maxM);
    printf("\n");
  }
  printf("\n");



  printf("There are ");
  print128(nn);
  printf(" unique ");
  print128(K2);
  printf("-bit patterns in ");
  print128(len2);
  printf(" ");
  print128(K2);
  printf("-bit chunks of ");
  print128(k2);
  printf(" values...\n");

  for (mm=0; mm<nn; mm++) { printBinary128(patterns[mm], bits); }
  printf("\n");




  uint64_t totalDeltaNcount = 0;
  for (size_t i=1; i<=deltaN; i++) totalDeltaNcount += deltaNcounts[i];

  printf(" need testing without any deltaN: %" PRIu64 "\n", count + totalDeltaNcount);
  printf(" need testing with only deltaN = 1: %" PRIu64 "\n", count + totalDeltaNcount - deltaNcounts[1]);

  printf(" total deltaN count = %" PRIu64 "\n", totalDeltaNcount);
  for (size_t i=deltaN; i>0; i--) {
    if (deltaNcounts[i] > 0)
      printf(" %zu: %" PRIu64 "\n", i, deltaNcounts[i]);
  }




/* **********************************

  //// of possible patterns, which don't occur? (only coded for K=6 so far)

  // The numbers in the following are referencing groups of 4 bits
  // To get nStart from the following, do 4 * # + 3
  int possible2[1] = {0};     // K = 2 has chunks of 4 bits
  int possible3[2] = {0, 1};  // K = 3 has chunks of 8 bits
  int possible4[3] = {1, 2, 3};  // K = 4 has chunks of 16 bits
  int possible5[4] = {1, 3, 6, 7};  // K=5 has chunks of 32 bits
  int possible6[7] = {1, 6, 7, 9, 11, 14, 15}; // K=6 has chunks of 64
  int possible7[11] = {6, 7, 9, 11, 15, 17, 22, 25, 27, 30, 31}; // K=7 has chunks of 128

  if (K==6) {
    int lenPossibility = 7;
    printf("The following patterns do not occur...\n");
    for (j=0; j < (1 << lenPossibility); j++){   // loop over the 2^7 possibilities

      // create one of the possible patterns
      a = 0;
      for (i2=0; i2<lenPossibility; i2++) {   // loop over the 7 positions
        if( j%(1<<(i2+1)) < (1<<i2) ) a += (__uint128_t)1 << possible6[i2];
      }

      // print if not already in patterns[]
      temp = 1;
      for (m=0; m<n; m++) {
        if (a == patterns[m]) {
          temp = 0;
          break;
        }
      }
      if (temp) {
        printBinary128(a, bits);
      }

    }
  }

********************************** */



  free(patterns);
  return 0;
}
