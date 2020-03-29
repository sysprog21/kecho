KDIR = /lib/modules/$(shell uname -r)/build

CFLAGS_user = -std=gnu99 -Wall -Wextra -Werror

obj-m += kecho.o
kecho-objs := \
    kecho_mod.o \
    echo_server.o

obj-m += drop-tcp-socket.o

GIT_HOOKS := .git/hooks/applied
all: $(GIT_HOOKS) bench user-echo-server
	make -C $(KDIR) M=$(PWD) KBUILD_VERBOSE=$(VERBOSE) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

check: all
	@scripts/test.sh

bench: bench.c
	$(CC) -o $@ $(CFLAGS_user) -pthread $<

plot:
	gnuplot scripts/bench.gp

user-echo-server: user-echo-server.c
	$(CC) -o $@ $(CFLAGS_user) $<

clean:
	rm -f *.o *.ko *.mod.c *.symvers *.order .kecho* user-echo-server bench
	rm -fr .tmp_versions
