# Paxos-分布式共识算法

目前公认的解决分布式公式问题最有效的算法之一。

Paxos解决了在分布式系统中如何使某一个值（提案指令）在集群中达成共识的问题。简化了分布式环境的复杂性，是一组成员可以虚拟为一个成员向客户端提供服务。Paxos的另一个使命就是在不可靠的环境中依旧保持算法的正常运行，这称为容错。

## Paxos的诞生

二阶段、三阶段是强一致性算法，而大多数场景不需要100%强一致性算法，相反，我们期望当一部分不能正常工作时，整个集群依旧可以工作量高，并保证数据的正确性。例如，在实现多副本的同步中，我们希望能允许部分副本同步失败，而不影响其他副本进行同步。

二阶段和三阶段的另一个问题就是太过于依赖协调者，这将导致性能瓶颈依赖于但成员，以及网络分区导致的脑裂问题。这些都是在分布式环境中难以解决的痛点。

lamport在1998年以Paxos小岛故事为背景，讲述了Paxos算法，但是这种方式并不为人所接受，2001年Lamport以Paxos Made Simple为题，重新发表论文，用通俗易懂的文字从科研角度触发对Paxos算法进行了严谨的解释，之后Paxos算法的追随者变得越来越多。

## 初探Paxos

先了解整个Paxos的运行过程，在了解推导过程。

### 基本概念

- 容错：在系统运行的过程中对故障的容忍程度。在分布式环境中，经常会出现成员故障、网络分区和网络丢包等问题，分布式容错是指在允许发生这些问题的情况下，系统依然可以正常工作。

- 共识：在对等的成员集合中，让每个成员都认可某一个值，其与一致稍有差别。一致要求每个成员的数据完全相同；而共识并不要求数据完全相同，只要求客户端从人以一名成员处获取的值是相同的即可。

例如·，由A,B两个客户端，都对共识系统中的X进行赋值。A需要设置X=1，B需要设置X=2，那么最终让A和B从共识系统中都获得同一个值（X=1或X=2）的结果就是达成共识。

- 多数派：多数派思想串联了整个协商过程。多数派是指一个集群中超过一半以上的成员组成的集合，即成员个数大于$\lfloor N/2 \rfloor + 1$ （N为成员总数）的集合。

多数派的设定可以保证安全的情况下有效的提高容错性。

- Instance：将其看作一个个单调递增的存储列表更容易理解，它用于存储达成共识后的提案。随着Paxos运行时间的增加，这个列表会变得无限长。在协商发生冲突的时候可能不会由任何提案达成共识，因此这个列表是允许存在空洞的。通常会在空洞的Instance上再运行一轮Paxos，使用默认值Noop来填充。

在Lamport论文中提到，在一个Instance上选择一个提案需要进行多轮RPC消息交互，这种方式过于保守和低效。为了解决这个问题，Lamport提出了Multi Paxos，即在一轮协商过程中多个Instance选择多个提案的优化方案。为了区分优化前和优化后的算法，前者称为Basic Paxos后者称为Multi Paxos。

在论文中Lamport仅给出了Multi Paxos关键部分的实现，对于细节方面，却留下了无限的想象空间。由此衍生出大量的过度解读，出现了不同版本的Multi Paxos实现，但是他们的整体思路都是围绕如何优化消息交互次数和Leader选举展开的。

- 提案编号：指一个单调递增的整数，它标识着一轮协商，提案协商之前需要生成一个全局唯一的提案编号。在Basix Paxos中，一轮协商只会存在一个Instance上达成共识，因此在没有冲突的情况下提案编号和Instance是同步递增的；而在Multi Paxos中，一个提案编号可能应用在多个Instance上。

另外，Paxos中还有很多意思相近的词：

- 提案：由提案编号和提案指令组成的实体。提案编号标识该提案所处的协商轮次，提案指令是指需要达成共识的内容，它可以是一个值或一个指令等。有些地方也习惯吧提案指令称为决议。

- 通过、批准、选择：他们都表示Acceptor同意某一请求或提案。

- 角色：Proposer（提案者）、Acceptor（接收者）、Learner（学习者）。

- 阶段：Prepare阶段、Accept阶段和Learn阶段

### 角色

#### 1. Proposer

整个算法的发起者，他驱动协商的进程，相当于会议的主持人，向所有参会人员公布提案，发起投票并统计投票。

再Paxos宣发中Proposer的主要工作是驱动算法的运转。Proposer在收到客户端的请求后将其封装为一个提案（Proposal），并将该提案发送给所有接受者，根据接受者的相应情况，驱动算法决定是否需要继续往下运行。

#### 2. Acceptor

提案的真正决策者，相当于会议的参会人员，当参会人员收到会议主持人的提案后，需要向主持人表决自己是否支持该提案。

