#include "midifile.h"

/* $Id: midifile.c,v 1.4 1991/11/17 21:57:26 piet Rel piet $ */
/*
 * midifile 1.11
 * 
 * Utilities to read and write a MIDI file.
 */

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
