#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include <linux/module.h>
#include <linux/workqueue.h>
#include <net/sock.h>

#define MODULE_NAME "fastecho"

struct echo_server_param {
    struct socket *listen_sock;
};

struct echo_service {
    bool is_stopped;
    struct list_head worker;
};

struct fastecho {
    struct socket *sock;
    struct list_head list;
    struct work_struct fastecho_work;
};

extern int echo_server_daemon(void *);

#endif
