PBS = 0
GANGLIA = 0
DISTRIB = 0
IP6 = 1

vpath %.c ..
vpath %.h ..
vpath %.y ..

CFLAGS=-g -O0 -Wall -Werror -fPIC -DPIC -DNET_NO_IO -DNET_NO_LOG -fno-stack-protector
LDFLAGS=-lpthread

CACHE_OBJ=pbs_cache.o memory.o dump.o comm.o log.o addrlist.o update.o
API_OBJ=api.o hash_table.o hash_functions.o net.o
LIBSO=libpbscache.so
LIBSA=libpbscache.a 

ifeq ($(DISTRIB), 1)
	LIBSA +=./distrib/libdist.a
else
	CFLAGS += -DDIST_DISABLED=1
endif

ifeq ($(IP6),1)
    CFLAGS += -DIPv6=1
endif

ifeq ($(PBS),1)
    CFLAGS += -I/software/pbs-7.0.0/include
    LDFLAGS += -L/software/pbs-7.0.0/lib -lnet -lpbs -lnet
    CACHE_OBJ += pbs.o
else
    CFLAGS += -DDISABLE_PBS=1
endif

ifeq ($(GANGLIA),1)
    CFLAGS += -I/usr/include/libxml2
    LDFLAGS += -lxml2
    CACHE_OBJ += ganglia.o
else
    CFLAGS += -DDISABLE_GANGLIA=1
endif

all: distr pbs_cache client client2 client3 update_cache delete_cache list_cache list_client local_test $(LIBSO) $(LIBSA)

net.o: ../net.c
	gcc $(CFLAGS) -c $< -o $@

distr:
ifeq ($(DISTRIB), 1)
	 make -C distrib all	
endif

%.o : %.c
	gcc $(CFLAGS) -c $<

$(LIBSO): $(API_OBJ)
	gcc -shared $(API_OBJ) -Wl,-soname -Wl,libpbscache.1 -o $(LIBSO)

$(LIBSA): $(API_OBJ)
	ar cru $(LIBSA) $(API_OBJ)
	ranlib $(LIBSA)
		

pbs_cache: $(CACHE_OBJ) $(LIBSA)
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)


client: client.o $(LIBSA)
	 gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

client2: client2.o $(LIBSA)
	 gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

client3: cli-report.o client3.o $(LIBSA)
	 gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

update_cache: update_cache.o $(LIBSA)
	 gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

delete_cache: delete_cache.o $(LIBSA)
	 gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

list_cache: list_cache.o $(LIBSA)
	 gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

list_client: list_client.o $(LIBSA)
	 gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

local_test: local_test.o $(LIBSA)
	 gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)


clean:
	rm -f *.o rm core* pbs_cache client client2 client3 update_cache list_cache delete_cache list_client local_test libpbscache.*
ifeq ($(DISTRIB), 1)
	make -C distrib clean
endif
todo:
	 grep -h TODO *c *h |sed 's/.*TODO//' |sort -r >TODO

