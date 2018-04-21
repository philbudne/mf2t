CFLAGS = -Wall -O2 -pipe -fPIC -fvisibility=hidden -DBUILDING_MIDIFILE
LDFLAGS = -s
LIBS =
INSTALL = install

LIBDIR = $(HOME)/lib
INCDIR = $(HOME)/include
MANDIR = $(HOME)/share/man

MAJOR = 1
MINOR = 0

LIB = libmidifile.so
OBJS = midifile.o
INCLUDES = midifile.h
MAN3 = midifile.3

$(LIB).$(MAJOR).$(MINOR): $(OBJS)
	gcc $(LDFLAGS) -shared -Wl,-soname,$(LIB).$(MAJOR) -o $(LIB).$(MAJOR).$(MINOR) \
		$(OBJS) $(LIBS)

install: $(LIB).$(MAJOR).$(MINOR)
	$(INSTALL) -d $(LIBDIR)
	$(INSTALL) -m 644 -s $(LIB).$(MAJOR).$(MINOR) $(LIBDIR)
	ln -sf $(LIBDIR)/$(LIB).$(MAJOR).$(MINOR) $(LIBDIR)/$(LIB).$(MAJOR)
	ln -sf $(LIBDIR)/$(LIB).$(MAJOR) $(LIBDIR)/$(LIB)
	$(INSTALL) -d $(INCDIR)
	$(INSTALL) -m 644 $(INCLUDES) $(INCDIR)

install-man: $(MAN3)
	$(INSTALL) -d $(MANDIR)/man3
	$(INSTALL) -m 644 $(MAN3) $(MANDIR)/man3

clean:
	rm -f $(LIB).$(MAJOR).$(MINOR) $(OBJS)
