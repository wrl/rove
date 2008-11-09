CFLAGS  += -I. -I../include -I/usr/include
LDFLAGS += -lmonome -ljack -lsndfile
TARGETS = rove

OBJS  = rove.o
OBJS += rove_file.o
OBJS += rove_jack.o
OBJS += rove_list.o
OBJS += rove_monome.o
OBJS += rove_pattern.o

all: $(TARGETS)

clean:
	echo "  CLEAN   src"
	rm -f rove $(OBJS)

install: all
	echo "  INSTALL $(BINDIR)/rove"
	install rove $(BINDIR)

rove: $(OBJS)
	echo "  LD      rove"
	$(LD) $(LDFLAGS) $^ -o $@

%: %.o

%.o: %.c
	echo "  CC      src/$@"
	$(CC) $(CFLAGS) -c $< -o $@