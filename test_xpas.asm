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
stop:
  ldimm   r5, 0
handle:
  ret r6
end main
