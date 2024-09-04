all: floppy

MSDOS.SYS: STDDOS.BIN
	cp $< $@

COMMAND.ASM: COMMAND_ORIG.ASM
	python fixzero.py $< > $@

%.OBJ: %.ASM
	../vm ../dos-2.0-bin/MASM.EXE '$*;'

%.EXE: %.OBJ
	../vm ../dos-2.0-bin/LINK.EXE '$*;'

%.BIN: %.EXE
	../vm ../dos-2.0-bin/EXE2BIN.EXE '$*'
%.COM: %.BIN
	cp $< $@

floppy:MSDOS.SYS COMMAND.COM
	rm -f $@
	fallocate -l 327680  $@ 
	mkfs.fat -F 12 $@
	mcopy -i floppy $^ ::/

clean:
	-rm -f floppy *.COM *.BIN *.EXE *.OBJ COMMAND.ASM MSDOS.SYS 
