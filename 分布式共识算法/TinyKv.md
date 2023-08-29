# TinyKv

## 分片算法

TinyKv采用Range分片算法

分片算法有两种Hash分片和Range静态分片

### Hash分片

按照数据记录中指定关键字的Hash值将数据记录映射到不同的分片中。本质上，Hash分片是一种静态分片，必须在设计之初约定分片的最大规模，同时因为Hash函数已经过滤掉了业务属性，也很难访问业务热点问题。

对节点数取余。

### 一致性Hash分片

环状，虚拟节点。

### range静态分片

Range分片的特点恰恰是能够加入对于业务的预估

相对Hash分片，Range分片的适用范围更加广泛。其中一个非常重要的原因是，Range分片可以更高效的扫描数据记录，而Hash分片由于数据被打散，扫描操作的I.O开销更大

### Range动态分片

1. 分片可以自动完成分裂与合并

2. 可以根据访问压力调度分片

有条件做到更好的动态调度，只有动态了，才能适应各种业务场景下的数据变化，平衡存储、访问压力、分布式事务和访问链路延时等多方面的诉求。

range分片，按照时间区间或ID区间来切分，优点单表大小可控，天然利于水平扩展，缺点热点数据成为性能瓶颈。

动态分片解决单点性能瓶颈问题

## Tinykv Range

TinyKv使用Range的原因是能更好的将相同前缀的key聚合在一起，便于scan等操作，这个Hash是没法支持的，当然，在split/merge上面Range也比Hash好处理的多，很多时候只会设计元信息的修改，都不用大范围的挪动数据。

当然，Range有一个问题在于很可能某一个Region会因为频繁的操作称为性能热点，可以通过PD将这些Region调度到更好的机器上，提供Follower分担读压力等。

总之，使用Range来对数据进行切分，将其分成一个一个的Raft Group，每一个Raft Group，我们使用Region来表示。

共识的单位是Region

## Server概述

### TiKV

TiKV作为TiDB的行存储引擎，位于存储层

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\TiDB.PNG)

TiKV通常部署为多个TiKV server，基本的处理流程（未优化

可以被认为：

- Region中的leader接收SQL引擎层的请求

- Leader将请求作为日志追加到自己的log中；

- Leader向follower同步日志

- Leader等follower的回复，如果符合quorum机制follower返回，那么leader可以commit这个请求，并在本地进行应用。

- Leader返回client处理结果；

- 开始服务后续的请求。

## Server的结构概述

### 简单说Region

- Region的划分是为了实现存储的水平扩展，将数据分散，每一个Raft Group专门负责一个Region数据的同步

- 在一个store上，存在若干个Peer，这些Peer管理着这个store上对应region的信息

- 同一个region，由一个raft group，依照Raft共识算法，Leader牵头来接收Client的命令，保证在不同store上的Peers有着一致的数据

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\Region.PNG)

### 模块层级

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\模块层级.PNG)

- 借鉴TiKV模块层级，所有与Raft有关的逻辑都在Raftstore里。它向下完成对RocksDB的读写交互，向上提供接口，完成Request的接收，response的发送

- 2B完成的Propose和apply两个过程是一个Peer在接收到Message之后发生的事情。

- 想要更好地了解整个流程，我们很有必要知道这个Message是怎么发送过来的，Respponse又是怎么发出去的，整个Peer的工作是怎么被驱动的。

### Request在Raftstore层面上发生了什么

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\Request在Raftstore层面上发生了什么.PNG)

1. Client请求到达Leader，首先要进行Propose，放入leader的日志中

2. 经过Raft group，将该log向其他节点同步

3. 当该log被大多数节点接收，也就是被commit后，Leader就会把这个日志中的请求取出并执行；（process会发生在所有节点身上，只是leader还需要回复response）

4. leader执行完毕后，发送response

### 从Raft ready开始

#### RawNode

1. Step    驱动Raft解决接收到的消息

2. Tick     驱动Raft的计时

3. Propose  Leader接收到来的entry，并存放至日志，向其他成员同步；

4. RaftReady 暂存即将发送的消息，在本次流程中已经被commit，等待被apply的entries，和unstable的entries。同时也会保存可能会有的pending Snapshot，发生更改的hardState和softState。

