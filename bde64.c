/*
    Copyright 2006,2007,2008,2009,2010 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    http://www.gnu.org/licenses/gpl-2.0.txt
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bde64.h"



/*int main(int argc, char *argv[]) {
    u8      *out,*in;
    int totlen=4;
        out = base64_encode("ciao", &totlen);
        printf("%s\n",out);
       
}*/

u8 *base64_encode(u8 *data, int *size) {
    int     len,
            a,
            b,
            c;
    u8      *buff,
            *p;
    static const u8 base[64] = {
        'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
    };

    if(!size || (*size < 0)) {      // use size -1 for auto text size!
        len = strlen(data);
    } else {
        len = *size;
    }
    buff = malloc(((len / 3) << 2) + 6);
    if(!buff) return(NULL);

    p = buff;
    do {
        a = data[0];
        b = data[1];
        c = data[2];
        *p++ = base[(a >> 2) & 63];
        *p++ = base[(((a &  3) << 4) | ((b >> 4) & 15)) & 63];
        *p++ = base[(((b & 15) << 2) | ((c >> 6) &  3)) & 63];
        *p++ = base[c & 63];
        data += 3;
        len  -= 3;
    } while(len > 0);
    for(*p = 0; len < 0; len++) *(p + len) = '=';

    if(size) *size = p - buff;
    return(buff);
}



