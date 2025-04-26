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

/***************************************/
/*   For the incurably fiddle prone:   */

/* log2 of sample size */
#define m 8 

/* Brightness */
#define brightness 150

/* Sample frequency*/
#define frequency 22050

/***************************************/


#define PROGNAME "synaesthesia"

void error(char *str);
#define attempt(x,y) if ((x) == -1) error(y)
void warning(char *str);
#define attemptNoDie(x,y) if ((x) == -1) warning(y)

#define n (1<<m)

/* core */
extern short *data;
extern unsigned char *output;

void go(void);

/* platform specific (you should also write main
   to setup cd, enter graphics mode, call go, leave graphics mode and exit */
void setPalette(int i,int r,int g,int b);
void realizePalette(void);
void showOutput(void);

int processUserInput(void); //True == abort now
void getNextFragment(void);

void error(char *str); //Display error and exit
void warning(char *str); //Display error

