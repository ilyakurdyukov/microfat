CFLAGS = -O2 -Wall -Wextra -std=c99 -pedantic -Wno-unused
APPNAME = fattest

CFLAGS += -DFATDATA_EXTRA="void *file;" -DFAT_WRITE=1 -DFAT_DEBUG=1 -DFAT_SHORT_FIND=0

.PHONY: all clean
all: $(APPNAME)

clean:
	$(RM) $(APPNAME)

$(APPNAME): microfat.h fatfile.h
$(APPNAME): $(APPNAME).c microfat.c fatfile.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
