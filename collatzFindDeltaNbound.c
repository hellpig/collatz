/* ******************************************

For n0 belonging to the numbers less than 2^k that remain to be excluded,
  one may want to see if n0 joins the path of any numbers n0 - deltaN <= n < n0
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
  In fact, that is what this code succeeds at doing!



This file finds the cases where the following is a positive integer
  deltaN = deltaS / 3^c
where
  deltaS = s2 - s1
where s2 and s1 correspond to different orderings of increases and decreases of s
given a k and c. I define s (s1 and s2) below.



Each s starts at 0.

For an increase...
  s -> 3*s + 2^step
where step starts at 0 and ends at k-1

For a decrease...
  s -> s

This file will always use 0 for an increase, and a 1 for a decrease.
So 001 is 2 increases followed by a decrease.

Here is some Python 3 code to find final s.
-----
possibility = [0,0,1,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,0,1,1,1,0,0,1,0,1,1,1,1,1,0,0,0,0,0,1,0]
k = len(possibility)
s = 0
for i in range(k):
  if possibility[i] == 0:
    s = 3*s + 2**i

print(s)
-----




If c = k, deltaS = 0.

The final steps of possibility1 and possibility2 can be ignored if
  all consecutive steps to the end are all identical between the two possibilities.
They don't change deltaN.
That is, for k=11 and c=7 (where a "1" means a decrease)...
  00001101010
  10001001010
is actually k=6 and c=4...
  000011
  100010
You can then conclude that the final non-identical step must have opposite directions.

Note that the first step determines if s is even or odd and that s1 is always odd.
So, deltaN is even only if s2 starts with an increase.
s2 starting with an increase can be a meaningful possibility because what n1 and n2
  increase into could be excluded (they may reduce in j<=k steps).

Note that the location of the final increase matters, let's call them step1 and step2.
2^step1 - 2^step2 is only divisible by 3 if step2-step1 is even.
2^step1 - 2^step2 must be divisible by 3 for deltaN to be an integer!
You can then conclude that, if only showing up to the final non-identical step,
  the possibilities must have the form
    ...011
    .....0
  or
    .01111
    .....0
  etc. (add two more 1's each time)
This rules out c = k-1 .

Note that "crossing" can happen. That is, the s1 is not guaranteed to be
  ...11
If you try to get the s2 to be ...11, the last 3 steps of the following
  could reduce a "crossed by 1" situation back to joining...
    s1: ...110
    s2: ...011
The condition for the initial k -> k-3 steps with c -> c-1 increases
  to "cross by 1" is
    deltaN = (deltaS - 2^k) / 3^c
  where deltaN is the initial deltaN.
For k = 11 -> 8 and c = 7 -> 6...
  s1 = 697      (from possibility = [0,0,0,0,0,1,0,1])
  s2 = 2660     (from possibility = [1,1,0,0,0,0,0,0])
  2^8 = 256
  3^6 = 729
so such a thing seems possible especially since...
  (2^5 - 2^4 - 2^6) mod 3 = 0
  (2^6 - 2^5 - 2^7) mod 3 = 0

Note that, for k=5 and c=1, the following gives deltaN = 5...
  01111
  11110
In general, for odd k and c=1, you'll get
  deltaN = something2 * (2^k - 2^c) / 3^c
  where something2 = 1/2
So, me enforcing minC makes a huge difference on deltaN !



The goal of this code is to place the increases in a way that
  keeps deltaN to be an integer.
The last increase is the first one to be placed.

Placing first increase (to keep deltaS%3 == 0)...
    deltaS % 3 = 0
  so...
    0 + (2^a - 2^b)%3 = 0
    where a and b are the positions of the first increase of s2 and s1 respectively.
  Choose a = k-1 to get...
    s1: ...011
    s2: .....0
  or
    s1: ...01111
    s2: .......0
  etc.
  Note that what is tentatively called s2 here may actually end up being s1 in
    deltaS = s2 - s1.
  Offset for next step is...
    next offset = (2^a - 2^b)/3

Placing 2nd increase  (to keep deltaS%9 == 0)...
    deltaS % 9 = 0
  so...
    (offset + 2^a - 2^b)%3 = 0
    where a and b are the positions of the 2nd increase
  For the following offset%3, C returns a negative answer if offset is negative,
    so add 3 to any negative answer.
  If offset%3 = 0
    a = b + 2*i   where i is any integer
  If offset%3 = 1
    a is even, and b is odd
  If offset%3 = 2
    a is odd, and b is even
  Offset for next step is...
    next offset = (offset + 2^a - 2^b)/3

Placing 3rd increase (to keep deltaS%27 == 0)...
    deltaS % 27 = 0
  so...
    (offset + 2^a - 2^b)%3 = 0
    where a and b are the positions of the 3rd increase
  If offset%3 = 0
    a = b + 2*i   where i is any integer
  If offset%3 = 1
    a is even, and b is odd
  If offset%3 = 2
    a is odd, and b is even
  Offset for next step is...
    next offset = (offset + 2^a - 2^b)/3

Keep going until all c increases are placed and deltaS % 3^c == 0.



This code is plenty fast, but I bet one could speed it up by only creating
  certain pairs of possibilities. You would need to create the pairs that
  would have the largest deltaN.



I get higher deltaN values here than with a direct experimental search.
I bet this is because not all of the possibilities are realized.
Note that the s1 and s2 must be realized by nearby n's.
But you can use the max deltaN from this file to feed into the experimental search
  so that the experimental search doesn't take forever!

(c) 2020 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>
struct timeval tv1, tv2;



// Increase k by 1 starting at k=6
// If a k value returns a max deltaN less than a smaller k,
//   make it equal to that of the smaller k.
//   I do this to speed up running the code,
//   so that I don't have to recheck all old possibilities.
// Note that s could overflow if k>81 !
//   I think __uint128_t only works on 64-bit gcc, so int should be 32-bit,
//   so maxDeltaN is fine being an int.
// k=41 takes no more than 20 minutes
#define k 51



// To prevent an n corresponding to s1 from ever reducing below start value,
//   here is the max number of decreases up to that point,
//   where each element of array is a step
// This should be good for up to k=65, but you can add more if needed!
// You can calculate these using: step - ceiling( step / log2(3) )
const int maxDecreases[] = {0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20, 21, 21, 21, 22, 22, 22, 23, 23, 23};



// Prints __uint128_t numbers since printf("%llu\n", x) doesn't work
//   since "long long" is only 64-bit in gcc.
// This function works for any non-negative integer less than 128 bits.
void print128(__uint128_t n) {
  char a[40] = { '\0' };
  char *p = a + 39;
  if (n==0) { *--p = (char)('0'); }
  else { for (; n != 0; n /= 10) *--p = (char)('0' + n % 10); }
  printf("%s\n", p);
}




// given possibility array, find s
__uint128_t getS(unsigned char possibility[]) {
  __uint128_t s = 0;
  for(int i=0; i<k; i++)
    if (possibility[i] == 0) s = 3*s + ((__uint128_t)1<<i);  // an increase step
  return s;
}



// Given possibility array, could it be s1 ?
//   Returns 0 or 1
int isS1(unsigned char possibility[]) {

  // does it decrease too fast?
  int decreaseCount = 0;
  for(int i=0; i<k; i++) {
    if (possibility[i]) decreaseCount++;
    if (decreaseCount > maxDecreases[i]) {   // check against maxDecreases[]
      return 0;      // cannot be s1
    }
  }
  return 1;

}



// Recursive function to build possibility arrays and send these arrays to getS() and isS1() to find maxDeltaN.
// Each time depth increases, you are now doing the previous increase step.
// Note that, internal to this function, s1 and s2 are not necessarily the deltaS = s2 - s1 since
//   it could be that deltaS = s1 - s2.
void sFind(__uint128_t *count, unsigned char possibility1[], unsigned char possibility2[], int shift1, int shift2, __int128_t offset, int depth, int *maxDepth, __uint128_t* c3, int* maxDeltaN){

  int increasesToPlaceAfterThis = *maxDepth - depth;

  for(int i = shift1; i >= increasesToPlaceAfterThis; i--) {    // for s1
    int iMod = i&1;  // i%2

    for(int j = shift2; j >= increasesToPlaceAfterThis; j--) {  // for s2
      int jMod = j&1;  // j%2

      // offset mod 3
      int offMod = offset%3;
      if (offMod < 0) offMod += 3;

/*
      // offset mod 3, trick for int64_t that actually slows it down
      int offMod;
      if (offset >= 0)
        offMod = ( ((uint32_t)offset % 3) + ((uint32_t)(offset >> 32) % 3) ) %3;
      else {
        offMod = ( ((uint32_t)(-offset) % 3) + ((uint32_t)(-offset >> 32) % 3) ) %3;
        if (offMod != 0) offMod = -(offMod - 3);
      }
*/

      if (offMod == 0) {
        if ( depth == 0 && j != (k-1) ) continue;  // tentatively define s2 as the one that is ....0
        if ( (iMod - jMod) != 0 ) continue;        // i = j + 2*N   where N is any integer
      } else if (offMod == 1) {
        if ( !iMod ) continue;    // s1 must be odd
        if ( jMod ) continue;     // s2 must be even
      } else {    // offMod == 2
        if ( iMod ) continue;     // s1 must be even
        if ( !jMod ) continue;    // s2 must be odd
      }

      // place the increases
      possibility1[i] = 0;
      possibility2[j] = 0;

      if (depth == *maxDepth) {

        // update maxDeltaN and count
        __uint128_t sVal1 = getS(possibility1);
        __uint128_t sVal2 = getS(possibility2);
        __uint128_t deltaS;
        int deltaN;
        if (sVal2 > sVal1) {
          deltaS = sVal2 - sVal1;
          if (isS1(possibility1)  ) {
            (*count)++;
            deltaN = deltaS / *c3;
            //printf(" deltaN = %llu\n", deltaN);
            if ( deltaN > *maxDeltaN ) *maxDeltaN = deltaN;
          }
        } else {       // in this reversed case, sVal2 belongs to s1 and sVal1 belongs to s2
          deltaS = sVal1 - sVal2;
          if (isS1(possibility2) ) {
            (*count)++;
            deltaN = deltaS / *c3;
            //printf("  deltaN = %llu\n", deltaN);
            if ( deltaN > *maxDeltaN ) *maxDeltaN = deltaN;
          }
        }

/*
        for (int m=0; m<k; m++) printf("%u,", possibility1[m]);
        printf("\n");
        for (int m=0; m<k; m++) printf("%u,", possibility2[m]);
        printf("\n");
        printf("  %llu\n", sVal1);
        printf("  %llu\n", sVal2);
*/

      } else {
        __int128_t newOffset = ( offset + ((__int128_t)1<<j) - ((__int128_t)1<<i) )/3;
        sFind(count, possibility1, possibility2, i-1, j-1, newOffset, depth+1, maxDepth, c3, maxDeltaN);    // recursion!
      }

      // set back to decreases
      possibility1[i] = 1;
      possibility2[j] = 1;

    }
  }

}




