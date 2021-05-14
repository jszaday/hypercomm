CHARMC = charmc -g
BINARY = tester

INCLUDES = -I../include
LIBS = -L../lib -lhypercomm-components -lhypercomm-utilities -lhypercomm-serialization -lhypercomm-messaging 

CHARMCFLAGS = $(OPTS)
CHARMCLINKFLAGS = -language charm++ $(OPTS) $(LIBS)

%.o: %.cc

all: $(BINARY)
$(BINARY): $(patsubst %.cc,%.o,$(wildcard *.cc))
	$(CHARMC) $(CHARMCLINKFLAGS) -o $@ $+

.SECONDARY: $(patsubst %.cc,%.decl.h,$(wildcard *.cc))
.SECONDARY: $(patsubst %.cc,%.def.h,$(wildcard *.cc))

%.o: %.cc %.decl.h %.def.h
	$(CHARMC) $(CHARMCFLAGS) $(INCLUDES) $<

%.def.h: %.decl.h ;

%.decl.h: %.ci
	$(CHARMC) $(CHARMCFLAGS) $<

test: $(BINARY)
	$(CHARM_HOME)/bin/testrun +p4 ./$(BINARY) ++ppn 4

debug: $(BINARY)
	gdb -args ./$(BINARY) ++local

clean:
	rm -f *.o *.decl.h *.def.h charmrun $(BINARY)
