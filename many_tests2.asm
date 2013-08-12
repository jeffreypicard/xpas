#
# exception_test.asm
# 
# Test exception catching abilities in assembly.
#
# Author: Jeffrey Picard
#

func main
  ldblkid r90, f1
  call r89, r90
  ret r89
end main

func f1
  ldblkid r100, f2
  call r101, r100
  ret r101
end f1

func f2
  ldblkid r100, f3
  call r101, r100
  ret r101
end f2

func f3

exception handle1, start, stop1
exception handle2, start, stop2

  ldimm r5, 0
  ldimm r6, 1
start:
  divl r7, r6, r5
stop1:
  ret r7
handle1:
  ldimm r1, -42
  ldnative r11, print_int
  calln r12, r11, 1
  cvtld r1, r1
  negd r1, r1
  cvtdl r1, r1
  calln r12, r11, 1
  cvtld r1, r1
  divd r100, r1, r5
stop2:
  ldimm r99, 1
handle2:
  ret r1

end f3
