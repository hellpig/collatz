# Challenges of any "reduce to 1" algorithm that counts steps

The 2017 paper I previously linked to also has a "repeated k steps" algorithm to count steps to 1 with some great results regarding the best size of lookup table (what I call k2) to use, though I'd imagine that they'd get somewhat different results if reducing much larger numbers. I started trying to write code to do this, but I soon found myself realizing that there is no clear way to define a step. Should (3n + 1)/2 count as 1 step or 2 steps? The convention seems to be to call it 2 steps, but I feel that it is far more natural to call it 1 step. In light of David Bařina's new "n++ n--" algorithm, a step could be each bit shift! Regardless of how steps towards 1 are counted, when doing the k steps of a 2^k sieve, (3n + 1)/2 must be counted as 1 step.

An annoying aspect of a "reduce to 1" code is that the code will take more time per number as you test larger numbers. You'll probably want to optimize your code for rather large n. Benchmarking your code will also need to be done carefully to make sure the same numbers are run.

For any scan such as the "reduce to 1" scan for max steps or a scan for max n, it is important to start testing numbers beginning at 1 without skipping. Anyone can find a number that has a huge number of steps, but the goal is to find the smallest number to require that many steps. 2^10000 takes 10000 steps to reduce to 1, but who cares? If you want to argue that 2^10000 can be reduced in 1 step (a single bit shift), I would probably agree with you, but you can always run the Collatz algorithm backwards to get more interesting numbers that require any number of steps! Here is **collatzSetDelay.py** to do this. Whenever (2n - 1)%3 == 0, decrease to (2n - 1)/3 unless (2n - 1)%9 == 0. The "mod 9" check is needed to prevent getting numbers that have 3 as a prime factor because they can only increase by a factor of 2 forever. You can then calculate that, on average, 3/7 of the steps will be decreases because, as you increase, (2n - 1)%9 cycles through a repeating pattern of 3, 7, 6, 4, 0, 1, and the 3 decreases to a 1, 4, or 7, and the 6 decreases to a 0, 3, or 6. The next decrease is twice as likely to be from a 3 than a 6. Also, n%27 == 11 can decrease immediately, but it is best to not decrease because, if it decreases, there can be 4 increases before the 3rd decrease, but, if it increases, there can be 3 increases before the 3rd decrease. Instead of the n%27 == 11 rule, using the following rules does even better: n%729 in \[209, 587, 452, 695, 533, 47, 344, 281, 38, 524, 245, 362, 416, 470, 335, 173, 425, 686, 443, 389, 119, 605, 659, 200, 92\]. A better way of getting a larger fraction of decreases is to have the code look ahead a certain number of steps, as I have done in **collatzSetDelay2.py**. Setting the code to look ahead 25 steps, I asked for a number that takes 1000 steps, and I got 476,003724,222891.

Certain types of sieves can still be used. A 3^1 or 3^2 sieve can still be used because the numbers removed by these sieves come from smaller numbers, and these smaller numbers will require more steps to reduce than the larger numbers that result from them. To generate my 2^k sieves up until now, I have (1) removed numbers that reduce in k steps then (2) removed numbers that join the path of smaller numbers in k steps. The second of these approaches can still be done because paths that join will require the same number of steps, and the smaller number is the one that needs to be found. **My "sieveless" approach could still work!** However, I am not sure if the second of these approaches is valid if counting steps towards 1 as bit shifts.

Note that all of my above code for deltaN including collatzFindDeltaNbound.c only try to join paths for numbers that do not reduce, so they need to be modified. My first step was modifying collatzFindDeltaNbound.c by (1) removing the two checks for *isS1* and (2) starting the *c* loop with 1 instead of *minC*. These modifications are very easy, so I won't provide the modified code, but I put the results in a column in largeK.xlsx. The deltaN bounds are *much* larger!