在Paxos算法中，Acceptor的主要工作是对提案进行抉择，它在收到Proposer发来的ti'an后，根据预先约定的规则对提案进行投票，向Proposer反馈自己的投票结果。

#### 3. Learner

Learner不参与提案的发送和决策，只是被动的接受提案选定的结果。当一个提议被选择后，会议将达成的提议公之于众，这意味着该提议不会再更改和变化。

Learner不参与算法的决策过程，英雌它们不是Paxos的重要组成部分，他们可以全部失败或断开连接。之所以说他们可以全部失败，是因为我们随时可以重放已达成共识的提案，从而构建与失败前一摸一样的Learner。

在Paxos中，Learner仅用于实现状态机，执行状态转移操作，这可以理解为所服务的业务实现。例如，在实现一个分布式数据库时，每一条DML语句都是一个提案，每个提案达成共识后，交由Learner执行对应的更新操作，并记录对应的业务数据。

而在工程实现中，Learner承担的更多，它可以实现扩展读性能和同步落后数据。

- 扩展读性能：当Proposer和Acceptor处理客户端的读请求达到瓶颈时，可以扩展Learner。因为他不参与协商过程，增加Learner的数量也不会影响协商效率。另一种情况是，当客户端需要跨地域访问Paxos集群时，可以在客户端所在地域增加Learner，客户端直接访问当前地狱的Learner可以降低读请求的网络延迟。

- 同步落后数据：Paxos允许少数成员数据落后，当集群中的多数派成员的数据处于落后状态时，需要先同步落后数据才能协商新的提案。当新成员上线时，也需要先扮演学习者学习过去已被选择的提案。

### 阶段

#### 1. Prepare阶段

协商过程的开始阶段，当Proposer收到客户端的写请求时，Proposer会为此生成全局唯一的递增的提案编号M，并向所有Acceptor发送包含提案编号的Prepare请求，记作[M, ]。当Acceptor收到提案[M, ]的Prepare请求后，会根据约定的规则决定是否需要响应Prepare的请求。

- 如果M大于Acceptor已经通过的Prepare请求中的最大提案编号，则通过本次Prepare请求，并承诺在当前Prepare请求的响应中，反馈已经批准的Accept请求中最大编号的提案指令；如果没有批准任何Accept请求，则在Prepare请求的响应中反馈Nil。

- 如果M小于等于Acceptor已通过的Prepare请求中最大的提案编号，则拒绝本次Prepare请求，不响应Proposer。

**注意：响应Nil和不响应并非相同的动作。如果不响应，Proposer可以认为该Acceptor拒绝了Prepare请求，或者该Acceptor发生了故障；如果响应Nil，则意味着该Acceptor通过了Prepare请求，并且没有批准任何一个提案**

根据Acceptor处理Prepare请求的规则，如果Acceptor通过了Prepare[M, ]请求。则向Proposer做出以下承诺：

- 不再通过编号小于等于M的提案的Prepare的请求。

- 不再批准编号小于M的提案的Accept请求。

- 如果Acceptor已经批准的提案编号小于等于M的Accept请求，则承诺在提案编号为M的Prepare请求的响应中，反馈已经批准的Accept请求中最大编号的提案指令。如果没有批准任何Accept请求，则在Prepare请求的响应中反馈Nil。

**注意：在Prepare请求中，Proposer只会发送提案编号，也就是[M, ], 提案指令需要根据Acceptor的响应才能确定**

#### 2. Accept阶段

在Proposer收到多数派的Acceptor的响应后，由Proposer向Acceptor发送Accept请求，此时的Accept请求包含提案编号和提案指令，记作提案[M, V]。Acceptor收到提案[M, V]的Accept请求后，会根据以下的约定对Proposer进行反馈。

- 如果Acceptor没有通过编号大于M的Prepare请求, 则批准该Accept请求，即批准提案[M, V]，并返回已通过的最大编号也就是[M, ]

- 如果Acceptor已经通过编号为N的Prepare请求，且N>M, 则拒绝该Accept请求，并返回已通过的最大编号即[M, ]

当前拒绝Accept请求时，Acceptor可以直接忽略Accept请求，不执行响应Proposer的操作，也可以给Proposer反馈自己已通过的最大提案编号，只要让Proposer明确知晓自己的决策，就不会影响Paxos的正确性。

在实践中，无论拒绝或接受Accept请求，Acceptor都会给Proposer反馈自己通过的最大提案编号。因为在下一轮协商中，Proposer可以基于响应中的提案编号进行递增，而不用尝试逐个递增提案编号。而Proposer可以对相应的提案编号和Prepare阶段所使用的提案编号进行比较，如果二者相等，则意味着对应的Acceptor批准了该Accept请求，否则为拒绝。

如果多数派的Acceptor批准了该Accept请求，则记作提案[M, V] 已被选择或者提案已达成共识；如果没有多数派的Acceptor批准该Accept请求，则需要回到Prepare阶段重新进行协商。

