
#CC=g++ -O6 -ffast-math -funroll-loops -Wall -D_REENTRANT
#CC=g++ -g -pg -O4 -DPROFILE -D_REENTRANT
CC=g++ -g -D_REENTRANT

#Uncomment these lines to enable X shared memory extensions
# These should give a slightly better frame rate BUT there is a memory leak
# which I haven't been able to find.
#MITSHM=-DMITSHM
#XEXT=-lXext

# Choose installation directory
#INSTALLDIR=/usr/local/bin
INSTALLDIR=/usr/games

title : 
	@echo "Select one of the following:"
	@echo
	@echo "  make svga       - build using svgalib"
	@echo "  make x          - build using X windows"
	@echo "  make sdl        - build using Simple DirectMedia Layer library"
	@echo 
	@echo "To install the resulting binary into" $(INSTALLDIR) "type"
	@echo
	@echo "  make install"
	@echo
	@echo "(edit the make file to change the install directory)"

svga : core.o main.o svga.o sound.o bitmap.o
	$(CC) core.o main.o svga.o sound.o bitmap.o -o synaesthesia -lm -lvga 
x    : core.o main.o xlibwrap.o xlib.o sound.o bitmap.o
	$(CC) core.o main.o xlibwrap.o xlib.o sound.o bitmap.o -o synaesthesia -lm -L /usr/X11R6/lib -lX11 $(XEXT) 
sdl  : core.o main.o sdlwrap.o sound.o bitmap.o
	$(CC) core.o main.o sdlwrap.o sound.o bitmap.o -o synaesthesia -lm -lSDL -ldl -lpthread

clean :
	rm -f *.o synaesthesia
install :
	install -s synaesthesia $(INSTALLDIR)/synaesthesia

core.o : core.cpp syna.h
	$(CC) -c core.cpp
main.o : main.cpp syna.h
	$(CC) -c main.cpp
sound.o : sound.cpp syna.h
	$(CC) -c sound.cpp
svga.o : svga.cpp syna.h
	$(CC) -c svga.cpp
xlib.o : xlib.c xlib.h
	gcc -O6 -c xlib.c $(MITSHM)
xlibwrap.o : xlibwrap.cpp xlib.h syna.h
	$(CC) -c xlibwrap.cpp $(MITSHM)
sdlwrap.o : sdlwrap.cpp syna.h
	$(CC) -c sdlwrap.cpp
bitmap.o : bitmap.cpp bitmap.h symbol.h syna.h 
	$(CC) -c bitmap.cpp 


