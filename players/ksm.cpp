/*
 * ksm.cpp - KSM Player for AdPlug by Simon Peter (dn.tlp@gmx.net)
 *
 * NOTES:
 * This is a nearly direct adaption of the original playksm
 * by Ken Silverman.
 */

#include <fstream.h>

#include "ksm.h"

static const unsigned int adlibfreq[63] = {
	0,
	2390,2411,2434,2456,2480,2506,2533,2562,2592,2625,2659,2695,
	3414,3435,3458,3480,3504,3530,3557,3586,3616,3649,3683,3719,
	4438,4459,4482,4504,4528,4554,4581,4610,4640,4673,4707,4743,
	5462,5483,5506,5528,5552,5578,5605,5634,5664,5697,5731,5767,
	6486,6507,6530,6552,6576,6602,6629,6658,6688,6721,6755,6791,
	7510};

/*** public methods **************************************/

bool CksmPlayer::load(istream &f)
{
	int i;
	char instbuf[11];

	f.read(trinst,16);
	f.read(trquant,16);
	f.read(trchan,16);
	f.ignore(16);
	f.read(trvol,16);
	f.read((char *)&numnotes,2);
	note = new unsigned long [numnotes];
	f.read((char *)note,numnotes << 2);

	if (!trchan[11]) {
		drumstat = 0;
		numchans = 9;
	}
	if (trchan[11] == 1)
	{
		for(i=0;i<11;i++)
			instbuf[i] = inst[trinst[11]][i];
		instbuf[1] = ((instbuf[1]&192)|(trvol[11])^63);
		setinst(6,instbuf[0],instbuf[1],instbuf[2],instbuf[3],instbuf[4],instbuf[5],instbuf[6],instbuf[7],instbuf[8],instbuf[9],instbuf[10]);
		for(i=0;i<5;i++)
			instbuf[i] = inst[trinst[12]][i];
		for(i=5;i<11;i++)
			instbuf[i] = inst[trinst[15]][i];
		instbuf[1] = ((instbuf[1]&192)|(trvol[12])^63);
		instbuf[6] = ((instbuf[6]&192)|(trvol[15])^63);
		setinst(7,instbuf[0],instbuf[1],instbuf[2],instbuf[3],instbuf[4],instbuf[5],instbuf[6],instbuf[7],instbuf[8],instbuf[9],instbuf[10]);
		for(i=0;i<5;i++)
			instbuf[i] = inst[trinst[14]][i];
		for(i=5;i<11;i++)
			instbuf[i] = inst[trinst[13]][i];
		instbuf[1] = ((instbuf[1]&192)|(trvol[14])^63);
		instbuf[6] = ((instbuf[6]&192)|(trvol[13])^63);
		setinst(8,instbuf[0],instbuf[1],instbuf[2],instbuf[3],instbuf[4],instbuf[5],instbuf[6],instbuf[7],instbuf[8],instbuf[9],instbuf[10]);
		drumstat = 32;
		numchans = 6;
	}

	rewind(0);
	return true;
}

