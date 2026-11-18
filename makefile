CC      = gcc
NASM    = nasm
OBJCOPY = objcopy
PYTHON  = python3

CFLAGS  = -Oz -c -nostdlib -fno-asynchronous-unwind-tables \
          -fno-unwind-tables -fno-stack-protector -fomit-frame-pointer \
          -fno-pie -fno-pic -fno-plt -fno-ident -fmerge-all-constants \
          -fno-builtin -fno-exceptions -fvisibility=hidden \
          -mpreferred-stack-boundary=3 -mno-red-zone
		
.DELETE_ON_ERROR:

all: game

# 1. Compile C -> raw machine code + strings
code.bin: main.c link.ld
	$(CC) $(CFLAGS) main.c -o main.o
	ld -T link.ld --oformat=binary main.o -o code.bin

# 2. Assemble 112-byte NASM ELF64 header + code.bin -> uncompressed binary
payload: code.bin header.asm
	$(NASM) -f bin header.asm -o payload
	chmod +x payload

# 3B. Pack with 53-byte sh+lzma dropper + brute-force LZMA context bits
game: payload
	@BEST=999999; \
	for lc in 0 1 2 3; do \
	for lp in 0 1 2; do \
		[ $$((lc + lp)) -le 4 ] || continue; \
		for pb in 0 1 2; do \
		for nice in 32 64 128 273; do \
			printf 'sed 1d $$0|xz -d>/tmp/a;chmod +x /tmp/a;/tmp/a;exit\n' > .tmp_sh; \
			if xz --format=lzma --lzma1=preset=9e,dict=4096,lc=$$lc,lp=$$lp,pb=$$pb,nice=$$nice --stdout payload >> .tmp_sh; then \
			SZ=$$(wc -c < .tmp_sh); \
			if [ "$$SZ" -lt "$$BEST" ]; then BEST=$$SZ; mv .tmp_sh game; fi; \
			fi; \
		done; \
		done; \
	done; \
	done
	rm -f .tmp_sh; \
	chmod +x game;
	wc -c < game; 

# Build a standard unstripped ELF with full DWARF debug symbols (-g) for GDB
debug_game: main.c
	$(CC) -g -O0 -nostdlib -fno-pie -no-pie -Wl,-e,_start main.c -o debug_game

gdb: debug_game
	gdb ./debug_game

run-raw: payload
	./payload

run: game
	./game

clean:
	rm -f main.o code.bin payload game .tmp_sh debug_game