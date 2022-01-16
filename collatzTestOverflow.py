#!/usr/bin/env python3.7
# This code is for checking numbers that overflowed when using 128-bit integers.
# Integers in Python 3 are of unlimited size!



nStart = 340282366920938463463374607431768211455   # 2^128 - 1

# the following is the first to require 129 bits...
nStart = 55247846101001863167
# source:
#   http://pcbarina.fit.vutbr.cz/path-records.htm
#
# The following is not a great source
# since given Mx must be divided by 2 before doing B(Mx)...
#   http://www.ericr.nl/wondrous/pathrecs.html

nStart = 274133054632352106267
nStart = 71149323674102624415
nStart = 55247846101001863167




# Optionally find A and B for nStart = A * 2**k + B
k = 51
A = nStart >> k
B = nStart - (A << k)
print("For k =", k)
print(" A =", A)
print(" B =", B)
print("")

n = nStart
steps = 0
while True:
  if (n >> 128):
    print(" ", n)  # to have the overflowing parts stand out
  else:
    print(n)
  if n == 1:
  #if n < nStart:
    break
  if n%2 == 1:   # odd
    n = (3*n+1) // 2
  else:
    n = n // 2
  steps += 1

print("steps =", steps)
