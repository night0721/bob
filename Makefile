.POSIX:

VERSION = 1.0
TARGET = bob
MANPAGE = $(TARGET).1
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
PKG_CONFIG = pkg-config

PKGS = libcurl
LIBS != $(PKG_CONFIG) --libs $(PKGS)
INCS != $(PKG_CONFIG) --cflags $(PKGS)
CFLAGS += -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE $(INCS)

.c.o:
	$(CC) -o $@ $(CFLAGS) -c $<

$(TARGET): $(TARGET).o
	$(CC) -o $@ $(TARGET).o $(LIBS)

dist:
	mkdir -p $(TARGET)-$(VERSION)
	cp -R README.md $(MANPAGE) $(TARGET) $(TARGET)-$(VERSION)
	tar -czf $(TARGET)-$(VERSION).tar.gz $(TARGET)-$(VERSION)
	rm -rf $(TARGET)-$(VERSION)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)
	cp -p $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BINDIR)/$(TARGET)
	cp -p $(MANPAGE) $(DESTDIR)$(MANDIR)/$(MANPAGE)
	chmod 644 $(DESTDIR)$(MANDIR)/$(MANPAGE)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(MANDIR)/$(MANPAGE)

clean:
	rm -f $(TARGET) *.o

all: $(TARGET)

.PHONY: all dist install uninstall clean
