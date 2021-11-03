# change application name here (executable output name)
TARGET=siliconsneaker
# install
prefix = /usr

# compiler
CC=gcc
# debug
DEBUG=-g
# optimisation
OPT=-O2
# warnings
WARN=-Wall
#gtk4
GTKFLAGS=`pkg-config --libs gtk4`

PTHREAD=-pthread

CCFLAGS=$(DEBUG) $(OPT) $(WARN) $(PTHREAD) $(GTKFLAGS)

LIBS=`pkg-config --cflags --libs gtk4 plplot osmgpsmap-1.0  librsvg-2.0 libxml-2.0`

# linker
LD=gcc
ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
    LDFLAGS=$(PTHREAD) $(LIBS) -lm -lxml2
else
    LDFLAGS=$(PTHREAD) $(LIBS) -export-dynamic -lm -lxml2
endif

OBJS= main.o fitwrapper.a ui.o tcx.o

all: $(OBJS)	
	$(LD) -o $(TARGET) $(OBJS) $(LDFLAGS)
    
main.o: main.c fitwrapper.a fitwrapper.h
	$(CC) -c $(CCFLAGS) main.c $(LIBS)
    
fitwrapper.a: fitwrapper.go 
	go build -buildmode=c-archive fitwrapper.go

tcx.o: tcx.c tcxwrapper.h
	$(CC) -c $(CCFLAGS) tcx.c $(LIBS)

ui.o: ui.c
	$(CC) -c $(CCFLAGS) ui.c $(LIBS)

# resource compiler
ui.c: siliconsneaker.glade ui.xml
	glib-compile-resources --target=ui.c --generate-source ui.xml

clean:
	rm -f *.o *.a ui.c $(TARGET)
	rm -f fitwrapper.h

install: all
	install -D siliconsneaker $(DESTDIR)$(prefix)/bin/siliconsneaker
	install -m 644 -D ./icons/siliconsneaker.svg $(DESTDIR)$(prefix)/share/icons/hicolor/scalable/apps/siliconsneaker.svg
	install -m 644 -D siliconsneaker.desktop $(DESTDIR)$(prefix)/share/applications/siliconsneaker.desktop

uninstall: 
	rm $(DESTDIR)$(prefix)/bin/siliconsneaker
	rm $(DESTDIR)$(prefix)/share/icons/hicolor/scalable/apps/siliconsneaker.svg
	rm $(DESTDIR)$(prefix)/share/applications/siliconsneaker.desktop
