include ../../Makefile.common

BINARY = tester

INCLUDES = -I$(HYPERCOMM_HOME)/include -I$(HYPERCOMM_HOME)/include/hypercomm/core
LIBS = -L$(HYPERCOMM_HOME)/lib -lhypercomm-components -lhypercomm-utilities -lhypercomm-serialization -lhypercomm-messaging -lSegFault

OPTS?=-g -O2
TESTOPTS?=4
CMK_LB_STRATEGY?=RotateLB
TESTOPTS:=$(TESTOPTS) +LBDebug 1 +balancer $(CMK_LB_STRATEGY)
CMK_NUM_PES?=2
CATCHSEGV?=$(shell which catchsegv)

CHARMCFLAGS = $(OPTS)
CHARMCLINKFLAGS = -language charm++ -memory isomalloc -module TreeLB $(OPTS) $(LIBS)

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
	$(CHARM_HOME)/bin/testrun +p$(CMK_NUM_PES) $(CATCHSEGV) ./$(BINARY) $(TESTOPTS)

debug: $(BINARY)
	gdb -args ./$(BINARY) ++local

clean:
	rm -f *.o *.decl.h *.def.h charmrun $(BINARY)
