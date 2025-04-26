

synaesthesia : syna-core.cpp syna-linux.cpp syna.h alphabet.h Makefile
	gcc syna-core.cpp syna-linux.cpp -lm -lvga -o synaesthesia -O4 -ffast-math 

