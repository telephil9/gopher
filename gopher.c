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
void message(char *s, ...);

Image *backi;
Image *fwdi;
Panel *root;
Panel *backp;
Panel *fwdp;
Panel *entryp;
Panel *urlp;
Panel *textp;
Panel *statusp;
Panel *urlp;
char *url;
Mouse *mouse;
Hist *hist = nil;

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

Link*
clonelink(Link *l)
{
	return mklink(l->addr, l->sel, l->type);
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

static char *Typestr[] = {
	"FILE", "DIR", "NS", "ERR", "HEX",
	"DOS", "UU", "?", "TELNET", "BIN",
	"MIRROR", "GIF", "IMG", "T3270", "DOC",
	"HTML", "", "SND", "EOF",
};

char*
seltypestr(int type)
{
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
	m->link = clonelink(l);
	m->text = nil;
	plrtstr(&m->text, 1000000, 0, 0, font, strdup(" "), 0, 0);
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
			n = mklink(strdup(f[1]+4), nil, Thtml); /* +4 skip URL: */
			break;
		default:
			n = mklink(netmkaddr(f[2], "tcp", f[3]), strdup(f[1]), type);
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
	char c, buf[255];
	int n, i;

	m = malloc(sizeof *m);
	if(m==nil)
		sysfatal("malloc: %r");
	m->link = clonelink(l);
	m->text = nil;
	plrtstr(&m->text, 1000000, 0, 0, font, strdup(" "), 0, 0);
	n = 0;
	for(;;){
		c = Bgetc(bp);
		if(c<0 || c=='.')
			break;
		else if(c=='\r' || c=='\n'){
			if(c=='\r' && Bgetc(bp)!='\n')
				Bungetc(bp);
			buf[n] = 0;
			plrtstr(&m->text, 1000000, 8, 0, font, strdup(buf), 0, 0);
			n = 0;
		}else if(c=='\t'){
			for(i=0; i<4; i++)
				buf[n++] = ' ';
		}else{
			buf[n++] = c;
		}
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
	if(fd < 0){
		message("unable to connect to %s: %r", l->addr);
		return nil;
	}
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
message(char *s, ...)
{
	static char buf[1024];
	char *out;
	va_list args;

	va_start(args, s);
	out = buf + vsnprint(buf, sizeof(buf), s, args);
	va_end(args);
	*out='\0';
	plinitlabel(statusp, PACKN|FILLX, buf);
	pldraw(statusp, screen);
	flushimage(display, 1);
}

char*
linktourl(Link *l)
{
	char *f[3], *a, *s;
	int n;

	a = strdup(l->addr);
	n = getfields(a, f, 3, 0, "!");
	if(n != 3)
		s = smprint("Url: gopher://%s%s", l->addr, l->sel);
	else if(atoi(f[2])!=70)
		s = smprint("Url: gopher://%s:%s%s", f[1], f[2], l->sel);
	else
		s = smprint("Url: gopher://%s%s", f[1], l->sel);
	free(a);	
	return s;	
}

void
seturl(Link *l)
{
	free(url);
	url = linktourl(l);
}

void
show(Gmenu *m)
{
	plinittextview(textp, PACKE|EXPAND, ZP, m->text, texthit);
	pldraw(textp, screen);
	plinitlabel(urlp, PACKN|FILLX, url);
	pldraw(urlp, screen);
	message("gopher!");
}

void 
freetext(Rtext *t){
	Rtext *tt;
	Link *l;

	tt = t;
	for(; t!=0; t = t->next){
		t->b=0;
		free(t->text);
		t->text = 0;
		if(l = t->user){
			t->user = 0;
			free(l->addr);
			if(l->sel!=nil && l->sel[0]!=0)
				free(l->sel);
			free(l);
		}
	}
	plrtfree(tt);
}

void
freehist(Hist *h)
{
	Hist *n;
	Gmenu *m;

	for(n = h->n; h; h = n){
		m = h->m;
		freetext(m->text);
		if(m->link!=nil){
			free(m->link->addr);
			free(m->link->sel);
			free(m->link);
		}
		free(h);
	}	
}

void
visit(Link *l)
{
	Gmenu *m;
	Hist *h;

	seturl(l);
	message("loading %s...", url);
	m = render(l);
	if(m==nil)
		return;
	show(m);
	h = malloc(sizeof *h);
	if(h == nil)
		sysfatal("malloc: %r");
/* FIXME
	if(hist != nil && hist->n != nil)
		freehist(hist->n);
*/
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
save(Link *l, char *name){
	char buf[1024];
	int ifd, ofd;

	ifd = dial(l->addr, 0, 0, 0);
	if(ifd < 0){
		message("save: %s: %r", name);
		return;
	}
	fprint(ifd, "%s\r\n", l->sel);
	ofd=create(name, OWRITE, 0666);
	if(ofd < 0){
		message("save: %s: %r", name);
		return;
	}
	switch(rfork(RFNOTEG|RFNAMEG|RFFDG|RFMEM|RFPROC|RFNOWAIT)){
	case -1:
		message("Can't fork: %r");
		break;
	case 0:
		dup(ifd, 0);
		close(ifd);
		dup(ofd, 1);
		close(ofd);

		snprint(buf, sizeof(buf),
			"{tput -p || cat} |[2] {aux/statusmsg -k %q >/dev/null || cat >/dev/null}", name);
		execl("/bin/rc", "rc", "-c", buf, nil);
		exits("exec");
	}
	close(ifd);
	close(ofd);
}

char*
linktofile(Link *l){
	char *n, *s;

	if(l==nil)
		return nil;
	n = l->sel;
	if(n==nil || n[0]==0)
		n = "/";
	if(s = strrchr(n, '/'))
		n = s+1;
	if(n[0]==0)
		n = "file";
	return n;
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
	case Tdos:
	case Tbinary:
	case Tbinhex:
	case Tuuencoded:
		snprint(buf, sizeof buf, "%s", linktofile(l));
		if(eenter("Save as:", buf, sizeof buf, mouse)>0){
			save(l, buf);
		}
		break;
	default:
		message("unhandled item type '%s'", Typestr[l->type]);
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
	seturl(hist->m->link);
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
	seturl(hist->m->link);
	show(hist->m);
}

void
entryhit(Panel *p, char *t)
{
	USED(p);
	switch(strlen(t)){
	case 0:
		return;
	case 1:
		switch(*t){
		case 'b':
			backhit(backp, 1);
			break;
		case 'n':
			nexthit(fwdp, 1);
			break;
		case 'q':
			exits(nil);
			break;
		default:
			message("unknown command %s", t);
			break;
		}
		break;
	default:
		visitaddr(t);
	}
	plinitentry(p, PACKN|FILLX, 0, "", entryhit);
	pldraw(p, screen);
}

void
mkpanels(void)
{
	Panel *p, *ybar, *xbar;

	root = plgroup(0, EXPAND);
	p = plgroup(root, PACKN|FILLX);
	statusp = pllabel(p, PACKN|FILLX, "gopher!");
	plplacelabel(statusp, PLACEW);
	plbutton(p, PACKW|BITMAP, backi, backhit);
	plbutton(p, PACKW|BITMAP, fwdi, nexthit);
	pllabel(p, PACKW, "Go:");
	entryp = plentry(p, PACKN|FILLX, 0, "", entryhit);
	p = plgroup(root, PACKN|FILLX);
	urlp = pllabel(p, PACKN|FILLX, "");
	plplacelabel(urlp, PLACEW);
	p = plgroup(root, PACKN|EXPAND);
	ybar = plscrollbar(p, PACKW|USERFL);
	xbar = plscrollbar(p, IGNORE);
	textp = pltextview(p, PACKE|EXPAND, ZP, nil, nil);
	plscroll(textp, xbar, ybar);
	plgrabkb(entryp);
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
	/* BUG: there is a redraw issue when scrolling
	   This fixes the issue albeit not properly */
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
	quotefmtinstall();
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
				plgrabkb(entryp);
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
			/* BUG: there is a redraw issue when scrolling
			   This fixes the issue albeit not properly */
			pldraw(textp, screen);
			break;
		}
	}
}

