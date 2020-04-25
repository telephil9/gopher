</$objtype/mkfile

TARG=gopher
LIB=libpanel/libpanel.$O.a
OFILES=gopher.$O
HFILES=dat.h libpanel/panel.h libpanel/rtext.h
BIN=/$objtype/bin/

</sys/src/cmd/mkone

CFLAGS=-FTVw -Ilibpanel

$LIB:V:
	cd libpanel
	mk

clean nuke:V:
	@{ cd libpanel; mk $target }
	rm -f *.[$OS] [$OS].out $TARG
