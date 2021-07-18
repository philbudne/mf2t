/* $Id: midifile.c,v 1.4 1991/11/17 21:57:26 piet Rel piet $ */
/*
 * midifile 1.11
 * 
 * Write a MIDI file.
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

#define NULLFUNC 0

/* public stuff */

/* declare pointers to read/write functions */
#define MIDIFILE_FUNC(RET,NAME,ARGS) MIDIFILE_PUBLIC RET (*NAME)ARGS = NULLFUNC;
MIDIFILE_WRITE_FUNCTIONS
#undef MIDIFILE_FUNC

/* 1 => continued system exclusives are not collapsed */
MIDIFILE_PUBLIC int Mf_nomerge = 0;

/* current time in delta‐time units */
MIDIFILE_PUBLIC mf_deltat_t Mf_currtime = 0;

/* PLB: log output to stderr */
MIDIFILE_PUBLIC int Mf_trace_output = 0;

#define TRACE_FUNC if (Mf_trace_output) fprintf(stderr, "%s\n", __func__)
#define TRACE_EOL if (Mf_trace_output) fprintf(stderr, "\n")

/* private stuff */
static mf_ssize_t Mf_numbyteswritten = 0;

static void
mferror(char *s) {
    if (Mf_werror)
        (*Mf_werror)(s);
    exit(1);
}

/* write a single character and abort on error */
static int
_eputc(unsigned char c) {
    int return_val;

    if ((Mf_putc) == NULLFUNC) {
        mferror("Mf_putc undefined");
        return(-1);
    }

    if (Mf_trace_output)
	fprintf(stderr, " %02X", c);
    return_val = (*Mf_putc)(c);

    if (return_val == EOF)
        mferror("error writing");

    Mf_numbyteswritten++;
    return(return_val);
}

static int
__eputc(const char *what, unsigned char c) {
    int ret;
    if (Mf_trace_output)
	fprintf(stderr, " %s =>", what);
    ret = _eputc(c);
    TRACE_EOL;
    return(ret);
} /* __eputc */

#define eputc(X) __eputc(#X, X)

/*
 * write32bit()
 * write16bit()
 *
 * These routines are used to make sure that the byte order of
 * the various data types remains constant between machines. This
 * helps make sure that the code will be portable from one system
 * to the next.
 */
static void
_write32bit(const char *what, int32_t data) {
    if (Mf_trace_output)
	fprintf(stderr, " %s =>", what);
    _eputc((unsigned)((data >> 24) & 0xff));
    _eputc((unsigned)((data >> 16) & 0xff));
    _eputc((unsigned)((data >> 8 ) & 0xff));
    _eputc((unsigned)(data & 0xff));
    TRACE_EOL;
}

static void
_write16bit(const char *what, int data) {
    if (Mf_trace_output)
	fprintf(stderr, " %s =>", what);
    _eputc((unsigned)((data & 0xff00) >> 8));
    _eputc((unsigned)(data & 0xff));
    TRACE_EOL;
}

static void
_WriteVarLen(const char *what, mf_varinum_t value) {
    mf_varinum_t buffer;

    if (Mf_trace_output)
	fprintf(stderr, " %s =>", what);
    buffer = value & 0x7f;
    while ((value >>= 7) > 0) {
        buffer <<= 8;
        buffer |= 0x80;
        buffer += (value & 0x7f);
    }
    while (1) {
        _eputc((unsigned)(buffer & 0xff));
       
        if (buffer & 0x80)
            buffer >>= 8;
        else
            break;
    }
    TRACE_EOL;
} /* end of WriteVarLen */

#define write32bit(X) _write32bit(#X, X)
#define write16bit(X) _write16bit(#X, X)
#define WriteVarLen(X) _WriteVarLen(#X, X)

static void
mf_w_header_chunk(int format, int ntracks, int division) {
    uint32_t ident,length;
    
    ident = MThd;           /* Head chunk identifier */
    length = 6;             /* Chunk length */

    /* individual bytes of the header must be written separately
       to preserve byte order across cpu types :-( */
    TRACE_FUNC;
    write32bit(ident);
    write32bit(length);
    write16bit(format);
    write16bit(ntracks);
    write16bit(division);
} /* end gen_header_chunk() */

