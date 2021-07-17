LIB=libmidifile-20150710

# also in libmidifile/Makefile!
CFLAGS = -std=c99 -Wall -Wextra -Wmissing-prototypes -Wmissing-declarations -Werror -O2 -I$(LIB) $(DEFS)

DEFS = -D_POSIX_C_SOURCE=2 -DYY_USE_PROTOS

INSTALL = install

BINDIR = $(HOME)/bin

MF2TPROG = mf2t
MF2TOBJS = mf2t.o $(LIB)/midifile.o

T2MFPROG = t2mf
T2MFOBJS = t2mf.o t2mflex.o $(LIB)/midifile.o

PROGS = $(MF2TPROG) $(T2MFPROG)
OBJS = $(MF2TOBJS) $(T2MFOBJS)

all: TESTED

TESTED: $(PROGS)
	./mf2t < orig/example1.mid > temp.txt
	cmp orig/example1.txt temp.txt
	./mf2t < orig/example2.mid > temp.txt
	cmp orig/example2.txt temp.txt
	./mf2t < orig/example3.mid > temp.txt
	cmp orig/example3.txt temp.txt
	./mf2t < orig/example4.mid > temp.txt
	cmp orig/example4.txt temp.txt
	./mf2t < orig/example5.mid > temp.txt
	cmp orig/example5.txt temp.txt
	./t2mf < orig/example1.txt > temp.mid
	cmp orig/example1.mid temp.mid
	./t2mf < orig/example2.txt > temp.mid
	cmp orig/example2.mid temp.mid
	./t2mf < orig/example3.txt > temp.mid
	cmp orig/example3.mid temp.mid
	./t2mf < orig/example4.txt > temp.mid
	cmp orig/example4.mid temp.mid
	./t2mf < orig/example5.txt > temp.mid
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

 $(LIB)/midifile.o: $(LIB)/midifile.c
	cd $(LIB); make

install: $(PROGS)
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 755 -s $(PROGS) $(BINDIR)

clean:
	rm -f $(PROGS) $(OBJS)
