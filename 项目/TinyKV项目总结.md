# TinyKV项目总结

## 项目亮点

1. Region划分与负载均衡处理：
   
   Region按照key Range进行划分，难以天然处理热点问题，而针对这一个问题，TinyKV有负载均衡的策略，可以对Region进行拆分与合并并进行迁移。各个raft group的peer按高可用与负载均匀等规则均匀分布在各个TinyKV节点中。

2. Raftstore为什么没用通用的线程池，而是使用Batch System？
   
   首先raftstore是使用多个线程，每个线程跑在一个循环里面，为活跃的region处理消息，并收集ready，在每轮循环结尾，集中处理。
   
   之所以不用通用的线程池是因为：
   
   第一点raft中的状态（tick，leader，log等）不能并发更改（通用的可以使用锁，但是这样就有很大的锁开销），而使用Batch System的时候可以保证但线程处理region消息。
   
   但是在TinyKV中是使用一个RaftWorker和一个StoreWorker进行处理，并没有实现一个线程池。

3. 使用多个workers进行后台异步处理
   
   将一些耗时操作在后台进行处理比如splitCheckWorker，regionWorker，raftLogGCWorker，schedulerWorker。

## 项目难点

## 项目可扩展点

1. batch System的线程池化
   
   目前是对batch数据用一个线程进行处理，可以对数据进行划分，分成多个小batch然后用多个线程并行处理提高效率。同时针对每个region的raft状态更改，其结果也是与线性执行的结果是一致的。

2. batch System的线程池化2：
   
   每一个peer有一个flag表明，此时的peer是否正在被线程处理，然后如果没有正在被处理，就设置flag，避免其他线程同时处理，然后将这个peer发送到全局的channel<Peer>中通知其他线程处理，如果Peer正在被处理，就通过router找到peer对应的msg_channel然后把消息发送过去

3. batch动态可调
   
   可以根据集群的负载动态的调整batch的大小。降低延迟。

4. 针对raftstore(batch system)的router的改进
   
   可以考虑使用第三方的数据库或键值服务来作为本地的router的命名服务，类似于lrpc的name server，这样的话监测起来更容易，但同时也更危险。也会拖慢系统运行。

5. 根据整个集群的负载，运维规则，对TinyKV发出调度命令：
   
   - Add peer
   
   - Remove peer
   
   - Split region
   
   - Merge region
   
   - Transfer leader
   
   - ...

6. TinyKV的SPLIT策略只检测大小是否到达最大值
   
   到了就分裂。router发送MsgTypeSplitRegion消息。
   
   可以考虑，按表切分、按个数切分，或者按照**热点切分**

7. 没有Merge策略和线程处理Merge。

        Merge很复杂，涉及两个正在运行的raft group。

8. 只在leader上进行存取。
