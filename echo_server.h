#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include <net/sock.h>

#define MODULE_NAME "fastecho"

struct echo_server_param {
    struct socket *listen_sock;
};

extern int echo_server_daemon(void *);

#endif
