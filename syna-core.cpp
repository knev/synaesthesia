/* Synaesthesia - program to display sound graphically
   Copyright (C) 1997  Paul Francis Harrison

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  675 Mass Ave, Cambridge, MA 02139, USA.

  The author may be contacted at:
    phar6@student.monash.edu.au
  or
    27 Bond St., Mt. Waverley, 3149, Melbourne, Australia
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "syna.h"

short *data = new short[n*2];
unsigned char *output = new unsigned char[320*200*2];

double cosTable[n], sinTable[n];
int bitReverse[n];

int bitReverser(int i) {
  int sum=0,j;
  for(j=0;j<m;j++) {
    sum = (i&1)+sum*2;
    i >>= 1;
  }
  return sum;
}

void fft(double *x,double *y) {
  int n2 = n, n1;
  int k, twoToTheK;
  for(twoToTheK=1;twoToTheK<n;twoToTheK*=2) {
    n1 = n2;
    n2 /= 2;
    for(int j=0;j<n2;j++) {
      double c = cosTable[j*twoToTheK%n], s = -sinTable[j*twoToTheK%n];
      for(int i=j;i<n;i+=n1) {
        int l = i+n2;
        double xt = x[i] - x[l];
	x[i] = (x[i] + x[l]);
	double yt = y[i] - y[l];
	y[i] = (y[i] + y[l]);
	x[l] = xt*c - yt*s;
	y[l] = xt*s + yt*c;
      }
    }
  }
}

void go(void) {
  data = new short[n*2];
  output = new unsigned char[320*200*2];
  
  double x[n], y[n];
  double a[n], b[n];
  int clarity[n]; //Surround sound
  int i,j, fragsLost = 0, fragsGot = 0;

  for(i=0;i<n;i++) {
    sinTable[i] = sin(3.141592*2/n*i);
    cosTable[i] = cos(3.141592*2/n*i);
    bitReverse[i] = bitReverser(i);
  }

#define BOUND(x) ((x) > 63 ? 63 : (x))
#define PEAKIFY(x) BOUND((x) - (x)*(63-(x))/63/2)
  for(i=0;i<256;i++)
    setPalette(i,PEAKIFY((i&15)*4),
                     PEAKIFY((i&15*16)/4+(i&15)),
		     PEAKIFY((i&15*16)/4));
  realizePalette();
  
  memset(output,0,320*200*2);
 
  for(;;) {
    if (processUserInput()) break;
   
    getNextFragment();
   
    for(i=0;i<n;i++) {
      x[i] = data[i*2];
      y[i] = data[i*2+1];
    }
    fft(x,y);
    for(i=0 +1;i<n;i++) {
      double x1 = x[bitReverse[i]], 
             y1 = y[bitReverse[i]], 
	     x2 = x[bitReverse[n-i]], 
	     y2 = y[bitReverse[n-i]],
	     aa,bb;
      a[i] = sqrt(aa= (x1+x2)*(x1+x2) + (y1-y2)*(y1-y2) );
      b[i] = sqrt(bb= (x1-x2)*(x1-x2) + (y1+y2)*(y1+y2) );
      clarity[i] = (int)(
        ( (x1+x2) * (x1-x2) + (y1+y2) * (y1-y2) )/(aa+bb) * 256 );
    } 
   
    unsigned char *ptr = output;
    i = 320*200*2;
    do {
      *(ptr++) -= *ptr+(*ptr>>1)>>4;
    } while(--i > 0);

    for(i=1;i<n/2;i++) {
      int h = (int)( b[i]*280 / (a[i]+b[i]+0.0001)+20 );
      if (h >= 10 && h < 310) {
	int br1, br2, br = (int)( 
	  (a[i]+b[i])*i*(brightness/65536.0/n));
	br1 = br*(clarity[i]+128)>>8;
	br2 = br*(128-clarity[i])>>8;
	if (br1 < 0) br1 = 0; else if (br1 > 255) br1 = 255;
	if (br2 < 0) br2 = 0; else if (br2 > 255) br2 = 255;
	unsigned char *p = output+ h*2+(164-((i<<8)>>m))*640; 

#define doit(o) if (p[o]   < 255-br1) p[o] += br1; else p[o] = 255; \
                if (p[o+1] < 255-br2) p[o+1] += br2; else p[o+1] = 255;
	doit(0)
	for(int x=2;br1 > 16 || br2 > 16;x+=2,
	         br1 = br1*200>>8,
		 br2 = br2*200>>8) {
	  doit(x)
	  doit(-x)
	  doit(x*320)
	  doit(-(x*320))
	}
      }
    }
  
    showOutput();
  }
  
  delete[] data;
  delete[] output;
}
