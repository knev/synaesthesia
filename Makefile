
CC=g++ -O6 -ffast-math -Wall -D_REENTRANT
#CC=g++ -g -pg -O4 -DPROFILE -D_REENTRANT
#CC=g++ -g -D_REENTRANT

#Uncomment these lines to enable X shared memory extensions
# These should give a slightly better frame rate BUT there is a memory leak
# which I haven't been able to find.
#MITSHM=-DMITSHM
#XEXT=-lXext

# Choose installation directory
#INSTALLDIR=/usr/local/bin
INSTALLDIR=/usr/games

all :
	@echo Now attempting to build Synaesthesia
	make synaesthesia ; make xsynaesthesia ; make sdlsynaesthesia ; true
	@echo
	@echo ===============================================
	@echo
	@echo The following versions were successfully built:
	@echo
	@ls *synaesthesia
	@echo
	@echo To install to $(INSTALLDIR) type
	@echo
	@echo "  make install"
	@echo
	@echo "(edit Makefile to change the install directory)"

synaesthesia : core.o main.o svga.o sound.o ui.o 
	$(CC) core.o main.o svga.o sound.o ui.o -o synaesthesia -lm -lvga 

xsynaesthesia    : core.o main.o xlibwrap.o xlib.o sound.o ui.o 
	$(CC) core.o main.o xlibwrap.o xlib.o sound.o ui.o -o xsynaesthesia -lm -L /usr/X11R6/lib -lX11 $(XEXT) 

sdlsynaesthesia  : core.o main.o sdlwrap.o sound.o ui.o 
	$(CC) core.o main.o sdlwrap.o sound.o ui.o -o sdlsynaesthesia -lm -lSDL -ldl -lpthread

clean :
	rm -f *.o *~ synaesthesia xsynaesthesia sdlsynaesthesia core

install :
	install -s synaesthesia $(INSTALLDIR)/synaesthesia ; \
	install -s xsynaesthesia $(INSTALLDIR)/xsynaesthesia ; \
	install -s sdlsynaesthesia $(INSTALLDIR)/sdlsynaesthesia ; true

core.o : core.cpp syna.h
	$(CC) -c core.cpp
main.o : main.cpp syna.h polygon.h
	$(CC) -c main.cpp
sound.o : sound.cpp syna.h
	$(CC) -c sound.cpp
ui.o : ui.cpp syna.h icons.h polygon.h
	$(CC) -c ui.cpp
svga.o : svga.cpp syna.h
	$(CC) -c svga.cpp
xlib.o : xlib.c xlib.h
	gcc -O6 -c xlib.c $(MITSHM) -I/usr/X11R6/include
xlibwrap.o : xlibwrap.cpp xlib.h syna.h
	$(CC) -c xlibwrap.cpp $(MITSHM) -I/usr/X11R6/include
sdlwrap.o : sdlwrap.cpp syna.h
	$(CC) -c sdlwrap.cpp


