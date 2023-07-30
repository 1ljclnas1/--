# ZAB--ZooKeeper技术核心

ZAB贯穿ZooKeeper的整个运行过程，包括事务请求处理、崩溃恢复以及Leader选举。

## Chubby简介

由于ZooKeeper解决的问题与Chubby大同小异，因此先介绍Chubby

Chubby是Google开发的分布式锁服务，着力解决分布式系统的协调问题，为分布式系统提供一个粗粒度锁服务的同时，还提供了可靠的小容量数据存储服务。在Google内部，GFS和BigTable都是用chubby来完成Master的选举，并在Chubby存储了少量的元数据。Chubby并不开源，对它的了解都来自于Google公开的论文The Chubby lock service for loosely-coupled distributed systems。

## ZooKeeper的简单应用

### 5.2.1 ZooKeeper是什么

独立于系统之外的协调服务。提供了包括配置服务、命名服务、分布式同步（锁）、提供者组等功能。

### 数据节点

ZooKeeper被认为是Chubby的开源实现，很大原因是其沿用了Chubby的数据模型。

ZooKeeper的数据存储为文件目录形式，可以通过ZooKeeper提供的API创建和更新文件。查询和更新时需要绝对路径 如 ls /ofcoder/ls。最常用的有两种节点**永久节点和临时节点**，他们都支持显示的删除。

#### 1. 永久节点

最普通的节点类型。一旦被创建就一直保存在ZooKeeper上，除非被显示的调用删除操作。不但参数的create创建的就是永久节点。

#### 2. 临时节点

当撞见该节点的客户端因会话超时或异常关闭时，ZooKeeper自动删除该节点。

在日常开发中通常使用临时节点来管理成员的上下线操作。临时节点不允许创建子节点。使用create -e可以创建。

#### 3. 有序节点

基于永久节点和临时节点之上的节点属性，即可以跟永久节点组合也可以和临时节点组合。

创建节点时，ZooKeeper会自动为节点名称增加单调递增的序号。

可以用来实现分布式环境下的公平锁，意味着获取锁时应该按照先后顺序进行处理。

#### 4. 其他类型节点

Container节点：使用create path [-c]创建，当该节点的最后一个子节点被删除时，该节点将在未来某一时刻由ZooKeeper自行删除（默认60秒）

如果Container节点没有子节点，表现为永久节点特性。

如果创建了子节点，并且删除了所有子节点，则该节点也会被删除。

TTL节点：分为有序TTL和无序TTL，使用create path -t。如果在该节点给定的时间内没有被修改且没有子节点，则该节点会被删除。

#### 5. 节点信息

每一个数据节点除了存储自身的文件内容之外，还维护者一个二进制数组，用于存储节点的元数据，如ACL控制信息、修改信息及子节点信息。可以使用get或stat命令查询节点信息。

### Watch机制

#### 1. 应用场景

Watch时ZooKeeper提供给客户端观察节点数据变更的一种机制。命名服务可以通过watch机制，让客户端及时感知服务列表变更情况；锁服务也可以这样。

**需要注意：Watch机制是一次性的，一旦事件触发后，对应的Watch注册就会被删除**

### ACL权限控制

通常情况下，ZooKeeper不是服务与某个单一的系统，而是为分布式下的多个系统服务。因此，需要权限控制来区分节点数据的归属。

一个ACL权限由三部分组成：鉴权模式、授权对象、操作权限。

- 鉴权模式：分别由Digest、Super、Ip和Word模式。

- 授权对象：即权限赋予对象，当鉴权模式为Digest时，授权对象为用户信息。

- 操作权限：包括Create、Delete、Read、Write和Admin权限。

ACL是基于会话的，当推出客户端重新创建会话时，需要使用addauth重新为当前会话增加授权。

### 会话

ZooKeeper与客户端建立的通信连接被称为会话。

通过心跳机制维持会话的活性。

会话过期后Watch和ACL权限需要重新绑定。

ZooKeeper也提供了类似Chubby的宽限期的等待时间，SessionTimeout以解决网络延迟或系统GC等。

对于绘画的管理ZooKeeper采用分桶策略。

对于相邻创建的会话，ZooKeeper会根据计算规则为其生成相同的过期时刻，然后以该过期时刻维护一个桶，桶中存储着该时刻过期的所有会话。这样可以批量销毁。

### 读请求处理

