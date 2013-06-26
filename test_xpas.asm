#
# test_xpas.asm
#
# Test program for parsing xpas assembly files.
#
exception handle, start, stop
func main
start:
  ldimm   r3, 1
stop:
  ldimm   r5, 0
handle:
  halt
end lol
