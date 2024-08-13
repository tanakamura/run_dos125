import sys
import re

def main():
    f = open(sys.argv[1])

    segbios_ignore_rem = 2
    zero_counter = 0

    for l in f.readlines():
        zerodef = False
        comment = re.match('(.*)[^"](;.*)', l)
        if comment:
            without_comment = comment.groups(1)[0]
        else:
            without_comment = l.rstrip('\n')

        tokens = [i for i in re.split('\\s+', without_comment) if i != '']

        if segbios_ignore_rem > 0:
            if len(tokens) > 0 and tokens[0] == "SEGBIOS":
                tokens = []
                segbios_ignore_rem -= 1

        if len(tokens) > 0 and tokens[0] == "ZERO":
            tokens[0] = f"ZERO{zero_counter}"
            zero_counter += 1
            zerodef = True

        if len(tokens) == 2 and tokens[0] == "IRET:":
            tokens = ["IRET_:", "IRET"]
        else:
            if len(tokens) > 3 and tokens[0] in ["JC", "JS"]:
                tokens = [tokens[0], tokens[3]]
            tokens2 = []
            for (pos,tok) in enumerate(tokens):
                if tok in ["IN", "OUT","PAUSE"]:
                    tokens2.append(tok + "_")
                elif tok in ["IN,", "OUT,","PAUSE,"]:
                    tokens2.append(tok[:-1] + "_,")
                elif tok in ["IN:", "OUT:","PAUSE:"]:
                    tokens2.append(tok[:-1] + "_:")
                else:
                    tok = tok.replace("DOSGROUP:IRET", "DOSGROUP:IRET_")
                    tok = tok.replace("FSAVE", "FSAVE_")
                    tok = tok.replace(":PAUSE", ":PAUSE_")
                    if not zerodef:
                        m = re.match(r'(.*)(ZERO)([^A-Z]*)$', tok)
                        if m:
                            tok = m.groups()[0] + m.groups()[1] + str(zero_counter-1) + m.groups()[2]
                    tokens2.append(tok)
            tokens = tokens2

        for tok in tokens:
            print(f"{tok} ", end="")

        if comment:
            print(" ", comment.groups(1)[1], "\r")
        else:
            print("\r")

main()
