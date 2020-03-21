#include "qed.h"

enum {
  UP = 1,
  DOWN = 0
};

struct buffer *curbuf = buffer;
int	*linp = line;
int	appflag = 0;
int	pagesize = PAGESIZE;
int	prflag = 0;	/* prflag==1 ==> much code to get it right. use the startup buffer */
int	lastttyc = '\n';
int	gflag = 0;
int	tflag = 0;
int	vflag = 0;	/* initialization code turns it on */
int	uflag = 's';
int	dflag = 0;
int	initflag = 1;
int	*option[] = {
	&cflag,	&dflag,	&eflag,	&iflag,	&prflag, &tflag, &vflag,
};
int lchars[] = {'p','P','l','L','\0'};
int	opcs[] = {'c','d','e','i','p','T','v','\0'};
int	tfile = -1;
int	QEDFILE[]={'Q','E','D','F','I','L','E','\0'};

void
rescue(int sig)
{
	/* Save in qed.hup:[ab]q on hangup */
	signal(SIGHUP,SIG_IGN);
	startstring();
	copystring(utfstr_qedhup);
	setstring(FILEBUF);
	savall();
	lasterr=SIGHUP;
	quit();
}

int *
filea(void)
{
	struct string *sp;
	int i, d;
	int c;

	sp = &string[FILEBUF];
	startstring();
	d = 0;
	i = 0;
	while((c=sp->str[i]) != '\0'){
		addstring(c);
		i++;
		if(c == '/')
			d = i;
	}
	if((i-d) > 12)		/* file name is >= 13 characters long */
		string[NSTRING].str -= (i-(d+12));	/* truncate string */
	copystring(utfstr_colonaq);
	setstring(FILEBUF);
	return(sp->str);
}

int *
fileb(void)
{
	struct string *sp;

	sp = &string[FILEBUF];
	sp->str[sp->len-2] = 'b';
	return(sp->str);
}

void
savall(void)
{
	int fi;

	syncbuf();
	addr1 = buffer[0].zero + 1;
	addr2 = buffer[NBUFS-1].dol;
	if(addr1 > addr2){
		error('$');
		return;
	}
	if((io = creat(utf8(filea()), 0644)) < 0)
		error('o'|FILERR);
	putfile();
	exfile();
	if((fi = creat(utf8(fileb()), 0644)) < 0)
		error('o'|FILERR);
	write(fi, (byte *)buffer, sizeof buffer);
	write(fi, (byte *)strarea, sizeof strarea);
	shiftstring(DOWN);
	write(fi, (byte *)string, sizeof string);
	shiftstring(UP);
	close(fi);
}

void
restor(void)
{
	int t;
	struct buffer *b;
	int fi;
	int getfile();
	curbuf = buffer;
	if((t = open(utf8(filea()), O_RDONLY)) < 0){
		lastc = '\n';
		error('o'|FILERR);
	}
	initio();
	init();
	io = t;
	ninbuf = 0;
	append(getfile, dol);
	exfile();
	if((fi = open(utf8(fileb()),O_RDONLY)) < 0)
		error('o'|FILERR);
	if(read(fi,(byte *)buffer,sizeof buffer) != sizeof buffer
		|| read(fi, (byte *)strarea, sizeof strarea) != sizeof strarea
		|| read(fi, (byte *)string, sizeof string) != sizeof string)
		error('R');
	close(fi);
	shiftstring(UP);
	newbuf(0);
	error(0);	/* ==> error, but don't print anything. */
}

/*
 *	On INT, generate error '?'
 */

void
interrupt(int sig)
{
	signal(SIGINT, interrupt);
	if(iflag){
		lasterr=SIGINT;
		quit();
	}
	linp=line;
	putchar('\n');
	lastc = '\n';
	error('?');
}

int cleanup[] = {'b','a',' ','z','~',':','\n','\0'};
int setvflag[] = {'o','v','?','\0'};
int boot1[] = {'G','/','^','[','\0'};
int boot2[] = {']','.','+','\t','.','/','r','\n','\0'};