为了充分发挥集群的性能，集群中所有成员必须提供自己力所能及的服务。例如Follower向客户端提供读服务。

同时，因为每个客户端都会执行写操作，而客户端与ZooKeeper建立的会话对象不一定是Leader，该成员或许不能处理事务请求。这是需要转发给Leader，再将Leader处理结果返回给客户端。

## ZAB设计

ZooKeeper的安全性主要依赖于ZAB，他保证集群数据的一致性和提案的全局顺序性。

### ZooKeeper背景分析

作为一个分布式协调组件，其可用性关系到所有依赖他的系统，同时也不能保证计算机永远按照期望的方向运行下去，因此，ZooKeeper本质上就是一个分布式系统，它的首要任务就是考虑如何在多副本的情况下提供服务。

#### 1. 多副本

多副本是提高可用性的有效方法，对于处理客户端的请求，通常有两种处理方式：

- 所有副本都可以处理客户端请求。

- 主成员处理客户端请求，其他成员作为备份。

第一种类比去中心化，对等服务。

第二种过度依赖主成员。

ZooKeeper权衡两种模式，选择了一个这种的方式。ZooKeeper仍然会选举出主成员，对于事务请求，只有主成员处理，而非事务请求，所有副本都可以处理。

这样既可以降低主成员的压力，又可以提高非事务请求的吞吐量。

#### 2. 集群角色

在解决单点故障的方案中最有效的就是Master和Slave模式。

ZooKeeper并未沿用Master和Slave模式，而是衍生出了三类成员角色，即Leader、Follower、Observer，并且通过专门定制的算法让他们各司其职。这个算法就是ZAB

Leader负责提供读和写服务，Folloer与Observer只提供读服务。

#### 3. 副本数据一致

按照状态机原理，只需要要求每个成员在一致的状态下输入一致，就能保证数据一致。这样，只要每个成员收到的事务请求和执行顺序一致，就可以。

事务请求只能由Leader来处理，由Leader广播给所有Follower。同时为了避免一部分成功一部分失败，广播应该是一个二阶段提交协议，第一阶段使所有Follower都收到一样的事务请求，第二阶段执行。

最后还要允许少数成员出现故障，这里可以考虑Paxos的多数派思想。

#### 4. 崩溃恢复

由于整个集群过于依赖Leader。所以无论是Leader自身出现故障，还是丢失多数派的Follower，都需要重新选举Leader。新Leader晋升之后，需要取得属于自己的多数派Follower，最后新Leader可能数据少于上一任Leader，因此需要进行一轮数据对齐。

### 为什么ZooKeeper不直接使用Paxos

#### 1. Paxos的局限

活锁、RPC交互次数多、实现难度大，这些问题虽然在Multi Paxos中改善，但并不完美，某些情况下，协商效率并不能达到期望的效果

活锁：Multi Paxos只能减少活锁所出现的概率，并不能避免

RPC次数多：只减少了Prepare阶段的交互次数，且有两个Leader的时候，执行Prepare阶段的概率依然很高。

实现难度大：兼顾性能与安全比较难，出现了各种Paxos变种，如Fast Paxos、EPaxos等，在工程实践中需要考虑的问题很多，因此多数框架中Paxos并不是最佳选择

#### 2. 提案顺序

Paxos着重于，各个成员之间就一系列提案按照相同的顺序达成一致，而不关心达成提案之间的依赖性。

#### 3. Paxos还缺少一些必要的实现

着重于如何达成共识，对于达成共识后的数据如何管理没有明确的方向。如：

- 日志对齐

- 成员变更

- 数据快照

- 崩溃恢复

### ZAB简介

专门为ZooKeeper设计的崩溃可恢复的原子广播算法。包含Leader选举，成员发现，数据同步和消息广播

ZAB注重于提案的顺序性。如果一个事务被处理了，那么所有依赖它的事务都应该被提前处理。

几个术语：

- 提案（Proposal）：进行协商的最小单元，常被称作操作（Operation）、指令（Command）

- 事务（Transaction）：指提案，常出现在代码中，并非指具有ACID特性的一组操作。

- 已提出的提案：广播的第一阶段所提出的但未提交到状态机的提案。在集群中，可能有多数派成员已拥有该提案，也可能仅Leader拥有该提案。

- 已提交的提案：指广播的第二阶段已提交到状态机的提案。在集群中，可能有多数派成员已提交该提案，也可能仅Leader提交了该提案。

