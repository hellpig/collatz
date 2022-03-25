# Collatz conjecture: BOINC test

Here are my efforts to make a BOINC server that runs my Collatz code. The following notes are a work in progress that I don't much intend to finish. I currently only am testing CPU-only code on a BOINC server on an Ubuntu laptop on my local network.

Instructions for installing BOINC server...  
[https://boinc.berkeley.edu/trac/wiki/ServerIntro](https://boinc.berkeley.edu/trac/wiki/ServerIntro)

I don't want to use any of the bundled BOINC installers because I'm not convinced that they are maintained or documented correctly, and you end up spending the same amount of time learning about all of their bundling stuff like Docker or whatever. Because they preconfigure everything, you eventually have to change all the settings to how you want them, so just do it the right way (from scratch) from the beginning.

I do want to use example\_app provided by BOINC! I will simply modify it.  
[https://boinc.berkeley.edu/trac/wiki/ExampleApps](https://boinc.berkeley.edu/trac/wiki/ExampleApps)

Helpful documentation for modifying example\_app...  
[https://boinc.berkeley.edu/trac/wiki/CreateProjectCookbook](https://boinc.berkeley.edu/trac/wiki/CreateProjectCookbook)  
[https://boinc.berkeley.edu/trac/wiki/CompileApp](https://boinc.berkeley.edu/trac/wiki/CompileApp)

Choices I make when installing BOINC. I will use these throughout this document, but you should use your own!
* use my own username: brad (not "boincadm")
* my MySQL password: admin
* my project name: awesome (not "cplan"; something like "Collatz" would be more descriptive)
* my administrative web interface (awesome\_ops) username and password: brad admin
* I want to start by using my local network only via a static IP address set on my router: 192.168.0.201

The following was done in 2022.


## setting up the BOINC server

To download and compile BOINC, I followed the BOINC website's instructions...
```
cd ~
git clone https://github.com/BOINC/boinc.git boinc-src
cd ~/boinc-src
./_autosetup
./configure --disable-client --disable-manager
make
```

In addition to the listed dependencies, the following was needed for *make* to work...
```
sudo apt install python-is-python3
```

I had to combine a few of their instructions to setup MySQL...
```
sudo mysql -h localhost -u root -p
> CREATE USER 'brad'@'localhost' IDENTIFIED BY 'brad';
> GRANT ALL ON *.* TO 'brad'@'localhost';
> SET PASSWORD FOR 'brad'@'localhost'='admin';
> quit
```

For my local server, I needed to add the --url\_base option in the following...
```
cd ~/boinc-src/tools
./make_project --url_base http://192.168.0.201/ --db_passwd admin --test_app awesome
```

Before running bin/update\_versions, I remove old versions and 32-bit stuff (leaving 3 64-bit platforms: Linux, Windows, and macOS). My code will not work on 32-bit computers.
```
cd ~/projects/awesome/apps/example_app/
rm -r 24253
cd 22489
rm -r i686-apple-darwin
rm -r i686-pc-linux-gnu
rm -r windows_intelx86
```
Instead of starting at version 242.89, I do
```
cd ~/projects/awesome/apps/example_app/
mv 22489 1.00
```

Just to be sure, whenever updating the Apache server, I run
```
sudo /etc/init.d/apache2 restart
```

After following all of their instructions, I did
```
gedit ~/projects/awesome/html/project/project.inc
```
to add the project name (awesome) and copyright name (brad).

To change the application name from "Example Application" to something like "collatz", change example\_app's "user\_friendly\_name" in project.xml then run
```
bin/xadd
```

On my local network, I could now go to  
[http://192.168.0.201/awesome](http://192.168.0.201/awesome)  
[http://192.168.0.201/awesome_ops](http://192.168.0.201/awesome_ops)  
When restarting your server, the BOINC server works even before you log into the server.



## getting new versions of example\_app installed

I edited the projects config.xml file...
* one\_result\_per\_user\_per\_wu changed to 1
* enable\_delete\_account changed to 1

To modify the work generator...
```
gedit ~/boinc-src/sched/sample_work_generator.cpp
```
then change REPLICATION\_FACTOR from 1 to 2, CUSHION from 10 to 10000, wu.delay\_bound from 86400 to 86400\*7, and add the line "wu.canonical\_credit = 1;" (also add the --credit\_from\_wu option to the validator via editing config.xml).
```
cd ~/boinc-src/sched/
make
cd ~/projects/awesome
bin/stop
cp ~/boinc-src/sched/sample_work_generator ~/projects/awesome/bin/
bin/start
```

I don't think you can modify an application version once it has been added to your BOINC project, so the thing to do is add a new version, and, if you want, go to awesome\_ops -> "Manage application versions" to mark the old ones as deprecated. Keep in mind that, if a new version changes the code's output files and a workunit sends out another task after the version change, the user running the previous version may be marked as invalid.

The source code for the application can be edited via...
```
gedit ~/boinc-src/apps/upper_case.cpp
```
where the Makefile in the same folder uses -O2 optimization (search for CXXFLAGS in the Makefile). After modifying the source code, you can compile and test it via...
```
cd ~/boinc-src/apps/
make
echo hi > in
./upper_case -cpu_time 1
rm out stderr.txt boinc_lockfile boinc_finish_called
```
You can then install a new version via (note that the filename of the executable must be unique, so put the version number inside of it)...
```
cd ~/projects/awesome/apps/example_app/
mkdir -p 1.01/x86_64-pc-linux-gnu/
cp ~/boinc-src/apps/upper_case 1.01/x86_64-pc-linux-gnu/example_app_101_x86_64-pc-linux-gnu
gedit 1.01/x86_64-pc-linux-gnu/version.xml
```
```
<version>
   <file>
      <physical_name>example_app_101_x86_64-pc-linux-gnu</physical_name>
      <main_program/>
   </file>
</version>
```
```
cd ~/projects/awesome
bin/update_versions
bin/stop
bin/start
```

To compile and test a new macOS version, I did the following on a macOS (with MacPorts installed for installing any necessary packages)...
```
cd ~
git clone https://github.com/BOINC/boinc.git boinc-src
cd ~/boinc-src
export LIBTOOLIZE=`which glibtoolize`
./_autosetup
./configure --disable-client --disable-manager --disable-server
cd apps
open upper_case.cpp
make
echo hi > in
./upper_case -cpu_time 1
cat out
```

I haven't figured out how to get things to compile on Windows.

See my **upper\_case\_template.cpp** here in my GitHub for how to easily modify BOINC's original upper\_case.cpp. Just put global code where is says to, then put the main() code where it says to.




## getting my Collatz CPU-only code into BOINC server

In addition to the changes already made to the work generator...
* Add the following include
```
include <cinttypes>
```
* Declare global variables
```
uint64_t taskID;
FILE* taskIDfile;
```
* Put the following in main() before main\_loop()
```
taskIDfile = fopen("taskID.txt", "r+");
if (!taskIDfile) return ERR_FOPEN;
fscanf(taskIDfile, "%" PRIu64, &taskID);
rewind(taskIDfile);
```
* Change
```
sprintf(name, "%s_%d_%d", app_name, start_time, seqno++);
```
to
```
sprintf(name, "CPU%" PRIu64, taskID);
```
* Change
```
fprintf(f, "This is the input file for job %s", name);
```
to
```
fprintf(f, "%" PRIu64, taskID);
taskID++;
```
* Then after the entire loop that starts with
```
for (int i=0; i<njobs; i++)
```
add
```
fprintf(taskIDfile, "%" PRIu64, taskID);
rewind(taskIDfile);
```

&nbsp;  
&nbsp;  
&nbsp;  

Then compile the work generator.

Initialize the taskID file. My computer is called pinkslime, but yours will be called something else!
```
cd ~/projects/awesome/tmp_pinkslime
echo 0 > taskID.txt
```

I want to use the "repeated k steps" algorithm, so use
```
partiallySieveless/collatzPartiallySieveless_repeatedKsteps.c
```

The CPU-only code uses essentially no RAM.

I added the code into my upper_case_template.cpp to create the **upper_case.cpp** file here on GitHub. When doing this, I made the following modifications...
* Got rid of the timing code (including "#include <sys/time.h>") because printing runtime to the out file affects validation
* My instances of  
  return 0;  
that appear following errors were replaced with  
exit(-1);
* I don't think stdout can be captured, so printf() changed to out.printf(), and fflush(stdout) changed to out.flush(). I moved  
MFILE out;  
to be global (above *my* global stuff) so that functions can use it.
* When printing to out, I also print to stderr via  
  fprintf(stderr, "whatever");  
so that users can see task results via BOINC project's webpage! You'll often want to flush this using  
  fflush(stderr);
* For the inputs to the code, I no longer use command options. Note that templates/example\_app\_in defines the command line options such as cpu\_time. Instead, I hardcoded task\_id0 to 0 (it only needs to be changed every couple years), and edited the work generator to have a number on the first line of the in file, which would become task\_id. In upper\_case.cpp, I added...
```
char idInput[25];
fscanf(infile, "%s", idInput);
rewind(infile);
```
so that idInput could be used for setting task\_id.
* So that the sieve file could be loaded, I changed  
  fp = fopen(file, "rb");  
to  
```
char input_path_sieve[512];
boinc_resolve_filename(file, input_path_sieve, sizeof(input_path_sieve));
fp = boinc_fopen(input_path_sieve, "rb");
```
* added the following lines to create a checksum that is useful for validation
```
uint64_t checksum_alpha = 0;
checksum_alpha += c;
checksum_alpha += k2;
printf("  Checksum = ");
print128(checksum_alpha);
```
* added the following lines to update BOINC's progress
```
uint64_t patternEnd = ((uint64_t)1 << (TASK_SIZE - 8));
double patternEndDouble = (double)patternEnd;
boinc_fraction_done((double)pattern / patternEndDouble);
```
* Because my code was C and I'm now compiling it as C++ for BOINC, I had to make a few minor changes to the part of the code that makes the 2^k2 lookup table.

&nbsp;  
&nbsp;  
&nbsp;  

I want to use the 2^37 sieve created by partiallySieveless/collatzCreateSieve.c, renamed as sieve37, which is a 1.07 GB sieve. Put sieve37 in the apps/example_app/ folder.
Other good choices for k1 are 32 and 35...
* sieve32 is 33 MB
* sieve35 is 268 MB
* sieve37 is 1.07 GB



You can compile and test the code via something like
```
echo 0 > in
ln -s ~/projects/awesome/apps/example_app/sieve37 .
./upper_case
cat out
```

Create a new folder for your version, let's say 1.03: apps/example\_app/1.03/  
For the Linux platform create the folder apps/example\_app/1.03/x86_64-pc-linux-gnu/  
then run the following to prevent having many copies of sieve37 on the server...
```
ln -s ~/projects/awesome/apps/example_app/sieve37 ~/projects/awesome/apps/example_app/1.03/x86_64-pc-linux-gnu/
```

Copy upper\_case to this folder, renaming it to example\_app\_103\_x86\_64-pc-linux-gnu, then create the following version.xml file
```
<version>
   <file>
      <physical_name>example_app_103_x86_64-pc-linux-gnu</physical_name>
      <main_program/>
   </file>
   <file>
      <physical_name>sieve37</physical_name>
   </file>
</version>
```

The sieve37 file will not have to be downloaded to the client for each task (just the client's first task). However, if on my client I set "no new tasks" and let all tasks finish, sieve37 would be deleted on my client's computer (the sieve is also deleted if the server runs out of tasks to send and my client has finished running of all of its tasks). Is there a way to keep the sieve37 file on the client's computer until the BOINC application is updated?

Then, do the usual to create the new version...
```
cd ~/projects/awesome
bin/update_versions
bin/stop
cp ~/boinc-src/sched/sample_work_generator ~/projects/awesome/bin/
bin/start
```

As the project runs, check the files in the sample\_results folder (use grep). If there is 128-bit integer overflow, this could be a number that disproves the Collatz conjecture! Much more likely, it is a very rare instance of the number getting too large before it finds its way to lower than where it started. You can use my collatzTestOverflow.py file to check numbers that overflow. My C code carefully checks if overflow will happen, then prints whenever overflow will occur so that you know to run this script.

If the CPU-only code never completes, you may have found an infinite cycle that disproves the Collatz conjecture!
Further analysis would be required if this occurs.  
To find these, you may need to go to the "awesome\_ops" webpage, then go to Results, then you can search for in-progress tasks sorted by send-time.

It is very important that tasks are not skipped, so find the oldest not-yet-returned workunit (as described above), then make sure all tasks have successfully returned between the last time you checked up to the oldest not-yet-returned workunit.  
Not wanting to skip tasks is why I require validation (due to things like most people not having ECC memory).

To remove old tasks from the database server, run db\_purge...  
[https://boinc.berkeley.edu/trac/wiki/DbPurge](https://boinc.berkeley.edu/trac/wiki/DbPurge)
```
./bin/db_purge --min_age_days 7 --one_pass
```
More info about the database...  
[https://boinc.berkeley.edu/trac/wiki/DataBase](https://boinc.berkeley.edu/trac/wiki/DataBase)  
[https://boinc.berkeley.edu/trac/wiki/MysqlConfig](https://boinc.berkeley.edu/trac/wiki/MysqlConfig)





## things you'd have to do if making a public BOINC server

Add checkpoints in the code next to the following line...
```
boinc_fraction_done((double)pattern/patternEndDouble);
```
BOINC's original upper\_case.cpp (before my edits) shows how to make and load from checkpoints. If saving checkpoints is slow, perhaps take care of checkpoints in the following conditional instead...
```
if (bufferStep >= bufferStepMax)
```

Perhaps add some amount to *aStart* manually (GPU code requires you to add to both change h\_minimum and h\_supremum) to match current experimental progress in published academic papers (that is, ignore the "progress" from Jon Sonntag's BOINC project). The CPU-only and GPU codes must have identical settings, and this setting cannot be changed until TASK\_SIZE0 (for GPU, TASK\_SIZE\_KERNEL2) has completed. If you do this, be sure to fix the "aMod = 0" line!

In both of the original .c files (one for CPU-only and one for GPU), read the comment about the "times 9" and decide if you want to remove it. If you remove it in CPU-only, remove it in GPU code too, and vice versa.

To prevent people hacking your project to get free BOINC credits, add some encryption and a secret phrase to be encrypted, then don't post your encryption code on GitHub. I'm not sure how much this would matter if you have one\_result\_per\_user\_per\_wu changed to 1 and if your project doesn't give a huge amount of credit, but it is worth doing.

Set the parameters in the code in this order...
1. For TASK\_ID\_KERNEL2 = 0 in GPU code (and TASK\_SIZE0 in CPU-only code), assuming that you got rid of the "time 9", maybe start with TASK\_SIZE\_KERNEL2 of 71.  
This value must agree between CPU-only and GPU code. It can always be increased a year or so in the future after this TASK\_SIZE\_KERNEL2 (for CPU-only, TASK\_SIZE0) has completed. If you set this too big, it will take decades for TASK\_SIZE\_KERNEL2 (or TASK_SIZE0) to finish.
1. Then choose k (not the k1 you already chose when saving the sieve) to optimize speed.  
This value must agree between CPU-only and GPU code. It can always be increased a year or so in the future after this TASK\_SIZE\_KERNEL2 (for CPU-only, TASK\_SIZE0) has completed.
1. Then choose TASK\_SIZE large enough to give enough to each GPU process (more than 10 for sure), but not too high to cause too much RAM usage (a concern for GPU code only). You can choose a larger TASK\_SIZE for GPU code compared to CPU-only codes, but then you'll have to do some thinking in BOINC's work generator in order to fit the smaller CPU-only tasks around the GPU tasks to make sure all numbers are tested. Do the CPU-only and GPU codes need to be different platforms in the same application for BOINC work generator to allow them to coordinate?

When setting these parameters, keep in mind that...
* For GPU code, frequent suspension of BOINC (that is going back to most recent checkpoint) requires the part of the sieve of length 2^TASK\_SIZE to be recalculated, and you could lose amount of work proportional to 2^(TASK\_SIZE\_KERNEL2 - k)
* You want each task to do the most work possible to prevent the BOINC server from having to work very hard dealing with many tasks.
* Change the amount of credit that the task gets to match. Don't give an unfair amount, else the people who want credit will volunteer at your project instead of at a project that cures diseases.  
[https://boinc.berkeley.edu/trac/wiki/CreditOptions](https://boinc.berkeley.edu/trac/wiki/CreditOptions)  
[https://boinc.berkeley.edu/wiki/Computation_credit](https://boinc.berkeley.edu/wiki/Computation_credit)

After choosing your parameters, you might want to add code to the work generator that prevents taskID from getting too large. If a taskID that is too large is about to be used, exit(1) should be called.




## things you'd have to do to get GPU code to work

For GPU, look in the partiallySieveless\_nonNvidiaGPU folder for the following files...
* collatzPartiallySieveless\_repeatedKsteps\_GPU\_nonNvidia.c
* kern1\_128byHand\_nonNvidia.cl
* kern1\_2\_repeatedKsteps\_nonNvidia.cl
* kern2\_repeatedKsteps\_128byHand\_nonNvidia.cl
Even though these say "nonNvidia", they still work on Nvidia.

Unlike the CPU-only code, there are time-limits and step-limits (due to watchdog timers). So, if an infinite cycle exists that disproves the conjecture, you have to check for limits being reached.

Set CHUNKS\_KERNEL2 to prevent the 4.5-second time limit from being reached, while being large enough to allow significant GPU computation. Perhaps change the code to let users set CHUNKS\_KERNEL2 and secondsKernel2 in some config file.

The GPU code uses 2^TASK\_SIZE bytes of RAM. The CPU-only code uses essentially no RAM.

You'll want to add checksums just like I did for CPU-only code. See the checksum\_alpha[] variable here...  
[https://github.com/xbarin02/collatz/blob/master/src/gpuworker/kernel32-precalc.cl](https://github.com/xbarin02/collatz/blob/master/src/gpuworker/kernel32-precalc.cl)

An output array for kern2 (instead of relying on in-kernel printf()) may be needed because I'm not sure that BOINC can capture stdout of an in-kernel printf(). Or maybe the checksum array could double as this.

boinc\_fraction\_done() and checkpoints can be set in the CHUNKS\_KERNEL2 loop (for kern2)

Is there a way to check if the user has enough GPU RAM before running the code?

Read through what I did to make the CPU-only code BOINC-ready, and do similar things to the GPU code.

If some non-Nvidia GPUs are giving math errors and are not validating, stop using them immediately. You could then use my CUDA code, but getting it to compile on Windows was tricky due to the CUDA and Windows combination not playing nicely with gcc (I've done it once and have some notes on my computer). I'd just use the OpenCL code in the partiallySieveless folder (and put up with the CPU busy waiting if printf() in kernel2). However, if there are watchdog timers or you want to implement checkpoints, you're back to using the partiallySieveless\_nonNvidiaGPU code.

First, look at boinc-src/samples/nvcuda/ and  
[https://boinc.berkeley.edu/trac/wiki/GPUApp](https://boinc.berkeley.edu/trac/wiki/GPUApp)  
[https://boinc.berkeley.edu/trac/wiki/AppCoprocessor](https://boinc.berkeley.edu/trac/wiki/AppCoprocessor)





