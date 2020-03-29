SRCS = kecho_mod.c echo_server.c
KERNEL_DIR = /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(shell pwd)

CFLAGS_user = -std=gnu99 -Wall -Wextra -Werror

GIT_HOOKS := .git/hooks/applied

obj-m := kecho.o
kecho-objs := $(SRCS:.c=.o)

all: $(GIT_HOOKS) kecho.ko bench user-echo-server
	
$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

kecho.ko:
	make -C $(KERNEL_DIR) M=$(BUILD_DIR) KBUILD_VERBOSE=$(VERBOSE) modules

bench: bench.c
	$(CC) -o $@ $(CFLAGS_user) -pthread $<

plot:
	gnuplot scripts/bench.gp

user-echo-server: user-echo-server.c
	$(CC) -o $@ $(CFLAGS_user) $<

clean:
	rm -f *.o *.ko *.mod.c *.symvers *.order .kecho* user-echo-server bench
	rm -fr .tmp_versions