int main(void) {

/*
  // find max s (k=40 works for 64-bit int, and k=81 works for 128-bit int)
  unsigned char possibility[k] = {0};
  possibility[0] = 1;
  print128(getS(possibility));
  return 0;
*/

/*
  // test the deltaN = 6 situation for k=19
  unsigned char possibility1[k] = {0,0,0,0,0,0,0,0,1,0,1,1,1,1,0,0,0,1,1};
  print128(getS(possibility1));
  unsigned char possibility2[k] = {0,1,0,0,1,0,0,1,1,1,0,0,1,0,0,0,0,1,0};
  print128(getS(possibility2));
  return 0;
*/

  // start timing
  gettimeofday(&tv1, NULL);

  printf("k = %i\n", k);

  int minC = 0.6309297535714574371 * k + 1.0;  // add 1 for ceiling

  for (int c = minC; c < k-1; c++) {

    printf("c = %i\n", c);

    // get 3^c (c=40 works for 64-bit; c=80 works for 128-bit)
    __uint128_t c3 = 1;
    for (int i=0; i<c; i++) c3 *= 3;

    // call sFind()
    // count and maxDeltaN are outputs
    __uint128_t count = 0;       // number of exclusion rules found
    int maxDeltaN = 0;
    int maxDepth = c-1;       // value will not be changed
    unsigned char possibility1[k];  // 0's are increases; 1's are decreases
    unsigned char possibility2[k];  // 0's are increases; 1's are decreases
    for (int i=0; i<k; i++){
      possibility1[i] = 1;
      possibility2[i] = 1;
    }
    sFind(&count, possibility1, possibility2, k-3, k-1, 0, 0, &maxDepth, &c3, &maxDeltaN);

    // print
    if (maxDeltaN != 0) {
      //printf("  count = %llu\n", count);
      printf("  max deltaN = %i\n", maxDeltaN);
    }

  }

  gettimeofday(&tv2, NULL);
  printf("  Elapsed wall time is %e seconds\n",
    (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec) ); 

  return 0;
}
