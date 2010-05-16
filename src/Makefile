CFLAGS  += -rdynamic -I../include $(MONOME_CFLAGS) $(JACK_CFLAGS) $(SNDFILE_CFLAGS) $(SRC_CFLAGS)
LDFLAGS += -rdynamic $(MONOME_LIBS) $(JACK_LIBS) $(SNDFILE_LIBS) $(SRC_LIBS)
TARGETS = rove

OBJS  = rove.o
OBJS += list.o
OBJS += jack.o
OBJS += group.o
OBJS += monome.o
OBJS += pattern.o
OBJS += config_parser.o
OBJS += file_loop.o

all: $(TARGETS)

clean:
	echo -e "    \033[1mCLEAN\033[0m   src"
	rm -f rove $(OBJS)

install: all
	echo -e "    \033[1mINSTALL\033[0m $(BINDIR)/rove"
	install rove $(BINDIR)

rove: $(OBJS)
	echo -e "    \033[1mLD\033[0m      src/rove"
	$(LD) $(LDFLAGS) $^ -o $@

%: %.o

%.o: %.c
	echo -e "    \033[1mCC\033[0m      src/$@"
	$(CC) $(CFLAGS) -c $< -o $@
