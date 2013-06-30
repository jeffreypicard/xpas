#
# test_xpas.asm
#
# Test program for parsing xpas assembly files.
#
func main
exception handle, start, stop
start:
  ldimm   r3, 1
stop:
  ldimm   r5, 0
handle:
  halt
end main