MIDIFILE_PUBLIC int Mf_RunStat = 0;    /* if nonzero, use running status */
static int laststat;                   /* last status code */
static int lastmeta;                   /* last meta event type */

static void
mf_write_data(mf_data_t *data, mf_size_t size) {
    if (Mf_trace_output)
	fprintf(stderr, " data =>");
    while (size-- > 0)
        _eputc(*data++);
    TRACE_EOL;
}

/*
 * mf_w_midi_event()
 * 
 * Library routine to mf_write a single MIDI track event in the standard MIDI
 * file format. The format is:
 *
 *                    <delta‐time><event>
 *
 * In this case, event can be any multi‐byte midi message, such as
 * “note on”, “note off”, etc.      
 *
 * delta_time – the time in ticks since the last event.
 * type – the type of event.
 * chan – The midi channel.
 * data – A pointer to a block of chars containing the META EVENT,
 *        data.
 * size – The length of the midi‐event data.
 */
MIDIFILE_PUBLIC int
mf_w_midi_event(mf_deltat_t delta_time,
        unsigned int type, unsigned int chan, mf_data_t *data,
        mf_size_t size) {
    unsigned char c;
    int ret = size;

    TRACE_FUNC;
    WriteVarLen(delta_time);

    /* all MIDI events start with the type in the first four bits,
       and the channel in the lower four bits */
    c = type | chan;

    if (chan > 15) {
        fprintf(stderr, "error: MIDI channel greater than 16\n");
	ret = -1;
    }
    if (!Mf_RunStat || laststat != c)
        __eputc("status", c);

    laststat = c;

    /* write out the data bytes */
    mf_write_data(data, size);
    return(ret);
} /* end mf_write MIDI event */

/*
 * mf_w_meta_event()
 *
 * Library routine to mf_write a single meta event in the standard MIDI
 * file format. The format of a meta event is:
 *
 *          <delta‐time><FF><type><length><bytes>
 *
 * delta_time – the time in ticks since the last event.
 * type – the type of meta event.
 * data – A pointer to a block of chars containing the META EVENT,
 *        data.
 * size – The length of the meta‐event data.
 */
MIDIFILE_PUBLIC int
mf_w_meta_event(mf_deltat_t delta_time, unsigned int type,
		mf_data_t *data, mf_size_t size) {
    int ret = size;

    TRACE_FUNC;
    WriteVarLen(delta_time);
    
    /* This marks the fact we’re writing a meta‐event */
    eputc(meta_event);
    laststat = meta_event;

    /* The type of meta event */
    eputc(type);
    lastmeta = type;

    /* The length of the data bytes to follow */
    WriteVarLen(size); 

    mf_write_data(data, size);
    return(ret);
} /* end mf_w_meta_event */

/*
 * mf_w_sysex_event()
 *
 * Library routine to mf_write a single sysex (or arbitrary)
 * event in the standard MIDI file format. The format of the event is:
 *
 *          <delta‐time><type><length><bytes>
 *
 * delta_time – the time in ticks since the last event.
 * data – A pointer to a block of chars containing the EVENT data.
 *        The first byte is the type (0xf0 for sysex, 0xf7 otherwise)
 * size – The length of the sysex‐event data.
 */
MIDIFILE_PUBLIC int
mf_w_sysex_event(mf_deltat_t delta_time,
        mf_data_t *data, mf_size_t size) {
    int ret = size;

    TRACE_FUNC;
    WriteVarLen(delta_time);
    
    /* The type of sysex event */
    __eputc("event", *data);
    laststat = 0;

    /* The length of the data bytes to follow */
    WriteVarLen(size-1); 
    mf_write_data(data, size);

    return(ret);
} /* end mf_w_sysex_event */

MIDIFILE_PUBLIC void
mf_w_tempo(mf_deltat_t delta_time, mf_tempo_t tempo) {
    /* Write tempo */
    /* all tempos are written as 120 beats/minute, */
    /* expressed in microseconds/quarter note     */

    WriteVarLen(delta_time);

    eputc(meta_event);
    laststat = meta_event;
    eputc(set_tempo);

    eputc(3);
    if (Mf_trace_output)
	fprintf(stderr, " tempo =>");
    _eputc((unsigned)(0xff & (tempo >> 16)));
    _eputc((unsigned)(0xff & (tempo >> 8)));
    _eputc((unsigned)(0xff & tempo));
    TRACE_EOL;
}

