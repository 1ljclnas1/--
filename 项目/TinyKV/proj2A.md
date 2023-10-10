# Raft算法

## 领导者选举

从raft.Raft.tick()（模拟逻辑时钟）开始，它驱动选举超时和心跳超时。将消息放入raft.Raft.msgs中即可，不用进行手动发送。它在raft.Raft.Step()中会进行发送。在raft.Raft.Step()中处理各种消息（这里就是raft的状态机）。对于领导者选举这个功能来说，需要处理requestVote，heartbeat及对应的response消息（4个）

那么RequestVote的消息是follower或者candidate发出的，当它们收到MsgHup消息后会进行选举

### RequestVote的处理

都会收到

如果自己的任期号小，或者任期相等的情况下，自己没有投过票，或者说投的票就是这个发起Vote的机器，并且它的日志足够新，那么就可以投票给他，并且自己变为follower。

否则的话拒绝投票，

然后将消息append到自己的msgs中

### RequestVoteResponse的处理

由于发出RequestVote后会马上变为candidate，所以只有Candidate需要处理对应的response

如果没被拒绝就更新节点的投票信息。

如果被拒绝：

1. 如果自己任期号小，马上变为follower

2. 如果半数以上的节点拒绝了投票，马上变为follower，这样后续的消息就不会被处理了，提高了性能。

如果累积的同意的人数超过了法定人数，马上变为leader

## 日志复制

在TinyKV中将commitIndex也作为了持久化的日志

实现一些帮助函数：newLog，unstableEntries，nextEnts等

newLog，在newRaft的时候从持久化池中读取数据

unstableEntries，返回所有未stable的log

nextEnts：返回所有已经commit但是没有apply的log

Term: 返回对应index的term，如果index大于dummyIndex说明一定在内存中直接返回，如果正在安装snap日志就看看是不是快照的最后一条，是的话返回不是的话，那它只能是snap中的日志，调用storge.Term(index)

TermNoErr: 不返回错误

# 实现raw Node接口


