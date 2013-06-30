#
# test_xpas.asm
#
# Test program for parsing xpas assembly files.
#
func main
exception handle, start, stop
start:
  ldimm   r3, 42
  cvtld   r6, r3
  ldimm   r4, 1
  ldblkid r10, lol
  call r11, r10
stop:
  ldimm   r5, 0
handle:
  ret r11
end main
func lol
  ldimm r10, 42
  cvtld r10, r10
  ret r10
end lol
