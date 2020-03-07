SRCS = fastecho_module.c echo_server.c
KERNEL_DIR = /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(shell pwd)

CFLAGS_user = -std=gnu99 -Wall -Wextra -Werror

GIT_HOOKS := .git/hooks/applied

obj-m := fastecho.o
fastecho-objs := $(SRCS:.c=.o)

all: $(GIT_HOOKS) user-echo-server
	make -C $(KERNEL_DIR) SUBDIRS=$(BUILD_DIR) KBUILD_VERBOSE=$(VERBOSE) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

user-echo-server: user-echo-server.c
	$(CC) -o $@ $(CFLAGS) $<

clean:
	rm -f *.o *.ko *.mod.c *.symvers *.order .fastecho* user-echo-server
	rm -fr .tmp_versions
