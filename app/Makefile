C++ = g++

ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA32
endif

CCFLAGS = -Wall -D$(os) -I../src -finline-functions -O3 -std=c++11

ifeq ($(arch), IA32)
   CCFLAGS += -DIA32 #-mcpu=pentiumpro -march=pentiumpro -mmmx -msse
endif

ifeq ($(arch), POWERPC)
   CCFLAGS += -mcpu=powerpc
endif

ifeq ($(arch), IA64)
   CCFLAGS += -DIA64
endif

ifeq ($(arch), SPARC)
   CCFLAGS += -DSPARC
endif

LDFLAGS = -L../src -ludt -lstdc++ -lpthread -lm

ifeq ($(os), UNIX)
   LDFLAGS += -lsocket
endif

ifeq ($(os), SUNOS)
   LDFLAGS += -lrt -lsocket
endif

DIR = $(shell pwd)

APP = appserver appclient sendfile recvfile test svcclient aoiclient bbrserver ptcloud_client ptcloud_server ptcloud_client_vivo

all: $(APP)

%.o: %.cpp
	$(C++) $(CCFLAGS) $< -c

appserver: appserver.o
	$(C++) $^ -o $@ $(LDFLAGS)
appclient: appclient.o
	$(C++) $^ -o $@ $(LDFLAGS)
sendfile: sendfile.o
	$(C++) $^ -o $@ $(LDFLAGS)
recvfile: recvfile.o
	$(C++) $^ -o $@ $(LDFLAGS)
test: test.o
	$(C++) $^ -o $@ $(LDFLAGS)
ptcloud_client: ptcloud_client.o
	$(C++) $^ -o $@ $(LDFLAGS)
ptcloud_server: ptcloud_server.o
	$(C++) $^ -o $@ $(LDFLAGS)
ptcloud_client_vivo: ptcloud_client_vivo.o
	$(C++) $^ -o $@ $(LDFLAGS)
svcclient: svcclient.o
	$(C++) $^ -o $@ $(LDFLAGS)
aoiclient: aoiclient.o
	$(C++) $^ -o $@ $(LDFLAGS)
bbrserver: bbrserver.o
	$(C++) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o $(APP)

install:
	export PATH=$(DIR):$$PATH
