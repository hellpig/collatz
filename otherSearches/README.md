# Other types of searches

In addition to testing the Collatz conjecture, why search for other things like records for max n or max steps? Statistics of max n from a random walk can be predicted, so I see why max n is searched for (so far, experimental data is consistent with a random walk). Also, on a practical level, the max n affects how many bits are needed to test the Collatz conjecture. As for max steps, perhaps these records can be predicted from various stochastic models? Certainly, max n is easier to find than max steps!

I found a website with a list of their results of various other searches, [http://www.ericr.nl/wondrous/#status](http://www.ericr.nl/wondrous/#status). In addition to searching for max n and max steps to 1 (where (3n + 1)/2 is counted as 2 steps), here are the other searches they have done...
1. ["Glide" records](http://www.ericr.nl/wondrous/glidrecs.html) are provided (N=2 is missing), where "glide" is the number of steps needed to reduce an n below its start value (where (3n + 1)/2 is counted as 2 steps).
1. [First n to take N steps to reduce to 1 for all N](http://www.ericr.nl/wondrous/classrec.html) are provided (where (3n + 1)/2 is counted as 2 steps). Instead of asking for the first n to take more steps than any previous n required, they are wondering, given any N, which is the first n to require exactly that many steps. Note that my sieveless "reduce to 1" approach can naturally find these results, but a 3^1 or 3^2 sieve can no longer be used.
1. [Ratios involving counts of odd and even n's when reducing starting n to 1](http://www.ericr.nl/wondrous/comprecs.html) are provided (where (3n + 1)/2 is counted as 2 steps), though N=2 is missing. This is clearly extremely related to [max-step records](http://www.ericr.nl/wondrous/delrecs.html) because numbers that require many steps for their size have a higher ratio of odd to even, whereas a number that reduces quickly for its size would be produce mostly even numbers. In fact, all records in the first table of results are found in the second one.
1. ["Residue" results](http://www.ericr.nl/wondrous/residues.html) are provided, where residue is defined using the counts of odd and even n's when reducing starting N to 1 (where (3n + 1)/2 is counted as 2 steps). The final 1 is not counted as an odd, and the initial n counts as an even or an odd. Residue is then defined as (2^evens / 3^odds) / N. I think the reason we care about this is that, if the Collatz conjecture didn't have the the "+1" in "3n + 1", N = 2^evens / 3^odds would be true for numbers that reduced to 1 (assuming that such an N would be an integer), and the residue would be 1. Since the "+1" only significantly matters when n is small, the residue's difference from 1 is measuring how long n is putzing around at small values before reaching 1. Note that, if residue were always equal to 1, paths would never join other paths, and nothing interesting could happen.
1. Just found another one: [strength records](http://www.ericr.nl/wondrous/strengths.html). It wasn't linked on the [main list](http://www.ericr.nl/wondrous/index.html#status), but I found it. Strength is defined as 5\*odds - 3\*evens for when (3n + 1)/2 is counted as two steps. Strength has to do with how long a number can "tread water" compared to dropping down to 1. While "treading water", I find that strength is approximately equal to 0.245 \* odds, where odds increases as the treading continues, but then any subsequent drop to 1 will reduce the strength. All strength records are delay records, but the strength records are the delay records that make a particularly high delay for the starting n.

For the last three, the website makes some conjectures that are interesting, so I understand why they are tested, but I don't really understand why the first two are tested. In general, I do not understand why people are experimentally counting steps to reduce to 1, perhaps this is because I do not know much about the theory behind trying to prove the Collatz conjecture. Perhaps the only reason is that getting experimental data can guide the mathematical theory if something surprising is found!

Anyway, I mention all this for two reasons. The first is to maybe have someone explain to me why we care about counting steps to reduce to 1. The second is to have a complete list of experimental searches so that I can evaluate if my "sieveless" approach can be applied to them. For some of the above searches, I see certain cases where some form of a sieve can be used. I might give this more thought at some point.



## One thing that would interest me is to see a rolling average of the "glide"

I take "glide" to be defined as the number of steps to reduce a number below where it started (as the website defined it).

I would expect this to be more-or-less constant as the starting n increases! Not many tricks could be used: no sieves, no "n++ n--", and no "repeated k steps". Though maybe "n++ n--" or "repeated k steps" could be used up until just before n reduces below its start value. For simplicity, I would just average the glides in each consecutive 2^30 block of numbers. I would define (3n + 1)/2 as a single step because then the glide is useful because it then tells you which sieves will exclude that number. Better yet, define a step as a bit shift so that the "n++ n--" algorithm can be used, and **collatzCPUglide_BitShift.c** is my code that does this! For the following chunks of 2^30, I get the average glide...  
  0: 2.246348  
  1: 2.246315  
  2: 2.246323  
  3: 2.246313  
  4: 2.246330  
  5: 2.246352  
  6: 2.246339  
  7: 2.246338  
  8: 2.246330  
  9: 2.246311  
  1000: 2.246329  
  1000000: 2.246361  
  1000000000: 2.246318  
  1000000000000: 2.246360  
  1000000000000000: 2.246339  
  1000000000000000000: 2.246354  
  
The last chunk of 2^30 runs numbers 2^30\*1000000000000000000 + 2 to 2^30\*1000000000000000001 + 1.

I would now like to predict the above average and standard deviation from a random walk. In trying this, I see that defining (3n + 1)/2 as a single step is the easier way to go, so **collatzCPUglide.c** is my code that finds experimental values when (3n + 1)/2 is a single step! For the following chunks of 2^30, I get the average glide...  
  0: 3.492694  
  1: 3.492627  
  2: 3.492635  
  3: 3.492623  
  4: 3.492647  
  5: 3.492700  
  6: 3.492652  
  7: 3.492654  
  8: 3.492659  
  9: 3.492626  
  1000: 3.492654  
  1000000: 3.492710  
  1000000000: 3.492637  
  1000000000000: 3.492715  
  1000000000000000: 3.492683  
  1000000000000000000: 3.492708
  
Note that, if counting (3n + 1)/2 as two steps, my results become 3/2 larger.

My code that predicts the above from a random walk is **collatzGlide.c**, which gives 3.492652 with a standard deviation of 6.481678. Since I'm running 2^30 numbers, the [central limit theorem](https://sphweb.bumc.bu.edu/otlt/mph-modules/bs/bs704_probability/BS704_Probability12.html) says that my experimental results, if from a random walk, should have a standard deviation of 6.481678 / sqrt(2^30) = 0.00020 between them. Seems that the Collatz conjecture is consistent enough with a random walk! Though, I am surprised at how similar all the glides are considering that, if running chunks of 2^1 instead of 2^30, there would be strong oscillation in the glide because n%4 == 3 are the only numbers that do anything interesting. 2^30 averages out enough of the oscillations because a sieve of that size would find that most numbers would reduce in no more than 30 steps.

## Another thing I would like to see is the average of (delay - expectedDelay)

I define "delay" as the number of steps to reduce to 1 (just like the website), and *expectedDelay* is given by a random walk. Unlike the website, I will count (3n + 1)/2 as a single step. Since the glide has already been compared to a random walk, there probably isn't much to gain scientifically from looking at the delay, but writing the code sounds fun. I think the reason I really care is that I am curious to see the expectedDelay values, and **collatzDelay.c** is my code that does this! I get expectedDelay = f(N) log2(N), where f(N) seems to be approaching a number, but I am unable to trust my results when I run N larger than 2^80 due to my steps being limited to 1023 (else overflow). Here are my results...  
  N = 2^10: expectedDelay = 5.030079 log2(N)  
  N = 2^20: expectedDelay = 4.923198 log2(N)  
  N = 2^30: expectedDelay = 4.888867 log2(N)  
  N = 2^40: expectedDelay = 4.871146 log2(N)  
  N = 2^50: expectedDelay = 4.860508 log2(N)  
  N = 2^60: expectedDelay = 4.853471 log2(N)  
  N = 2^70: expectedDelay = 4.848431 log2(N)  
  N = 2^80: expectedDelay = 4.844653 log2(N)

Multiply this by 3/2 if you count (3n + 1)/2 as two steps.

I believe the results are approaching  
    expectedDelay = 2 / log2(4/3) log2(N) = **4.818842 log2(N)**  
The above comes from writing N approximately as (4/3)^(delay/2) by building N up by an equal number of "backward" increases and decreases. Each pair of increases and decreases changes N by a factor of 4/3 and takes 2 steps. I want to thank Eric Roosendaal for this formula. My code also predicts a standard deviation of about 3.8 sqrt(expectedDelay).

I decided to write **collatzDelayGMP.c** to use the GMP library to allow me to run *much* larger numbers! Unlike the original non-GMP code, I optimized this code for speed. Here are the results...  
  N = 2^1000: expectedDelay = 4.820915 log2(N), standard deviation = 8.384883 sqrt(log2(N))  
  N = 2^2000: expectedDelay = 4.819878 log2(N), standard deviation = 8.383898 sqrt(log2(N))  
  N = 2^5000: expectedDelay = 4.819256 log2(N), standard deviation = 8.383410 sqrt(log2(N))  
  N = 2^10000: expectedDelay = 4.819049 log2(N), standard deviation = 8.383238 sqrt(log2(N))  
  N = 2^20000: expectedDelay = 4.818945 log2(N), standard deviation = 8.383170 sqrt(log2(N))  
  N = 2^50000: expectedDelay = 4.818883 log2(N), standard deviation = 8.383100 sqrt(log2(N))  
  N = 2^100000: expectedDelay = 4.818862 log2(N), standard deviation = 8.383085 sqrt(log2(N))  
  N = 2^200000: expectedDelay = 4.818852 log2(N), standard deviation = 8.383074 sqrt(log2(N))  
  N = 2^500000: expectedDelay = 4.818846 log2(N), standard deviation = 8.383073 sqrt(log2(N))  
  N = 2^1000000: expectedDelay = 4.818844 log2(N), standard deviation = 8.383070 sqrt(log2(N))

I was curious about the prediction for what the standard deviation is approaching. First of all, for very large N, the standard deviation of a random walk is effectively constant as steps increases during the time when most numbers are reaching 1, giving a standard deviation of ΔIncreases = sqrt(steps) / 2 = sqrt(expectedDelay) / 2. The division by 2 is because our random walk graphed as Increases vs. steps spreads half as fast as the "usual" random walk. Note that most of the walks occur near the line Increases = steps/2 on this graph. The standard deviations listed above are Δsteps = step2 - step1, where step2 = 2 log2(N) / (2 - log2(3)) is when the line Increases = (steps - log2(N)) / log2(3) crosses the line Increases = steps/2, and step1 = 2 (log2(N) - log2(3) ΔIncreases) / (2 - log2(3)) is when the line Increases = (steps - log2(N)) / log2(3) crosses the line Increases = steps/2 - ΔIncreases. This gives Δsteps = ΔIncreases / (1/log2(3) - 1/2) = **8.383068 sqrt(log2(N))**.

As for experimentally testing the above delays, I chose to run chunks of 2^30 numbers. Here is my code **collatzCPUdelay.c** that does this. The expected delay noticeably drifts for a run of 2^30 when starting at 2^40, so I started at 2^50. By the way, 2^100 caused overflow (I suppose this depends on how k2 is set). When starting at 2^50, I got the average delay to be 4.833497 log2(N), which seemed promising! But 2^60 gave 5.020711 log2(N). I then ran 2^60.1 and got 4.587167 log2(N), so there's clearly a huge standard deviation! For running 2^30 numbers, the standard deviation should be 0.00026 sqrt(log2(N)), which should be around 0.002, not approximately 10! If I were to run 2^42 instead of 2^30 numbers, I could reduce the standard deviation by a factor of 2^6 = 64 due to the central limit theorem allowing for a better measurement of the average. 2^42 would take many days to run on a single CPU core using a "repeated k steps" code. I ran the following amount of numbers starting at N = 2^60 giving the following results...  
  running 2^30 numbers starting at N=2^60: delay = 5.020711 log2(N)  
  running 2^31 numbers starting at N=2^60: delay = 5.086590 log2(N)  
  running 2^32 numbers starting at N=2^60: delay = 5.207521 log2(N)  
  running 2^33 numbers starting at N=2^60: delay = 5.281781 log2(N)  
  running 2^34 numbers starting at N=2^60: delay = 5.228182 log2(N)  
  running 2^35 numbers starting at N=2^60: delay = 5.158507 log2(N)  
  running 2^36 numbers starting at N=2^60: delay = 5.172008 log2(N)  
  running 2^37 numbers starting at N=2^60: delay = 5.211181 log2(N)  
  running 2^38 numbers starting at N=2^60: delay = 5.135604 log2(N)  
  running 2^39 numbers starting at N=2^60: delay = 4.937276 log2(N)  
  running 2^40 numbers starting at N=2^60: delay = 4.940875 log2(N)  
  running 2^41 numbers starting at N=2^60: delay = 4.914249 log2(N)  
  running 2^42 numbers starting at N=2^60: delay = 4.897469 log2(N)

Why does the delay have such a large standard deviation compared to a random walk? The glide is an average over oscillations of various size. For example, only n%4 == 3 do anything interesting. The delay does not have these simple patterns! The glide just has to reduce a number, but the delay must "stuff the number down a thinner funnel", which might explain things. I would love to see a rolling average of the delay! I am now interested in counting steps to 1! There are already some graphs [here](https://carleton.ca/andrewsimonslab/?p=247) that show it, but I'm more interested in the patterns that arise.

**graph70_20.png** and **graph90_20.png** are my results from again running 2^30 numbers, but now as 2^10 consecutive groups of 2^20 numbers, starting at 2^70 and starting at 2^90, respectively. Various features such as oscillations and sudden jumps can be seen! Note that the numbers in the graphs should have a standard deviation between data points of about 0.001 if from a random walk.
