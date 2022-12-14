# Copyright: Владимир Игнатов, 2000; Dmitry Kotlyar <dm.kotlyar@yandex.ru>, 2021.
# Source: http://www.opennet.ru/docs/RUS/gnumake/index.html
# Source: https://blog.podkalicki.com/how-to-compile-and-burn-the-code-to-avr-chip-on-linuxmacosxwindows/
# Source: https://ph0en1x.net/77-avrdude-full-howto-samples-options-gui-linux.html
#
# Installation guide
# $ sudo apt-get install gcc-avr avr-libc avrdude

# Instructions
# 1. Use `$ make all` or `$ make` to create release directory and build your
#    AVR project to .hex file
# 2. Use `$ make clean` to clean and remove release directory
# 3. Use `$ make flash` to burn .hex file to microcontroller after build project.
#    You can use -e options to replace default properties. For example: `$ make flash -e port=/dev/ttyUSB1`

###############################################################################
######################### USER CONFIGURATION SECTION ##########################
###############################################################################

# Program name (please use only A-Za-z0-9_- characters for capability)
program_name := firmware

# Source directories. If you have next sources tree:
# ./
# - lib/
# - lib/a.h
# - lib/a.c
# - main.h
# - main.c
# source_dirs := . lib
source_dirs  := . canlib

# MCU type
# See -mmcu option: https://gcc.gnu.org/onlinedocs/gcc/AVR-Options.html
mcu        := at90can128

# Programmer type
# See https://ph0en1x.net/77-avrdude-full-howto-samples-options-gui-linux.html#avrdude-programmer-types
programmer := jtag1

# Programmer port
# See dmesg
port := /dev/ttyUSB0


###############################################################################
############## Makefile section. PLEASE DO NOT CHANGE ANYTHING ################
################### Of course you can but do it carefully #####################
###############################################################################

#libdir			 := /usr/lib/avr/include/
source_dirs      := $(addprefix ./,$(source_dirs))
search_wildcards := $(addsuffix /*.c,$(source_dirs))
elf_file		 := ./release/$(program_name).elf
hex_file		 := ./release/$(program_name).hex

all: mkdir $(program_name)

mkdir:
	mkdir -p $(addprefix release/,$(source_dirs))

clean:
	rm -rf release

$(program_name): $(addprefix release/,$(patsubst %.c,%.o,$(wildcard $(search_wildcards))))
	avr-gcc -Wall -ggdb -Os -mmcu=$(mcu) $^ -o $(elf_file)
	avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures $(elf_file) $(hex_file)
	avr-size --mcu=$(mcu) --format=avr $(elf_file)

VPATH := $(source_dirs)

release/%.o: %.c %.h
	avr-gcc -c -O3 -mmcu=$(mcu) -MD $(addprefix -I,$(source_dirs)) -o $@ $<

flash:
	avrdude -p $(mcu) -c $(programmer) -P $(port) -U flash:w:$(hex_file):i -F

flash-elf:
	avrdude -p $(mcu) -c $(programmer) -P $(port) -U flash:w:$(elf_file):e -F

include $(wildcard *.d)