#### Ready

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\Ready.PNG)

#### 从Peer的角度看整体的流程

在server这个层次上，我们把每个节点都封装了成了Peer（定义在kv/raftstore/peer.go），对节点的操作也就变成了对Peer的操作。先简单总结一下Raft Message的处理流程

1. 当有Raft Message发送至指定的Peer，调用RaftGroup（RawNode类型）的Step()函数。进入处理流程；

2. 节点根据自身身份和消息类型，进入不同的处理流程，更改状态，同步日志，并生成将要发送的消息，放在r.msgs中；

3. 当上层需要进一步处理时，首先通过HasReady()来判断目前是否有需要待处理的事项；：
   
   - 是否有消息需要发送？
   
   - 是否有snapshot需要apply
   
   - 是否有需要被apply的entry和新增的unstable entry
   
   - hardState是否发生了改变？
   
   - Group里面的Leader或者自身的身份是否发生了改变
   1. 如果有符合以上的待处理事件，那么就会接着调用Ready()来生成一个包含所有待处理信息的Ready结构
   
   2. 在Ready被处理完毕后，最后调用Advance来更新group状态

### Request从Propose到执行

#### Server接收消息

当Raft Storage收到Client的Read/Write调用，首先考虑的是将用户的任务批量打包成消息，交付由raftstore来进行处理（即直接通过router发送），设置callback管道来等待接收Resp。（kv/storage/raft_storage/raft_server.go）

- 一个RaftCmdRequest，要么包含多个Normal Request，要么包含一个Admin request

- Normal request分为Write和Read两种，Write包括Put，Delete两种类型，Read指的是Snap，Get。一个RaftRequest可能包含了多个request，但是只会包含同一类。

- 当为Write类型时，server等待的是返回的Response，这个Response主要是用来判别Write请求是否成功。当为Read类型时，server等待的是Response中的transaction，通过这个transaction返回一个reader，共client使用。

#### 测试里的Request的发送流程

1. 以一个Put请求为例，在构造好这样一个Put请求后，把它装进上下文提到的Request消息中，通过Request()发送

2. 在Request中（定义在kv/test_raftstore/cluster.go），先通过cluster获取要发送Peer的region信息，然后构造一个RaftCmdRequest，通过CallCommandOnLeader()进行实际上的发送。在Request()中,同一个request最多将会被重复发送十次，直到收到正确的Response并且没有到达规定的Request的超时限制。

3. 在callCommandOnLeader()中（kv/test_raftstore/cluster.go），会先找到这个Region的Leader，然后通过CallCommand()命令，将这个request命令发送至Leader。如果在1s内没有收到回复，那么就重新获取当前Region的Leader，再次尝试发送；同样，如果发现response是错误的，就根据错误中的提示信息，换一个Leader或者重新获取Region，再次发送。重复上述的过程直到收到正确的Response，或者超时。
   
   1. 在callCommand()中，是通过NodeSimulator（kv/test_raftstore/node.go），将request通过router发送到Sender中，交由Worker处理。

#### Raftstore启动worker

1. Raft Worker
   
   - 它持有着peerSender的接收端，由raftstore在启动worker线程执行run(),进行主要操作
   
   - RaftWorker会执行一个内嵌select的循环，阻塞在等待接收raftCh中的msg，如果接收到，就依次使用PeerMsgHandler来进行消息的proposer，处理完毕，就调用HandleRaftReady(), 来处理节点生成的Ready信息。做完这些工作后，就继续等待
   
   - 直到收到CloseCh信号，则结束这个过程。

2. Store Worker
   
   - 它持有storeSender，来接受与store有关的消息，他也是通过run()来完成功能的执行
   
   - 基本任务：
     
     - 定期向调度器发送心跳，汇报store内的信息，自身的状态。
     
     - 定期清理断线Peer留下的Snapshot。这些Snapshot是Idle的，且由于产生了snapshot，这些Peer一定是存在的。
     
     - storeWorker中有独立的handle msg的过程，主要处理的是store本身需要处理和汇报的消息，同时也处理raftworker没有发送成功的Raft Msg。
   
   - Peer的被动创建
     
     - 尝试发送之前未成功发送的msg
     
     - 发现发送Peer合法且未被创建，会继续尝试被动创建该Peer。

