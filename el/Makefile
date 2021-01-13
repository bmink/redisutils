P = el
OBJS = main.o hiredis_helper.o
CFLAGS = -g -Wall -Wstrict-prototypes
LDLIBS = -lb -lhiredis

$(P): $(OBJS)
	$(CC) -o $(P) $(LDFLAGS) $(OBJS) $(LDLIBS)

clean:
	rm -f *o; rm -f $(P)

install: $(P)
	cp $(P) ~/bin
