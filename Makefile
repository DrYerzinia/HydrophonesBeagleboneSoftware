INCLUDE_PATHS=-I$(PRU_CGT)/includeSupportPackage -I$(PRU_CGT)/include -I$(PRU_CGT)/lib
CFLAGS=$(INCLUDE_PATHS) --hardware_mac=on --c99 -O3
LNK_CMDS1=-z -i$(PRU_CGT)/lib -i$(PRU_CGT)/include -i$(PRU_CGT)/includeSupportPackage
LNK_CMDS2=-i$(PRU_CGT)/includeSupportPackage/am335x --reread_libs --stack_size=0x100
LNK_CMDS3=--heap_size=0x100 --library=$(PRU_CGT)/lib/rpmsg_lib.lib

all: $(SOURCES)
	mkdir -p build
	mkdir -p bin
	clpru --asm_directory=./build/ --keep_asm $(CFLAGS) ./src/pru0adc.c $(LNK_CMDS1) $(LNK_CMDS2) $(LNK_CMDS3) ./src/AM335x_PRU.cmd -o ./bin/am335x-pru0-fw --library=libc.a
	gcc -std=c99 ./src/adc_record.c -o ./bin/adc_record
	cp ./bin/am335x-pru0-fw /lib/firmware