3. 其他的Worker
   
   - raftlog-gc worker 日志删除
   
   - region worker snapshot生成，apply
   
   - split checker worker split命令
   
   - scheduler worker 接受一些leader心跳，发送split

#### PeerMsgHandler Propose请求

- Raft Woker在收到Raft消息后，首先调用PeerMsgHandler的HandleMsg(),来Propose消息

- Propose的目的就是将这个Request以entry的形式，放到Peer目前的RaftLog中。等到这个日志被group的大部分commit后，再通过HandleRaftReady()来进行执行。

- 能够成功接收Propose请求的应该是Leader，Leader才能处理一个MsgPropose格式的消息，并且保留callback, 以便在process的过程中回复。

#### HandleRaftReady Process请求

- Leader在Propose后，会在Peer中将callback记录在proposals，等待process后回复。那么Raft Worker在处理完一批Msg后，就调用HandleRaftReady(),来处理一批已经被commited的entry。

HandleRaftReady的大致流程如下：

1. 如果这个Peer已经被destroy掉，自然就不需要进行。

2. 判断一下当前是否有group要更新的信息，如果有，那么获取一个Ready；

3. 首先通过PeerStorage的SaveReadyState(),将ready里面的状态信息存入Peerstorage中（主要是raftState和applyState中的内容，如果有snapshot，则将snapshot apply后的状态也在这里存入）。

4. 把该发送的信息发送出去。

5. 对这次的commited entry进行Apply

6. 通过advance()来更新Raft group的状态，继续下一个loop。

#### HandleRaftReady Request的处理

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\Request.PNG)

以上是一个Request的定义，Cmd Type表明这个Request的操作类型，根据不同的类型，到对应的GetRequest/PutRequest/DeleteRequest/SnapRequest中取出可能需要的Cf，Key，Value。

如果一个RaftCmdRequest中包含的是Normal Request，那么一次性可能需要处理多个Read或者Write的request，多个request的response需要装在一起，并通过callback返回。

#### HandleRaftReady Request的处理

1. 首先先从entry中将存入的request取出来，如果发现这是一个Normal Request就交由processNormalRequest()来处理。

2. 寻找这条Request的Callback，在Propose的时候Callback已经被放在了Peer的Proposals中，寻找只需要按从前到后的顺序寻找即可。在这条entry的Index之前的Proposal，可能是因为entry没有被成功commit，孤儿没有进入process流程，这些proposal都可以舍弃掉。
   
   > 当然只有当时的Leader才拿有proposal，也就可以进行response，就算没有proposal，也需要执行一遍process的流程，只是不用回复response。

3. 一次执行这个Requests里面的所有读/写请求，这些读写请求都需要构造独立的Response，并将这些Response统一放进整个Request的RaftCmdResponse中。当然对于一个读请求或者写请求，他们的process流程是有区别的。
   
   - 如果是Write类，那么不管Peer是否具有Callback，都需要执行一遍Put/Delete操作。如果拥有Callback，则还需要构造一个空Put/Delete Response, append到RaftCmdResponse的Responses中。
   
   - 如果是Read类，则只需要当时的Leader进行回复，它们即完成向Callback填入RaftCmdResponse的步骤。如果一个读请求是Get，那么它的Response是希望获取到Value值；而如果是一个Snap请求，那么他希望得到的是开启一个供读的新transaction，并附上当前Peer的Region。

4. 一个Request中的所有请求都处理完成后，就可以认为这个entry已经被apply，更新applystate到当前entry的Index，并写入KV DB。

### 从Peer Storage的角度看index变化

#### 各种index的含义

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\index.PNG)

- lastIndex：指向目前存储日志的最后一个。从stable到last index这一部分的日志属于还没有persist到storage的（被叫做unstabled），等待着下次调用ready时，被处理存入storage。

- Stabled指向被存入storage的最后一个日志条目。从commited到stabled这一区间的日志是指已经被存入storage，但还没有被commit的。当Group中大多数节点收到这些日志条目时，即认为被commit。

- Commited：指向被commit的最后一个日志条目。从applied到commit这一段的日志条目是指等待下一次调用ready时，进行执行的日志。

