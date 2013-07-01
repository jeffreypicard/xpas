#
# exception_test.asm
# 
# Test exception catching abilities in assembly.
#
# Author: Jeffrey Picard
#

func main

exception handle, start, stop

  ldimm r5, 0
  ldimm r6, 1
start:
  divl r7, r6, r5
stop:
  ret r7
handle:
  ldimm r10, -1
  cvtld r10, r10
  ret r10
end main
