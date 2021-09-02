CC=gcc

CFLAGS=-I. -I./opus -Os -Wall -Wextra -Wno-unused-parameter -fno-stack-check\
 -fno-stack-protector -mno-stack-arg-probe -march=i486 -mtune=i686 \
 -mpreferred-stack-boundary=2

# -freorder-blocks -fweb -frename-registers -funswitch-loops\
# -fwhole-program -fstrict-aliasing -fschedule-insns
# -D__SSE1 sse_func.o 

LDFLAGS= -nostdlib -lgcc -lkernel32 -lmsvcrt -luser32 -lgdi32 -lwsock32 -s 

in_opus.dll: in_opus.o resample.o infobox.o http.o wspiapi.o resource.o 
	$(CC) -o in_opus.dll -static in_opus.o resample.o infobox.o http.o wspiapi.o resource.o \
	 opusfile/*.o -lopus -logg -e_DllMain@12 -mdll $(LDFLAGS) $(CFLAGS) 

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

clean :
	rm *.o in_opus.dll
