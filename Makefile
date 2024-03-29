CC=gcc

CFLAGS=-I. -I./opus \
    -Os \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -fno-stack-check \
    -fno-stack-protector \
    -mno-stack-arg-probe \
    -momit-leaf-frame-pointer \
    -march=i486 -mtune=i686 \
    -flto \
    -fno-ident \
    -mpreferred-stack-boundary=2 \
    -foptimize-strlen \
 	-fno-exceptions \
	-fno-dwarf2-cfi-asm \
	-fno-asynchronous-unwind-tables \
    -Wstack-usage=4096 \
    -fgcse-sm \
    -fgcse-las \

#    -fomit-frame-pointer \

# -freorder-blocks -fweb -frename-registers -funswitch-loops\
# -fwhole-program -fstrict-aliasing -fschedule-insns
# -D__SSE1 sse_func.o

LDFLAGS= -nostdlib -lgcc -lkernel32 -lmsvcrt -luser32 -lgdi32 -lwsock32 -s
LDFLAGS+= -Wl,-s,-dynamicbase \
    -Wl,-nxcompat \
    -Wl,--no-seh \
    -Wl,--relax \
    -Wl,--disable-runtime-pseudo-reloc \
    -Wl,--enable-auto-import \
    -Wl,--disable-stdcall-fixup

in_opus.dll: in_opus.o resample.o infobox.o http.o wspiapi.o resource.o
	$(CC) -o in_opus.dll in_opus.o resample.o infobox.o http.o wspiapi.o resource.o\
	 oflto/*.o opusfile/*.c -e_DllMain@12 -mdll $(LDFLAGS) $(CFLAGS)

	cp in_opus.dll "D:\Program Files\MediaPlayers\Winamp\Plugins"
	cp in_opus.dll "D:\Shared docs"
	cp in_opus.dll "D:\Program Files\MediaPlayers\winamp566\Plugins"
	cp in_opus.dll "D:\Program Files\MediaPlayers\winamp58\Plugins"
	cp in_opus.dll "D:\Program Files\MediaPlayers\winamp531\Plugins"
	cp in_opus.dll "D:\Program Files\MediaPlayers\MediaMonkey3\Plugins"
	cp in_opus.dll "D:\Program Files\MediaPlayers\MediaMonkey4\Plugins"

in_opus.o : in_opus.c infobox.h resample.h resource.h
	$(CC) -c in_opus.c $(CFLAGS)
infobox.o : infobox.c infobox.h utf_ansi.c
	$(CC) -c infobox.c $(CFLAGS)
resource.o: resource.rc
	windres resource.rc resource.o
wspiapi.o : wspiapi.c wspiapi.h
	$(CC) -c wspiapi.c $(CFLAGS)
http.o    : http.c http.h wspiapi.h winerrno.h
	$(CC) -c http.c  $(CFLAGS)
resample.o: resample.c resample.h
	$(CC) -c resample.c $(CFLAGS) -O2 -ffast-math
#info.o: opusfile/info.c
#	$(CC) -c opusfile/info.c -o opusfile/info.o $(CFLAGS)
#opusfile.o: opusfile/opusfile.c
#	$(CC) -c opusfile/opusfile.c -o opusfile/opusfile.o $(CFLAGS)
#opusfile.o: opusfile/opusfile.c
#	$(CC) -c opusfile/opusfile.c -o opusfile/opusfile.o $(CFLAGS)
#stream.o: opusfile/stream.c
#	$(CC) -c opusfile/stream.c -o opusfile/stream.o $(CFLAGS)
clean :
	rm *.o in_opus.dll