#### 1. 集群角色

Leader、Follower、Observer

- Leader：领导者，整个ZAB核心
  
  - 事务操作的唯一处理者，将每个事务请求封装成提案广播给每个跟随者，根据跟随着的执行结果控制提案的提交。
  
  - 维护和调度ZooKeeper内部各个成员。

- Follower：跟随者。Follower类似于Paxos中的Acceptor
  
  - 接受并处理非事务请求：读请求。收到写请求转发给Leader
  
  - 参与提案的决策，对Leader提出的提案进行投票。
  
  - 参与Leader选举

- Observer：观察者。不参与提案的决策。不参与Leader选举。
  
  - 可以处理非事务请求
  
  - 在跨地域场景中，增加Observer，可以降低所在地域的读请求网络延迟。
  
  - 在读性能不佳的情况下，增加Observer可以在不影响集群写性能的情况下提升读性能。

#### 2. 成员状态

四个状态：

- ELECTION：已丢失与Leader的连接。可能是没有Leader、Leader出现故障或者由于网络原因感知不到Leader，该状态下，当前成员会发起选举。

- FOLLOWING：跟随者状态

- OBSERVER：OBSERVER状态

- LEADING：当前成员是Leader，并于多数派Follower保持稳定连接。

#### 3. 运行阶段

ZAB的另一个重要目标是在居群崩溃时能自动恢复至正常状态，因此整个过程可以化为四个阶段：Leader选举、成员发现、数据同步和消息广播。

前三个被称为崩溃恢复阶段，最后一个被称为消息广播阶段。

消息广播阶段类似于二阶段提交，针对客户端事务请求，Leader将其生成对应的Proposal，并发给所有的Follower。Leader收集多数派Follower选票后决定是否提交。

与二阶段的区别是，只需要多数派支持即可进入二阶段提交；消息广播移除了第二阶段的中断逻辑，所有Follower要么批准每一个Proposal要么抛弃Leader服务器。

### 事务标识符

为了从集群崩溃状态中快速恢复，ZAB使用事务表示模式，重度依赖事务标识符选举Leader，以及数据对齐。

使用zxid命名

zxid是一组成对的值，由一个64位字符组成，低32位可以看成一个计数器，高32位代表当前Leader所处的周期epoch。每进行一次Leader选举周期自增1，且低32位重置为0.

### 多数派机制

与Paxos一致。从资源节约的角度，没必要部署偶数个成员。

### Leader周期

针对网络延迟产生的脑裂问题，引入Leader周期epoch，每一轮Leader选举都会增加Leader周期。当Leader选举时，要求Leader周期高的不能给低的投票。

所处周期高的Follower不处理来自低周期的Leader的事务操作。

## ZAB描述

四个阶段环环相扣，相互依赖

### Leader选举阶段

包含两个阶段，第一阶段选出一个准Leader，第二阶段是准Leader在新的epoch上建立明确的领导地位。第二阶段包含成员发现阶段和数据同步阶段。

在ZooKeeper中使用提案比较的方式来选举准Leader

### 成员发现阶段

虽然状态变成了LEADING、FOLLOWING、OBSERVING，但不足以确认领导地位。

成员发现的目的除了让follower与leader之间建立领导关系，另一个目的是使得旧的Leader不能继续提交新的事务，这需要双方交换epoch来达成。

### 数据同步阶段

由Leader发起，确定完初始提案集Ie的前提是都到多数派Q的ACK-E(fa,hf)消息，这意味已经建立明确的领导关系，之后需要数据对齐

1. Leader发送NEWLEADER(e', Ie')消息给Q中的所欲Follower

2. Follower收到后如果fp!=e'，说明Follower已经接受了更高的epoch，需要重新选举，不然
   
   - 更新fa <--- e'
   
   - 遍历自己的历史提案，如果每个提案<zxid, val>属于Ie',则意味着接受该初始提案集Ie'，更新hf。

3. 在Leader收到多数派的ACK-LD消息后，Leader接着发送COMMIT-LD消息给这些Follower，Leader结束数据同步阶段

4. Follower收到来自Leader的COMMIT-LD消息时，Follower将按照Ie'的顺序提交Ie'中的所有提案，并结束数据同步阶段

### 消息广播阶段

完成数据同步阶段后，ZAB集群才会对外提供服务。

消息广播的核心思想来自于两阶段提交。

