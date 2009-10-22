CFLAGS  += -I../include $(MONOME_CFLAGS) $(LO_CFLAGS) $(JACK_CFLAGS) $(SNDFILE_CFLAGS) $(SRC_CFLAGS)
LDFLAGS += $(MONOME_LIBS) $(LO_LIBS) -rdynamic $(JACK_LIBS) $(SNDFILE_LIBS) $(SRC_LIBS)
TARGETS = rove

OBJS  = rove.o
OBJS += rove_file.o
OBJS += rove_jack.o
OBJS += rove_list.o
OBJS += rove_group.o
OBJS += rove_config.o
OBJS += rove_monome.o
OBJS += rove_pattern.o

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
