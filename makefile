CC      = gcc
NASM    = nasm
EXEC	= tinyRL
SRC		= main

CFLAGS  = -c -nostdlib -ffreestanding -fno-asynchronous-unwind-tables \
          -fno-unwind-tables -fno-stack-protector -fomit-frame-pointer \
          -fno-pie -fno-pic -fno-plt -fno-ident -fmerge-all-constants \
          -fno-builtin -fno-exceptions -fvisibility=hidden \
          -mpreferred-stack-boundary=3 -mno-red-zone

DFLAGS	= -g -O0 -ffreestanding -nostdlib -fno-pie -no-pie -Wl,-e
		
.DELETE_ON_ERROR:

all: $(EXEC)

$(EXEC): $(SRC).c link.ld header.asm	
	@BEST=999999; \
	for mode in -Os -Oz; do \
		$(CC) $$mode $(CFLAGS) $(SRC).c -o $(SRC).o; \
		ld -T link.ld --oformat=binary $(SRC).o -o code.bin; \
		$(NASM) -f bin header.asm -o payload; \
		chmod +x payload && \
		for lc in 0 1 2 3; do \
			for lp in 0 1 2; do \
				[ $$((lc + lp)) -le 4 ] || continue; \
				for pb in 0 1 2; do \
					for nice in 32 64 128 273; do \
						printf 'sed 1d $$0|xz -d>/tmp/a;chmod +x /tmp/a;/tmp/a;exit\n' > .tmp_sh; \
						if xz --format=lzma --lzma1=preset=9e,dict=4096,lc=$$lc,lp=$$lp,pb=$$pb,nice=$$nice --stdout payload >> .tmp_sh; then \
							SZ=$$(wc -c < .tmp_sh); \
							if [ "$$SZ" -lt "$$BEST" ]; then BEST=$$SZ; mv .tmp_sh $(EXEC); fi; \
						fi; \
					done; \
				done; \
			done; \
		done; \
	done; \
	rm -f .tmp_sh; \
	chmod +x $(EXEC); \
	wc -c < $(EXEC); \

gdb: $(SRC).c
	$(CC) $(DFLAGS),_start $(SRC).c -o debug_$(EXEC)
	gdb ./debug_$(EXEC)

run: $(EXEC)
	./$(EXEC)

clean:
	rm -f $(SRC).o code.bin payload $(EXEC) .tmp_sh debug_$(EXEC)