- Applied：指向已经被执行的最后一个日志条目。从truncated到applied这一区间是还没有被snapshot压缩，但已经被执行过的日志。

- truncated：最左侧的日志被称为truncated日志，也即被snapshot压缩过的日志。它们已经不在Raft日志中。

#### Peer storage的初始化

1. 加载raftEngine中存储的RaftLocalState。

2. 加载KV Engine中的ApplyState

InitRaftLocalState出现加载错误的情况：

- 如果是因为Badge中未找到Key，并且这个region是存在peer的，那么就说明这时此store上全新的Region（一般是split产生的新Region）。为了标明其初始的身份，需要生成一个初始的raftstate，将LastIndex，LastTerm，HardState中的Commit均置为5，并存入Rat DB。

为什么要初始化为5？与被动创建区别开

#### Peer storage的日志查询工作

**Entries**

接收参数：（low，high uint64）待取出entries的两端Index，索引范围[low, high)

返回值：([]entry, error)取出所有的entry

可能产生的错误信息：

- low, high能够索引出的范围需要在[truncatedIndex+1, lastIndex+1)中，若不符合这个范围，那么会返回对应的越界错误

- 迭代器在访问DB中的数据时，可能不获发生的错误

- ErrUnavailable：如果得到的entry数目不符合要求的数目，会范围ErrUnavailable错误

Entries的要取出的日志范围必须要大于truncatedIndex，这样才会在PeerStorage中存在。然后申请一个tramsaction，通过迭代，一个一个取出，直到最后一个Key（不包含）

最后进行判断，如果数目不足，那么就要返回ErrUnavailiable错误

**Term**

接收参数：(idx uint64)待查询日志条目的index

返回值：(uint64, error)查询日志的term以及error信息

可能产生的错误：

- idx所在范围应该是[truncatedIndex, lastIndex+1)，超出则越界

- 会捕获在GetMeta时可能产生的错误。

Term查询通过GetMeta来获取日志，由于storage本身保存了最后一个被truncate的日志Index和Term，所以可以额外查到truncatedIndex对应的日志。

1. 如果idx是truncatedIndex，那么就返回截断时那条日志的Term

2. 其他情节下不符合range要求，则产生越界信息

3. 如果是正常的查询，就通过GetMeta来获取

#### Peer storage Index的更新

##### Last index的更新（stabled）

Last Index指向的是Peer Storage存入的最后一个日志，也就是stabled的最后一个日志，每次在Ready中，都会将这一阶段的unstable entry放入。然后Peer Storage就会在SaveReadyState()中调用Append，将这些entry存入DB，并更新LastIndex

##### Commited index的更新

commit的变化是在Raft Group层面发生的，只要Leader日志同步的过程是正常的，这里就不会出现问题。Peer Storage在raftState中保存HardState，这里面会包括目前日志的commit情况，它的更新也是发生在Process entry之前的SaveReadState()。

##### Applied index的更新

Applied Inde的更新会发生在SaveReadyState()和process entry两个阶段，前者的更新是因为snapshot，而后者就是entry被apply后的正常更新。

##### Snapshot导致的State更新

既然说到这，就不得不提到Snapshot导致的State更新了。在这里让我们先只关注state的更新。

在SaveReadyState()阶段如果发现有需要apply的snapshot，那么就通过ApplySnapshot()进行state的更新。由于PeerStorage由专门的一个snapState记录snapshot目前的状态，还有发送的Receiver。

> 考虑到Snapshot的Apply是比较花时间的，为了不让这个过程阻塞线程，会有一个线程来额外去做这个事情，所以会有一个Receiver。

更新snapshot的状态为Snapshot_Applying，snapshot本身携带着一个Index，来表示truncated Index到了哪里。这时候就可以设置LastIndex，Applied最后将snapshot的data部分通过Receiver发送，让实际的Apply过程由另一个线程完成。

### Raft的可改进之处

#### 读写分离

TinyKV Server需要保证线性一致性

原先做法：将Read走一遍log，实现线性read

不走Log，需保证：当前的Leader是稳定的，当前读取的value是最新的。

可用ReadIndex改进：

