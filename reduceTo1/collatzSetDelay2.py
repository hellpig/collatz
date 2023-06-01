#!/usr/bin/env python3.7
# Get an interesting number that has a delay of your choosing.
# Look ahead N steps for the best path,
#   then keep the smallest 2^(N-1) paths for each step you take.
# (2*n - 1)/3 is counted as 1 step by default (can be easily changed)


delay = 1000

# N + 4 < delay
# N=1 corresponds to always decreasing whenever possible
# For large enough N, RAM usage and required time are proportional to 2^N
N = 17





dontCheckMod9 = False     # don't change this


def takeStep(list, index, offset):

    if list[index] == float('inf'):   # if branch has been killed, kill sub branches
      list[index] = float('inf')
      list[index + offset] = float('inf')
      return

    temp = list[index]<<1

    #temp2 = list[index]//3     # to make (2*n - 1)/3 be counted as 2 steps
    temp2 = temp//3

    #if list[index]%3 == 1 and temp2&1 and (dontCheckMod9 or temp2%3 != 0):     # to make (2*n - 1)/3 be counted as 2 steps
    if temp%3 == 1 and (dontCheckMod9 or temp2%3 != 0):
      list[index + offset] = temp2          # decrease
    else:
      list[index + offset] = float('inf')   # kill this branch

    list[index] = temp         # increase


def spreadHalfWithSmallest(list):

    # smallest 2^(N-1) numbers
    res = sorted(list)[:halfLength]
    # This approach is way better than choosing either the top or bottom half of list[]

    # spread out these numbers
    for i in range(halfLength):
      list[2*i] = res[i]


# initialize list[] by looking forward N steps
length = 1<<N    # list[] contains 2^N values
halfLength = 1<<(N-1)
list = [0] * length
list[0] = 8    # do first 3 steps to avoid the trivial cycle
for i in range(N):     # bifurcate for N steps
  m = 1<<(N-i)
  mHalf = 1<<(N-i-1)   # half of m
  for j in range(1<<i):  # bifurcate the previous 2^i values
    takeStep(list, j*m, mHalf)
  #print(list)    # run this for N = 4 to understand how this code works

# for each step, choose the path containing the smallest number
for j in range(delay - 3 - N - 2):
  spreadHalfWithSmallest(list)
  for i in range(0, length, 2):
    takeStep(list, i, 1)

# do the final N+2 steps
dontCheckMod9 = True
for i in range(2):
  spreadHalfWithSmallest(list)
  for i in range(0, length, 2):
    takeStep(list, i, 1)
print(min(list))
