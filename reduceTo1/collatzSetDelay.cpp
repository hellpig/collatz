/*
  Get an interesting number that has a delay of your choosing.
  Look ahead for the best path
    keeping the smallest PATHS paths for each step you take.
  (2*n - 1)/3 is counted as 1 step by default,
    but this can be easily changed by uncommenting 2 parts.

  You are meant to run this on a 64-bit computer.

  To use...
    g++ -O3 collatzSetDelay.cpp
    time ./a.out
*/

#include <vector>
#include <algorithm>  // for std::sort


// set target
const int delay = 1000;


/*
  For large enough PATHS...
    RAM usage is proportional to PATHS. RAM is about 20*PATHS bytes.
    Required time is roughly proportional to PATHS.
      PATHS = 10^6 takes a minute for delay=1000.
*/
const size_t PATHS = 1000000;



#define UINT128_MAX (~(__uint128_t)0)
bool dontCheckMod9 = false;    // don't change this initialization
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
    uint64_t a = (uint32_t)(num >> 96);
    uint64_t b = (uint32_t)(num >> 64);
    uint64_t c = (uint32_t)(num >> 32);
    uint64_t d = (uint32_t)(num);
    return (a+b+c+d)/3 + c*(1431655765) + b*((__uint128_t)6148914691236517205ull) + a*((__uint128_t)1431655765ull << 64 | (__uint128_t)6148914691236517205ull);
}



void takeStep() {
  size_t stop = list.size();
  size_t i = 0;
  while (i<stop) {

    // handle overflow
    if (list[i] > (UINT128_MAX >> 1)) {

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
    list[i++] = temp;             // increase


  }
}



int main () {
  list.reserve(2 * PATHS);   // feel free to reduce this!

  // do first 3 steps manually to avoid the trivial cycle
  list.push_back(8);

  // do one step at a time
  for (int j=0; j < delay - 3 - 2; j++) {
    if (list.size() > PATHS) {
      std::sort(list.begin(), list.end());  // sort entire list[]
      list.resize(PATHS);
    }
    takeStep();
    //checkSteps(j+3+1);
  }

  // do the final 2 steps
  dontCheckMod9 = true;
  for (int j=0; j < 2; j++) {
    if (list.size() > PATHS) {
      std::sort(list.begin(), list.end());
      list.resize(PATHS);
    }
    takeStep();
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
