#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>
#include <keyboard.h>
#include <panel.h>
#include <bio.h>
#include <plumb.h>
#include "dat.h"
#include "icons.h"

void texthit(Panel *p, int b, Rtext *t);

Image *backi;
Image *fwdi;
Panel *root;
Panel *backp;
Panel *fwdp;
Panel *urlp;
Panel *textp;
Panel *statusp;
char *url;
Mouse *mouse;
Hist *hist = nil;

Gmenu*
mkmenu(Link *l)
{
	Gmenu *m;

	m = malloc(sizeof *m);
	if(m==nil)
		sysfatal("malloc: %r");
	m->link = l;
	m->text = nil;
	return m;
}

Link*
mklink(char *addr, char *sel, int type)
{
	Link *l;

	l = malloc(sizeof *l);
	if(l==nil)
		sysfatal("malloc: %r");
	l->addr = strdup(addr);
	l->sel  = sel!=nil ? strdup(sel) : nil;
	l->type = type;
	return l;
}

int
seltype(char c)
{
	int t;

	t = Tinfo;
	switch(c){
	case '0': t = Ttext; break;
	case '1': t = Tmenu; break;
	case '2': t = Tns; break;
	case '3': t = Terror; break;
	case '4': t = Tbinhex; break;
	case '5': t = Tdos; break;
	case '6': t = Tuuencoded; break;
	case '7': t = Tsearch; break;
	case '8': t = Ttelnet; break;
	case '9': t = Tbinary; break;
	case '+': t = Tmirror; break;
	case 'g': t = Tgif; break;
	case 'I': t = Timage; break;
	case 'T': t = Tt3270; break;
	case 'd': t = Tdoc; break;
	case 'h': t = Thtml; break;
	case 'i': t = Tinfo; break;
	case 's': t = Tsound; break;
	case '.': t = Teof; break;
	default:
		fprint(2, "unknown seltype '%c'\n", c);
		break;
	}
	return t;
}

char*
seltypestr(int type)
{
	static char *Typestr[] = {
		"FILE", "DIR", "NS", "ERR", "HEX",
		"DOS", "UU", "?", "TELNET", "BIN",
		"MIRROR", "GIF", "IMG", "T3270", "DOC",
		"HTML", "", "SND", "EOF",
	};
	return smprint("%6s", Typestr[type]);
};


Gmenu*
rendermenu(Link *l, Biobuf *bp)
{
	char *s, *f[5], *t;
	Gmenu *m;
	Link *n;
	int type;

	m = malloc(sizeof *m);
	if(m==nil)
		sysfatal("malloc: %r");
	m->link = l;
	m->text = nil;
	plrtstr(&m->text, 1000000, 0, 0, font, " ", 0, 0);
	for(;;){
		n = nil;
		s = Brdstr(bp, '\n', 0);
		if(s==nil || s[0]=='.')
			break;
		type = seltype(s[0]);
		getfields(s+1, f, 5, 0, "\t\r\n");
		switch(type){
		case Tinfo:
			break;
		case Thtml:
			n = mklink(f[1]+4, nil, Thtml); /* +4 skip URL: */
			break;
		default:
			n = mklink(netmkaddr(f[2], "tcp", f[3]), f[1], type);
			break;
		}
		t = strdup(f[0]);
		plrtstr(&m->text, 1000000, 8, 0, font, seltypestr(type), PL_HEAD, 0);
		if(type == Tinfo)
			plrtstr(&m->text, 8, 0, 0, font, t, 0, 0);
		else
			plrtstr(&m->text, 8, 0, 0, font, t, PL_HOT, n);

	}
	return m;
}

Gmenu*
rendertext(Link *l, Biobuf *bp)
{
	Gmenu *m;
	char *s;
	int n;

	m = malloc(sizeof *m);
	if(m==nil)
		sysfatal("malloc: %r");
	m->link = l;
	m->text = nil;
	plrtstr(&m->text, 1000000, 0, 0, font, " ", 0, 0);
	for(;;){
		s = Brdstr(bp, '\n', 0);
		if(s==nil || s[0]=='.')
			break;
		n = Blinelen(bp);
		s[n-1] = 0;
		if(s[n-2]=='\r')
			s[n-2] = 0;
		if(s[0]=='\t'){
			plrtstr(&m->text, 1000000, 8, 0, font, "    ", 0, 0);
			plrtstr(&m->text, 4, 0, 0, font, s+1, 0, 0);
		}else
			plrtstr(&m->text, 1000000, 8, 0, font, s, 0, 0);
	}
	return m;
}

