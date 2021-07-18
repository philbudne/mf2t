/* $Id: midifile.c,v 1.4 1991/11/17 21:57:26 piet Rel piet $ */
/*
 * midifile 1.11
 * 
 * Read a MIDI file.  Externally‐assigned function pointers are 
 * called upon recognizing things in the file.
 *
 * Original release by Tim Thompson, tjt@twitch.att.com
 *
 * June 1989 – Added writing capability, M. Czeiszperger.
 *
 * Oct 1991 – Modifications by Piet van Oostrum <piet@cs.ruu.nl>:
 *      Changed identifiers to be 7 char unique.
 *      Added sysex write capability (mf_w_sysex_event)
 *      Corrected a bug in writing of tempo track
 *      Added code to implement running status on write
 *      Added check for meta end of track insertion
 *      Added a couple of include files to get proper int=short compilation
 *
 * Nov 1991 – Piet van Oostrum <piet@cs.ruu.nl>
 *      mf_w_tempo needs a delta time parameter otherwise the tempo cannot
 *      be changed during the piece.
 *
 * Apr 1993 – Piet van Oostrum <piet@cs.ruu.nl>
 *      decl of malloc replaced by #include <malloc.h>
 *      readheader() declared void.
 *
 * Aug 1993 – Piet van Oostrum <piet@cs.ruu.nl>
 *      sequencer_specific in midifile.h was wrong
 *
 *          The file format implemented here is called
 *          Standard MIDI Files, and is part of the Musical
 *          instrument Digital Interface specification.
 *          The spec is avaiable from:
 *
 *               International MIDI Association
 *               5316 West 57th Street
 *               Los Angeles, CA 90056
 *
 *          An in‐depth description of the spec can also be found
 *          in the article “Introducing Standard MIDI Files”, published
 *          in Electronic Musician magazine, April, 1989.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "midifile.h"

#ifdef _WIN32
#include "windows.h"
#endif

/* public stuff */

/* declare pointers to read/write functions */
#define MIDIFILE_FUNC(RET,NAME,ARGS) MIDIFILE_PUBLIC RET (*NAME)ARGS = NULL;
MIDIFILE_READ_FUNCTIONS
#undef MIDIFILE_FUNC

/* 1 => continued system exclusives are not collapsed */
MIDIFILE_PUBLIC int Mf_nomerge = 0;

/* current time in delta‐time units */
MIDIFILE_PUBLIC mf_deltat_t Mf_currtime = 0;

/* private stuff */
static mf_ssize_t Mf_toberead = 0;

static void
mferror(char *s) {
    if (Mf_rerror)
        (*Mf_rerror)(s);
    exit(1);
}

static void
badbyte(int c) {
    char buff[32];

    (void) sprintf(buff,"unexpected byte: 0x%02x",c);
    mferror(buff);
}

static int
egetc(void) {		/* read a single character and abort on EOF */
    int c = (*Mf_getc)();

    if ((c == EOF) || feof(stdin))
        mferror("premature EOF");
    Mf_toberead--;
    return(c);
}

/* readvarinum – read a varying‐length number, and return the */
/* number of characters it took. */

static mf_varinum_t
readvarinum(void) {
    mf_varinum_t value;
    int c;

    c = egetc();
    value = c;
    if (c & 0x80) {
        value &= 0x7f;
        do {
            c = egetc();
            value = (value << 7) + (c & 0x7f);
        } while (c & 0x80);
    }
    return(value);
}

static int32_t
to32bit(int c1, int c2, int c3, int c4) {
    long value = 0L;

    value = (c1 & 0xff);
    value = (value<<8) + (c2 & 0xff);
    value = (value<<8) + (c3 & 0xff);
    value = (value<<8) + (c4 & 0xff);
    return (value);
}

static int
to16bit(int c1, int c2) {
    return ((c1 & 0xff ) << 8) + (c2 & 0xff);
}

static int32_t
read32bit(void) {
    int c1, c2, c3, c4;

    c1 = egetc();
    c2 = egetc();
    c3 = egetc();
    c4 = egetc();
    return to32bit(c1, c2, c3, c4);
}

static int
read16bit(void) {
    int c1, c2;
    c1 = egetc();
    c2 = egetc();
    return to16bit(c1, c2);
}

/* 
 * This routine converts delta times in ticks into seconds. The
 * else statement is needed because the formula is different for tracks
 * based on notes and tracks based on SMPTE times.
 *
 */