static void
mf_w_track_chunk(int tempo_track, FILE *fp) {
    uint32_t trkhdr,trklength;
    long offset, place_marker;

    trkhdr = MTrk;
    trklength = 0;

    /* Remember where the length was written, because we don’t
       know how long it will be until we’ve finished writing */
    offset = ftell(fp); 

#ifdef DEBUG
    printf("offset = %d\n",(int) offset);
#endif

    /* Write the track chunk header */
    write32bit(trkhdr);
    write32bit(trklength);

    Mf_numbyteswritten = 0L; /* the header’s length doesn’t count */
    laststat = 0;

    /* "wtempotrack -1 is harmless" */
    if (tempo_track)
        (*Mf_wtempotrack)(-1);
    else
        (*Mf_wtrack)();

    if (laststat != meta_event || lastmeta != end_of_track) {
        /* mf_write End of track meta event */
        eputc(0);
        eputc(meta_event);
        eputc(end_of_track);
        eputc(0);
    }

    laststat = 0;
     
    /* It’s impossible to know how long the track chunk will be beforehand,
       so the position of the track length data is kept so that it can
       be written after the chunk has been generated */
    place_marker = ftell(fp);

    /* This method turned out not to be portable because the
       parameter returned from ftell is not guaranteed to be
       in bytes on every machine */
    /* track.length = place_marker - offset - (long) sizeof(track); */

#ifdef DEBUG
    printf("length = %d\n",(int) trklength);
#endif

    if (fseek(fp,offset,0) < 0)
        mferror("error seeking during final stage of write");

    trklength = Mf_numbyteswritten;

    /* Re‐mf_write the track chunk header with right length */
    write32bit(trkhdr);
    write32bit(trklength);

    fseek(fp,place_marker,0);
} /* End gen_track_chunk() */

/*
 * mfwrite() – The only function you’ll need to call to write out
 *             a midi file.
 *
 * format      0 – Single multi‐channel track
 *             1 – Multiple simultaneous tracks
 *             2 – One or more sequentially independent
 *                 single track patterns                
 * ntracks     The number of tracks in the file.
 * division    This is kind of tricky, it can represent two
 *             things, depending on whether it is positive or negative
 *             (bit 15 set or not).  If  bit  15  of division  is zero,
 *             bits 14 through 0 represent the number of delta‐time
 *             “ticks” which make up a quarter note.  If bit  15 of
 *             division  is  a one, delta‐times in a file correspond to
 *             subdivisions of a second similiar to  SMPTE  and  MIDI
 *             time code.  In  this format bits 14 through 8 contain
 *             one of four values – 24, -25, -29, or -30,
 *             corresponding  to  the  four standard  SMPTE and MIDI
 *             time code frame per second formats, where  -29
 *             represents  30  drop  frame.   The  second  byte
 *             consisting  of  bits 7 through 0 corresponds the the
 *             resolution within a frame.  Refer the Standard MIDI
 *             Files 1.0 spec for more details.
 * fp          This should be the open file pointer to the file you
 *             want to write.  It will have be a global in order
 *             to work with Mf_putc.  
 */ 

MIDIFILE_PUBLIC void
mfwrite(int format, int ntracks, int division, FILE *fp) {
    int i;

    if (Mf_putc == NULLFUNC)
        mferror("mfmf_write() called without setting Mf_putc");

    if (Mf_wtrack == NULLFUNC)
        mferror("mfmf_write() called without setting Mf_mf_writetrack"); 

    /* every MIDI file starts with a header */
    mf_w_header_chunk(format,ntracks,division);

    /* In format 1 files, the first track is a tempo map */
    if (format == 1 && Mf_wtempotrack) {
        mf_w_track_chunk(1, fp);
        ntracks--;
    }

    /* The rest of the file is a series of tracks */
    for (i = 0; i < ntracks; i++)
        mf_w_track_chunk(0, fp);
}
