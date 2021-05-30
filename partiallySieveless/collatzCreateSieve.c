/* ******************************************

Uses a 16-to-1 compression by storing a uint16_t to reference 256 numbers.
This only works for k >= 8
Each bit of the uint16_t will be referring to the following 16 bits of the 256...
 27
 31
 47
 71
 91
 103
 111
 127
 155
 159
 167
 191
 231
 239
 251
 255

Use...
  clang -O3 collatzCreateSieve.c
  time ./a.out

Currently saves to file called "sieve"
I change the name of the file AFTER I create it.

I currently don't create a buffer before writing out.
Should I?
I just write two bytes at a time.

k < 41 because I use uint64_t instead of __uint128_t

I find the following command useful to get the first 4 2-byte patterns..
  xxd -b -l 8 sieve

(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>



// 7 < k < 41
// k = 32 took 3 minutes to run
const int k = 27;



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



// 2^8 sieve
const uint8_t sieveSmall[16] = {27, 31, 47, 71, 91, 103, 111, 127, 155, 159, 167, 191, 231, 239, 251, 255};



int main(void) {


  FILE *file0;
  file0 = fopen("sieve", "wb");      // you might want to change filename


  int j;
  uint64_t b, b0, m, deltaN, lenList, n0;
  int temp;           // acts as a boolean
  uint16_t bytes = 0;   // 2 bytes that store the pattern


  // the number of 2-byte patterns
  const uint64_t patterns  = (uint64_t)1 << (k - 8);   // 2^(k - 8)



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

  printf("  k = %i\n", k);
  printf("  deltaN = ");
  print128(deltaN);
  printf("\n");

  // in the following lists, the 0th element is for n0
  uint64_t* nList = (uint64_t*)malloc((deltaN+1)*sizeof(uint64_t));
  int* cList = (int*)malloc((deltaN+1)*sizeof(int));


 
  for (uint64_t pattern = 0; pattern < patterns; pattern++) {
  for (int bit = 0; bit < 16; bit ++) {      // loop over 16 bits in pattern

    b0 = pattern * 256 + sieveSmall[bit];

    temp = 1;

    // check to see if 2^k*N + b0 is reduced in no more than k steps
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
      for(j=1; j<=k; j++) {           // steps
        for(m=0; m<lenList; m++) {    // loop over lists

          // take step to update lists
          if (nList[m] & 1) {                 // bitwise test for odd
            nList[m] = 3*(nList[m]/2) + 2;    // note that nList[m] is odd
            cList[m]++;
          } else {
            nList[m] >>= 1;
          }

          // check against 0th element
          if ( m>0 && nList[m]==nList[0] && cList[m]==cList[0] ) {
            temp = 0;
            j=k+1;       // to prevent future loops
            break;
          }
      
        }
      }
    }

    if (temp) {
      //bytes |= (uint16_t)1 << bit;
      bytes += (uint16_t)1 << bit;
    }

  }

  fwrite(&bytes, sizeof(uint16_t), 1, file0);
  bytes = 0;

  }


  fclose(file0);
  return 0;
}