Gmenu*
render(Link *l)
{
	int fd;
	Biobuf *bp;
	Gmenu *m;

	fd = dial(l->addr, 0, 0, 0);
	if(fd < 0)
		sysfatal("dial: %r");
	fprint(fd, "%s\r\n", l->sel);
	bp = Bfdopen(fd, OREAD);
	if(bp==nil){
		close(fd);
		sysfatal("bfdopen: %r");
	}
	switch(l->type){
	case Tmenu:
		m = rendermenu(l, bp);
		break;
	case Ttext:
		m = rendertext(l, bp);
		break;
	default:
		/* TODO error */
		m = nil;
		break;
	}
	Bterm(bp);
	close(fd);
	return m;
}

void
show(Gmenu *m)
{
	plinittextview(textp, PACKE|EXPAND, ZP, m->text, texthit);
	pldraw(textp, screen);
}

void
visit(Link *l)
{
	Gmenu *m;
	Hist *h;

	m = render(l);
	show(m);
	h = malloc(sizeof *h);
	if(h == nil)
		sysfatal("malloc: %r");
	h->p = hist;
	h->n = nil;
	h->m = m;
	hist = h;
}

void
visitaddr(char *addr)
{
	visit(mklink(netmkaddr(addr, "tcp", "70"), "", Tmenu));
}

void
plumburl(char *u)
{
	int fd;

	fd = plumbopen("send", OWRITE|OCEXEC);
	if(fd<0)
		return;
	plumbsendtext(fd, "gopher", nil, nil, u);
	close(fd);
}

void
dupfds(int fd, ...)
{
	int mfd, n, i;
	va_list arg;
	Dir *dir;

	va_start(arg, fd);
	for(mfd = 0; fd >= 0; fd = va_arg(arg, int), mfd++)
		if(fd != mfd)
			if(dup(fd, mfd) < 0)
				sysfatal("dup: %r");
	va_end(arg);
	if((fd = open("/fd", OREAD)) < 0)
		sysfatal("open: %r");
	n = dirreadall(fd, &dir);
	for(i=0; i<n; i++){
		if(strstr(dir[i].name, "ctl"))
			continue;
		fd = atoi(dir[i].name);
		if(fd >= mfd)
			close(fd);
	}
	free(dir);
}

void
page(Link *l)
{
	int fd;

	fd = dial(l->addr, 0, 0, 0);
	if(fd < 0)
		sysfatal("dial: %r");
	fprint(fd, "%s\r\n", l->sel);	
	switch(rfork(RFFDG|RFPROC|RFMEM|RFREND|RFNOWAIT|RFNOTEG)){
	case -1:
		fprint(2, "Can't fork!");
		break;
	case 0:
		dupfds(fd, 1, 2, -1);
		execl("/bin/rc", "rc", "-c", "page -w", nil);
		_exits(0);
	}
	close(fd);
}

void
texthit(Panel *p, int b, Rtext *t)
{
	Link *l;
	char *s, buf[1024] = {0};

	USED(p);
	if(b!=1)
		return;
	if(t->user==nil)
		return;
	l = t->user;
	switch(l->type){
	case Tmenu:
	case Ttext:
		visit(l);
		break;
	case Thtml:
		plumburl(l->addr);
		break;
	case Tdoc:
	case Tgif:
	case Timage:
		page(l);
		break;
	case Tsearch:
		if(eenter("Search:", buf, sizeof buf, mouse)>0){
			s = smprint("%s\t%s", l->sel, buf);
			visit(mklink(l->addr, s, Tmenu));
			free(s);
		}
		break;
	}		
}

void
backhit(Panel *p, int b)
{
	USED(p);
	if(b!=1)
		return;
	if(hist==nil || hist->p==nil)
		return;
	hist->p->n = hist;
	hist = hist->p;
	show(hist->m);
}