1. 将当前自己的commit index记录到一个local变量ReadIndex里面。

2. 向其他节点发起一次heartbeat，如果大多数节点返回了对应的heartbeat response，那么leader就能够确定自己仍然是leader。

3. Leader等待自己的状态机执行，直到apply index超过了ReadIndex，这样就能够安全的提供linearizable read了。

4. Leader也可以执行read请求，将结果返回给client

### Asynchronous Apply

当一个log被大部分节点append之后，我们就可以认为这个log被commited了，被commited的log在什么时候被apply都不会再影响数据的一致性。所以当一个log被commited之后，我们可以用另一个线程去异步的apply这个log。

所以整个Raft流程就可以变成：

1. Leader接受一个client发送的request

2. Leader将对应的log发送给其他follower并本地append

3. Leader继续接受其他client的requests，持续进行步骤2.

4. Leader发现log已经被commited，在另一个线程apply

5. Leader异步apply log之后，返回结果给对应的client

#### 没有必要的多余选举

如果一个Follower因为物理隔离，导致无法和其他节点通信。如果他开始了选举，那么它自身的Term会+1，并且成为了候选者。这时候如果网络恢复，将会有以下可能的情况：

1. 原集群有了很多新日志或者经历了很多Term，这时候这个Follower会乖乖变成Follower

2. 如果原集群没有什么变化，这个follower就会带起新的选举，很有可能导致集群leader的改变，影响性能。

Prevote机制：

1. 节点A决定选举，先不自增Term，改变自己的投票状态

2. 相机群其他节点发送一个PreVote消息。该消息携带的内容与RequestVote一致（Term都是A的Term+1），名字仅用于区别两种消息。同时A更新自身的投票状态（即PreVote发起时，不投票给自己）

3. 其他节点收到消息后，判断是否支持该选举请求，并返回结果。判断方式与RequestVote相同，但步更新自身的投票状态。

4. A统计所有的票数（包括自己的一票），如果确认可以当选，那么A发起普通的选举流程。

#### 隔离下的退化机制

设想这样一种情况，如果在一个Group中，Leader因为网络被单独隔离。Leader发送的消息实际上并没有被接受，而其他节点发送的消息也没有被这个Leader收到。

由于选举超时这一机制，其他节点也自然会展开新的选举，产生新的leader，但是老Leader对此并不知情。在没有收到外界消息时，仍然认为自己保持着Leader的身份。一段时间内在上层的Region信息中，仍然认可这个老Leader身份，并向他发送Client请求。这样子Group就无法正常工作。

所以我们需要有一个退化机制，这里可以用到一个CheckQuorum机制。

leader发送的心跳一段时间没有收到后，leader就可以尝试向其他节点发送一个专门的CheckQuorum消息，与心跳消息类似，如果一段时间后发现自己连接的节点个数没有超过半数，就自动把自己退化为follower。

### 从Request执行流程来学习Raft Store

#### Request执行流程

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\Request执行流程.PNG)

1. 事务逻辑（MVCC Percolator）

2. 调用Storage接口

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\request流程图.PNG)

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\Async——Apply优化.PNG)

1. Apply模块如何设计？

2. Apply模块和Peer分别需要维护什么状态？

3. Apply模块需要返回处理结果么？需要返回什么处理结果？处理结果如何返回 

peer层的时候，写代码不要太耦合，不利于3b做新的处理逻辑。（Admin指令的判断逻辑比较多）

#### Snapshot相关细节与优化

1. Generate
   
   生成是一个异步操作，将RegionTaskGen丢入regionSched中，请求region worker执行生成任务。

2. Send
   
   Leader认为有follower落后太多的时候，发一个Snapshot过去（peer.sendRaftMessage，通过transport发送到其他节点）ServerTransport.SendSnalshotSock()

3. Apply
   
   peerStorage.ApplySnapshot

Leader无法感知是否给某一个follower发送过snapshot，重复发送导致网络带宽占用过大。

可以在Leader上保存每一个follower最近的活跃时间，防止发过多的snapshot

### Multi-Raft算法简介及代码框架介绍

#### Agenda

- Multi-Raft in TiKV: RaftStore

- The runtime of raftstore: Batch System

- Handle schedule

- Q & A