1. Leader按照zxid递增顺序，通过PROPOSE(e', <zxid, val>)向多数派Q中的Follower发起提案。

2. Follower在解说来自Leader的提案后，将按照顺序追加到hf中，即提案已持久化，才会回复ACK(e', <zxid, val>)消息给Leader。

3. Leader收到多数派的ACK后，接着发送COMMIT(e', <zxid, val>)消息。

4. 一旦Follower收到COMMIT就提交提案 <zxid, val>。在这个过程中会顺带提交zxid之前的所有提案。

消息广播阶段是允许其他成员处于非消息广播阶段的。

在工程实现中，没有必要让PROPOSE、ACK、COMMIT消息都携带val值的，PROPOSE将val复制到多数派Follower后，ACK和COMMIT仅携带zxid即可。

### 算法小结

略

## ZooKeeper中的ZAB实现

在一些必要的地方进行了优化，具体实现也与ZAB论文中介绍的稍有不同，如：

- 成员状态：ZooKeeper中将其定义为LOOKING, LEADING, FOLLOWING, OBSERVING

- 消息名称：
  
  - CEPOCH --> FOLLOWERINFO
  
  - NEWEPOCH --> LEADERINFO
  
  - ACK-E --> ACKEPOCH
  
  - NEWLEADER --> NEWLEADER
  
  - ACK-LD --> ACKNEWLEADER
  
  - COMMIT-LD --> UPTODATE

- 消息内容：鉴于Follower的历史提案可以无限多，在RPC消息中传输整个提案数据并不现实，有必要通过其他方式来减少数据传输，如：
  
  - ACK-E(fa, hf)消息：Follower无需将整个历史提案列表发送给Leader，因为Leader已经有完整的数据了，所以不必挑选，直接使用自己的就可以。
  
  - 因为zxid严格按照顺序自增，所以针对ACK-E消息，Follower仅回复自己所见的最大的zxid即可。

- 数据同步阶段：ZooKeeper并不需要通过NEWLEADER消息传输过去的所有提案数据，这是非常占用资源且无用的。
  
  - ZooKeeper根据Follower缺失的提案数据分别使用DIFF、SNAP和TUNC这三种方式对齐历史数据，不再依靠NEWLEADER消息来发送历史提案数据。
  
  - NEWLEADER消息在ZooKeeper中的重要作用是将成员配置同步到Follower。

- 消息广播阶段：ACK(e', <zxid, val>)和COMMIT(e', <zxid, val>)可以省略其中的提案值val，以减少消息的大小。

### 选举阶段

ZAB是一个强领导者模型的算法，Leader的选举关乎整个集群的可用性，希望以最快的方式选举出Leader，以减少对客户端的影响。本质上，就是选举出一个拥有最完整数据的成员。

ZooKeeper为Leader选举提供了三种算法，在配置文件zoo.cfg中使用electionAlg属性来指定，并分别通过数字1-3进行配置，

- 1：LeaderElection算法

- 2：AuthFastLeaderElection算法

- 3：FastLeaderElection算法

默认值是3，3.4版本以后另外两种算法已被废弃，主要介绍FastLeaderElection

#### 1. 逻辑时钟

logicalclock，是一个自增的Long类型的数字，每进行一轮选举之前logicalclock都自增1，成员收到选票后，只处理比自己大的选票。

在Leader选举过程中，有两个名称比较相似，但含义不同的两个变量：

- electionEpoch：
  
  选举周期。每进行一轮选举后，logincalclock都会自增，当生成选票时，electionEpoch字段取值于logicalclock，并依赖于logicalclock的自增随时更新。用于检验选票的有效性。

- peerEpoch：
  
  成员周期，指所支持的成员所处的周期，即本次选举前Leader所处的周期，用于选票比较。

#### 2. 发起投票

成员感知不到Leader的时候，将状态更新为LOOKING状态，ZAB状态更新为ELECTION, 并发起选举。在开启一轮选举之前，每个成员会对自己维护的logicalclock进行自增，logicalclock表示选举的轮次，即ElectionEpoch（选举周期）。

在第一轮投票中，任何成员都会投票给自己，并将该选票发送给其他的成员（包含自己）。一张选票表示为<logicalclock, state, selfId, selfZxid, voteId, voteZxid>，其中：

- logicalclock：投票者所处的选举轮次

- state：投票者的状态

- selfId：投票者ID，即配置文件中myid值

- selfZxid：投票者所保存的最大Zxid

- voteId：被投票者的ID

- voteZxid：被投票者所保存的最大Zxid。

#### 3. 处理选票

任意成员收到其他成员发来的选票时，将依次完成以下5项工作：

1. 检查logicalclock，选票接收者只会处理大于或等于自己logicalclock的选票
   
   - 对于小于自己logicalclock的选票，选票接收者会拒绝该选票。
   
   - 对于大于自己的，清空自己的选票池，然后更新logicalclock，然后完成后续工作
   
   - 对于等于自己的，属于正常选票

2. 选票比较，按照规则更新自己的选票

3. 广播选票
   
   - 对于大于自己的，无论是否更新自己的选票，最后都会重新广播自己的选票
   
   - 对于等于自己的，只有自己的选票更新了，才会重新广播自己的选票。

4. 记录选票，将收到的选票记录到本地维护的选票池中。

5. 更新状态，如果自己选票支持的成员在选票池中存在多数派的支持者，则意味着本次选举结束，新Leader诞生，从而更新自己的状态
   
   - 如果Leader是自己，状态更新为LEADER，并更新自己的追随成员列表
   
   - 如果Leader不是自己，则更新为FOLLOWING或OBSERVING。

#### 4. 选票比较

用自己的选票和收到的选票进行比较，选出最合适的选票作为自己的选票，最新最完整的数据

- 任期编号：大的epoch获胜。

- 事务标识符：比较成员收到的最大zxid，大的获胜。

- myid，myid大的获胜

### 成员发现阶段

Leader选举完成后，无论成员是LEADING还是FOLLOWING，ZAB都会将状态更新为DISCOVERY

这个阶段是为了每个Follower与Leader之间建立明确的关系，交换各自的epoch并建立新的epoch，使旧Leader不能继续提交新的事务，并使Leader能收集每个Follower收到的最后一个zxid。这两项工作主要由三个RPC消息来完成。

- FOLLOWERINFO消息是由Follower向Leader发送的，用于向Leader汇报自己所接受的acceptEpoch。

- LEADERINFO消息是由Leader向Follower发送的，用于向Follower发送新一轮Leader周期，即newEpoch。

- ACKEPOCH消息由Follower向Leader发送，向Leader汇报自己收到的最后一个zxid及currentEpoch。

### 数据同步阶段

使用ACKEPOCH中的zxidf对比自己的最大最小日志项，为每一个成员选择同步方案

1. 准备同步数据：数据同步方案分为三种：TRUNC， DIFF和SNAP，多的用TRUNC删除，在min和max之间的用DIFF同步，小于min的说明，落后的数据太多使用SNAP同步。

2. 同步数据：也是Leader主动发起，开启一个新线程将为每一个成员准备的数据按照zxid顺序发送给Follower，以DIFF或TUNC开启同步，之后将所需同步的提案以PROPOSAL和COMMIT消息通知Follower处理相关提案。SNAP稍有不同不会将所需同步的数据放入队列，而是发送SNAP开启数据同步后，立即发送快照文件。

3. 发送NEWLEADER消息：主要作用是将成员配置发送给Follower，携带有newEpoch及成员配置信息。数据同步阶段收到NEWLEADER消息，则会按照约定一次完成以下工作：
   
   - 更新成员配置
   
   - 更新acceptedEpoch
   
   - 将PROPOSAL消息中的日志项批量持久化
   
   - 回复ACK消息

4. 发送UPTODATE消息
   
   等待多数派回复ACK消息后，发送UPTODATE给Follower，然后通知Follower可以准备提交的提案并推出数据同步阶段。

**此时Follower还会重新持久化一次PROPOSAL消息队列中剩余的日志项，这是由于ZAB版本问题，Leader可能在发现阶段发送了NEWLEADER消息，而数据同步阶段，Follower没有收到该消息，因此在这里统一补偿。**

### 消息广播阶段

Follower完成上述工作后，将推出数据同步阶段，更新ZAB为BROADCAST。而Leader本身也作为Follower，在等待多数派Follower回复的NEWLEADER的ACK消息后，更新ZAB为BROADCAST。

在消息广播阶段, ZooKeeper处理客户端请求采用的是责任链模式，一个客户端请求的处理由多个不同的处理器协作完成，每个处理器仅负责单一的工作。


