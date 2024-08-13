all: jwasm/STDDOS.COM jwasm/COMMAND.COM

jwasm/%.ASM: %.ASM
	python dosasm2jwasm.py $< > $@

jwasm/STDDOS.OBJ: jwasm/MSDOS.ASM

jwasm/%.OBJ: jwasm/%.ASM
	cd jwasm; jwasm -0 -Zm  -Fl=../$@.LST -Fo ../$@ $(notdir $<) 

jwasm/%.EXE: jwasm/%.OBJ
	touch $@
	cd jwasm; dosemu -dumb LINK.EXE "$(basename $(notdir $<));"

jwasm/%.COM: jwasm/%.EXE
	python exe2bin.py $< $@


clean:
	rm -rf jwasm/*.COM jwasm/COMMAND.EXE jwasm/STDDOS.EXE jwasm/*.OBJ jwasm/*.ASM jwasm/*.err jwasm/*.LST
