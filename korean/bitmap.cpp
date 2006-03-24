 /*
  * Unitex
  *
  * Copyright (C) 2001-2006 Universit� de Marne-la-Vall�e <unitex@univ-mlv.fr>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License as published by the Free Software Foundation; either
  * version 2.1 of the License, or (at your option) any later version.
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  * 
  * You should have received a copy of the GNU Lesser General Public
  * License along with this library; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
  *
  */
//
// bitmap.cpp
//
unsigned int bitSetL[32] = {
0x00000001,0x00000002,0x00000004,0x00000008,
0x00000010,0x00000020,0x00000040,0x00000080,
0x00000100,0x00000200,0x00000400,0x00000800,
0x00001000,0x00002000,0x00004000,0x00008000,
0x00010000,0x00020000,0x00040000,0x00080000,
0x00100000,0x00200000,0x00400000,0x00800000,
0x01000000,0x02000000,0x04000000,0x08000000,
0x10000000,0x20000000,0x40000000,0x80000000
};

unsigned int bitMaskL[32] = {
0x00000001,0x00000003,0x00000007,0x0000000F,
0x0000001F,0x0000003F,0x0000007F,0x000000FF,
0x000001FF,0x000003FF,0x000007FF,0x00000FFF,
0x00001FFF,0x00003FFF,0x00007FFF,0x0000FFFF,
0x0001FFFF,0x0003FFFF,0x0007FFFF,0x000FFFFF,
0x001FFFFF,0x003FFFFF,0x007FFFFF,0x00FFFFFF,
0x01FFFFFF,0x03FFFFFF,0x07FFFFFF,0x0FFFFFFF,
0x1FFFFFFF,0x3FFFFFFF,0x7FFFFFFF,0xFFFFFFFF
};

 unsigned short bitSetW[16] = {
0x0001,0x0002,0x0004,0x0008,
0x0010,0x0020,0x0040,0x0080,
0x0100,0x0200,0x0400,0x0800,
0x1000,0x2000,0x4000,0x8000
};

 unsigned short bitMaskW[16] = {
0x0001,0x0003,0x0007,0x000F,
0x001F,0x003F,0x007F,0x00FF,
0x01FF,0x03FF,0x07FF,0x0FFF,
0x1FFF,0x3FFF,0x7FFF,0xFFFF
};
 unsigned char bitSetB[8] = {
0x01,0x02,0x04,0x08,
0x10,0x20,0x40,0x80
};

 unsigned char bitMaskB[8] = {
0x01,0x03,0x07,0x0F,
0x1F,0x3F,0x7F,0xFF
};
