Fork of https://github.com/dewhisna/mf2t
a fork of https://github.com/codenotes/mf2t

# Goals:
* ***FULLY*** ANSIfied (using C99 __func__)
* eliminate use of "long" for 32-bit values!!!
* using 
* cranked up warning options
* all warnings fatal
* clean builds (no warnings) on Linux, Mac OS, FreeBSD
* split midifile.c into midifile_{read,write,time}.c
* added t2mf '-d' option to display how/why bytes are output.
* added midifile_read.c get_lookfor(); explain why change needed.

# Non-priorities (none have been intentionally broken in source files):
* shared library support
* install of library/include files
* CMake support
* Windows support
