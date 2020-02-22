# kecho

This is a lightweight echo server implementation in Linux kernel mode.

## Usage

Run make to build the module and the generic benchmarking tool:
```
$ make
```

On module insertion, you can pass following parameters to the module:
  - port (`12345` by default)

    Port you want the module to listen. If the default port is in use, or you simply want to use another port, you can use this param to specify.
  - backlog (`128` by default)

    Backlog amount you want the module to use. Typically, you only need to change this if you encounter too much concurrent connections warning, which will be logged in the kernel message like this, `Possible SYN flooding on port 12345`. For details about SYN flooding, you can refer to the [SYN flood wiki](https://en.wikipedia.org/wiki/SYN_flood). Changing this param allows the kernel to handle more/less connections concurrently.
```
$ sudo insmod fastecho.ko port=<port_you_want> backlog=<amount_you_want>
```

After the module is loaded, you can use `telnet` to interact with the module:
```
$ telnet 127.0.0.1 12345
```

Also, you can start benchmarking either `kecho` or `user-echo-server` by running the command at below. The benchmarking tool evaluates response time of the echo servers at given amount of concurrent connections. It starts by creating number of threads (which is specified via `MAX_THREAD` in `bench.c`) requested, once all threads are created, it starts the benchmarking by waking up all threads with `pthread_cond_broadcast()`, each thread then creates their own socket and sends message to the server, afterward, they wait for echo message sent by the server and then record time elapsed by sending and receiving the message.
```
$ ./bench
```
 
Note that too much concurrent connections would be treated as sort of DDoS attack, this is caused by the kernel attributes and application specified TCP backlog (kernel: `tcp_max_syn_backlog` and `somaxconn`. Application (`fastecho`/`user-echo-server`): `backlog`). Nevertheless, maximum number of fd per-process is `1024` by default. These limitations can cause performance degration of the module, if you want to perform the benchmarking without such degration, try following modifications:

- Use following commands to adjust kernel attributes:
    ```
    $ sudo sysctl net.core.somaxconn=<depends_on_MAX_THREAD>
    ```
    ```
    $ sudo sysctl net.ipv4.tcp_max_syn_backlog=<ditto>
    ```
    Note that `$ sysctl net.core.somaxconn` can get current value. `somaxconn` is max amount of established connection, whereas `tcp_max_syn_backlog` is max amount of connection at first step of TCP 3-way handshake (SYN).

- Use following command to enlarge limitation of fd per-process:
  ```
  $ ulimit -n <ditto>
  ```
  Note that this modification only effects on process which executes the command and its child processes

- Specify `backlog` with value as large as `net.ipv4.tcp_max_syn_backlog`.

Remember to reset the modifications after benchmarking to keep stability of your machine.

To visualize the benchmarking result with [gnuplot](http://www.gnuplot.info/), run following command to generate the image, and view the result with your image viewer.
```
$ make plot
```