bool CksmPlayer::update()
{
	int i,j,quanter,bufnum,chan,drumnum,freq;
	int track,volevel,volval;
	unsigned long temp,templong;

	count++;
	if (count >= countstop)
	{
		bufnum = 0;
		while (count >= countstop)
		{
			templong = note[nownote];
			track = (int)((templong>>8)&15);
			if ((templong&192) == 0)
			{
				i = 0;
				while (((chanfreq[i] != (templong&63)) || (chantrack[i] != ((templong>>8)&15))) && (i < numchans))
					i++;
				if (i < numchans)
				{
					databuf[bufnum] = (char)0; bufnum++;
					databuf[bufnum] = (unsigned char)(0xb0+i); bufnum++;
					databuf[bufnum] = (unsigned char)((adlibfreq[templong&63]>>8)&223); bufnum++;
					chanfreq[i] = 0;
					chanage[i] = 0;
				}
			}
			else
			{
				volevel = trvol[track];
				if ((templong&192) == 128)
				{
					volevel -= 4;
					if (volevel < 0)
						volevel = 0;
				}
				if ((templong&192) == 192)
				{
					volevel += 4;
					if (volevel > 63)
						volevel = 63;
				}
				if (track < 11)
				{
					temp = 0;
					i = numchans;
					for(j=0;j<numchans;j++)
						if ((countstop - chanage[j] >= temp) && (chantrack[j] == track))
						{
							temp = countstop - chanage[j];
							i = j;
						}
					if (i < numchans)
					{
						databuf[bufnum] = (char)0, bufnum++;
						databuf[bufnum] = (unsigned char)(0xb0+i); bufnum++;
						databuf[bufnum] = (unsigned char)0; bufnum++;
						volval = (inst[trinst[track]][1]&192)+(volevel^63);
						databuf[bufnum] = (char)0, bufnum++;
						databuf[bufnum] = (unsigned char)(0x40+op_table[i]+3); bufnum++;
						databuf[bufnum] = (unsigned char)volval; bufnum++;
						databuf[bufnum] = (char)0, bufnum++;
						databuf[bufnum] = (unsigned char)(0xa0+i); bufnum++;
						databuf[bufnum] = (unsigned char)(adlibfreq[templong&63]&255); bufnum++;
						databuf[bufnum] = (char)0, bufnum++;
						databuf[bufnum] = (unsigned char)(0xb0+i); bufnum++;
						databuf[bufnum] = (unsigned char)((adlibfreq[templong&63]>>8)|32); bufnum++;
						chanfreq[i] = templong&63;
						chanage[i] = countstop;
					}
				}
				else if ((drumstat&32) > 0)
				{
					freq = adlibfreq[templong&63];
					switch(track)
					{
						case 11: drumnum = 16; chan = 6; freq -= 2048; break;
						case 12: drumnum = 8; chan = 7; freq -= 2048; break;
						case 13: drumnum = 4; chan = 8; break;
						case 14: drumnum = 2; chan = 8; break;
						case 15: drumnum = 1; chan = 7; freq -= 2048; break;
					}
					databuf[bufnum] = (char)0, bufnum++;
					databuf[bufnum] = (unsigned char)(0xa0+chan); bufnum++;
					databuf[bufnum] = (unsigned char)(freq&255); bufnum++;
					databuf[bufnum] = (char)0, bufnum++;
					databuf[bufnum] = (unsigned char)(0xb0+chan); bufnum++;
					databuf[bufnum] = (unsigned char)((freq>>8)&223); bufnum++;
					databuf[bufnum] = (char)0, bufnum++;
					databuf[bufnum] = (unsigned char)(0xbd); bufnum++;
					databuf[bufnum] = (unsigned char)(drumstat&(255-drumnum)); bufnum++;
					drumstat |= drumnum;
					if ((track == 11) || (track == 12) || (track == 14))
					{
						volval = (inst[trinst[track]][1]&192)+(volevel^63);
						databuf[bufnum] = (char)0, bufnum++;
						databuf[bufnum] = (unsigned char)(0x40+op_table[chan]+3); bufnum++;
						databuf[bufnum] = (unsigned char)(volval); bufnum++;
					}
					else
					{
						volval = (inst[trinst[track]][6]&192)+(volevel^63);
						databuf[bufnum] = (char)0, bufnum++;
						databuf[bufnum] = (unsigned char)(0x40+op_table[chan]); bufnum++;
						databuf[bufnum] = (unsigned char)(volval); bufnum++;
					}
					databuf[bufnum] = (char)0, bufnum++;
					databuf[bufnum] = (unsigned char)(0xbd); bufnum++;
					databuf[bufnum] = (unsigned char)(drumstat); bufnum++;
				}
			}
			nownote++;
			if (nownote >= numnotes) {
				nownote = 0;
				songend = true;
			}
			templong = note[nownote];
			if (nownote == 0)
				count = (templong>>12)-1;
			quanter = (240/trquant[(templong>>8)&15]);
			countstop = (((templong>>12)+(quanter>>1)) / quanter) * quanter;
		}
		for(i=0;i<bufnum;i+=3)
			opl->write(databuf[i+1],databuf[i+2]);
	}
	return !songend;
}

void CksmPlayer::rewind(unsigned int subsong)
{
	int i,j,k;
	unsigned char instbuf[11];
	unsigned long templong;

	songend = false;
	opl->init(); opl->write(1,32); opl->write(4,0); opl->write(8,0); opl->write(0xbd,drumstat);

	for(i=0;i<numchans;i++)
	{
		chantrack[i] = 0;
		chanage[i] = 0;
	}
	j = 0;
	for(i=0;i<16;i++)
		if ((trchan[i] > 0) && (j < numchans))
		{
			k = trchan[i];
			while ((j < numchans) && (k > 0))
			{
				chantrack[j] = i;
				k--;
				j++;
			}
		}
	for(i=0;i<numchans;i++)
	{
		for(j=0;j<11;j++)
			instbuf[j] = inst[trinst[chantrack[i]]][j];
		instbuf[1] = ((instbuf[1]&192)|(63-trvol[chantrack[i]]));
		setinst(i,instbuf[0],instbuf[1],instbuf[2],instbuf[3],instbuf[4],instbuf[5],instbuf[6],instbuf[7],instbuf[8],instbuf[9],instbuf[10]);
		chanfreq[i] = 0;
	}
	k = 0;
	templong = *note;
	count = (templong>>12)-1;
	countstop = (templong>>12)-1;
	nownote = 0;
}

std::string CksmPlayer::getinstrument(unsigned int n)
{
	if(trchan[n])
		return std::string(instname[trinst[n]]);
	else
		return std::string();
}

void CksmPlayer::loadinsts(istream &f)
{
	int i;

	for(i=0;i<256;i++) {
		f.read(instname[i],20);
		f.read(inst[i],11);
		f.ignore(2);
	}
}

/*** private methods *************************************/

void CksmPlayer::setinst(int chan,
						 unsigned char v0,unsigned char v1,unsigned char v2,
						 unsigned char v3,unsigned char v4,unsigned char v5,
						 unsigned char v6,unsigned char v7,unsigned char v8,
						 unsigned char v9,unsigned char v10)
{
	int offs;

	opl->write(0xa0+chan,0);
	opl->write(0xb0+chan,0);
	opl->write(0xc0+chan,v10);
	offs = op_table[chan];
	opl->write(0x20+offs,v5);
	opl->write(0x40+offs,v6);
	opl->write(0x60+offs,v7);
	opl->write(0x80+offs,v8);
	opl->write(0xe0+offs,v9);
	offs+=3;
	opl->write(0x20+offs,v0);
	opl->write(0x40+offs,v1);
	opl->write(0x60+offs,v2);
	opl->write(0x80+offs,v3);
	opl->write(0xe0+offs,v4);
}
