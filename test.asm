#
# test.asm
#
# Test program that sums together a list of numbers.
# Taken from Phil Hatcher's cs520 lecture slides.
#

sum:
  word 0
len:
  word 5
vector:
  word 1
  word 2
  word 3
  word 4
  word 5
skipData:
  ldimm   r0, 0
  load    r1, len
  ldimm   r2, 0
  ldimm   r3, 1
top:
  beq     r0, r1, done
  ldind   r5, 0(r2)
  addi    r3, r5
  addi    r2, r4
  addi    r0, r4
  jmp     top
done:
  store   r3, sum
  halt
