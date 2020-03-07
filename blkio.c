#include "qed.h"

enum {
  ALIGN = 2,      /* align strings to (2^ALIGN)-byte boundaries in a block */
  ALIGNMASK = ((1 << ALIGN) - 1),
  SHUNT = 1, 
  BLKBITS = 12,
  BLKSHIFT = BLKBITS-1,
  BLKSIZE = (1 << BLKBITS),
  OFFMASK = BLKSIZE - ALIGNMASK,
  LOTSOFBITS = 12,
  MAXBLOCKS = ((1 << LOTSOFBITS)-1),
  BLMASK = MAXBLOCKS
};

#define getblock(a) (((a)>>BLKSHIFT) & BLMASK)
#define getoffset(a) (((a)<<SHUNT) & OFFMASK)
#define cookie(b,o) (((b)<<BLKSHIFT) + ((o)>>SHUNT))
#define align(a) (((a)+ALIGNMASK)&~ALIGNMASK)

char	ibuff[BLKSIZE];
int	iblock = -1;
int	oblock = 0;
char	obuff[BLKSIZE];
int	ooff;		/* offset of next byte in obuff */

void
initio(void)
{
	iblock = -1;
	oblock = 0;
	ooff = 0;
}

char
*getline(int tl, char *lbuf)
{
	char *bp, *lp;
	int nl;

	extern int read();

	lp = lbuf;
        nl = -getoffset(tl);
        tl = getblock(tl);
	do {
		if (nl<=0) {
			if (tl==oblock)
				bp = obuff;
			else {
				bp = ibuff;
				if (tl!=iblock) {
					iblock = -1;	/* signal protection */
					blkio(tl, bp, read);
					iblock = tl;
				}
			}
			tl++;
			bp -= nl;
			nl += BLKSIZE;
		}
		nl--;
	} while (*lp++ = *bp++);
	return(lbuf);
}

int
putline(void)
{
	char *op, *lp;
	int r;
	extern int write();

	modified();
	lp = linebuf;
	r = cookie(oblock,ooff);	/* ooff may be BLKSIZE! */
	op = obuff + ooff;
	do {
		if (op >= obuff+BLKSIZE) {
			/* delay updating oblock until after blkio succeeds */
			blkio(oblock, op=obuff, write);
			oblock++;
			ooff = 0;
		}
		if((*op = *lp++) == '\n') {
			*op++ = '\0';
			linebp = lp;
			break;
		}
	} while (*op++);
	ooff = align(op-obuff);
	return (r);
}

void
blkio(int b, char *buf, int (*iofcn)())
{
	if (b>=MAXBLOCKS
	|| (lseek(tfile, ((long) b) * ((long) BLKSIZE), 0)<0L)
	|| (*iofcn)(tfile, buf, BLKSIZE) != BLKSIZE) {
		error('T');
	}
}
