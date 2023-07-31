# Raft-----共识算法的宠儿

## Raft简介

### Raft诞生背景

Paxos难以理解，实现困难，算法描述和实现之间存在巨大鸿沟，最终的系统往往建立在未被验证的算法之上。

### 可理解性

它必须保证被大多数人理解，容错和高效，做出的努力包括问题分解、消除不确定性及详细的实现细节：

- 问题分解：分解为多个独立的且易于理解的子问题。Leader选举、日志复制、安全性及成员变更。

- 消除不确定性：尽可能减少在异常情况下遇到的问题。

- 详细的实现细节：避免工程师对实现方式产生歧义，如对超时的引入。

### 基本概念

涉及的关键术语分三个方面，成员状态、运行阶段和RPC消息。

1. 多数派
   
   同Paxos。

2. 任期
   
   Raft将时间分割为不同长度的任期，记作term。类比epoch，term使用连续整数进行单数递增，并伴随着RPC消息在成员之间交换。Raft使用term作为Leader的任期号并遵循以下规则：
   
   - 每个Raft成员在本地维护自己的term
   
   - 在每轮Leader选举之前递增自己的term
   
   - 每个RPC消息都将携带自己本地的term
   
   - 每个Raft成员拒绝处理小于自己本地的term的消息。

    可以让Follower发现一些过期的消息和过时的Leader，如果Candidate或Leader发现自己的term小于消息的term，则立即回到Follower状态。

3. 成员状态
   
   Leader、Candidate、Follower
   
   Follower在等待超时后会进入Candidate状态，Leader只会从Candidate中诞生。当某一个晋升之后，其余变为Follower。
   
   为了适应复杂的生产场景，Raft又引入了无投票权成员和Witness成员。前者用于新成员上线时尽快赶上Leader数据；后者用于在不降低集群可用性的基础上降低部署成本。具体如下：
   
   - Follower：只会处理和响应来自Leader、Candidate的请求。如果客户端与Follower通信会被重定向到Leader。类比ZAB。但与ZAB不同的是ZAB在新Leader诞生之后会参考Follower的数据，而Raft的Follower只会以Leader数据为准，被动接收。
   
   - Candidate：如果Leader出现故障或者Follower等待心跳超时，则Follower变更为Candidate。
   
   - Leader：当某一个Candidate获得多数派选票时，晋升为Leader，负责处理接下来的客户端请求、日志复制管理及心跳消息管理。
   
   - 无投票权成员：当新成员上线时，以学习者的身份加入集群，在不影响集群的情况下，快速和Leader完成数据对齐。无投票权成员也可用于在不影响协商效率的基础上提供额外的数据备份。
   
   - Witness成员：一个之投票但不存储数据的成员。
   
   Raft是强Leader模型，日志项只能由Leader复制给其他成员，这意味着日志是单向的，Leader从来不会覆盖本地日志。

4. 运行阶段
   
   Raft强化了Leader地位，整个算法可以清晰地分为两个阶段：
   
   - Leader选举：在集群启动之初或Leader出现故障时，需要选出一个新的Leader，接替上一任Leader的工作
   
   - 日志复制：Leader可以接收客户端请求并正常处理，由Leader向Follower同步日志项。

5. RPC消息
   
   分为四种，其中前两种是Raft基础功能的实现，后两种是为了优化引入的
   
   - RequestVote：请求投票消息，用于选举Leader
   
   - AppendEntries：追加条目消息，用于以下场景：
     
     - Leader用该消息向Follower执行日志复制
     
     - Leader与Follower之间心跳检测
     
     - Leader与Follower之间日志对齐
   
   - InstallSnapshot：快照传输消息
   
   - TimeoutNow：快速超时消息，用于Leader切换，可立即发起Leader选举。

## Raft算法描述

### Leader选举

日志项只能由Leader流向Follower。

### 日志复制
