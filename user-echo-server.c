#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#define SERVER_PORT 12345
#define EPOLL_SIZE 256
#define BUF_SIZE 512
#define EPOLL_RUN_TIMEOUT -1

typedef struct client_list_s {
    int client;
    char *addr;
    struct client_list_s *next;
} client_list_t;

static void delete_list(client_list_t **list)
{
    while (*list) {
        client_list_t *delete = *list;
        *list = (*list)->next;
        free(delete->addr);
        free(delete);
    }
}

static client_list_t *delete_client(client_list_t **list, int client)
{
    if (!(*list))
        return NULL;

    if ((*list)->client == client) {
        client_list_t *tmp = (*list)->next;
        free((*list)->addr);
        free((*list));
        return tmp;
    }

    (*list)->next = delete_client(&(*list)->next, client);
    return *list;
}

static int size_list(client_list_t *list)
{
    int size = 0;
    for (client_list_t *tmp = list; tmp; tmp = tmp->next)
        size++;
    return size;
}

static void server_err(const char *str, client_list_t **list)
{
    perror(str);
    delete_list(list);
    exit(-1);
}

static client_list_t *new_list(int client, char *addr, client_list_t **list)
{
    client_list_t *new = (client_list_t *) malloc(sizeof(client_list_t));
    if (!new)
        server_err("Fail to allocate memory", list);
    new->addr = strdup(addr);
    if (!new->addr)
        server_err("Fail to duplicate string", list);
    new->client = client;
    new->next = NULL;
    return new;
}

static void push_back_client(client_list_t **list, int client, char *addr)
{
    if (!(*list))
        *list = new_list(client, addr, list);
    else {
        client_list_t *tmp = *list;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = new_list(client, addr, list);
    }
}

static int setnonblock(int fd)
{
    int fdflags;
    if ((fdflags = fcntl(fd, F_GETFL, 0)) == -1)
        return -1;
    fdflags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, fdflags) == -1)
        return -1;
    return 0;
}

static int handle_message_from_client(int client, client_list_t **list)
{
    int len;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);
    if ((len = recv(client, buf, BUF_SIZE, 0)) < 0)
        server_err("Fail to receive", list);
    if (len == 0) {
        if (close(client) < 0)
            server_err("Fail to close", list);
        *list = delete_client(list, client);
        printf("After fd=%d is closed, current numbers clients = %d\n", client,
               size_list(*list));
    } else {
        printf("Client #%d :> %s", client, buf);
        if (send(client, buf, BUF_SIZE, 0) < 0)
            server_err("Fail to send", list);
    }
    return len;
}

int main(void)
{
    static struct epoll_event events[EPOLL_SIZE];
    struct sockaddr_in addr = {
        .sin_family = PF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    socklen_t socklen = sizeof(addr);

    client_list_t *list = NULL;
    int listener;
    if ((listener = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        server_err("Fail to create socket", &list);
    printf("Main listener (fd=%d) was created.\n", listener);

    if (setnonblock(listener) == -1)
        server_err("Fail to set nonblocking", &list);
    if (bind(listener, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        server_err("Fail to bind", &list);
    printf("Listener was binded to %s\n", inet_ntoa(addr.sin_addr));

    if (listen(listener, 1) < 0)
        server_err("Fail to listen", &list);

    int epoll_fd;
    if ((epoll_fd = epoll_create(EPOLL_SIZE)) < 0)
        server_err("Fail to create epoll", &list);

    static struct epoll_event ev = {.events = EPOLLIN | EPOLLET};
    ev.data.fd = listener;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener, &ev) < 0)
        server_err("Fail to control epoll", &list);
    printf("Listener (fd=%d) was added to epoll.\n", epoll_fd);

    while (1) {
        struct sockaddr_in client_addr;
        int epoll_events_count;
        if ((epoll_events_count = epoll_wait(epoll_fd, events, EPOLL_SIZE,
                                             EPOLL_RUN_TIMEOUT)) < 0)
            server_err("Fail to wait epoll", &list);
        printf("epoll event count: %d\n", epoll_events_count);
        clock_t start_time = clock();
        for (int i = 0; i < epoll_events_count; i++) {
            /* EPOLLIN event for listener (new client connection) */
            if (events[i].data.fd == listener) {
                int client;
                if ((client = accept(listener, (struct sockaddr *) &client_addr,
                                     &socklen)) < 0)
                    server_err("Fail to accept", &list);
                printf("Connection from %s:%d, socket assigned: %d\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port), client);
                setnonblock(client);
                ev.data.fd = client;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &ev) < 0)
                    server_err("Fail to control epoll", &list);
                push_back_client(&list, client,
                                 inet_ntoa(client_addr.sin_addr));
                printf("Add new client (fd=%d) and size of client_list is %d\n",
                       client, size_list(list));
            } else {
                /* EPOLLIN event for others (new incoming message from client)
                 */
                if (handle_message_from_client(events[i].data.fd, &list) < 0)
                    server_err("Handle message from client", &list);
            }
        }
        printf("Statistics: %d event(s) handled at: %.6f second(s)\n",
               epoll_events_count,
               (double) (clock() - start_time) / CLOCKS_PER_SEC);
    }
    close(listener);
    close(epoll_fd);
    exit(0);
}
