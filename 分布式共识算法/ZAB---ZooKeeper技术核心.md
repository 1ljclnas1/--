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