void (*onhup)(int);
void (*onquit)(int);
void (*onintr)(int);

int
main(int argc, char **argv)
{
        union pint_t uc;
	int *p1;
	int i;
	int buf;
	int rvflag;
	int *startup=(int *)0;
	byte *b;  /* for getenv result */

	argv++;
	onquit = signal(SIGQUIT, SIG_IGN);
	onhup = signal(SIGHUP, SIG_IGN);
	onintr = signal(SIGINT, SIG_IGN);
	rvflag = 1;
	for(i=0;i!=NSTRING;i++){
		string[i].str = nullstr;
		string[i].len=0;
	}
	/* initialize strfree */
	string[NSTRING].str = strchars;
	/* initialize core */
	core=(addr_t*)0;
	while(argc > 1 && **argv=='-'){
		switch(argv[0][1]){
		casedefault:
		default:
			rvflag = 0;
			break;
		case 'q':
		/* allow debugging quits? */
			signal(SIGQUIT, SIG_DFL);
			break;
		case 'i':
		/* allow interrupt quits? */
			iflag++;
			break;
		case 'e':
		/* Exit on error? */
			eflag++;
			break;
		case 'x':
			if(argc == 2)
				goto casedefault;
			startup = ucode(argv[1]);
			argv++;
			--argc;
		}
		argv++;
		--argc;
	}

	if((startup==0)&&(b=getenv(utf8(QEDFILE))))
	 	startup = ucode(b);

	curbuf = &buffer[0];
	init();
	if(onhup != SIG_IGN)
		signal(SIGHUP, rescue);
	if(onintr != SIG_IGN)
		signal(SIGINT, interrupt);
	/*
	 * Build the initialization code in register z~
	 */
	if(startup){
		startstring();
		copystring(startup);
		setstring(FILE(NBUFS-1));
		p1 = utfstr_br;
	} else
		p1 = utfstr_nul;
	startstring();
	copystring(p1);
	setvflag[2] = "rs"[rvflag];
	copystring(setvflag);
	/*
	 * z~ now has startup-buffer initialization; prepare the file list
	 * and generate a GLOBUF to read them in
	 */
	if(--argc > 0) {
		if(argc >= 53)	/* buffer names a-zA-Z */
			puts(utfstr_queryi);
		else {
			copystring(boot1);
			for(i=0; i<argc; i++)	/* argument files only */
				addstring(bname[i]);
			copystring(boot2);
			copystring(cleanup);
			setstring(NBUFS-1);
			buf = 0;
			while(argc > 0) {
				startstring();
				copystring(ucode(*argv));
				setstring(FILE(buf++));
				--argc;
				argv++;
			}
		}
	}
	else{
		copystring(cleanup);
		setstring(NBUFS-1);
	}
        uc.i=NBUFS-1;
	pushinp(STRING, uc, FALSE);
	setjmp(savej);
	lastttyc = '\n';
	commands();
	quit();
}

int	noaddr;