### Part I  Multi-Raft in TiKV: RaftStore

##### Why Multi-Raft

- 水平扩展性：一个raft group，加机器只能加强可用性，瓶颈还是单台机器所能承受的数据量

- 并发度：raft依赖可复制状态机，提供线性一致性，apply log one by one in order

- 负载均衡：细数据粒度--方便调度（负载，cpu，磁盘空间...etc）raftgroup数量随数据量以及业务负载动态变化

- ...

##### Multi-Raft 于Region

- Region
  
  - TiKV中管理数据的基本单位
  
  - 数据切片中，按连续的一段key range划分
  
  - 一个region一个raft group
  
  - 多个数据副本对应raft group中的多个peer

- Multi-Raft
  
  - 一个集群的数据被划分为多个region
  
  - 多个独立运行的raft group
  
  - **各个raft group的peer按高可用与负载均匀等规则均匀分布在各个TiKV节点中**

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\Raftstore.PNG)

#### Part II The runtime of raftstore: Batch System

Raftstore 怎么跑起来？在内存里怎么组织

- Batch System
  
  - 一个线程池
  
  - 每个线程跑在一个循环里面
  
  - 每轮循环，为活跃region处理消息，并收集其ready
  
  - 每轮循环结尾，集中处理所有ready

##### Batch System

- why not a thread pool library
  
  - raft中的状态不能并发更改，batch system保证单线程处理region消息
  
  - batch io提高效率

##### Batch System - structs

内存里存了什么？

- Router：HashMap<region id, PeerFsm>
  
  - 每个peer创建时注册
  
  - 每个peer销毁时注销
  
  - PeerFsm：some states & msg_channdel

- Each thread: a global Chandel<PeerFsm>

##### Batch System - How to notify

收到一条raft message

- 如果Peer正在被线程处理
  
  - 根据Router找到Peer的msg_channel
  
  - 发送message到msg_channel

- 如果Peer为正在被线程处理
  
  - 设置flag，避免其他线程同时处理
  
  - 发送Peer到全局Chandel<PeerFsm>
  
  - goto "如果Peer正在被线程处理"流程

##### Batch System - Handling

- begin: 初始化一些处理状态

- handle_control: 处理一些全局相关的消息
  
  - store heartbeat tick
  
  - ...

- handle_normal: 处理每个peer
  
  - handle raft message -> step 函数
  
  - collect ready -> ready函数

- end：集中处理ready
  
  - append logs
  
  - 发送raft message
  
  - 发送 commited logs 去 apply（另一个batch system）

##### 负载均衡

根据整个集群的负载，运维规则，对TinyKV发出调度命令：

- Add peer

- Remove peer

- Split region

- Merge region

- Transfer leader

- ...

#### Multi-Raft与Region

##### Region

- 基本数据单位

- 数据切片，按照key range切分

- 一个region一个raft group

- 多个数据副本对应raft group中多个peer

##### Multi-Raft

- 一个集群的数据划分为多个region

- 多个独立运行的raft group

- 各个raft group的peer按高可用与负载均匀等规则均匀分布在各个TinyKV节点中

### Part III - Handle

#### Region Split

为什么需要split

- 加大并发

- 缓解热点

Split策略

- 按表切分

- 按大小切分

- 按key个数切分

- 按热点切分
  
  - load base split  

Split流程

- 后台检测region是否需要split

- 为region计算split key

- 向PD申请新region以及peer的ID

- Leader构建split command作为一条admin raft log

- 进行与普通写入一样的propose & commit流程

- Apply split command
  
  - 根据split key计算key range
  
  - 并根据region id，peer id，key range等构建新region的元信息
  
  - 更改原region的key range
  
  - 不涉及数据搬迁。

#### Add/Remove Peer

目的一般是在TinyKV节点间移动副本

- 负载均衡

- 上下线TinyKV节点

- 节点故障

- ...

通过Raft ConfChange进行

- Simple Confchange：只包含一个变动

- Joint consensus ConfChange：可包含多个变动 （TiKV）raft小论文的joint

Add/Remove peer流程

- 下发ConfChange command

- ConfChange command作为admin raft log被propose & commit

- Apply ConfChange command
  
  - 更改region元信息中的peer list