To find the experimental (actual) deltaN and to count the numbers that need testing in each sieve, I then modified collatzSieve2toK_FindPatterns.c by (1) removing the bit of code under the comment that says "check to see if if 2^k\*N + b0 is reduced in no more than k steps", (2) updating the starting deltaN, and (3) making the *b0* loop now start at 0 and increase by 1 each iteration (instead of starting at 3 and increasing by 4 each time). For the final change, I added a test for if *b0* is 0 before seeing if it was in the sieve. **I learned that not all numbers that join a path of a smaller number work!** For a 2^3 sieve, 5 becomes 2 after 3 steps (one of them is an increase), and 4 becomes 2 after 3 steps (one of them is an increase). The problem is that the path that started with 4 should no longer be followed after it reduced to 1 because the 5 takes more steps to reduce to 1, so I added the following line after the *nList[m] >>= 1* line...
```
if ( nList[m] == 1 && m>0 ) nList[m] = 0;
```
and I had to loop over the steps as the outer loop making the m loop (including checking against the 0th element) the interior loop. Note that, with the code in this configuration, the code could claim an exclusion by a larger deltaN even though a smaller deltaN will exclude it in a later step.  
Results...
* **A very interesting pattern emerged! The experimental deltaN are all 2^(k-4).** Though, I only tested up to k=16. The explanation seems to be how, for k=4, 13 joins paths with 12. For k=5, just double the 13 and the 12, and we have that 26 joins paths with 24. Doubled numbers must join paths because doubling just adds a decrease step for each. Doubling also doubles the deltaN between them.
* Regarding the density of large-deltaN exclusions, for k=16, only 1 number required deltaN=4096, only 1 number required the next deltaN=3243, and only 3 numbers required the next deltaN=2048.
* Of course, these "reduce to 1" sieves take more time to analyze than the usual reduce-below-start-n sieves. The overall design of my code is not optimized for such large deltaN. My "sieveless" approach also does not favor a large deltaN.
* These sieves are not nearly as good as the ones you can use if *not* wanting to reduce to 1! As k increases, there is less gain with these "reduce to 1" sieves than with the usual sieves because the fraction that needs testing no longer exponentially decays.

I wonder if the "numbers cannot join numbers that have already reduced to 1" rule can be lifted somewhat. For our 2^3 sieve, 5 joins 4 after 3 steps, but 4 already reduced to 1. This is a problem because 4 takes 2 steps to reduce, and 5 takes 4 steps to reduce, so 5 needs to be tested. However, 2^3 + 5 must also join 2^3 + 4 in 3 steps, but 2^3 + 4 has not yet reduced to 1. Perhaps I could prove that for A\*2^k + B that joins A\*2^k + b, where b is smaller than B, A\*2^k + b with A > 0 never reduces to 1 before the paths join. If I could prove this, then better sieves could be made for testing all numbers except the A=0 numbers. Actually, the proof is very easy: 2^k is the largest number to reduce to 1 in k steps, and nothing above it can join its path. For large enough k, the sieves are barely better if I let numbers join numbers that have already reduced to 1, and the cost of doing this, as far as I have experimentally tested, is that deltaN doubles plus the cost of having to do A=0 separately. Doing A=0 separately is no minor cost if using my "sieveless" approach, which aims to use *huge* sieves.

As for making these sieves using a GPU, my approach on GPU so far won't work with the "numbers cannot join numbers that have already reduced to 1" rule. My GPU approach would not be able to find cases where numbers joined *before* reducing to 1 in k steps. That is, my GPU code creates A > 0 sieves. However, unlike the modified CPU code I mentioned, the approach I use in the GPU code would be optimized for these larger deltaN! Unlike my non "reduce to 1" version of this code, comparing number of increases is now certainly required, hold[] and holdC[] arrays are also certainly required, and you'll start using considerable RAM after k=26 (due to deltaN being large). My GPU runs for only a tiny fraction of the run time, so I should probably explore making a second kernel instead of OpenMP! To do this, my 2 *collect* arrays, my 2 *hold* arrays, *bits*, and *n0start* would need to be passed to the GPU. In the meantime, I wrote a CPU-only code that makes A > 0 sieves that are not limited by the amount of GPU RAM (limited only by CPU RAM)...
* collatzSieve2toK_FindPatterns_reduceTo1.c (my CPU-only code)
* collatzSieve2toK_FindPatterns_GPU_reduceTo1.c
* kernel_reduceTo1.cl

