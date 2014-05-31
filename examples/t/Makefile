CFLAGS += -I../../include -emit-llvm -c -pthread
CC = clang

SRC = $(wildcard *.c)
BC = $(SRC:.c=.bc)

.PHONY: clean

all: $(BC)

%.bc: %.c
	$(CC) $(CFLAGS) $< -o $@
	opt --dot-cfg $@ >/dev/null
	opt --dot-postdom $@ >/dev/null
	llvm-dis $@

pdf:
	for i in $$(ls *.dot); do \
		dot -Tpdf -o $${i%.dot}.pdf $$i; \
	done

clean:
	rm -f *.bc *.ll *.dot *.pdf
