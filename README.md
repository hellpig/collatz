# Experimentally testing the Collatz conjecture

I have made the world's fastest codes for CPU and GPU!

What is the Collatz conjecture?  
[https://youtu.be/5mFpVDpKX70](https://youtu.be/5mFpVDpKX70)

The goal of my codes is to test numbers for 128-bit overflow (these numbers could go off to infinity) and for infinite cycles (these numbers would never finish running). If code detects overflow, Python 3 can easily check it; see my **collatzTestOverflow.py**. The GMP library could also be used, but only use it on the numbers that overflow because it's slow!

Note that, when I count steps, I count (3n + 1)/2 as one step. It's the natural way of counting steps.

To compile CPU-only and OpenCL code, I use gcc or clang. To compile CUDA code, I use nvcc.

To test OpenCL on your system, see **testBug64.c**, **testBug128.c**, and **cldemo.c**.

Unless mentioned otherwise, GPU code is for Nvidia GPUs because I found many unforgivable arithmetic errors from Intel and AMD GPUs. Even if using OpenCL instead of CUDA, Nvidia requires that you install Nvidia CUDA Toolkit. Unless mentioned otherwise, GPU code is OpenCL.

I of course would recommend Linux as your OS.

For OpenCL on macOS, use the *-framework opencl* option of gcc (instead of the usual Linux *-lOpenCL* option). Keep in mind that OpenCL (and CUDA) on macOS is deprecated.

For OpenCL on Windows, you need Cygwin. Select the gcc-core and libOpenCL-devel packages when installing Cygwin (does Nvidia need libOpenCL-devel?). For Cygwin, gcc needs */cygdrive/c/Windows/System32/OpenCL.DLL* as an argument (instead of the usual *-lOpenCL* option). To get a printf() in an OpenCL kernel to work in Cygwin, I had to add C:\cygwin64\bin\ to the Windows Path then run via the usual cmd.exe.

Nvidia on Windows has a weird thing for OpenCL (maybe CUDA too). A printf() inside an OpenCL kernel needs *llu* for ulong (*lu* only prints 32 bits), but, even though this is OpenCL and *lu* works on Windows with Intel and AMD GPUs, needing *llu* is a typical Windows thing I guess. On second thought, a long integer is always 64-bit in OpenCL, so this behavior by Nvidia is actually a bug. Luckily, *llu* works for Nvidia-on-Linux and for a couple other situations, so I just switched all my OpenCL kernels to use *llu*. Anyway, I wonder if PRIu64 works in OpenCL for all devices and platforms. 

At any point, please ask me if anything here is unclear! I achieved my codes by communicating with various people, so please do the same!



## There are two good algorithms

I call one algorithm the "n++ n--" algorithm (or, in my filenames, just "npp"). This is fastest for finding the max n...  
[http://pcbarina.fit.vutbr.cz/path-records.htm](http://pcbarina.fit.vutbr.cz/path-records.htm)  
[http://www.ericr.nl/wondrous/pathrecs.html](http://www.ericr.nl/wondrous/pathrecs.html)  
The algorithm is described in a great paper and corresponding GitHub code [1]. Unlike his codes, I currently do not try to find the max n in my code, but adding this capability would be very easy.

I call another algorithm the "repeated k steps" algorithm. This is fastest for testing the Collatz conjecture (and for counting steps to 1). The algorithm is described here [2].

[1] Bařina, David. (2020). Convergence verification of the Collatz problem. The Journal of Supercomputing. 10.1007/s11227-020-03368-x.  
[https://rdcu.be/b5nn1](https://rdcu.be/b5nn1)  
[https://github.com/xbarin02/collatz](https://github.com/xbarin02/collatz)

[2] Honda, Takumi & Ito, Yasuaki & Nakano, Koji. (2017). GPU-accelerated Exhaustive Verification of the Collatz Conjecture. International Journal of Networking and Computing. 7. 69-85. 10.15803/ijnc.7.1_69.  
[http://www.ijnc.org/index.php/ijnc/article/download/135/144](http://www.ijnc.org/index.php/ijnc/article/download/135/144)



## There are many already-known math tricks for greatly speeding up either of the above algorithms

Just reduce n below starting number. By not reducing to 1 and just reducing below the starting number, you only need to do 3.492652 steps on average, which is a *vast* improvement over the 4.8 log2(N) steps of reducing to 1. What I mean by the starting number is the 9 in the following example...  
9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1  
Once you get to 7, you stop because it is less than the starting 9. Just 2 steps are needed instead of the above many steps. Then, when you test the starting number of 10, it immediately reduces to 5, so you move to 11. Of course, a good sieve would prevent 9, 10, and 11 from ever being tested, but you get the idea. For this to work, you can't skip anything not excluded by a sieve.

Use a 2^k sieve! As k increases, the fraction of starting numbers you have to test more-or-less exponentially decreases. This is a very standard thing to do. For example, a 2^2 sieve only requires you to run numbers of the form  
A 2^2 + 3  
because all other numbers reduce after k=2 steps. So you only need to test 25% of the numbers!  
In general, a 2^k sieve considers starting numbers of the form  
A 2^k + B  
where B < 2^k, and only certain B need to be run.  
The 2^k sieve also has the advantage of pre-calculating the first k steps of the remaining starting numbers. For each B, calculate its value after k steps, fk(B), while counting the number of steps that are increases, (c), then A 2^k + B after k steps is  
A 3^c + fk(B)  
This speeds things up if you run a bunch of A values using this same fk(B) and c.

For much larger k, I exclude about 25% more numbers than some people by also removing numbers that join the paths of smaller numbers in k steps (the number of steps that are increases must also be equal when the numbers join).  

Use a 3^1 or 3^2 sieve! A nice optimization is to not run n%3==2 numbers because n = 3N + 2 always follows the already tested 2N + 1 number (note that 2N+1 is all odd numbers). It is important that 2N + 1 is less than 3N + 2. This is called a 3^1 sieve.  
A better sieve can be to also exclude n%9==4 numbers. This is a 3^2 sieve. Any n = 9N + 4 number always follows the already tested 8N + 3.  
There are tons of these rules, but almost all exclusions occur with the above rules. For example, all mod 3^9 rules will block only a few percent more n than a 3^2 sieve, but you won't gain because checking against these rules takes time.

Just because these are known does not mean that modern codes are taking advantage of them! For example, the BOINC project by Jon Sonntag is garbage.



## There are various computational tricks to speed things up

64-bit integers, uint64\_t, aren't nearly large enough. 128-bit integers are! Overflow very rarely occurs, but, since you'd have to check for overflow regardless, who cares? Just use Python 3 or the GMP library on numbers that overflow. There is no need for larger-than128-bit integers.

When checking for overflow, use lookup tables for (~(\_\_uint128\_t)0) / 3^c instead of calculating these each time.

For some reason, CPU-only code is fastest using a 3^2 sieve, but GPU code is faster using a 3^1 sieve.

For the 3^1 or 3^2 sieve, you'll need to calculate the modulo operation. There are fast tricks for doing this on 128-bit integers!

For some reason, CPU-only code is fastest using \_\_uint128\_t, but GPU code is faster when implementing by-hand 128-bit integers.

When coding on GPU, be very mindful that all threads in the work group must wait at a line of code for the slowest thread to get there. Because of this, when checking the 3^1 sieve, instead of using a *continue* (which would cause that thread to wait on other threads), have a small loop that increases A until a suitable number is found.




## My "sieveless" approach speeds things up much more!

People save their 2^k sieves to storage drives, so lack of storage prevents huge sieves. Since the fraction of starting numbers you have to test more-or-less exponentially decreases as k increases, huge sieves are greatly desired, causing people to use clever compression tricks.

My "sieveless" approach does not save the sieve to a storage drive, so *huge* sieves can be used! Each computational task is given a small segment of the huge sieve to generate then test *many* A values.

To generate the sieve segments quickly and without requiring *huge* amounts of RAM, realize that not all paths can join. Only "nearby" paths can join. As k gets larger, more distant paths can join, but still within a k-dependent distance. If you look in my code, you'll see these distances called deltaN. For k=34, deltaN = 46. For k=40, deltaN = 120. Using deltaN, I can create any 2^k sieve using very little RAM! **collatzSieve2toK_FindPatterns.c** is my code that experimentally finds deltaN (among other things) for any k. Note that this code takes over a day to run k=40.

But, to experimentally find deltaN, you would still need all 2^k values stored in RAM because you don't know deltaN yet. If you read the comments at the top of collatzSieve2toK_FindPatterns.c, you see that deltaN can be bounded by 2^(k - minC), where minC > k / log2(3) is the lowest c that does not include already-ruled-out numbers, where c is how many increases are encountered in k steps (steps and "already-ruled-out n" are defined in the code's comments). Confused? Just read the copious comments at the top of the code! I was proud of myself for figuring this out, but, even with this bound, for k=40, putting 2^(40 - ceiling(40/log2(3))) into WolframAlpha gives a bound of 16384. This solves the issue with RAM, but CPU time is going to take almost half a year. The code's comments mention a code I use to get a tighter bound, and **collatzFindDeltaNbound.c** is this tricky code. I developed the algorithm myself, and I wonder if anyone else has done this. Read the comments at the top with pen and enough paper to figure out how the math of it works! For k=40, the tighter bound is just 679, which can be run in a week by collatzSieve2toK_FindPatterns.c to find deltaN = 120.

For larger k, I put my GPU and many CPU cores to work using OpenCL and OpenMP! Run **collatzSieve2toK_FindPatterns_GPU.c** and **kernel.cl**.

See my **largeK.xlsx** for all the results of these and other codes.

Armed with deltaN, we are ready to implement!

Instead of having a task ID that you increment each run, you will have two task IDs: the first controls a huge amount of numbers such as 9 × 2^67, and the second refers to a chunk of these 9 × 2^67 numbers. Only increment the first when the second has reached its final value. The second task ID determines which segment of the very large sieve will be produced. The total size (9 × 2^67 in this example) should be chosen carefully because it would be a shame to have to leave a chunk unfinished. The idea is that you make the 2^k sieve as large as possible while keeping the sieve generation time negligible compared to time running numbers using that chunk of the sieve.

For the GPU code, we need two kernels. Kernel1 will generate the part of the sieve determined by the second task ID. Kernel2 will test 9 × 2^(67 - k) numbers for each number in the sieve (this number should be large to not notice the extra time needed to generate the sieve live). Luckily, most of what kernel2 needs as inputs are the same as what kernel1 creates as outputs, so the GPU RAM can mostly just stay in the GPU between the kernels! This allows kernel2 to start with pre-calculated numbers! The "repeated k steps" needs another kernel, kernel1_2, to make the 2^k2 lookup table for doing k2 steps at a time.

Challenges with this "sieveless" approach...
* Getting exact speeds (numbers per second) is difficult because each task has a different part of the sieve, but who cares?
* The only real con is that you must finish running each of the second task ID for the results to be valid (or to have any useful results at all).

While running various tests on my GPU code to see if it was working correctly, I discovered something interesting: my hold[] and holdC[] were never being used, yet I was always getting the correct results! So, for k=34, I searched for numbers of the binary form ...0000011 that did not reduce but joined the path of smaller numbers (because these are the numbers that would require hold[] and holdC[]), but I didn't find any! I only found (many) numbers of the form ...1011. I could then test hold[] and holdC[] code using the very unrealistic settings: TASK_SIZE = 3 and task_id = 120703 (and k=34). This is interesting because maybe I could prove that, if TASK_SIZE is large enough, hold[] and holdC[] are unnecessary!

A larger k (and therefore TASK_SIZE) might be better even if a larger fraction of the time will be spent making the sieve. If you get get a 10% faster sieve at the cost of creating the sieve taking 5% of the time, you still win!

Here are the steps to setting your parameters...
1. Choose an amount of numbers that you could commit to finishing in a month or whatever your timescale is. If 2^68, set TASK_SIZE_KERNEL2 in the GPU code and TASK_SIZE0 in the CPU code to be 68 (you'll also need to remove the some 9's in the code as explained in the code).
1. Then set k and deltaN_max to maximize the numbers per second! If you set k too large, then the code spends too large a fraction of time generating the sieve. I have found that the very large deltaN only occur very rarely, so deltaN_max need not be huge. If your TASK_SIZE_KERNEL2 and TASK_SIZE0 are not large enough and you find that the fastest k is something small like 35, you may wish to not use this sieveless approach and just use a sieve that you store on your storage drive. See my spreadsheet, largeK.xlsx, for good k values to run.
1. Choose the amount of numbers that each task should test—let's say 9 × 2^40—then set TASK_SIZE = 40 + k - (TASK_SIZE0 or TASK_SIZE_KERNEL2). Each task will look at a 2^TASK_SIZE chunk of the sieve to test 2^(TASK_SIZE0 - k) or 2^(TASK_SIZE_KERNEL2 - k) numbers per number in the sieve. Because there is apparent randomness to the sieve, TASK_SIZE should at least be 10 to give each process something to run! Unlike the CPU code, which needs very little RAM, TASK_SIZE in the GPU code determines RAM usage, and you want it large enough to minimize your CUDA Cores sitting around doing nothing.

If you want to run both the GPU and CPU code simultaneously, be sure to use the same sieve size for both, and have the size controlled by the first task ID be the same for both. I would normally say that TASK_SIZE needs to also be the same, but, if the CPU tasks need to finish in a certain time, it may be best to set TASK_SIZE differently: the CPU should have a lower one. This would require just a bit of organization when setting the second task ID to make sure that the tiny CPU tasks fit around the GPU tasks and to make sure that all of the sieve is eventually run.

If you have already have up to some number tested by a non-sieveless approach, you can still easily use this sieveless approach! For my CPU code, adjust aStart manually. For my GPU code, adjust h_minimum and h_supremum in kernel2, keeping in mind that integer literals over 64-bit don't work (so use bit shifts).

If you want to use general GPUs...
* For computers with watchdog timers, a maxSteps variable must be introduced, and a message should be printed whenever testing a number goes beyond those steps. Having a way to detect an infinite cycle or a number that is slowly making its way to infinity is crucial.
* For computers with watchdog timers, you will likely have to modify the code to call kernel2 multiple times, each time with a different h_minimum and h_supremum.
* To accommodate GPUs (and CPUs) without ECC RAM, you'll have to run each task twice to validate.
* The 128-bit integers should become OpenCL's uint4 to prevent arithmetic errors on non-Nvidia GPUs. I believe that ulong arithmetic should never be used on Intel or AMD GPUs. Another solution could be to look at my testBug64.c and testBug128.c to understand the bugs, then test each device for these bugs before running the Collatz code. Update: I have now discovered that whether or not certain GPUs return a correct checksum is not perfectly correlated with the bugs in those two test files, so, before allowing a GPU to contribute, I'd also run some test numbers on each GPU to make sure the correct checksum is created!
* To accommodate weaker GPUs, set TASK_SIZE to be smaller.

To find the largest number or to calculate checksums, you can easily edit kernel2 to output some extra things. To see how, search [David Bařina's kernel](https://github.com/xbarin02/collatz/blob/master/src/gpuworker/kernel32-precalc.cl) for mxoffset, max_n, and checksum_alpha. Of course, something similar can be done for my CPU code. I don't expect this to greatly slow down the code. I temporarily added the checksum code to my CPU and GPU codes, and the checksums matched those of David Bařina's codes.

If finding the max number and using a 2^k sieve, you have to be careful that the max number does not occur in the first k steps. That is, your sieve cannot be too large. Even after k increases, the number will get approximately (3/2)^k times larger requiring approximately log2(3/2) k more bits. From this list of max numbers, [http://pcbarina.fit.vutbr.cz/path-records.htm](http://pcbarina.fit.vutbr.cz/path-records.htm), finding a maximum number in these k steps would be difficult to do since even a k=80 sieve is very safe (unless you start testing up to 2^89 and no new new records are found!). 

Note that Nvidia has various OpenCL bugs that cause CPU busy waiting (100% usage) while waiting for the GPU to finish. CUDA does not have these issues. See my "partially sieveless" OpenCL host codes for an attempt I made at getting Nvidia to stop busy waiting for kernel2, but a printf() in kernel2 prevents my GPU from being fixed.



## My "partially sieveless" codes are actually what you should use!

I learned that nearly all of the numbers removed by a deltaN (after checking for reduction in no more than k steps) are from deltaN = 1!  
For k=21, 38019 numbers need testing using if using all deltaN, 38044 need testing if only using deltaN=1, and 46611 need testing if not using any deltaN.  
For k=32, 33880411 numbers need testing using if using all deltaN, 33985809 need testing if only using deltaN=1, and 41347483 need testing if not using any deltaN.  
See largeK.xlsx for numbers that need testing if only using deltaN = 1. I now recommend to set deltaN_max to 1! This allows for a larger k!

I once had the idea of saving out a 2^32 sieve that a "partially sieveless" code could load in to help generate a much larger "sieveless" 2^k sieve. My GPU approach for creating a sieve is to do k steps at a time for all numbers, so, for large deltaN, I'd still have to do k steps for all numbers anyway. My CPU-only approach would benefit, but I didn't think it would benefit enough to be revolutionary. So I settled for only using a 2^2 sieve to help generate a much larger 2^k sieve. However, a deltaN-equals-1 2^k sieve could use this 2^32 sieve very nicely! For each n in the 2^k sieve for which n % 2^32 belongs to the 2^32 sieve, do k steps for n and n-1. In fact, since the 2^32 sieve uses all possible deltaN values, doing k steps for n-1 (that is, using deltaN = 1 instead of deltaN = 0) for the numbers in the 2^32 sieve barely helps. This "partially sieveless" code should now greatly speed up sieve generation allowing for larger k values! Also, for GPU code, TASK_SIZE could be increased more without running out of RAM. I realized that this using-multiple-sieves idea could be great for deltaN=1 when Eric Roosendaal was telling me about how he uses a 2^8 sieve (which only requires 16 numbers to be run), to create a larger sieve.

I will call these codes "partially sieveless" because the smaller sieve of size 2^k1 is saved to disk, so it's not "sieveless" (that is, it's not generated as you go). However, the larger 2^k sieve will be "sieveless" (that is, it is generated as you go).

I mentioned k1 = 32 because a 2^32 sieve can be created in a couple minutes by a single CPU core. The only con of a larger k1 is that the sieve file is harder to create, transfer, and store. Because a buffer is used to load only parts of the 2^k1 sieve, a larger k1 does not use more time or RAM to use. Once you have a large-k1 sieve file, use it! Especially good values for k1 are 32, 35, 37, 40, ... See my largeK.xlsx for why I say this.

Almost everything mentioned in the previous section about "Sieveless" code still applies. Read it!

On CPU, creating the 2^k sieve is vastly faster with this approach! Even though deltaN = 1, the amount of numbers tested by a "partially sieveless" or an any-deltaN "sieveless" code are basically the same partly because the 2^k1 sieve uses any deltaN. There is no need for me to provide speeds because the speeds of a k=51 sieve will be the same as with the "sieveless" approach. The difference from "sieveless" code is that we can now run larger k values! Or, to think if it another way, use the same k value, but commit to a much smaller TASK_SIZE0 or TASK_SIZE_KERNEL2.

On GPU, "partially sieveless" is many times faster than the "sieveless" code for making the sieve, but it doesn't get the extreme speedup that the CPU sees. Perhaps this is because, for "partially sieveless", the different threads in the work group can test a very different number of numbers (the threads in the same work group must wait on the thread that takes the longest time). Note that the "sieveless" code spends most of the time to make the sieve on the CPU (when using my usual Nvidia device), so a very fast GPU would prefer my "partially sieveless" code even more. Feel free to use "partially sieveless" code for CPU and "sieveless" for GPU (or vice versa)! If you do this, as previously discussed, just be sure to use the same k and same TASK_SIZE0 or TASK_SIZE_KERNEL2 (different k1 or k2 values are just fine). Also, for large TASK_SIZE, my "partially sieveless" code uses about 9% of the RAM compared to my "sieveless" code!

As for how to test the validity of this code, I temporarily added the previously-mentioned checksum code. I tested the 2^k1 sieve by removing the 2^k code that does the first k steps, and I tested the 2^k sieve by removing the code that checks against the 2^k1 sieve. When comparing to my "sieveless" codes, keep in mind that 2^k1 uses any deltaN, but the 2^k sieve uses deltaN = 1.

### CUDA and making 128-bit integers by hand

I was curious to try Nvidia's CUDA instead of OpenCL, which only works on Nvidia GPUs. Learning CUDA can be useful because (1) it might be faster, (2) it sure is easier to code, and (3), on Nvidia GPUs, CPUs don't have to busy wait (at least on some GPUs, Nvidia using OpenCL currently causes CPU busy waiting due to an Nvidia printf-in-kernel bug). CUDA currently has no native support for 128-bit integers in device code, so CUDA requires that I find my own 128-bit integer class. Learning to do 128-bit integers can be useful because (1) not all OpenCL implementations have a 128-bit integers and (2) one could perhaps speed up the Collatz algorithm by optimizing this 128-bit-integer code for Collatz.

I obtained 128-bit integers in CUDA via this GitHub project (has a GPL license). The idea is to download a single file into your current folder, and put...  
#include "cuda_uint128.h"  
in your .cu file. Note that...  
* multiply and divide require the 2nd argument to be uint64_t
* a / for division uses div128to64(), which is an unexpected behavior, especially since div128to64() returns (uint64_t)-1 when the quotient overflows uint64_t, so use div128to128() for division (redefining operator/() would break many internal things)
* casting to 128-bit works as usual with something like (uint128_t)foo, but casting away from 128-bit requires using .lo
* a ++ or -- must be placed before the variable (not after it)
* subtraction always has the 2nd argument be uint128_t
* addition has a version where the 2nd argument is uint128_t and another that has the 2nd argument be uint64_t, but ++ and += always make the 2nd argument be uint128_t
* bit shifts and comparisons are included, but \*= is not even though -= and += are defined

I found many serious errors in the code (causing incorrect arithmetic to be done), so I gave the author the fixes to the errors that I found, and he then updated GitHub with those fixes (some GitHub updates still pending). Then I made some additions to the code to overcome the last three of the above limitations, and then I made some optimizations specific to my Collatz code, so I provided my version of the .h file complete with comments explaining all my changes.

My goal was to keep the CUDA code as similar as I could to my OpenCL code. However, I did make a change to prevent the CPU from busy waiting during kernel2, which I couldn't successfully do on my Nvidia GPU using OpenCL due to Nvidia's less than perfect implementation of OpenCL. I checked this code's validity by temporarily adding in code to calculate checksums (as described above).
* cuda_uint128.h
* collatzPartiallySieveless_npp_CUDA.cu
* collatzPartiallySieveless_repeatedKsteps_CUDA.cu

The CUDA code is about 10% faster than the \_\_uint128\_t OpenCL code!

Since implementing 128-bit integers as two 64-bit integers in OpenCL might be better than using \_\_uint128\_t in OpenCL (or even better than CUDA), I want to try it! In fact, some of my non-Nvidia GPUs have errors with \_\_uint128\_t. However, I still do not recommend using anything but Nvidia due to the unforgivable ulong arithmetic errors that Intel and AMD GPUs make (as previously mentioned). For CUDA, I used Nvidia's assembly language, PTX, via the asm() function in any addition involving 128-bit integers (all other arithmetic used in kernels was done without using PTX). Since OpenCL can run on many GPU types, there is no standard assembly language, so my 128-bit implementation may be slower than \_\_uint128\_t! The following are written in C (because I was curious how it could work in C and because C++ in OpenCL kernels is not yet widely supported), which doesn't allow me to overload operators like I did using C++ for CUDA, so the codes are hard to read!
* kern1_128byHand.cl
* kern2_npp_128byHand.cl
* kern2_repeatedKsteps_128byHand.cl

I tested speeds on a Nvidia Quadro P4000. The 128byHand codes are always faster than the older OpenCL codes. CUDA seems to be faster than these 128byHand codes when TASK_SIZE_KERNEL2 is quite large, and these 128byHand codes seem to be faster than CUDA when TASK_SIZE is quite large.

Using by-hand uint128_t is faster than native \_\_uint128\_t on GPU, which was surprisingly, so trying CPU-only code that does uint128_t seems like a good idea. Partly because cuda_uint128.h has limitations (for example, cannot divide by uint128_t), my goal is always to use the minimal amount of by-hand code, so, on GPU, I only changed the kernel code. For OpenCL, since some of my devices don't like the native 128-bit implementation (won't compile or will give arithmetic errors), all 128-bit integers in kernels was made to be by-hand. For CUDA, I had to change all the code in the kernels because there is no alternative. For CPU-only code, I just need everything in the main loop (over patterns) to be by-hand, which collatzPartiallySieveless_128byHand.cpp does. On my I5-3210M CPU, 128byHand is slower, though maybe ARM CPUs would be different?