void
nexthit(Panel *p, int b)
{
	USED(p);
	if(b!=1)
		return;
	if(hist==nil || hist->n==nil)
		return;
	hist = hist->n;
	show(hist->m);
}

void
entryhit(Panel *p, char *t)
{
	USED(p);
	if(strlen(t)<=0)
		return;
	visitaddr(t);
	plinitentry(p, PACKN|FILLX, 0, "", entryhit);
	pldraw(p, screen);
}

void
mkpanels(void)
{
	Panel *p, *ybar, *xbar;

	root = plgroup(0, EXPAND);
	p = plframe(root, PACKN|FILLX);
	plbutton(p, PACKW|BITMAP, backi, backhit);
	plbutton(p, PACKW|BITMAP, fwdi, nexthit);
	pllabel(p, PACKW, "Go:");
	plentry(p, PACKN|FILLX, 0, "", entryhit);
	p = plgroup(root, PACKN|EXPAND);
	ybar = plscrollbar(p, PACKW|USERFL);
	xbar = plscrollbar(p, IGNORE);
	textp = pltextview(p, PACKE|EXPAND, ZP, nil, nil);
	plscroll(textp, xbar, ybar);
	statusp = pllabel(root, PACKN|FILLX, url);
	plplacelabel(statusp, PLACEW);
}

void
eresized(int new)
{
	if(new && getwindow(display, Refnone)<0)
		sysfatal("cannot reattach: %r");
	plpack(root, screen->r);
	pldraw(root, screen);
}

Image*
loadicon(Rectangle r, uchar *data, int ndata)
{
	Image *i;
	int n;

	i = allocimage(display, r, RGBA32, 0, DNofill);
	if(i==nil)
		sysfatal("allocimage: %r");
	n = loadimage(i, r, data, ndata);
	if(n<0)
		sysfatal("loadimage: %r");
	return i;
}

void
loadicons(void)
{
	Rectangle r = Rect(0,0,16,16);
	
	backi = loadicon(r, ibackdata, sizeof ibackdata);
	fwdi  = loadicon(r, ifwddata, sizeof ifwddata);
}

void scrolltext(int dy, int whence)
{
	Scroll s;

	s = plgetscroll(textp);
	switch(whence){
	case 0:
		s.pos.y = dy;
		break;
	case 1:
		s.pos.y += dy;
		break;
	case 2:
		s.pos.y = s.size.y+dy;
		break;
	}
	if(s.pos.y > s.size.y)
		s.pos.y = s.size.y;
	if(s.pos.y < 0)
		s.pos.y = 0;
	plsetscroll(textp, s);
	pldraw(textp, screen);
}
	
void
main(int argc, char *argv[])
{
	Event e;
	char *url;

	if(argc == 2)
		url = argv[1];
	else
		url = "gopher.floodgap.com";

	if(initdraw(nil, nil, "gopher")<0)
		sysfatal("initdraw: %r");
	einit(Emouse|Ekeyboard);
	plinit(screen->depth);
	loadicons();
	mkpanels();
	visitaddr(url);
	eresized(0);
	for(;;){
		switch(event(&e)){
		case Ekeyboard:
			switch(e.kbdc){
			default:
				plkeyboard(e.kbdc);
				break;
			case Khome:
				scrolltext(0, 0);
				break;
			case Kup:
				scrolltext(-textp->size.y/4, 1);
				break;
			case Kpgup:
				scrolltext(-textp->size.y/2, 1);
				break;
			case Kdown:
				scrolltext(textp->size.y/4, 1);
				break;
			case Kpgdown:
				scrolltext(textp->size.y/2, 1);
				break;
			case Kend:
				scrolltext(-textp->size.y, 2);
				break;
			case Kdel:
				exits(nil);
				break;
			}
			break;
		case Emouse:
			mouse = &e.mouse;
			if(mouse->buttons & (8|16) && ptinrect(mouse->xy, textp->r)){
				if(mouse->buttons & 8)
					scrolltext(textp->r.min.y - mouse->xy.y, 1);
				else
					scrolltext(mouse->xy.y - textp->r.min.y, 1);
				break;
			}
			plmouse(root, mouse);
			pldraw(textp, screen);
			break;
		}
	}
}

