# Experimentally testing the Collatz conjecture

For experimentally testing the Collatz conjecture, I have made the world's fastest codes for GPU and CPU-only!

What is the Collatz conjecture?  
[https://youtu.be/5mFpVDpKX70](https://youtu.be/5mFpVDpKX70)  
[https://youtu.be/094y1Z2wpJg](https://youtu.be/094y1Z2wpJg)

The goal of my codes is to test numbers for 128-bit overflow (these numbers could go off to infinity) and for infinite cycles (these numbers would never finish running). If the code detects overflow, Python 3 can easily check it; see my **collatzTestOverflow.py**. The GMP library could also be used, but only use it on the numbers that overflow because it's slow! I do not use GMP.

I have not run this code on a supercomputer to get new results, nor do I care to. If you want to run it (on BOINC for example), let me know, and I can help.

Note that, when I count steps, I count (3n + 1)/2 as one step. It's the natural way of counting steps.

To compile CPU-only and OpenCL code, I use gcc or clang. To compile CUDA code, I use nvcc.

To test OpenCL on your system, see **testBug64.c**, **testBug128.c**, and **cldemo.c**.

Unless mentioned otherwise, GPU code is for Nvidia GPUs because I found many unforgivable arithmetic errors from Intel and AMD GPUs. Even if using OpenCL instead of CUDA, Nvidia requires that you install Nvidia CUDA Toolkit. Unless mentioned otherwise, GPU code is OpenCL.

I of course would recommend Linux as your OS.

For OpenCL on macOS, use the *-framework opencl* option of gcc (instead of the usual Linux *-lOpenCL* option). Keep in mind that OpenCL (and CUDA) on macOS is deprecated.

For OpenCL on Windows, you need Cygwin. Select the gcc-core and libOpenCL-devel packages when installing Cygwin (does Nvidia need libOpenCL-devel?). For Cygwin, gcc needs */cygdrive/c/Windows/System32/OpenCL.DLL* as an argument (instead of the usual *-lOpenCL* option). To get a printf() in an OpenCL kernel to work in Cygwin, I had to add C:\cygwin64\bin\ to the Windows Path then run via the usual cmd.exe.

Nvidia on Windows has a weird thing for OpenCL (maybe CUDA too). A printf() inside an OpenCL kernel needs *llu* for ulong (*lu* only prints 32 bits), but, even though this is OpenCL and *lu* works on Windows with Intel and AMD GPUs, needing *llu* is a typical Windows thing I guess. On second thought, a long integer is always 64-bit in OpenCL, so this behavior by Nvidia is actually a bug. Luckily, *llu* works for Nvidia-on-Linux and for a couple other situations, so I just switched all my OpenCL kernels to use *llu*. Anyway, I wonder if PRIu64 works in OpenCL for all devices and platforms. 

At any point, please ask me if anything here is unclear! I achieved my codes by communicating with various people, so please do the same!

I am curious how these codes will run on ARM CPUs. Nvidia Jetson has Nvidia GPUs running on ARM. I wouldn't expect the OpenCL or CUDA kernels to be effected by the CPU, but how will the host code run? How will my CPU-only codes run on Apple's M1 chips, which are ARM? Is gcc's \_\_builtin_ctzll(0) still undefined?


## There are two good algorithms

I call one algorithm the "n++ n--" algorithm (or, in my filenames, just "npp"). This is fastest for finding the max n...  
[http://pcbarina.fit.vutbr.cz/path-records.htm](http://pcbarina.fit.vutbr.cz/path-records.htm)  
[http://www.ericr.nl/wondrous/pathrecs.html](http://www.ericr.nl/wondrous/pathrecs.html)  
The algorithm is described in a great paper and corresponding GitHub code [1]. Unlike his codes, I currently do not try to find the max n in my code, but adding this capability would be very easy.

I call another algorithm the "repeated k steps" algorithm. This is fastest for testing the Collatz conjecture (and for counting steps to 1). The algorithm is described in [2]. The "n++ n--" can still be used for creating sieves and lookup tables, though, for CPU-only code, especially 64-bit-unsigned-integer code (as opposed to 128-bit), it is not faster because it has to keep making sure it doesn't go too many steps.

[1] Bařina, David. (2020). Convergence verification of the Collatz problem. The Journal of Supercomputing. 10.1007/s11227-020-03368-x.  
[https://rdcu.be/b5nn1](https://rdcu.be/b5nn1)  
[https://github.com/xbarin02/collatz](https://github.com/xbarin02/collatz)

[2] Honda, Takumi & Ito, Yasuaki & Nakano, Koji. (2017). GPU-accelerated Exhaustive Verification of the Collatz Conjecture. International Journal of Networking and Computing. 7. 69-85. 10.15803/ijnc.7.1_69.  
[http://www.ijnc.org/index.php/ijnc/article/download/135/144](http://www.ijnc.org/index.php/ijnc/article/download/135/144)



## There are many already-known math tricks for greatly speeding up either of the above algorithms

Just reduce n below starting number. By not reducing to 1 and just reducing below the starting number, you only need to do 3.492652 steps on average, which is a *vast* improvement over the 4.8 log2(N) steps of reducing to 1. What I mean by the starting number is the 9 in the following example...  
9 -> 14 -> 7 -> 11 -> 17 -> 26 -> 13 -> 20 -> 10 -> 5 -> 8 -> 4 -> 2 -> 1  
Once you get to 7, you stop because it is less than the starting 9. Just 2 steps are needed instead of the above many steps. Then, when you test the starting number of 10, it immediately reduces to 5, so you move to 11. Of course, a good sieve would prevent 9, 10, and 11 from ever being tested, but you get the idea. For this to work, you must start with testing 1, then, as you increase, you can't skip anything not excluded by a sieve.

Use a 2^k sieve! As k increases, the fraction of starting numbers you have to test more-or-less exponentially decreases. This is a very standard thing to do. For example, a 2^2 sieve only requires you to run numbers of the form  
A 2^2 + 3  
because all other numbers reduce after k=2 steps. So you only need to test 25% of the numbers!  
In general, a 2^k sieve considers starting numbers of the form  
A 2^k + B  
where B < 2^k, and only certain B need to be run.  
The 2^k sieve also has the advantage of allowing you to make lookup tables for pre-calculating the first k steps of the remaining starting numbers. For each B, calculate its value after k steps, fk(B), while counting the number of steps that are increases, c, then A 2^k + B after k steps is  
A 3^c + fk(B)  
This speeds things up if you run a bunch of A values using this same fk(B) and c.

For much larger k, I exclude about 25% more numbers than some people by also removing numbers that join the paths of smaller numbers in k steps (the number of steps that are increases must also be equal when the numbers join).  

Use a 3^1 or 3^2 sieve! A nice optimization is to not run n%3==2 numbers because n = 3N + 2 always follows the already tested 2N + 1 number (note that 2N+1 is all odd numbers). It is important that 2N + 1 is less than 3N + 2. This is called a 3^1 sieve.  
A better sieve can be to also exclude n%9==4 numbers. This is a 3^2 sieve. Any n = 9N + 4 number always follows the already tested 8N + 3.  
There are tons of these rules, but almost all exclusions occur with the above rules. For example, all mod 3^9 rules will block only a few percent more n than a 3^2 sieve, but you won't gain because checking against these rules takes time.

Just because these are known does not mean that modern codes are taking advantage of them! For example, the BOINC project by Jon Sonntag is not good at all, as I explain here: [https://boinc.berkeley.edu/forum_thread.php?id=14159](https://boinc.berkeley.edu/forum_thread.php?id=14159)



## There are various computational tricks to speed things up

64-bit integers, uint64\_t, aren't nearly large enough. 128-bit integers are! Overflow only very rarely occurs when using 128-bit integers. Since you'd have to check for overflow even if using 256-bit integers, use 128-bit integers! Just use Python 3 or the GMP library on numbers that overflow, and *only* on numbers that overflow.

When checking for overflow, use lookup tables for (~(\_\_uint128\_t)0) / 3^c instead of calculating these each time.

For some reason, CPU-only code is fastest using a 3^2 sieve, but GPU code is faster using a 3^1 sieve.

For the 3^1 or 3^2 sieve, you'll need to calculate the modulo operation. There are fast tricks for doing this on 128-bit integers!

For some reason, CPU-only code is fastest using \_\_uint128\_t, but GPU code is faster when implementing by-hand 128-bit integers.

When coding on GPU, be very mindful that all threads in the work group must wait at a line of code for the slowest thread to get there. Because of this, when checking the 3^1 sieve, instead of using a *continue* (which would cause that thread to wait on other threads), have a small loop that increases A until a suitable number is found. Also, the *goto even* I use in CPU-only code is not at all helpful on GPU.




## My "sieveless" approach speeds things up much more!

People save their 2^k sieves to storage drives, so lack of storage prevents huge sieves. Since the fraction of starting numbers you have to test more-or-less exponentially decreases as k increases, huge sieves are greatly desired, causing people to use clever compression tricks.

My "sieveless" approach does not save the sieve to a storage drive, so *huge* sieves can be used! Each computational task is given a small segment of the huge sieve to generate then test *many* A values.

To generate the sieve segments quickly and without requiring *huge* amounts of RAM, realize that not all paths can join. Only "nearby" paths can join. As k gets larger, more distant paths can join, but still within a k-dependent distance. If you look in my code, you'll see these distances called deltaN. For k=34, deltaN = 46. For k=40, deltaN = 120. Using deltaN, I can create any 2^k sieve using very little RAM! **collatzSieve2toK_FindPatterns.c** is my code that experimentally finds deltaN (among other things) for any k. Note that this code takes over a day to run k=40.

But, to experimentally find deltaN, you would still need all 2^k values stored in RAM because you don't know deltaN yet. If you read the comments at the top of collatzSieve2toK_FindPatterns.c, you see that deltaN can be bounded by 2^(k - minC), where minC > k / log2(3) is the lowest c that does not include already-ruled-out numbers, where c is how many increases are encountered in k steps (steps and "already-ruled-out n" are defined in the code's comments). Confused? Just read the copious comments at the top of the code! I was proud of myself for figuring this out, but, even with this bound, for k=40, putting 2^(40 - ceiling(40/log2(3))) into WolframAlpha gives a bound of 16384. This solves the issue with RAM, but CPU time is going to take almost half a year. The code's comments mention a code I use to get a tighter bound, and **collatzFindDeltaNbound.c** is this tricky code. I developed the algorithm myself, and I wonder if anyone else has done this. Read the comments at the top with pen and enough paper to figure out how the math of it works! For k=40, the tighter bound is just 679, which can be run in a week by collatzSieve2toK_FindPatterns.c to find deltaN = 120.

For larger k, I put my GPU and many CPU cores to work using OpenCL and OpenMP! Run **collatzSieve2toK_FindPatterns_GPU.c** and **kernel.cl**.

See my **largeK.xlsx** for all the results of these and other codes.

Armed with deltaN, we are ready to implement!

Instead of having a task ID that you increment each run, you will have two task IDs: the first controls a huge consecutive amount of numbers such as 9 × 2^67, and the second refers to a chunk of these 9 × 2^67 numbers. Only increment the first when the second has reached its final value. The second task ID determines which segment of the very large sieve will be produced. The total size (9 × 2^67 in this example) should be chosen carefully because it would be a shame to have to leave a chunk unfinished. The idea is that you make the 2^k sieve as large as possible while keeping the sieve generation time negligible compared to time running numbers using that chunk of the sieve.

For the GPU code, we need two kernels. Kernel1 will generate the part of the sieve determined by the second task ID. Kernel2 will test 9 × 2^(67 - k) numbers for each number in the sieve (this number should be large to not notice the extra time needed to generate the sieve live). Luckily, most of what kernel2 needs as inputs are the same as what kernel1 creates as outputs, so the GPU RAM can mostly just stay in the GPU between the kernels! This allows kernel2 to start with pre-calculated numbers! The "repeated k steps" code needs another kernel, kernel1_2, to make the 2^k2 lookup table for doing k2 steps at a time.

Challenges with this "sieveless" approach...
* Getting exact speeds (numbers per second) is difficult because each task has a different part of the sieve, but who cares?
* The only real con is that you must finish running each of the second task ID for the results to be valid (or to have any useful results at all).

While running various tests on my GPU code to see if it was working correctly, I discovered something interesting: my hold[] and holdC[] were never being used, yet I was always getting the correct results! So, for k=34, I searched for numbers of the binary form ...0000011 that did not reduce but joined the path of smaller numbers (because these are the numbers that would require hold[] and holdC[]), but I didn't find any! I only found (many) numbers of the form ...1011. I could then test hold[] and holdC[] code using the very unrealistic settings: TASK_SIZE = 3 and task_id = 120703 (and k=34). This is interesting because maybe I could prove that, if TASK_SIZE is large enough, hold[] and holdC[] are unnecessary!

A larger k (and therefore TASK_SIZE) might be better even if a larger fraction of the time will be spent making the sieve. If you get get a 10% faster sieve at the cost of creating the sieve taking 5% of the time, you still win!

Here are the steps to setting your parameters...
1. Choose an amount of numbers that you could commit to finishing in a month or whatever your timescale is. If 9 × 2^68, set TASK_SIZE_KERNEL2 in the GPU code and TASK_SIZE0 in the CPU-only code to be 68 (you may wish to remove some 9's in the code as explained in the code).
1. Then set k and deltaN_max to maximize the numbers per second! If you set k too large, then the code spends too large a fraction of time generating the sieve. I have found that the very large deltaN only occur very rarely, so deltaN_max need not be huge. If your TASK_SIZE_KERNEL2 and TASK_SIZE0 are not large enough and you find that the fastest k is something small like 35, you may wish to not use this sieveless approach and just use a sieve that you store on your storage drive. See my spreadsheet, largeK.xlsx, for good k values to run.
1. Choose the amount of numbers that each task should test—let's say 9 × 2^40—then set TASK_SIZE = 40 + k - (TASK_SIZE0 or TASK_SIZE_KERNEL2). Each task will look at a 2^TASK_SIZE chunk of the sieve to test 2^(TASK_SIZE0 - k) or 2^(TASK_SIZE_KERNEL2 - k) numbers per number in the sieve. Because there is apparent randomness to the sieve, TASK_SIZE should at least be 10 to give each process something to run! Unlike the CPU-only code, which needs very little RAM, TASK_SIZE in the GPU code determines RAM usage, and you want it large enough to minimize your CUDA Cores sitting around doing nothing.

If you want to run both the GPU and CPU-only code simultaneously, be sure to use the same sieve size for both, and have the size controlled by the first task ID be the same for both. I would normally say that TASK_SIZE needs to also be the same, but, if the CPU-only tasks need to finish in a certain time, it may be best to set TASK_SIZE differently: the CPU-only should have a lower one. This would require just a bit of organization when setting the second task ID to make sure that the tiny CPU-only tasks fit around the GPU tasks and to make sure that all of the sieve is eventually run.

If you have already have up to some number tested by a non-sieveless approach, you can still easily use this sieveless approach! For my CPU-only code, adjust aStart manually. For my GPU code, adjust h_minimum and h_supremum in kernel2, keeping in mind that integer literals larger than 64-bit don't work (so use bit shifts). If you do this, be sure to change the "aMod = 0" line!

If you want to use general GPUs...
* For computers with watchdog timers, a maxSteps variable must be introduced, and a message should be printed whenever testing a number goes beyond those steps. Having a way to detect an infinite cycle or a number that is slowly making its way to infinity is crucial.
* For computers with watchdog timers, you will likely have to modify the code to call kernel2 multiple times, each time with a different h_minimum and h_supremum.
* To accommodate GPUs (and CPUs) without ECC RAM, you'll have to run each task twice to validate.
* The 128-bit integers should become OpenCL's uint4 to prevent arithmetic errors on non-Nvidia GPUs. I believe that ulong arithmetic should never be used on Intel or AMD GPUs. Another solution could be to look at my testBug64.c and testBug128.c to understand the bugs, then test each device for these bugs before running the Collatz code. Update: I have now discovered that whether or not certain GPUs return a correct checksum is not perfectly correlated with the bugs in those two test files, so, before allowing a GPU to contribute, I'd also run some test numbers on each GPU to make sure the correct checksum is created!
* To accommodate weaker GPUs, set TASK_SIZE to be smaller.

To find the largest number or to calculate checksums, you can easily edit kernel2 to output some extra things. To see how, search [David Bařina's kernel](https://github.com/xbarin02/collatz/blob/master/src/gpuworker/kernel32-precalc.cl) for mxoffset, max_n, and checksum_alpha. Of course, something similar can be done for my CPU-only code. I don't expect this to greatly slow down the code. I temporarily added the checksum code to my CPU-only and GPU codes, and the checksums matched those of David Bařina's codes.

If finding the max number and using a 2^k sieve, you have to be careful that the max number does not occur in the first k steps. That is, your sieve cannot be too large. Even after k increases, the number will get approximately (3/2)^k times larger requiring approximately log2(3/2) k more bits. From this list of max numbers, [http://pcbarina.fit.vutbr.cz/path-records.htm](http://pcbarina.fit.vutbr.cz/path-records.htm), finding a maximum number in these k steps would be difficult to do since even a k=80 sieve is very safe (unless you start testing up to 2^89 and no new new records are found!). 

Note that Nvidia has various OpenCL bugs that cause CPU busy waiting (100% usage) while waiting for the GPU to finish. CUDA does not have these issues. See my "partially sieveless" OpenCL host codes for an attempt I made at getting Nvidia to stop busy waiting for kernel2, but a printf() in kernel2 prevents my GPU from being fixed.



## My "partially sieveless" codes are actually what you should use!

I learned that nearly all of the numbers removed by a deltaN (after checking for reduction in no more than k steps) are from deltaN = 1!  
For k=21, 38019 numbers need testing using if using all deltaN, 38044 need testing if only using deltaN=1, and 46611 need testing if not using any deltaN.  
For k=32, 33880411 numbers need testing using if using all deltaN, 33985809 need testing if only using deltaN=1, and 41347483 need testing if not using any deltaN.  
See largeK.xlsx for numbers that need testing if only using deltaN = 1. I now recommend to set deltaN_max to 1! This allows for a larger k! In fact, I have hardcoded a deltaN_max of 1 into my "partially sieveless" codes.

I once had the idea of saving out a 2^32 sieve that a "partially sieveless" code could load in to help generate a much larger "sieveless" 2^k sieve. My GPU approach for creating a sieve is to do k steps at a time for all numbers, so, for large deltaN, I'd still have to do k steps for all numbers anyway. My CPU-only approach would benefit, but I didn't think it would benefit enough to be revolutionary. So I settled for only using a 2^2 sieve to help generate a much larger 2^k sieve. However, a deltaN-equals-1 2^k sieve could use this 2^32 sieve very nicely! For each B in the 2^k sieve, do k steps for B and B-1. In fact, since the 2^32 sieve uses all possible deltaN values, doing k steps for B-1 (that is, using deltaN = 1 instead of deltaN = 0) for the numbers in the 2^32 sieve barely helps, so you may wish to change the code to deltaN = 0. This "partially sieveless" code should now greatly speed up sieve generation allowing for larger k values! Also, for GPU code, TASK_SIZE could be increased more without running out of RAM. I realized that this using-multiple-sieves idea could be great for deltaN=1 when Eric Roosendaal was telling me about how he uses a 2^8 sieve (which only requires 16 numbers to be run), to create a larger sieve.

I will call these hybrid codes "partially sieveless" because the smaller sieve of size 2^k1 is saved to disk, so it's *not* "sieveless" (that is, it's not generated as you go). However, the larger 2^k sieve will be "sieveless" (that is, it is generated as you go).

I mentioned k1 = 32 because a 2^32 sieve can be created in a couple minutes by a single CPU core. The only con of a larger k1 is that the sieve file is harder to create, transfer, and store. Because a buffer is used to load only parts of the 2^k1 sieve, a larger k1 does not use more time or RAM to use. Once you have a large-k1 sieve file, use it! Especially good values for k1 are  
..., 24, 27, 29, 32, 35, 37, 40, ...  
See my largeK.xlsx for why I say this.

Almost everything mentioned in the previous section about "Sieveless" code still applies. Read it!

On CPU-only, creating the 2^k sieve is vastly faster with this approach! Even though deltaN = 1, the amount of numbers tested by a "partially sieveless" or an any-deltaN "sieveless" code are basically the same partly because the 2^k1 sieve uses any deltaN. There is no need for me to provide speeds because the speeds of a k=51 sieve will be the same as with the "sieveless" approach. The difference from "sieveless" code is that we can now run larger k values! Or, to think if it another way, use the same k value, but commit to a much smaller TASK_SIZE0 or TASK_SIZE_KERNEL2.

On GPU, "partially sieveless" is many times faster than the "sieveless" code for making the sieve, but it doesn't get the extreme speedup that the CPU-only sees. Perhaps this is because, for "partially sieveless", the different threads in the work group can test a very different number of numbers (the threads in the same work group must wait on the thread that takes the longest time). Note that the "sieveless" code spends most of the time to make the sieve on the CPU (when using my usual Nvidia device), so a very fast GPU would prefer my "partially sieveless" code even more. Feel free to use "partially sieveless" code for CPU-only and "sieveless" for GPU (or vice versa)! If you do this, as previously discussed, just be sure to use the same k and same TASK_SIZE0 or TASK_SIZE_KERNEL2 (different k1 or k2 values are just fine). Also, for large TASK_SIZE, my "partially sieveless" code uses about 9% of the RAM compared to my "sieveless" code!

As for how to test the validity of this code, I temporarily added the previously-mentioned checksum code. I tested the 2^k1 sieve by removing the 2^k code that does the first k steps, and I tested the 2^k sieve by removing the code that checks against the 2^k1 sieve. When comparing to my "sieveless" codes, keep in mind that 2^k1 uses any deltaN, but the 2^k sieve uses deltaN = 1.

### CUDA and making 128-bit integers by hand

I was curious to try Nvidia's CUDA instead of OpenCL, which only works on Nvidia GPUs. Learning CUDA can be useful because (1) it might be faster, (2) it sure is easier to code, and (3), on Nvidia GPUs, CPUs don't have to busy wait (at least on some GPUs, Nvidia using OpenCL currently causes CPU busy waiting due to an Nvidia printf-in-kernel bug). CUDA currently has no native support for 128-bit integers in device code, so CUDA requires that I find my own 128-bit integer class. Learning to do 128-bit integers can be useful because (1) not all OpenCL implementations have a 128-bit integers and (2) one could perhaps speed up the Collatz algorithm by optimizing this 128-bit-integer code for Collatz.

I obtained 128-bit integers in CUDA via this GitHub project (has a GPL license). The idea is to download a single file into your current folder, and put...  
\#include "cuda_uint128.h"  
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

Using by-hand uint128_t is faster than native \_\_uint128\_t on GPU, which was surprisingly, so trying CPU-only code that does uint128_t seems like a good idea. Partly because cuda_uint128.h has limitations (for example, cannot divide by uint128_t), my goal is always to use the minimal amount of by-hand code, so, on GPU, I only changed the kernel code. For OpenCL, since some of my devices don't like the native 128-bit implementation (won't compile or will give arithmetic errors), all 128-bit integers in kernels was made to be by-hand. For CUDA, I had to change all the code in the kernels because there is no alternative. For CPU-only code, I just need everything in the main loop (over patterns) to be by-hand, which **collatzPartiallySieveless_npp_128byHand.cpp** does. On my I5-3210M CPU, 128byHand is slower, though maybe ARM CPUs would be different?


## non-Nvidia GPUs

I have had minimal success getting this OpenCL code to work on my old Intel HD Graphics 4000. I could only get the code to "successfully" run on macOS in that it would find the correct numbers that would overflow, but it consistently returned an incorrect checksum (I temporarily added some code to calculate checksums), perhaps due to the bug described in my testBug64.c (or a similar bug!).  
*Update*: I was using another old Intel GPU (on Windows) that had the 64-bit bug, but it was returning the correct checksum! Does this guarantee that the 64-bit bug will never cause problems? Heck no, especially since I wasn't running huge numbers! Does this mean that there are other problems with the Intel HD Graphics 4000 on macOS? I'd say so! I was playing around with the Intel HD Graphics 4000 on macOS, and simply commenting out a printf() I added in kernel2 would change the checksum!

Here are things I typically had to do to get things to work...
* In kern2, I always had to move const struct uint128_t UINT128_MAX = {~(ulong)0, ~(ulong)0} into the actual kernel.
* For my Intel HD Graphics 4000, I often had to change all instances of -cl-std=CL2.0 to -cl-std=CL1.2 in the host code. Simply not specifying the OpenCL version also works!
* I think I always have to get rid of all goto statements in the kernels. I don't really know if this is always required because only my macOS code actually runs successfully! For sure, macOS didn't like certain goto statements in the kernels (though perhaps just goto statements that I no longer use for any of my codes). Because I couldn't figure out why these goto statements were creating issues, I decided to get rid of all goto statements. Getting rid of goto statements wasn't super hard: (1) several goto statements can be replaced with logic using variable R, and (2) the rest of the goto statements can be replaced with logic of an int you create (I called mine go or stop, initialized to 1 or 0).
* Using Beignet on Linux, I had to change %llu to %lu in the kernels (as previously discussed).
* On macOS, I had to write my own ctz() function! I shouldn't have to do this!
* For my Intel HD Graphics 4000, the variable TASK_SIZE_KERNEL2 must be reduced because my GPU has a watchdog timer of about 5 seconds. A better fix would be to change my kern2 to run only a fraction of the h values, then I could call clEnqueueNDRangeKernel() multiple times so that each call would take less than 5 seconds, so this is how I did it!

I tried my AMD Radeon R5 (Stoney Ridge) GPU on Windows next (I don't think there are good Linux OpenCL drivers for it?). The -cl-std=CL2.0 is required (specifying nothing doesn't work), the goto statement is not allowed in kernels (see above list for how to change this), and changing %llu to %lu in the kernels is required. It gave the correct checksum! This GPU also passes the testBug64.c test (though it fails the testBug128.c test), so I'm not surprised it works. Also, the GPU doesn't seem to have a watchdog timer, which is great!
For running high TASK_SIZE_KERNEL2 values, the code (kern2) would sometimes just never complete! The problem is perhaps not a watchdog timer because I made a test kernel that successfully ran for many minutes. This is very strange because, as far as I can tell, the only resource that a larger TASK_SIZE_KERNEL2 requires is more time. Setting TASK_SIZE lower helps, as did raising clEnqueueNDRangeKernel()'s local_work_size argument. I was worried that I had a coding error, but testing (manually editing indices[]) eventually revealed that running the same numbers but with multiple smaller kernel executions worked perfectly. I believe there to be a problem with the GPU or its drivers because I often run this GPU on BOINC, and it will too often take a very long time to run a task, then time out, and the returned task is then found to be invalid. But I feel that I have succeeded in getting the code to run on a non-Nvidia GPU! When errors do occur on this GPU, at least their occurrence is not hidden. Instead of using clFinish(), I could use the commented out loop with usleep() to enforce a time limit.  
*Update*: I was using another old Intel GPU (on Windows) that clearly had a watchdog timer, but this timer would either cause clEnqueueReadBuffer() to fail, or, for longer tasks, would cause the kernel to never complete, just like my AMD would do! I now believe that my AMD has a watchdog timer, but it is more complicated than stopping a kernel after a certain amount of time! Just like the AMD GPU's issue with BOINC, this other old Intel GPU would too often get stuck then time out on BOINC!

I reduced k1 from 37 to 27 because k1=27 is much easier to generate and copy. However, when I first made this change, my Intel HD Graphics 4000 now started always giving the correct checksums (in spite of its 64-bit arithmetic errors), but my AMD GPU started behaving much worse (doesn't freeze only when CHUNKS_KERNEL2 is set just right). Since k1 truly should have no effect, I conclude from this bullcrap that cheap non-Nvidia GPUs always need to have checksums checked against an Nvidia GPU or CPU-only code.

See my partiallySieveless_nonNvidiaGPU/ folder for the code. I used what I learned in the above two paragraphs to make it. Depending on your GPU, you may need to set g_ocl_ver1 to 1 for it to work. I permanently added checksum code. I have also added a check to see if 64-bit arithmetic has the testBug64.c bug. For GPUs with a watchdog timer, kern2 can now be called in a loop (just change CHUNKS_KERNEL2 variable), and kern2 checks for numbers that exceed a maximum number of steps. You will still need to first create a sieve using collatzCreateSieve.c (found in partiallySieveless/ folder). Default needed sieve is a 2^27 sieve.

For "repeated k steps" with its current parameters, running ./a.out 0 0 should return a checksum of 8639911566 for TASK_SIZE_KERNEL2 = 60 and a checksum of 1105908468282 for TASK_SIZE_KERNEL2 = 67. Running ./a.out 26 71353562 should return an overflow for TASK_SIZE_KERNEL2 = 60, and running ./a.out 0 71353562 should return an overflow for TASK_SIZE_KERNEL2 = 67.

For "n++ n--" with its current parameters, running ./a.out 0 0 should return a checksum of 5574358187 for TASK_SIZE_KERNEL2 = 60 and a checksum of 713529553875 for TASK_SIZE_KERNEL2 = 67. Running ./a.out 5 130502703 should return an overflow for TASK_SIZE_KERNEL2 = 60, and running ./a.out 0 130502703 should return an overflow for TASK_SIZE_KERNEL2 = 67.

Not surprisingly, this code runs successfully on Nvidia too! Though I suppose that an Nvidia GPU on Windows would need all the %lu in kernels changed to %llu due to the Nvidia Windows bug. Keep in mind the Nvidia bug where you can get CPU busy-waiting when using OpenCL with Nvidia; my CUDA codes would never cause CPU busy waiting.



## Things that may or may not be minor improvements to my codes

If you enjoy the details of coding, please improve my code!
1. I am still somewhat confused about how caching on GPUs works with OpenCL. For my "repeated k steps" code, there may be a better way to get the lookup table (arrayLarge2[] in kernel2) into a faster cache.
1. In kernel2_npp.cl, I have some code commented out that pre-calculates a variable, threeToSalpha, because using threeToSalpha slows down the code for some reason. Why? Would threeToSalpha slow down CUDA code also?
1. On a GPU, why does a 3^2 sieve run slower than a 3^1 sieve? On a CPU, the 3^2 sieve is faster than a 3^1 sieve. I never compared speeds of 3^1 vs 3^2 for any "repeated k steps" code or any CUDA code.
1. In all of my kernel2s, I calculate N0Mod from aMod, bMod, and cMod, where aMod increases by 1 each iteration. However, this could be sped up. If cMod is 1, then N0Mod will simply increase by 1 each iteration. If cMod is 2, then N0Mod will simply decrease by 1 each iteration. Note that cMod is either 1 or 2. Since cMod only depends on SIEVE_LOGSIZE, an *#if* could be used to choose the algorithm at compile time. My CPU-only codes could do the same thing but the variables are now called nMod, aMod, k2Mod, k, etc.
Also, in all my kernel2 (or kern2) files, I for some reason calculate N0 each iteration while finding a good N0Mod value for each thread in the work group. My CPU-only codes do it better by calculating the start value after checking the modulus!
1. In my "partially sieveless" GPU codes, I set sieve8[] to be in constant GPU RAM simply because of an online example I was looking at. I don't believe this was the best choice, and maybe also copying to shared/local RAM would help, but I also don't think it really matters!?
1. In many of my kernels, I calculate the powers of 3 and save them into shared/local RAM. Is there a better way? I also do this calculation using only 1 thread in the group of threads that is sharing the RAM (while the other threads wait), but I don't see why I couldn't use more threads! Currently, each power of 3 is calculated from scratch instead of multiplying the previous value by 3.
1. In some of my kernels, I set alpha and beta variables to size_t since they are used to index arrays, but would it be faster to use 32-bit integers?
1. I always wonder if I've optimized the very-low-level stuff. For example, in kern2_128byHand.cl, I do *mulEquals(N, (ulong)lut[alpha])*, but this actually might cause *(ulong)lut[alpha]* to be calculated more than once since mulEquals() is a macro. I'm not about to dig into the assembly code to see how the compiler optimizes this. Would it be better to get *(ulong)lut[alpha]* before calling mulEquals()? I'm sure there are lots of possible low-level optimizations for either my CUDA or OpenCL by-hand implementations of 128-bit integers, perhaps these.
1. For my "n++ n--" kernel2 codes, I should try removing the loop around the code that increases the number according to the number of trailing zeros (the loop between the "n++" and the "n--"). If I do this, I should also remove the loop around the decreases. For my 128byHand codes, I don't think removing these loops may not be possible because bit shifting by 0 is undefined in my 128byHand codes.



## Many thanks to...

David Bařina was extremely helpful in helping me understand the math! Also, I greatly appreciate his code being on GitHub.

Brad Philipbar let me SSH into his fantastic machine so that I could use an Nvidia GPU!

A couple conversations with Eric Roosendaal proved very helpful!

I also want to anonymously thank a couple people for letting me talk about Collatz stuff, helping me come up with new ideas!