MIDIFILE_PUBLIC float
mf_ticks2sec(mf_ticks_t ticks, int division, mf_tempo_t tempo) {
    float smpte_format, smpte_resolution;

    if (division > 0)
        return ((float) (((float)(ticks) * (float)(tempo)) /
                ((float)(division) * 1000000.0)));
    else {
        smpte_format = upperbyte(division);
        smpte_resolution = lowerbyte(division);
        return (float) ((float) ticks / (smpte_format * smpte_resolution *
                1000000.0));
    }
} /* end of ticks2sec() */

MIDIFILE_PUBLIC mf_ticks_t
mf_sec2ticks(float secs, int division, unsigned int tempo) {    
    return (long)(((secs * 1000.0) / 4.0 * division) / tempo);
}

/* The code below allows collection of a system exclusive message of */
/* arbitrary length.  The Msgbuff is expanded as necessary.  The only */
/* visible data/routines are msginit(), msgadd(), msg(), msgleng(). */

#define MSGINCREMENT 128
static char *Msgbuff = NULL;  /* message buffer */
static int Msgsize = 0;       /* Size of currently allocated Msg */
static int Msgindex = 0;      /* index of next available location in Msg */

static void
msginit(void) {
    Msgindex = 0;
}

static char *
msg(void) {
    return(Msgbuff);
}

static int
msgleng(void) {
    return(Msgindex);
}

static void
biggermsg(void) {
    char *newmess;
    char *oldmess = Msgbuff;
    int oldleng = Msgsize;

    Msgsize += MSGINCREMENT;
    newmess = (char *)malloc((unsigned)(sizeof(char)*Msgsize));

    if (newmess == NULL)
        mferror("malloc error!");

    /* copy old message into larger new one */
    if (oldmess != NULL) {
        register char *p = newmess;
        register char *q = oldmess;
        register char *endq = &oldmess[oldleng];

        for (; q!=endq; p++, q++)
            *p = *q;
        free(oldmess);
    }
    Msgbuff = newmess;
}

static void
msgadd(int c) {
    /* If necessary, allocate larger message buffer. */
    if (Msgindex >= Msgsize)
        biggermsg();
    Msgbuff[Msgindex++] = c;
}

static void
metaevent(int type) {
    int leng = msgleng();
    char *m = msg();

    switch (type) {
    case 0x00:
	if (Mf_seqnum)
            (*Mf_seqnum)(to16bit(m[0],m[1]));
	break;
    case 0x01:      /* Text event */
    case 0x02:      /* Copyright notice */
    case 0x03:      /* Sequence/Track name */
    case 0x04:      /* Instrument name */
    case 0x05:      /* Lyric */
    case 0x06:      /* Marker */
    case 0x07:      /* Cue point */
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x0e:
    case 0x0f:
	/* These are all text events */
	if (Mf_text)
	    (*Mf_text)(type,leng,m);
	break;
    case 0x2f:      /* End of Track */
	if (Mf_eot)
	    (*Mf_eot)();
	break;
    case 0x51:      /* Set tempo */
	if (Mf_tempo)
	    (*Mf_tempo)(to32bit(0,m[0],m[1],m[2]));
	break;
    case 0x54:
	if (Mf_smpte)
	    (*Mf_smpte)(m[0],m[1],m[2],m[3],m[4]);
	break;
    case 0x58:
	if (Mf_timesig)
	    (*Mf_timesig)(m[0],m[1],m[2],m[3]);
	break;
    case 0x59:
	if (Mf_keysig)
	    (*Mf_keysig)(m[0],m[1]);
	break;
    case 0x7f:
	if (Mf_sqspecific)
	    (*Mf_sqspecific)(leng,m);
	break;
    default:
	if (Mf_metamisc)
	    (*Mf_metamisc)(type,leng,m);
    }
}

static void
sysex(void) {
    if (Mf_sysex)
        (*Mf_sysex)(msgleng(),msg());
}

static void
chanmessage(int status, int c1, int c2) {
    int chan = status & 0xf;

    switch (status & 0xf0) {
    case 0x80:
	if (Mf_off)
	    (*Mf_off)(chan, c1, c2);
	break;
    case 0x90:
	if (Mf_on)
	    (*Mf_on)(chan, c1, c2);
	break;
    case 0xa0:
	if (Mf_pressure)
	    (*Mf_pressure)(chan, c1, c2);
	break;
    case 0xb0:
	if (Mf_parameter)
	    (*Mf_parameter)(chan, c1, c2);
	break;
    case 0xe0:
	if (Mf_pitchbend)
	    (*Mf_pitchbend)(chan, c1, c2);
	break;
    case 0xc0:
	if (Mf_program)
	    (*Mf_program)(chan, c1);
	break;
    case 0xd0:
	if (Mf_chanpressure)
	    (*Mf_chanpressure)(chan, c1);
	break;
    }
}