The following is OpenMP CPU-only code for A >= 0 sieves. The algorithm is much slower, but it requires basically no RAM. If I ever care, I'll write GPU code for this: **collatzSieve2toK_FindPatterns_reduceTo1_Aequals0.c**


"Reduce to 1" conclusion: A 3^2 (or 3^1) sieve works very nicely, but the 2^k sieve that you can create has a huge deltaN that is a real pain for not-miraculous gain.

It seems that [I'm not the first to conclude such things](http://www.ericr.nl/wondrous/techpage.html)! See his section called "The class record algorithm".

The strategy I would recommend is to use the "A > 0" sieves because they are easier to generate. For A=0, you can just [look up the record for highest steps](http://www.ericr.nl/wondrous/delrecs.html), so you don't have to run it (note that this list counts (3n + 1)/2 as 2 steps). Use the same 3 steps I gave for choosing "sieveless" parameters, and your k will have to be small unless you set your deltaN_max to be low enough.

I wrote **collatzSieveless_reduceTo1_Aequals0.c** to run the A=0 case. It is a very simply code that doesn't even use any 2^k sieve. I quickly ran up to 2^36, and I verified that, up through 2^36, counting (3n + 1)/2 as a single step gives the same records as the table I just linked to (with simply a smaller delay). I also found that 762 is the record delay for a 2^36 sieve.

I ran the following "sieveless" files using a 2^36 sieve to allow for a 2^50 sieve. A 2^50 sieve would require setting *STEPS_MAX* (or *stepsMax* for CPU-only code) to at least 1161. I verified that, up through 9\*2^47, counting (3n + 1)/2 as a single step gives the same records as counting it as 2 steps (with simply a different delay).
* collatzSieveless_reduceTo1.c (my CPU-only code)
* collatzSieveless_reduceTo1_GPU.c
* kernel1_reduceTo1.cl
* kernel1_2_reduceTo1.cl
* kernel2_reduceTo1.cl

My results were added to the following file (the other delays in the file are from [here](http://www.ericr.nl/wondrous/delrecs.html)): **delayRecords.txt**

To analyze the log file from the GPU code, I ran the Linux commands...
```
grep "steps = " log.txt | sed 's/^.steps.=.\([0-9]*\).found:.(\([0-9]*\)....64)...\([0-9]*\).*/\1 \3/' | sort -n -k 1,1 -k 2,2 | uniq -w4 > lowestForEachDelay.txt
cat lowestForEachDelay.txt | sort -n -k2 > lowestN.txt
```

I then ran the Python 3 script...
```
stepsMax = 762
f = open("lowestN.txt", "r", encoding="utf8").read().splitlines()
for line in f:
  steps = int(line.split(" ")[0])
  if steps > stepsMax:
    stepsMax = steps
    print(line)
```

As for the density of deltaN in these "A > 0" sieves, the large deltaN are very rare. k=16 has deltaN = 8192 and a total of 46261 numbers removed from the sieve. For the following deltaN_max settings, I provide the numbers not excluded by the sieve...  
  deltaN_max = 8192: 19275  
  deltaN_max = 4096: 19276  
  deltaN_max = 2048: 19279  
  deltaN_max = 1024: 19288  
  deltaN_max = 512: 19308  
  deltaN_max = 256: 19356  
  deltaN_max = 128: 19457  
  deltaN_max = 64: 19693  
  deltaN_max = 32: 20285  
  deltaN_max = 16: 21582  
  deltaN_max = 8: 24000  
  deltaN_max = 4: 27984  
  deltaN_max = 2: 32997  
  deltaN_max = 1: 40295 out of 65536 must be run

Here are the numbers not excluded as a function of deltaN_max for k=10 with deltaN = 128 and a total of 586 numbers removed from the sieve...  
  deltaN_max = 128: 438  
  deltaN_max = 64: 439  
  deltaN_max = 32: 442  
  deltaN_max = 16: 452  
  deltaN_max = 8: 471  
  deltaN_max = 4: 513  
  deltaN_max = 2: 572  
  deltaN_max = 1: 673 out of 1024 must be run  
Note that deltaN's that are of the form 2^integer are the most common. 
