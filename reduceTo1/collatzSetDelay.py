#!/usr/bin/env python3.7
# Get an interesting number that has a delay of your choosing


# must be at least 5
delay = 1000


# do first 3 steps to avoid the trivial cycle
n = 8
odds = 0
evens = 3



# Do backwards steps, decreasing whenever you can.
# You cannot decrease when the result has 3 as a prime factor,
#   else you will increase forever
# If n%27==11, it is best to NOT decrease and increase instead
for i in range(delay - 3 - 2):

  temp0 = 2*n

  #temp = n//3       # to make (2*n - 1)/3 be counted as 2 steps
  temp = temp0//3

  #if n%3 == 1 and temp&1 and temp%3 != 0:  # to make (2*n - 1)/3 be counted as 2 steps
  #if temp0%3 == 1 and temp%3 != 0:
  #if temp0%3 == 1 and temp%3 != 0 and n%27 != 11:
  #if temp0%3 == 1 and temp%3 != 0 and (n%81 not in [11, 38, 47]):
  #if temp0%3 == 1 and temp%3 != 0 and (n%243 not in [209, 101, 47, 38, 119, 173, 200, 92]):
  if temp0%3 == 1 and temp%3 != 0 and (n%729 not in [209, 587, 452, 695, 533, 47, 344, 281, 38, 524, 245, 362, 416, 470, 335, 173, 425, 686, 443, 389, 119, 605, 659, 200, 92]):
    n = temp
    odds += 1
    #print("", n%81)
  else:
    n = temp0
    evens += 1
    #print(n%81)
  #print(n)



# Do final two steps, but now don't check if 3 is a prime factor
for i in range(2):

  #temp = n//3     # to make (2*n - 1)/3 be counted as 2 steps
  temp = 2*n//3

  #if n%3 == 1 and temp&1:  # to make (2*n - 1)/3 be counted as 2 steps
  if (2*n)%3 == 1:
    n = temp
    odds += 1
  else:
    n = 2*n
    evens += 1



print(n)
print("ups =", evens)
print("downs =", odds)