void
commands(void)
{
        union pint_t uc;
	addr_i a;
	int c, lastsep;
	int getfile(), gettty();
	int r;
	int changed;
	long locn, lseek();
	int startline;

	for (;;) {
	startline = (lastttyc == '\n' && peekc == 0);
	cprflag=prflag;
	c = '\n';
	for (addr1=0;;) {
		lastsep = c;
		a=address();
		c=getchar();
		if (c!=',' && c!=';')
			break;
		if (lastsep==',')
			error('a');
		if (a==0) {
			a = zero+1;
			if (a>dol)
				--a;
		}
		addr1 = a;
		if (c==';')
			dot = a;
	}
	if (lastsep!='\n' && a==0)
		a=dol;
	if((addr2=a) == 0) {
		addr2=dot;
		noaddr = TRUE;
	} else
		noaddr = FALSE;

	if(addr1 == 0)
		addr1 = addr2;

	cprflag=FALSE;
	switch(c){
	case 'a':
		setdot();
		setapp();
		append(gettty, addr2);
		continue;
	case 'b':
		if(posn((c=nextchar()), bname)<0){	/* browse command */
			setdot();
			nonzero();
			bcom();
			continue;
		}
		c = getaz('b');
		setnoaddr();
		chngbuf(c);
		continue;
	case 'c':
		setdot();
		nonzero();
		setapp();
		append(gettty, addr2);
		a = dot-(addr2-addr1+1);
		delete();
		dot = a;
		continue;
	case 'd':
		if(posn(nextchar(),utfstr_suffix) < 0)
			error('x');
		delete();
		continue;
	case 'E':
	case 'e':
		setnoaddr();
		if(c=='e' && !eok && cflag){
			eok=TRUE;
			error('e');
		}
		newfile(TRUE, SAVEALWAYS, utfstr_nul);
		delall();
		addr1 = zero;
		addr2 = zero;
		modified();	/* In case file open causes error */
		goto caseread;
	case 'f':
		setnoaddr();
		if((c = getchar()) != '\n'){
			ungetchar(c);
			if(newfile(FALSE, SAVEALWAYS, string[savedfile].str))
				modified();
			if(vflag)
				ncom('f');
		}
		else
			ncom('f');
		continue;
	case 'g':
		global(TRUE);
		continue;
	case 'G':
		globuf(TRUE);
		continue;
	case 'h':
		setnoaddr();
		if(nextchar()=='\n')
			error('x');
		if('0'<=peekc && peekc<='9')
			until(TRUE, getnum());
		else
			until(FALSE, 0);
		continue;
	case 'i':
		setdot();
		nonzero();
		setapp();
		append(gettty, addr2-1);
		continue;
	case 'j':
		setdot();
		if (addr1 == addr2 && lastsep == '\n'){
			addr1--;
			if(addr1 <= zero)
				error('$');
		}
		nonzero();
		join();
		continue;
	case 'k':
		c = getaz(c);
		setdot();
		nonzero();
		names[c] = core[addr2] | 01;
		continue;
	case 'm':
		move(FALSE);
		continue;
	case 'n':
	case 'N':
		ncom(c);
		continue;
	case 'o':
		setnoaddr();
		c = getchar();
		r=posn(c, opcs);
		if(r >= 0)
			setreset(option[r]);
		else switch(c) {
		case 'B':
			if(nextchar() == '\n')
				clearstring(BROWSE);
			else {
				startstring();
				while((c=getchar()) != '\n')
					addstring(c);
				copystring(utfstr_bignl);
				setstring(BROWSE);
			}
			break;
		case '?':
			if ((r = posn(getchar(), opcs)) < 0)
				error('O');
			settruth(*option[r]);
			break;
		case 'q':
			c = getchar();
			if(c=='s' || c =='r')
				signal(SIGQUIT, c=='r' ? SIG_IGN : SIG_DFL);
			else
				error('x');
			break;
		case 'u':
			c = getchar();
			if(c == 'r')
				uflag = 0;
			else if(posn(c, utfstr_slu) >= 0)
				uflag = c;
			else
				error('x');
			break;
		case 'b':
			if((r=getnum()) > 0)
				pagesize = r;
			if(posn(nextchar(), lchars) >=0)
				bformat = getchar();
			break;
		default:
			error('x');
		}
		continue;
	case '\n':
		if (a==0) {
			if(stackp != &stack[0] || !startline)
				continue;
			if(*string[BROWSE].str){
                                uc.i=0;
				pushinp(BRWS, uc, FALSE);
				continue;
			}
			a = dot+1;
			addr2 = a;
			addr1 = a;
		}
		if (lastsep==';')
			addr1 = a;
		c = 'p';	/* fall through */
	case 'L':
	case 'l':
	case 'p':
	case 'P':
		display(c);
		continue;
	case EOF:
		return;
	case 'Q':
	case 'q':
		setnoaddr();
		if(c!=EOF && (!startline || getchar()!='\n'))
			error('x');
		if(c!='Q' && !qok){
			struct buffer *bp;
			syncbuf();
			qok=TRUE;
			for(bp=buffer; bp<&buffer[NBUFS]; bp++)
				if(bp->cflag && (bp->dol>bp->zero ||
					string[FILE(bp-buffer)].str[0]))
					error('q');
		}
		quit();
	case 'r':
		newfile(TRUE, SAVEIFFIRST, string[savedfile].str);
	caseread:
		if((io = open(utf8(string[FILEBUF].str), O_RDONLY)) < 0){
			if(initflag){
				putchar('?');
				putchar('o');
				putchar(' ');
				puts(string[FILEBUF].str);
				continue;
			}
			lastc = '\n';
			error('o'|FILERR);
		}
		setall();
		changed = (zero!=dol);
		uioinit(io,uio);
		if(uioinitrd(uio)<0) {
			lastc = '\n';
			error('o'|FILERR);
		}
		ninbuf = 0;
		append(getfile, addr2);
		if(eqstr(string[savedfile].str, string[FILEBUF].str))
			if(cflag = changed)	/* Assignment = */
				modified();
		/* else append got cflag right */
		exfile();
		continue;
	case 'R':
		setnoaddr();
		newfile(TRUE, SAVENEVER, utfstr_q);
		restor();
		continue;
	case 's':
		setdot();
		nonzero();
		substitute(stackp != &stack[0], -1);
		continue;
	case 'S':
		setnoaddr();
		newfile(TRUE, SAVENEVER, utfstr_q);
		savall();
		continue;
	case 't':
		move(TRUE);
		continue;
	case 'u':
		setnoaddr();
		undo();
		modified();
		continue;
	case 'v':
		global(FALSE);
		continue;
	case 'V':
		globuf(FALSE);
		continue;
	case 'W':
	case 'w':
		if(addr2==0 && dol==zero)
			error('$');
		setall();
		if(newfile(TRUE, SAVEIFFIRST, string[savedfile].str))
			changed = cflag;
		else
			changed = (addr1>(zero+1) || addr2!=dol);
		if(c=='w' || (io=open(utf8(string[FILEBUF].str),O_WRONLY))==-1){
		  Create:
			if ((io = creat(utf8(string[FILEBUF].str), 0666)) < 0)
				error('o'|FILERR);
		}else{
			if((locn=lseek(io, 0L, 2)) == -1L)
				goto Create;
			if(locn != 0L)	/* W on non-empty file */
				changed = TRUE;	/* PHEW! figured it out */
		}
		uioinit(io,uio);
		putfile();
		if(cflag = changed)	/* Assignment = */
			modified();
		exfile();
		continue;
	case 'x':
		setdot();
		nonzero();
		xform();
		continue;
	case 'y':
		jump();
		continue;
	case 'z':
		strcom(getaz('z'));
		continue;
	case 'Z':
		setnoaddr();
		if((c=getchar())!=' ' && c!='\n')
			error('x');
		delall();
		cflag=FALSE;
		clearstring(savedfile);
		continue;
	case '"':
		setdot();
		dot=addr2;
		comment();
		continue;
	case '=':
		setall();
		putdn((addr2-zero)&077777);
		continue;
	case '>':
	case '<':
	case '|':
		setall();
	case '!':
		Unix(c);
		continue;
	case '#':
		setnoaddr();
		allnums();
		continue;
	case '%':
		setnoaddr();
		allstrs();
		continue;
	}
	error('x');
	}
}

void
setreset(int *opt)
{
	int c;

	c = getchar();
	if(c!='s' && c!= 'r')
		error('x');
	*opt = (c=='s');
}

void
delall(void)
{
	if(dol!=zero){
		addr1=zero+1;
		addr2=dol;
		delete();
	}
}
