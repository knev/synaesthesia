
CC=gcc -O4 -ffast-math -funroll-loops
#CC=gcc -g -pg -O4 -DPROFILE
#CC=gcc -g

# Try commenting out these lines if something goes bang
MITSHM=-DMITSHM
XEXT=-lXext

all : synaesthesia xsynaesthesia

synaesthesia : core.o main.o svga.o sound.o bitmap.o
	$(CC) core.o main.o svga.o sound.o bitmap.o -o synaesthesia -lm -lvga 
xsynaesthesia : core.o main.o xlibwrap.o xlib.o sound.o bitmap.o
	$(CC) core.o main.o xlibwrap.o xlib.o sound.o bitmap.o -o xsynaesthesia -lm -lvga -L /usr/X11R6/lib -lX11 $(XEXT) 
clean :
	rm *.o

core.o : core.cpp syna.h
	$(CC) -c core.cpp
main.o : main.cpp syna.h
	$(CC) -c main.cpp
sound.o : sound.cpp syna.h
	$(CC) -c sound.cpp
svga.o : svga.cpp syna.h
	$(CC) -c svga.cpp
xlib.o : xlib.c xlib.h
	$(CC) -c xlib.c $(MITSHM)
xlibwrap.o : xlibwrap.cpp xlib.h syna.h
	$(CC) -c xlibwrap.cpp $(MITSHM)
bitmap.o : bitmap.cpp bitmap.h symbol.h syna.h 
	$(CC) -c bitmap.cpp 


