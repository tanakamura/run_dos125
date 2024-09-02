import sys
import re

def main():
    f = open(sys.argv[1])
    zero_counter = 0

    for l in f.readlines():
        l = l.rstrip()
        if l == "ZERO    =       $" or l == "ZERO    EQU     $":
            zero_counter += 1

        l = l.replace("ZERO", f"ZERO{zero_counter}")
        print(l+"\r\n", end="")

main()