static int
readmt(char *s) { /* read through the “MThd” or “MTrk” header string */
    int n = 0;
    char *p = s;
    int c;

    while (n++ < 4 && (c = (*Mf_getc)()) != EOF) {
        if (c != *p++) {
            char buff[32];
            (void) strcpy(buff,"expecting ");
            (void) strcat(buff,s);
            mferror(buff);
        }
    }
    return(c);
}

static void
readheader(void) {			/* read a header chunk */
    int format, ntrks, division;

    if (readmt("MThd") == EOF)
        return;

    Mf_toberead = read32bit();
    format = read16bit();
    ntrks = read16bit();
    division = read16bit();

    if (Mf_header)
        (*Mf_header)(format,ntrks,division);

    /* flush any extra stuff, in case the length of header is not 6 */
    while (Mf_toberead > 0)
        (void) egetc();
}

static int
readtrack(void) {			/* read a track chunk */
    /* This array is indexed by the high half of a status byte.  It’s */
    /* value is either the number of bytes needed (1 or 2) for a channel */
    /* message, or 0 (meaning it’s not  a channel message). */
    static int chantype[] = {
        0, 0, 0, 0, 0, 0, 0, 0,    /* 0x00 through 0x70 */
        2, 2, 2, 2, 1, 1, 2, 0     /* 0x80 through 0xf0 */
    };
    mf_varinum_t varinum, lookfor;
    int c, c1 = 0, type;
    int sysexcontinue = 0; /* 1 if last message was an unfinished sysex */
    int running = 0;       /* 1 when running status used */
    int status = 0;        /* status value (e.g. 0x90==note‐on) */
    int needed;

    if (readmt("MTrk") == EOF)
        return(0);

    Mf_toberead = read32bit();
    Mf_currtime = 0;

    if (Mf_starttrack)
        (*Mf_starttrack)();

    while (Mf_toberead > 0) {
        Mf_currtime += readvarinum();    /* delta time */

        c = egetc();

        if (sysexcontinue && c != 0xf7)
            mferror("didn’t find expected continuation of a sysex");

        if ((c & 0x80) == 0) {   /* running status? */
            if (status == 0)
                mferror("unexpected running status");
            running = 1;
            c1 = c;
            c = status;
        } else if (c < 0xf0) {
            status = c;
            running = 0;
        }

        needed = chantype[(c>>4) & 0xf];

        if (needed) { /* ie. is it a channel message? */
            if (! running)
                c1 = egetc();
            chanmessage(status, c1, (needed>1) ? egetc() : 0);
            continue;;
        }

        switch (c) {
	case 0xff:     /* meta event */
	    type = egetc();
	    /*
	     * This doesn’t work with GCC
	     * lookfor = Mf_toberead - readvarinum();
	     */
	    varinum = readvarinum();
	    lookfor = Mf_toberead - varinum;
	    msginit();

	    while (Mf_toberead > lookfor)
		msgadd(egetc());

	    metaevent(type);
	    break;

	case 0xf0:     /* start of system exclusive */
	    /*
	     * This doesn’t work with GCC
	     * lookfor = Mf_toberead - readvarinum();
	     */
	    varinum = readvarinum();
	    lookfor = Mf_toberead - varinum;
	    msginit();
	    msgadd(0xf0);

	    while (Mf_toberead > lookfor)
		msgadd(c = egetc());

	    if (c == 0xf7 || Mf_nomerge == 0)
		sysex();
	    else
		sysexcontinue = 1;  /* merge into next msg */
	    break;

	case 0xf7:     /* sysex continuation or arbitrary stuff */
	    /*
	     * This doesn’t work with GCC
	     * lookfor = Mf_toberead - readvarinum();
	     */
	    varinum = readvarinum();
	    lookfor = Mf_toberead - varinum;

	    if (! sysexcontinue)
		msginit();

	    while (Mf_toberead > lookfor)
		msgadd(c=egetc());

	    if ( ! sysexcontinue ) {
		if (Mf_arbitrary)
		    (*Mf_arbitrary)(msgleng(),msg());
	    } else if (c == 0xf7) {
		sysex();
		sysexcontinue = 0;
	    }
	    break;
	default:
	    badbyte(c);
	    break;
        }
    }

    if (Mf_endtrack)
        (*Mf_endtrack)();
    return(1);
}

MIDIFILE_PUBLIC void
mfread(void) {
    if ( Mf_getc == NULL )
        mferror("mfread() called without setting Mf_getc"); 

    readheader();
    while (readtrack())
	;
}

/* for backward compatibility with the original lib */
MIDIFILE_PUBLIC void
midifile(void) {
    mfread();
}

