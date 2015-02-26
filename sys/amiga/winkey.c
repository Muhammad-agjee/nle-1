/* NetHack 3.5	winkey.c	$NHDT-Date$  $NHDT-Branch$:$NHDT-Revision$ */
/* NetHack 3.5	winkey.c	$Date: 2009/05/06 10:48:37 $  $Revision: 1.3 $ */
/*    SCCS Id: @(#)winkey.c    3.5    1993/04/02 */
/* Copyright (c) Gregg Wonderly, Naperville, Illinois,  1991,1992,1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "NH:sys/amiga/windefs.h"
#include "NH:sys/amiga/winext.h"
#include "NH:sys/amiga/winproto.h"

amii_nh_poskey(x, y, mod)
    int*x, *y, *mod;
{
    struct amii_WinDesc *cw;
    WETYPE type;
    struct RastPort *rp;
    struct Window *w;

    if( cw = amii_wins[WIN_MESSAGE] )
    {
	cw->wflags &= ~FLMAP_SKIP;
	if( scrollmsg )
	    cw->wflags |= FLMSG_FIRST;
	cw->disprows = 0;
    }

    if( WIN_MAP != WIN_ERR && (cw = amii_wins[ WIN_MAP ]) && ( w = cw->win ) )
    {
	cursor_on( WIN_MAP );
    }
    else
	panic( "no MAP window opened for nh_poskey\n" );

    rp = w->RPort;

    while( 1 )
    {
	type = WindowGetevent( );
	if( type == WEMOUSE )
	{
	    *mod = CLICK_1;
	    if( lastevent.un.mouse.qual )
		*mod = 0;

	    /* X coordinates are 1 based, Y are 1 based. */
	    *x = ( (lastevent.un.mouse.x - w->BorderLeft) / mxsize ) + 1;
	    *y = ( ( lastevent.un.mouse.y - w->BorderTop - MAPFTBASELN ) /
			mysize ) + 1;
#ifdef	CLIPPING
	    if( clipping )
	    {
		*x += clipx;
		*y += clipy;
	    }
#endif
	    return( 0 );
	}
	else if( type == WEKEY )
	{
	    lastevent.type = WEUNK;
	    return( lastevent.un.key );
	}
    }
}

int
amii_nhgetch()
{
    int ch;
    struct amii_WinDesc *cw=amii_wins[WIN_MESSAGE];

    if( WIN_MAP != WIN_ERR && amii_wins[ WIN_MAP ] )
    {
	cursor_on( WIN_MAP );
    }
    if(cw)
	cw->wflags &= ~FLMAP_SKIP;

    ch = WindowGetchar();
    return( ch );
}

void
amii_get_nh_event()
{
    /* nothing now - later I have no idea.  Is this just a Mac hook? */
}

void
amii_getret()
{
    register int c;

    raw_print( "" );
    raw_print( "Press Return..." );

    c = 0;

    while( c != '\n' && c != '\r' )
    {
	if( HackPort )
	    c = WindowGetchar();
	else
	    c = getchar();
    }
    return;
}

