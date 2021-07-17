LIB=libmidifile-20150710

# also in libmidifile/Makefile!
CFLAGS = -std=c99 -Wall -Wextra -Wmissing-prototypes -Wmissing-declarations -Werror -pedantic-errors -O2 -g -I$(LIB) $(DEFS)

#DEFS = -D_POSIX_C_SOURCE=2 -DYY_USE_PROTOS
DEFS = -D_POSIX_C_SOURCE=200809 -DYY_USE_PROTOS

INSTALL = install

BINDIR = $(HOME)/bin

MF2TPROG = mf2t
MF2TOBJS = mf2t.o midifile.o

T2MFPROG = t2mf
T2MFOBJS = t2mf.o t2mflex.o midifile.o

PROGS = $(MF2TPROG) $(T2MFPROG)
OBJS = $(MF2TOBJS) $(T2MFOBJS)

all: TESTED

# NOTE! Cannot use stdour for t2mf!! needs to seek!
TESTED: $(PROGS)
	./mf2t < orig/example1.mid | cmp orig/example1.txt -
	./mf2t < orig/example2.mid | cmp orig/example2.txt -
	./mf2t < orig/example3.mid | cmp orig/example3.txt -
	./mf2t < orig/example4.mid | cmp orig/example4.txt -
	./mf2t < orig/example5.mid | cmp orig/example5.txt -
	./mf2t < orig/example1.mid > temp.txt
	./t2mf -r < orig/example1.txt > temp.mid
	cmp orig/example1.mid temp.mid
	./t2mf -r < orig/example2.txt > temp.mid
	cmp orig/example2.mid temp.mid
	./t2mf -r < orig/example3.txt > temp.mid
	cmp orig/example3.mid temp.mid
	./t2mf -r < orig/example4.txt > temp.mid
	cmp orig/example4.mid temp.mid
	./t2mf -r < orig/example5.txt > temp.mid
	cmp orig/example5.mid temp.mid

$(MF2TPROG): $(MF2TOBJS)
	$(CC) $(LDFLAGS) -o $(MF2TPROG) $(MF2TOBJS)

$(T2MFPROG): $(T2MFOBJS)
	$(CC) $(LDFLAGS) -o $(T2MFPROG) $(T2MFOBJS)

t2mflex.c: t2mf.fl
	flex -i -s -Ce -8 t2mf.fl
	mv lex.yy.c t2mflex.c

t2mflex.o: t2mflex.c
	$(CC) -c $(CFLAGS) -Wno-implicit-function-declaration -Wno-unused-function t2mflex.c

midifile.o: $(LIB)/midifile.c
	$(CC) -c $(CFLAGS) $(LIB)/midifile.c

install: $(PROGS)
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 755 -s $(PROGS) $(BINDIR)

clean:
	rm -f $(PROGS) $(OBJS)
