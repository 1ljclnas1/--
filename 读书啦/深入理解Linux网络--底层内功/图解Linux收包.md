[图解Linux网络包接收过程 (qq.com)](https://mp.weixin.qq.com/s/GoYDsfy9m0wRoXi_NCfCmg)[yanfeizhang/coder-kung-fu: 开发内功修炼 (github.com)](https://github.com/yanfeizhang/coder-kung-fu)

# 图解Linux收包

一个最简单的UDP收报例子如下

```cpp
  int main(){
    int serverSocketFd = socket(AF_INET, SOCK_DGRAM, 0);
    bind(serverSocketFd, ...);

    char buff[BUFFSIZE];
    int readCount = recvfrom(serverSocketFd, buff, BUFFSIZE, 0, ...);
    buff[readCount] = '\0';
    printf("Receive from client:%s\n", buff);

}
```

但是当网络包到达网卡后，直到recvfrom收到数据，这中间，究竟都发生了什么？

## 一、Linux网络收报总览

TCP/IP 网络分层模型中，分为物理层、链路层、网络层、传输层、应用层。物理层是网卡网线，应用层是Nginx、FTP等应用。Linux实现的是链路层、网络层和传输层。

Linux内核中，链路层靠网卡驱动实现，内核协议栈实现的是网络层和传输层。内核对更上层的应用层提供socket接口来供用户进程访问。

![](C:\Users\ljc\Documents\GitHub\--\读书啦\深入理解Linux网络--底层内功\图片\Linux内核网络收包总览.png)

内核和网络设备驱动是通过中断方式处理。有数据到达后，向CPU发出硬中断，进行简单处理，然后触发软中断，软中断通过给内存中一个变量的二进制以通知软中断处理程序ksoftirqd，由它负责软中断。

网卡上收到数据后，Linux中第一个工作的是网络驱动。网络驱动以DMA的方式将网卡上的帧写到内存中。再向CPU发起硬中断（触发电压变化）。然后CPU收到硬中断后，会调用网络驱动注册的中断处理函数（发出软中断），尽快释放CPU。ksoftirqd检测到了软中断请求到达，调用poll开始轮询收包，然后交由各级协议栈处理。对于UDP来说会被放到用户socket的接收队列中。

## 二、Linux启动

但是在这些组件处理网络包之前需要做很多的初始化工作。比如提前创建ksoftirad内核线程，注册号各个协议对应的处理函数，网络设备子系统提前初始化，网卡启动等。当这些都准备好之后，才能真正开始接收数据包。

### 2.1 创建ksoftirqd内核线程

Linux的软中断都是在专门的内核线程中进行的，这个进程数量不是1个而是N个（核数）

![](C:\Users\ljc\Documents\GitHub\--\读书啦\深入理解Linux网络--底层内功\图片\创建ksoftirqd内核线程.png)

当ksoftirqd被创建出来之后，进入自己的线程循环函数ksoftirqd_should_run和run_ksoftirqd。不停的判断有没有软中断需要处理，不仅仅只有网络中断。

```cpp
enum{
    HI_SOFTIRQ=0,
    TIMER_SOFTIRQ,
    NET_TX_SOFTIRQ,
    NET_RX_SOFTIRQ,
    BLOCK_SOFTIRQ,
    BLOCK_IOPOLL_SOFTIRQ,
    TASKLET_SOFTIRQ,
    SCHED_SOFTIRQ,
    HRTIMER_SOFTIRQ,
    RCU_SOFTIRQ,  
};
```

### 2.2 网络子系统初始化

![](C:\Users\ljc\Documents\GitHub\--\读书啦\深入理解Linux网络--底层内功\图片\网络子系统初始化.png)

linux内核通过调用subsys_initcall来初始化各个子系统，网络子系统的初始化会执行到net_dev_init函数

```c
static int __init net_dev_init(void){

    ......

    for_each_possible_cpu(i) {
        struct softnet_data *sd = &per_cpu(softnet_data, i);

        memset(sd, 0, sizeof(*sd));
        skb_queue_head_init(&sd->input_pkt_queue);
        skb_queue_head_init(&sd->process_queue);
        sd->completion_queue = NULL;
        INIT_LIST_HEAD(&sd->poll_list);
        ......
    }
    ......
    open_softirq(NET_TX_SOFTIRQ, net_tx_action);
    open_softirq(NET_RX_SOFTIRQ, net_rx_action);

}

subsys_initcall(net_dev_init);
```

在这个函数里，会为每一个CPU申请一个softnet_data数据结构·，在这个数据结构里的poll_list是等待驱动程序（驱动程序初始化）将其poll函数注册进来。

另外open_softirqd注册了每一种软中断都注册一个处理函数。NET_TX_SOFTIRQD的处理函数是net_tx_action，NET_RX_SOFTIRQ的为net_rx_action。继续跟踪发现这个注册方式是记录在softirq_vec变量里的。后面ksoftirqd线程收到软中断的时候，会用这个变量来找到每一种软中断对应的处理函数。

```c
//file: kernel/softirq.c

void open_softirq(int nr, void (*action)(struct softirq_action *)){

    softirq_vec[nr].action = action;

}
```

## 2.3 协议栈注册

内核实现了网络层的ip协议，也实现了tcp和udp协议。这些协议对应的实现函数分别是ip_rcv()，tcp_v4_rcv(), udp_rcv()。内核是通过注册的方式实现的。

Linux中的fs_initcall和subsys_initcall类似，也是初始化模块的入口。fs_initcall调用inet_init后开始网络协议栈的注册。通过inet_init，将这些函数注册到了**inet_protos**（**UDP和TCP**）和**ptyoe_base** （**IP**）数据结构中。

![](C:\Users\ljc\Documents\GitHub\--\读书啦\深入理解Linux网络--底层内功\图片\AF_INET协议栈注册.png)

```c
static struct packet_type ip_packet_type __read_mostly = {

    .type = cpu_to_be16(ETH_P_IP),
    .func = ip_rcv,
};
static const struct net_protocol udp_protocol = {
    .handler =  udp_rcv,
    .err_handler =  udp_err,
    .no_policy =    1,
    .netns_ok = 1,
}; 
static const struct net_protocol tcp_protocol = {
    .early_demux    =   tcp_v4_early_demux,
    .handler    =   tcp_v4_rcv,
    .err_handler    =   tcp_v4_err,
    .no_policy  =   1,
    .netns_ok   =   1,

};

static int __init inet_init(void){
    ......
    if (inet_add_protocol(&icmp_protocol, IPPROTO_ICMP) < 0)
        pr_crit("%s: Cannot add ICMP protocol\n", __func__);
    if (inet_add_protocol(&udp_protocol, IPPROTO_UDP) < 0)
        pr_crit("%s: Cannot add UDP protocol\n", __func__);
    if (inet_add_protocol(&tcp_protocol, IPPROTO_TCP) < 0)
        pr_crit("%s: Cannot add TCP protocol\n", __func__);
    ......
    dev_add_pack(&ip_packet_type);
}
```

udp_protocol结构体中的handler是udp_rcv，tcp_protocol结构体中的handler是tcp_v4_rcv，通过inet_add_protocol被初始化了进来。

```c
int inet_add_protocol(const struct net_protocol *prot, unsigned char protocol){
    if (!prot->netns_ok) {
        pr_err("Protocol %u is not namespace aware, cannot register.\n",
            protocol);
        return -EINVAL;
    }

    return !cmpxchg((const struct net_protocol **)&inet_protos[protocol],
            NULL, prot) ? 0 : -1;

}
```

inet_add_protocol函数将·tcp和udp对应的处理函数都注册到了inet_protos数组中。再看dev_add_pack(&ip_packet_type);，ip_packet_type结构体中的type是协议名，func是ip_rcv函数，在dev_add_pack中会被注册到ptype_base哈希表中。

```c
//file: net/core/dev.c

void dev_add_pack(struct packet_type *pt){

    struct list_head *head = ptype_head(pt);
    ......

}

static inline struct list_head *ptype_head(const struct packet_type *pt){

    if (pt->type == htons(ETH_P_ALL))
        return &ptype_all;
    else
        return &ptype_base[ntohs(pt->type) & PTYPE_HASH_MASK];

}
```

这里要记住**inet_protos记录着udp，tcp的处理函数地址**，**ptype_base存储ip_rcv()函数地址**， 之后的ksoftirqd会通过ptype_base找到ip_rcv函数地址，ip_rcv中通过inet_protos找到tcp、udp的处理函数然后转发给tcp或udp处理。
