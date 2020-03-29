# kecho

This is a lightweight echo server implemented in the Linux kernel mode.
Since a device driver named "echo" exists in directory "drivers/misc/echo"
of Linux kernel tree, we specify `kecho` as the registered kernel module
name.

## Usage

Run `make` to build the kernel module along with userspace implementation
and  generic benchmarking tool:
```shell
$ make
```

On module loading, you can pass the following parameters to the module:
  - port (`12345` by default)

    Port you want the module to listen. If the default port is in use, or you simply want to use another port, you can use this param to specify.
  - backlog (`128` by default)

    Backlog amount you want the module to use. Typically, you only need to change this if you encounter warning of too much concurrent connections, which will be logged in the kernel message like this, `Possible SYN flooding on port 12345`.  For details about SYN flooding, you can refer to the [SYN flood wiki](https://en.wikipedia.org/wiki/SYN_flood). Changing this param allows the kernel to handle more/less connections concurrently.
  - bench (`0` by default)
  
    By setting this param to `1`, you gain better cache locality during benchmarking the module. More specifically, we use [`WQ_UNBOUND`](https://www.kernel.org/doc/html/latest/core-api/workqueue.html#flags) as workqueue (hereafter called "wq") creation flag for the module by default, because this flag allows you to establish both long term (use telnet-like program to interact with the module) and short term (benchmarking) connection to the module. However, this flag has a trade-off, which is cache locality. The origin of this trade-off is that tasks submitted to a unbounded wq are executed by arbitrary CPU core. Therefore, you can set the param to `1` to disable `WQ_UNBOUND` flag. By disabling this flag, tasks submitted to the CMWQ are actually submitted to a wq named system wq, which is a wq shared by the whole system. Tasks in the system wq are executed by the CPU core who submitted the task at most of the time. **BE AWARE** that if you use telnet-like program to interact with the module with the param set to `1`, your machine may get unstable since your connection may stalls other tasks in the system wq. For details about the CMWQ, you can refer to the [documentation](https://www.kernel.org/doc/html/latest/core-api/workqueue.html).
```shell
$ sudo insmod kecho.ko port=<port_you_want> backlog=<amount_you_want> bench=<either_1_or_0>
```

After the module is loaded, you can use `telnet` command to interact with it:
```shell
$ telnet 127.0.0.1 12345
```

Alternatively, you can execute the builtin test suite:
```shell
$ make check
```

Also, you can start benchmarking either `kecho` or `user-echo-server` by running the command at below. The benchmarking tool evaluates response time of the echo servers at given amount of concurrent connections. It starts by creating number of threads (which is specified via `MAX_THREAD` in `bench.c`) requested, once all threads are created, it starts the benchmarking by waking up all threads with `pthread_cond_broadcast()`, each thread then creates their own socket and sends message to the server, afterward, they wait for echo message sent by the server and then record time elapsed by sending and receiving the message.
```shell
$ ./bench
```
 
Note that too much concurrent connections would be treated as sort of DDoS attack, this is caused by the kernel attributes and application specified TCP backlog (kernel: `tcp_max_syn_backlog` and `somaxconn`. Application (`kecho`/`user-echo-server`): `backlog`). Nevertheless, maximum number of fd per-process is `1024` by default. These limitations can cause performance degration of the module, if you want to perform the benchmarking without such degration, try the following configurations:

- Use the following commands to adjust kernel attributes:
    ```shell
    $ sudo sysctl net.core.somaxconn=<depends_on_MAX_THREAD>
    $ sudo sysctl net.ipv4.tcp_max_syn_backlog=<ditto>
    ```
    Note that `$ sysctl net.core.somaxconn` can get current value. `somaxconn` is max amount of established connection, whereas `tcp_max_syn_backlog` is max amount of connection at first step of TCP 3-way handshake (SYN).

- Use the following command to enlarge limitation of fd per-process:
  ```shell
  $ ulimit -n <ditto>
  ```
  Note that this modification only effects on process which executes the command and its child processes

- Specify `backlog` with value as large as `net.ipv4.tcp_max_syn_backlog`.

Remember to reset the modifications after benchmarking to keep stability of your machine.

To visualize the benchmark results with [gnuplot](http://www.gnuplot.info/), run the following command to generate the image, and view the result with your image viewer.
```shell
$ make plot
```

## drop-tcp-socket

In case of being unable to release TCP connections, there is another Linux kernel module,
`drop-tcp-socket`, which allows one to drop specific TCP connections. It is extremely
useful for dropping `TIME-WAIT` sockets.

Load `drop-tcp-socket` kernel module:
```shell
$ sudo insmod drop-tcp-socket.ko
```

Drop single TCP connection: (Assume port 12345 was listened by kecho)
```shell
$ netstat -n | grep WAIT | grep 12345
tcp        0      0 127.0.0.1:36986         127.0.0.1:12345         TIME_WAIT
$ echo "127.0.0.1:36986 127.0.0.1:12345" | sudo tee /proc/net/drop_tcp_sock
```

Drop multiple TCP connections:
```shell
$ netstat -n | grep WAIT | grep 12345 | awk '{print $4" "$5}' | sudo tee /proc/net/drop_tcp_sock
```


## License

`kecho` is released under the MIT License. Use of this source code is governed by
a MIT License that can be found in the LICENSE file.
