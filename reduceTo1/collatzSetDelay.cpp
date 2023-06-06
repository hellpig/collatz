/*
  Get an interesting number that has a delay of your choosing.
  Look ahead for the best path
    keeping the smallest PATHS paths for each step you take.
  (2*n - 1)/3 is counted as 1 step by default,
    but this can be easily changed by uncommenting 2 parts.

  You are meant to run this on a 64-bit computer.
  This code uses 128-bit unsigned integers.

  To use...
    g++ -O3 collatzSetDelay.cpp
    time ./a.out
*/

#include <vector>
#include <algorithm>  // for std::sort


/*
  Set target.
  delay >= 5 since the first 3 and last 2 steps are always done.
*/
const int delay = 1000;


/*
  For large enough PATHS...
    RAM usage is proportional to PATHS. RAM is about 20*PATHS bytes.
    Required time is roughly proportional to PATHS.
      PATHS = 10^6 takes a minute for delay=1000.

  I was trying to find all the delay records with this code
  using (2*n - 1)/3 as 1 step,
  and the first one to require many PATHS is...
    26623 with a delay of 194 requiring 104441 PATHS
  The next delay record to require more PATHS is... 
    142587 with a delay of 236 requiring 1314652 PATHS
  The next delay record to require more PATHS is... 
    1501353 with a delay of 333 requiring 120E6 < PATHS < 200E6
*/
const size_t PATHS = 100000;



std::vector<__uint128_t> list;



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

// for debugging
void printList() {
  for (size_t i = 0; i<list.size(); i++)
    print128(list[i]);
}



// for debugging
void checkSteps(int j) {   // do all values in list[] take j steps?

  for (size_t i = 0; i<list.size(); i++) {
    __uint128_t n = list[i];
    int steps = 0;
    while (n > 1) {
      steps++;
      if (n&1) n = 3*(n>>1) + 2;
      else n >>= 1;
    }
    if (steps != j) {
      print128(steps);
      print128(j);
      printList();
      print128(i);
      printf("**************\n");
    }
  }
}



// trick for quickly doing num%3 if num is __uint128_t
inline int mod3(__uint128_t num) {
    uint64_t r = 0;
    r += (uint32_t)(num);
    r += (uint32_t)(num >> 32);
    r += (uint32_t)(num >> 64);
    r += (uint32_t)(num >> 96);
    return (int) (r%3);
}


// trick for quickly doing num%9 if num is __uint128_t
inline int mod9(__uint128_t num) {
    uint64_t r = 0;
    r += (uint64_t)(num)        & 0xfffffffffffffff;
    r += (uint64_t)(num >>  60) & 0xfffffffffffffff;
    r += (uint64_t)(num >> 120);
    return (int) (r%9);
}


// trick for quickly doing num/3 if num is __uint128_t
inline __uint128_t div3(__uint128_t num) {
    uint64_t a = (uint64_t)(num)        & 0xfffffffffffffff;
    uint64_t b = (uint64_t)(num >>  60) & 0xfffffffffffffff;
    uint64_t c = (uint64_t)(num >> 120);
    return (a+b+c)/3 + b*((__uint128_t)384307168202282325ull) + c*((__uint128_t)24019198012642645ull << 64 | (__uint128_t)6148914691236517205ull);
}




void takeStep(bool dontCheckMod9) {
  const __uint128_t MAX = (~(__uint128_t)0) >> 1;  // 2^127 - 1
  size_t stop = list.size();
  size_t i = 0;
  while (i<stop) {

    // handle overflow
    if (list[i] > MAX) {

/*
      // use this instead to make (2*n - 1)/3 be counted as 2 steps
      __uint128_t temp2 = div3(list[i]);
      if (mod3(list[i]) == 1 && temp2&1 && (dontCheckMod9 || mod3(temp2) != 0)) {
        list[i++] = temp2;    // decrease only
*/
      int temp2 = (2*mod9(list[i]))%9;
      if ( temp2==4 || temp2==7 || (dontCheckMod9 && temp2==1) ) {
        list[i] = 2*div3(list[i]) + 1;   // decrease only
        i++;
      } else {
        // back-swap trick to remove list[i]
        list[i] = list[--stop];
        list[stop] = list.back();
        list.pop_back();
      }

      printf("*");
      continue;
    }


/*
    // use this instead to make (2*n - 1)/3 be counted as 2 steps
    __uint128_t temp = list[i] << 1;
    __uint128_t temp2 = div3(list[i]);
    if (mod3(list[i]) == 1 && temp2&1 && (dontCheckMod9 || mod3(temp2) != 0))
      list.push_back(temp2);    // decrease
    list[i++] = temp;          // increase
*/


    __uint128_t temp = list[i] << 1;
    int temp2 = mod9(temp);
    if ( temp2==4 || temp2==7 || (dontCheckMod9 && temp2==1) )
      list.push_back(div3(temp));  // decrease
    list[i++] = temp;    // increase; try to keep the main (first) part of list[] sorted


  }
}



int main () {

  list.clear();
  list.reserve(2 * PATHS);   // feel free to reduce this! PATHS only grows by about 33% each step
  printf("  delay = %i, PATHS = %zu\n", delay, PATHS);

  // do first 3 steps manually to avoid the trivial cycle
  list.push_back(8);

  // do one step at a time
  for (int j=0; j < delay - 3 - 2; j++) {
    if (list.size() > PATHS) {
      std::sort(list.begin(), list.end());  // sort entire list[]
      list.resize(PATHS);
    }
    takeStep(false);
    //checkSteps(j+3+1);
  }

  // do the final 2 steps
  for (int j=0; j < 2; j++) {
    if (list.size() > PATHS) {
      std::sort(list.begin(), list.end());
      list.resize(PATHS);
    }
    takeStep(true);
    //checkSteps(delay-1+j);
  }

  // print smallest in list[]
  std::sort(list.begin(), list.end());
  if (list.size())
    print128(list[0]);
  else
    printf("  all paths overflowed\n");

  return 0;
}
