/*
 * Drop TCP TIME-WAIT sockets.
 */

#include <linux/ctype.h>
#include <linux/inet.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>

#include <net/netns/generic.h>
#include <net/tcp.h>

#include <net/inet6_hashtables.h>
#include <net/inet_hashtables.h>

#ifndef CONFIG_IPV6
#define in6_pton(args...) 0
#define inet6_lookup(args...) NULL
#endif

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Drop TCP TIME-WAIT sockets");
MODULE_VERSION("0.1");

#define DROPTCP_PDE_NAME "drop_tcp_sock"

struct droptcp_data {
    uint32_t len;
    uint32_t avail;
    char data[0];
};

struct droptcp_pernet {
    struct net *net;
    struct proc_dir_entry *pde;
};

struct droptcp_inet {
    char ipv6 : 1;
    const char *p;
    uint16_t port;
    uint32_t addr[4];
};

static void droptcp_drop(struct net *net,
                         const struct droptcp_inet *src,
                         const struct droptcp_inet *dst)
{
    struct sock *sk;

    if (!src->ipv6) {
        sk = inet_lookup(net, &tcp_hashinfo, NULL, 0, (__be32) dst->addr[0],
                         htons(dst->port), (__be32) src->addr[0],
                         htons(src->port), 0);
        if (!sk)
            return;
    } else {
        sk = inet6_lookup(net, &tcp_hashinfo, NULL, 0,
                          (const struct in6_addr *) dst->addr, htons(dst->port),
                          (const struct in6_addr *) src->addr, htons(src->port),
                          0);
        if (!sk)
            return;
    }

    printk("Drop socket:%p (%s -> %s) state %d\n", sk, src->p, dst->p,
           sk->sk_state);

    if (sk->sk_state == TCP_TIME_WAIT) {
        inet_twsk_deschedule_put(inet_twsk(sk));
    } else {
        tcp_done(sk);
        sock_put(sk);
    }
}

static int droptcp_pton(struct droptcp_inet *in)
{
    char *p, *end;
    if (in4_pton(in->p, -1, (void *) in->addr, -1, (const char **) &end)) {
        in->ipv6 = 0;
    } else if (in6_pton(in->p, -1, (void *) in->addr, -1,
                        (const char **) &end)) {
        in->ipv6 = 1;
    } else
        return -EINVAL;

    p = (end += 1);
    while (*p && isdigit(*p))
        p++;
    *p = 0;

    return kstrtou16(end, 10, &in->port);
}

static void droptcp_process(struct droptcp_pernet *dt, struct droptcp_data *d)
{
    char *p = d->data;
    struct droptcp_inet src, dst;

    while (*p && p < d->data + d->len) {
        while (*p && isspace(*p))
            p++;
        if (!*p) /* skip spaces */
            return;

        src.p = p;
        while (*p && !isspace(*p))
            p++;
        if (!*p) /* skip non-spaces */
            return;

        while (*p && isspace(*p))
            p++;
        if (!*p) /* skip spaces */
            return;

        dst.p = p;
        while (*p && !isspace(*p))
            p++;
        if (!*p) /* skip non-spaces */
            return;

        if ((droptcp_pton(&src) || droptcp_pton(&dst)) ||
            (src.ipv6 != dst.ipv6))
            break;

        droptcp_drop(dt->net, &src, &dst), p++;
    }
}

static int droptcp_proc_open(struct inode *inode, struct file *file)
{
    struct droptcp_data *d = kzalloc(PAGE_SIZE, GFP_KERNEL);
    if (!d)
        return -ENOMEM;
    d->avail = PAGE_SIZE - (sizeof(*d) + 1);
    file->private_data = d;
    return 0;
}

static ssize_t droptcp_proc_write(struct file *file,
                                  const char __user *buf,
                                  size_t size,
                                  loff_t *pos)
{
    struct droptcp_data *d = file->private_data;

    if (d->len + size > d->avail) {
        size_t new_avail = d->avail + roundup(size, PAGE_SIZE);
        struct droptcp_data *dnew =
            krealloc(d, new_avail + (sizeof(*d) + 1), GFP_KERNEL);
        if (!dnew) {
            kfree(d), file->private_data = NULL;
            return -ENOMEM;
        }
        (d = dnew)->avail = new_avail;
        file->private_data = d;
    }

    if (copy_from_user(d->data + d->len, buf, size))
        return -EFAULT;
    d->data[(d->len += size)] = 0;

    return size;
}

static int droptcp_proc_release(struct inode *inode, struct file *file)
{
    struct droptcp_data *d = file->private_data;
    if (d) {
        droptcp_process(PDE_DATA(file_inode(file)), d);
        kfree(d), file->private_data = NULL;
    }
    return 0;
}

static const struct file_operations droptcp_proc_fops = {
    .owner = THIS_MODULE,
    .open = droptcp_proc_open,
    .write = droptcp_proc_write,
    .release = droptcp_proc_release,
};

static int droptcp_pernet_id = 0;

static int droptcp_pernet_init(struct net *net)
{
    struct droptcp_pernet *dt = net_generic(net, droptcp_pernet_id);
    dt->net = net;
    dt->pde = proc_create_data(DROPTCP_PDE_NAME, 0600, net->proc_net,
                               &droptcp_proc_fops, dt);
    return !dt->pde;
}
static void droptcp_pernet_exit(struct net *net)
{
    struct droptcp_pernet *dt = net_generic(net, droptcp_pernet_id);
    BUG_ON(!dt->pde);
    remove_proc_entry(DROPTCP_PDE_NAME, net->proc_net);
}

static struct pernet_operations droptcp_pernet_ops = {
    .init = droptcp_pernet_init,
    .exit = droptcp_pernet_exit,
    .id = &droptcp_pernet_id,
    .size = sizeof(struct droptcp_pernet),
};

static int drop_tcp_init(void)
{
    int res = register_pernet_subsys(&droptcp_pernet_ops);
    if (res)
        return res;
    return 0;
}

static void drop_tcp_exit(void)
{
    unregister_pernet_subsys(&droptcp_pernet_ops);
}

module_init(drop_tcp_init);
module_exit(drop_tcp_exit);
