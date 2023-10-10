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

## Proj1 - 单机 KV Server

- 基于Badger实现了一个单点的KV存储引擎

- 实现了行（raw）的get、put、delete、scan的KV API

## Proj2 - 基于Raft的分布式KV服务器

### Part A

- 实现了 Raft的基本算法，包括领导人选举、日志复制等

- 支持了启动时从存储中恢复Raft状态

- 提供了RawNode接口与上层应用交互

### Part B

- 在Raft之上构建了一个键值服务

- 将客户端的KV请求转换为Raft日志命令

- 实现了读请求的线性化读优化

- 处理了各种Region错误和事务错误

### Part C

- 支持了Raft让日志压缩，避免日志无限增长

- 实现了生成快照和恢复快照

- 加速了日志复制和节点恢复过程

### 总结

Part A实现了Raft算法的核心部分

Part B在Raft之上构建了具体的KV服务

Part C进一步优化了日志和快照功能

## Proj 3 - Multi-Raft Server

### Part A

- 在Raft中支持了成员变更（membership change），可以动态添加和删除节点

- 支持了领导者转移，可以在节点间转移领导权

### Part B

- 支持了Raft的配置变更，可以在线改变Raft组成员

- 实现了Region分裂，将一个Region分成两个，支持多Raft组

### Part C

- 引入了Scheduler组件，负责副本的放置和调度

- Scheduler收集各节点的心跳信息对节点进行负载均衡

- 根据负载情况进行节点间的副本迁移

### 总结

Part A加强了Raft算法自身的功能

Part B在RaftStore上支持了配置变更和分裂

Part C进一步引入Scheduler实现负载均衡

使得系统可以动态调整和Scaling，形成一个弹性的分布式KV服务

## Proj4 - Transaction Support

- 使用MVCC为事务提供快照隔离

- 实现事务API，如预写、提交、解析锁定等

- 支持并发请求并解决冲突

### Part A

- 实现了多版本并发控制（MVCC）机制

- 使用多个时间戳版本来存储键值对

- 支持按时间戳获取键值对的指定版本

### Part B

- 实现了事务的读操作，可以读出事务开始时间的快照版本

- 实现了事务的预写操作，锁定并写入键值对

- 实现了事务的提交操作，确认实物的成功提交

### Part C

- 实现了事务的扫描操作，扫描时间戳版本的数据

- 实现了事务状态检查、回滚、解锁等操作

- 支持处理事物间的冲突和超时

### 总结

Part A提供了MVCC的基础存储机制

Part B在此基础上实现了事务的基本操作

Part C进一步完善了事务相关的辅助功能

# CF在TinyKV中的作用？

proj1中的CF，在proj4发挥作用！！！

1. 支持事务
   
   TinyKV使用MVCC和多版本并发控制来实现事务。在同一个CF内，通过不同的时间戳版本来存储同一个key的多个值。这样可以提供事务的快照隔离特性，读取时看到事务开始时的数据库试图。

2. 分离不同类型的数据
   
   不同的CF可以分离不同语义的数据。比如default CF存放业务数据lock CF用于锁信息，write CF用于记录写入数据。

3. 支持多租户
   
   不同的CF可以用来隔离不同的租户数据，避免相互影响。将数据分类存放。

4. 提升性能
   
   可以单独对不同CF进行优化，例如设置更佳的压缩算法，提高查询性能。

5. 方便数据管理
   
   针对不同的CF指定不同的备份策略。
