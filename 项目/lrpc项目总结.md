# lrpc

## 项目亮点

### 1. 提供一个Promise, Future异步处理框架

 支持了future的then链式回调，支持超时回调，when-all、when-any功能。

### 2. 网络库

基于多reactor思想实现的，封装了poll和epoll（与muduo一样使用的是epoll）实现了事件的分发器，可以异步的处理数据的读写、连接创建与关闭

### 3. RPC框架

首先是使用TCP长连接的方式，并使用protobuf作为序列化反序列化工具、反射工具。提供redis作为name service。也支持自定义的name server及其消息编解码器。

## 项目难点

### 1. Promise/Future

### 2. 网络库

### 3. rpc

## 项目扩展

### 1.

### 2.

### 3.
