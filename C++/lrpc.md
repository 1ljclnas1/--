# lrpc

## reactor核心

1. EventLoop中有一个ChannelList和Poller，每一个Channel和Poller都隶属于一个EventLoop

## Channel

对文件描述符进行管理，将对某个文件描述符的可读可写关闭错误等回调函数绑定到Channel类上

Channel与epoller poller eventloop关联

通常需要用epoll监听fd上有哪些事情发生，然后内核从红黑树上把活跃事件取下来，放入双向链表中，再返回给用户空间，

epoll被封装在poller和epoller中，活跃的事件被返回给eventloop，eventloop调用对应的channel的回调函数。

eventloop相当于channel和epoll之间的桥梁。

## Poller

内部对poll封装。

## EventLoop

Channel和Poller之间的桥梁。

比较重要的是runInLoop和queueInLoop。

wakeup：

EventLoop相当于是一个Reactor。

muduo是一个多Reactor模型。支持单Reactor，但是对于高并发的场景，无法承受。

muduo开多个线程，创建多个reactor，每个reactor当作是一个epoll_wait去阻塞，

使用一个主reactor去接受连接请求的到来，将accept的文件描述符分发给子Reactor，子Reactor负责处理事件。

如果主reactor中有一个函数要执行，交给子Reactor执行，而子Reactor正在epoll_wait阻塞，而且没有事件发生不会结束阻塞，这时要想注册函数到子Reactor，可以用wakeup唤醒它，结束阻塞。

lrpc这里在判断当前调用EventLoop的线程是否是EventLoop所属线程的时候与muduo不一样，这里使用的是在创建EventLoop的时候使用std:: thread:id threadId_;判断的时候使用std::this_thread::get_id()；

handleRead是wakeup绑定的回调

doPendingFunctors 执行上层函数，上层向底层注册的回调。

当epoll_wait结束后，先处理事件回调，然后处理上层注册下来的回调。

如果是一个单reactor的时候，runInLoop中直接执行回调函数，

如果是多reactor模型，在非当前EventLoop线程中执行cb，调用queueInLoop将回调函数放入队列中等待执行。

doPendingFuntors函数中没有使用锁，来保证functors的执行顺序，而是，将functorsswap到另一个地方，然后解除阻塞，其他的上层回调可以写入到队列中，减少了临界区长度，提高了效率。

如果在执行上册回调的时候，又加入了5个回调，这个时候是没有办法立即执行的，因此在queueInLoop最后，先判断，不在这个线程中（是上层给下层注册的回调），或者现在正在执行functors，这个时候有一个wakeup操作。来保证后来的回调可以被立刻执行。

如果某个回调函数是queueInLoop的话，是加锁然后执行上层回调函数，而不是swap的话，就会产生死锁问题。

## EventLoopThreadPool

析构函数不去释放loop，因为它是一个栈空间变量

start函数开启对应数量的EventLoopThread

   与muduo不同的是这里的start没有回调函数，仅仅只是创建了对应数量的EventLoopThread。

getNextLoop以轮询的方式取出loop

完成的主要工作：创建对应数量的eventloopthread，放入vector中，并将对应的loop放入loops中，baseloop用来表示主reactor。

## InetAddress

将IP地址端口封装起来，并提供格式化输出。

只提供了ToHostPort，没有像muduo提供三个格式化输出。

但是提供了==运算符

这里用的bzero进行清零，muduo用的memset

## Socket

accept的参数是一个传出参数，accept后这个参数保存的远端连接。

setTcpNoDelay()，是否采用如果数据包特别小，就在网卡中进行堆积，然后发送。这个算法。

## Acceptor

接收器，有连接的时候调用接收器，接收这个连接。

NewConnectionCallback类似线程初始化回调，当连接到达的时候会调用这个函数。可以被归结为上层回调。

handleRead()

这里的loop指针指向的是baseloop

这里对于EMFILE使用idleFd预留一个坑位，用完了之后关闭再恢复idleFd为/dev/null

## TcpConnection

doPendingFunctor解决这里的set***函数

handle*函数是对channel注册的回调。

两个Buffer，一个用于接收数据，一个用于发送数据

## Buffer

使用readIndex和writeIndex将buffer换分为三部分

当两个Index重合时，进行复位

自动增长扩容vector。扩容的时候会前移，以充分利用空间。

kCheapPrepend，用来解决粘包问题，进行分包。

readv可以开辟连续的内存空间，分散读。

用iovec分配两个缓冲区

第一个就是可写部分

第二个是一个栈空间的缓冲区

为了尽快将内核缓冲区的数据尽快移出来所以有第二个栈空间的缓冲区。提高效率

为什么只实现了readFd：从muduo的Buffer类设计来看,有以下几点原因导致其只实现了readFd而未实现writeFd:

1. Buffer类的设计初衷是用于异步读操作,作为读缓冲区。
2. write操作可以直接作用在Socket上,不需要额外的Buffer。
3. 读操作需要填充Buffer,之后从Buffer读取数据处理,更复杂。
4. write只是简单写入Socket,不需要缓存,所以无需writeFd。
5. 提供readFd使得从文件描述符填充Buffer更简单直接。
6. read和write操作不对称,读需要更多Buffer支持。
7. 过早优化,如果没有明确需求,也不需要支持writeFd。
8. 类设计应该遵循单一功能原则,只提供与初衷相关的接口。

所以综上考虑,Buffer专注设计为读操作的缓冲区是合理的,以提高类的内聚性和可维护性。只提供必要的readFd即可。

## Protobuf
