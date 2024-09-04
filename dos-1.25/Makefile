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
	rm -f $@ $@.tmp
	fallocate -l 327680  $@.tmp
	mkfs.fat -F 12 $@.tmp
	mcopy -i $@.tmp $^ ::/
	mv $@.tmp $@

clean:
	-rm -f floppy *.COM *.BIN *.EXE *.OBJ COMMAND.ASM MSDOS.SYS 
