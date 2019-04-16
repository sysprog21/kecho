SRCS = fastecho_module.c echo_server.c
KERNEL_DIR = /lib/modules/$(shell uname -r)/build
BUILD_DIR := $(shell pwd)
VERBOSE   := 1

GIT_HOOKS := .git/hooks/applied

obj-m := fastecho.o
fastecho-objs := $(SRCS:.c=.o)

all:
	make -C $(KERNEL_DIR) SUBDIRS=$(BUILD_DIR) KBUILD_VERBOSE=$(VERBOSE) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	rm -f *.o *.ko *.mod.c *.symvers *.order .fastecho*
	rm -fr .tmp_versions
