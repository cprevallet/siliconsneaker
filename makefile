# change application name here (executable output name)
TARGET=runplotter

# compiler
CC=gcc
# debug
DEBUG=-g
# optimisation
OPT=-O0
# warnings
WARN=-Wall

PTHREAD=-pthread

CCFLAGS=$(DEBUG) $(OPT) $(WARN) $(PTHREAD)

GTKLIB=`pkg-config --cflags --libs gtk+-3.0 plplot cairo champlain-0.12 champlain-gtk-0.12 clutter-gtk-1.0`

# linker
LD=gcc
LDFLAGS=$(PTHREAD) $(GTKLIB) -export-dynamic -lm

OBJS= main.o decode.o fit.o fit_convert.o fit_product.o fit_crc.o

all: $(OBJS)	
	$(LD) -o $(TARGET) $(OBJS) $(LDFLAGS)
    
main.o: main.c
	$(CC) -c $(CCFLAGS) main.c $(GTKLIB)
    
decode.o: decode.c
	$(CC) -c $(CCFLAGS) decode.c 

fit.o: ./fit/fit.c
	$(CC) -c $(CCFLAGS) ./fit/fit.c
fit_convert.o: ./fit/fit_convert.c
	$(CC) -c $(CCFLAGS) ./fit/fit_convert.c
fit_product.o: ./fit/fit_product.c
	$(CC) -c $(CCFLAGS) ./fit/fit_product.c
fit_crc.o: ./fit/fit_crc.c
	$(CC) -c $(CCFLAGS) ./fit/fit_crc.c

clean:
	rm -f *.o $(TARGET)