**提案中的V值，如果在Prepare请求响应中，部分Acceptor反馈了提案指令，则V为Prepare请求反馈中最大的提案编号对应的提案指令，否则V可以由Proposer任意指定。**

#### 3. Learn阶段

Learn阶段不属于Paxos的协商阶段，它的主要作用时将达成共识的提案交给Learner进行处理，然后执行状态转移操作。如何让Learner知晓已达成共识的提案有以下几种方案：

- 进行Peoposer同步。在协商的过程中，只有提案对应的Proposer才知道提案是否已达成共识和最终达成共识的真正提案。因此在Accept阶段，如果一个提案已达成共识，那么由Proposer立即将该提案发送给Learner是最简单的方案。

- 转发Accept请求给Learner。当Acceptor批准一个Accept请求时，会将其转发给Learner，Learner需要判断是否有多数派的Acceptor给它发送了同样的Accept请求，一决定是否需要执行状态转移，这要求Learner承担一部分属于Paxos的计算能力。

- 在Acceptor之间交换已批准的Accept请求。当Acceptor批准一个Accept请求时，会将其广播给其他Acceptor，那么所有的Acceptor都可以判断提案是否已达成共识，并将达成共识的提案发给Learner。这样做明显又增加了一轮消息交互，但好处是，每个Acceptor都可以为提案记录是否已达成共识的标志，这可以使读请求不必再执行一轮协商。

对于第二种和第三种，当发生提案冲突而导致没有任何提案达成共识时，Learner不会为任何提案执行状态转移操作，本次计算和消息交互就白费了。所以第一种方案仍然是最有效的。但是，通常来说Learner是一个集合，如何高效的让所有Learner都拥有某个已达成共识的提案有三种数据同步的方案：

- 逐一同步
  
  最简单的方式，当触发Learn阶段时，Proposer将逐个向所有的Learner发送需要达成共识的提案。
  
  这种方式虽然简单，但是会增加Proposer的负担，并且同步效率不高，如果Learner数量为N，则需要发送N个消息才能同步。

- 选举主Learner

    将提案发送给其他Learner的工作，由Proposer交给主Learner做。

这种方案解决了Proposer负担过重的问题的同时，引入了新的麻烦：即主Learner的可用性。

为了解决主Learner出现故障的问题，可以选举多个主Learner，同样主Learner集合不能过大，不然会增加Proposer负担。

- 流言传播

    当Learner的集合实在太大的时候，可以考虑留言传播，即Gossip协议。当有数据发生变更时，Gossip通过各个成员之间互相感染来传播数据。Gossip的传播能力是极强的，除非人为阻断传播，否则它会将变更的数据复制到整个集群中，哪怕该集群有成千上万个成员。

## Paxos详解

### Paxos模拟

Paxos的协商过程由4个RPC交互来完成，Prepare、PrepareResp、Accept和AcceptResp。前两者发生在Prepare阶段，后两者发生在Accept阶段。每个RPC消息需要完成的工作如下：

1. 当Proposer收到客户端请求时，首先要为提案生成一个递增的、全局唯一的提案编号N。

2. Proposer向所有Acceptor发送Prepare [N, ]请求。

3. 当Acceptor收到Prepare[N, ]请求后，会和自己所通过的Prepare请求中最大的提案编号MaxNo进行比较。
   
   - 如果N > MaxNo, 则更新MaxNo的值为N，并通过PrepareResp[AcptNo, AcptValue]请求向Proposer反馈自己所批准的最大的提案编号和其对应的提案指令。
   
   - 如果N<= MaxNo， 则忽略本次Prepare请求。

4. 如果Preposer未收到多数派的反馈，则回到Prepare阶段， 递增提案编号，重新协商；如果Proposer收到多数派的反馈，按照以下规则选择提案指令Value，向Acceptor发送Accept[N, Value]请求。
   
   - 如果在反馈中存在一个AcptValue的值不为Nil，则使用所有反馈中提案编号最大的提案指令作为自己的提案指令Value。
   
   - 如果在所有反馈中AcptValue都为Nil，则由Proposer任意指定提案指令为Value。

5. Acceptor收到Accept[N, Value]请求后，回合自己所通过的Prepare请求中最大的提案编号MaxNo进行比较。
   
   - 如果N=MaxNo，则更新自己所批准的提案编号和提案指令，并通过AcceptResp[MaxNo]向Proposer反馈MaxNo。
   
   - 如果N<MaxNo，则忽略本次Accept请求。

6. 如果Proposer未收到多数派的反馈，则回到Prepare阶段，递增提案编号，重新协商；如果Proposer收到多数派的反馈，则说明该提案已被选择，在各个Acceptor之间达成共识。

![](C:\Users\ljc\Documents\GitHub\--\分布式共识算法\图片\paxos模拟.jpg)

### Prepare阶段










