cmake_minimum_required(VERSION 3.1)

project(mf2t
	VERSION 1.0.20150711
	LANGUAGES C CXX)

set(libmidifile_SOURCES
	libmidifile-20150710/midifile.c
	libmidifile-20150710/midifile.h
)

set(mf2t_SOURCES
	mf2t.c
	version.h
)

set(t2mf_SOURCES
	t2mf.c
	t2mf.h
	t2mflex.c
	version.h
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(libmidifile STATIC ${libmidifile_SOURCES})

add_executable(mf2t ${mf2t_SOURCES})
add_dependencies(mf2t libmidifile)
target_link_libraries(mf2t libmidifile)
include_directories(libmidifile-20150710)

add_executable(t2mf ${t2mf_SOURCES})
add_dependencies(t2mf libmidifile)
target_link_libraries(t2mf libmidifile)
include_directories(libmidifile-20150710)

