typedef struct Gmenu Gmenu;
typedef struct Link Link;
typedef struct Hist Hist;

struct Gmenu
{
	Link	*link;
	Rtext	*text;
};

struct Link
{
	char *addr;
	char *sel;
	int  type;
};

struct Hist
{
	Hist *p;
	Hist *n;
	Gmenu *m;
};

enum
{
	Ttext,
	Tmenu,
	Tns,
	Terror,
	Tbinhex,
	Tdos,
	Tuuencoded,
	Tsearch,
	Ttelnet,
	Tbinary,
	Tmirror,
	Tgif,
	Timage,
	Tt3270,
	Tdoc,
	Thtml,
	Tinfo,
	Tsound,
	Teof
};
