# lrpc

## 项目亮点

### 1. 提供一个Promise, Future异步处理框架

 支持了future的then链式回调，支持超时回调，when-all、when-any功能。

在 C++ 中实现promise和future的关键组件是  

- Result 和 ResultWrapper 类，用于表示异步操作的结果，可以是值或异常。  

- State 类，用于保存未来的结果值、异常和其他状态。  

- Promise 类，用于设置 future 的结果。  

- Future 类，表示未来值。它可以等待以获得结果，也可以通过 then() 连接回调，以便在结果就绪时调用。  

- 实用函数 makeReadyFuture、makeExceptionFuture 可创建就绪的未来。  
  whenAll、whenAny、whenN 等函数用于组合多个期货。  

基本工作原理  

- Promise 和 Future 耦合在一起--Promise 设置结果，Future 返回结果。  

- Future 的状态由 Promise 设置。为了线程安全，它受互斥保护。  

- 在 Future 上等待会检查状态--如果还没准备好，则在条件变量上等待。  
  使用 then() 设置回调时，会在 Future 的状态中设置回调函数。当 Promise 设置结果时，它会调用回调函数。  

- 通过 then() 链接的回调会创建一系列Future，每个回调都会设置下一个future的结果。  
  像 whenAll 这样的实用功能通过设置共享上下文、在每个future上注册回调来设置上下文中的结果，并创建一个新的future来返回汇总结果，从而将多个future结合在一起。  

因此，总的来说，promise和future可以通过 callacks 实现异步任务链，同时将线程、互斥、条件变量等底层并发处理隐藏在简单的 future/promise API 之后。

#### 它是如何支持future的链式回调的

通过创建一个future序列来实现的。

当在一个future对象上调用then的时候，会创建一个新的future，并且在当前future完成时执行then指定的回调函数，并使用回调函数的结果来设置下一个future的值。

具体：

1. 当调用future.then(f)时候，会创建一个新的Promise和Future对象nextFuture

2. 在当前future的状态State中，注册一个回调函数，该回调函数会在当前future完成时被执行。

3. 回调函数中会调用f来生成下一个future的值，并设置到nextFuture中

4. then方法返回这个nextFuture

5. 如果f的返回值也是一个future，会递归的进行解包，直到返回一个非future型别

6. 对nextFuture的操作会延续执行then链，每次都创建新的future，形成一个future序列

7. 通过这种方式，then就可以链式调用，每个then对应一个future，前一个的完成会触发下一个的计算

8. 最后等待或获取序列中最后一个future的结果，就能异步的执行整个then调用链式操作。

这样通过痛恨每次创建新的future对象，并将回调函数注册到前一个future中，实现了then的链式调用以及异步执行的效果。

#### Result

##### 移动构造函数/拷贝构造函数 中使用了placement new

拷贝不适用move

```cpp
Result(Result<T> &&r) : state_(r.state_) {
    if (state_ == State::Value)
      new (&value_) T(std::move(r.value_));
    else if (state_ == State::Exception)
      new (&exception_) std::exception_ptr(std::move(r.exception_));
  }
```

1. placement new可以宠用栈空间，不需要heap分配，更高效

2. 可以复用对象存储空间，避免重复构造与析构的开销

3. 使用移动构造移动exception_ptr避免深拷贝

4. 与移动语义的配合实现对象复制到已有内存的效果

5. 使用std::exception_ptr管理异常，

##### exception_ptr

在C++11引入，提供了一个存储和传递异常的统一方式：

1. 通过exception_ptr可以在线程、任务或其它形式的执行流之间传递异常

2. 异常的重新抛出：通过exception_ptr持有一个异常，然后在后续的时机重新抛出

3. 异常的类型消抹：可以隐藏实际的异常类型

主要操作：

- make_exception_ptr:捕获或创建

- current_exception:获取当前抛出的异常

- rethrow_exception:通过exception_ptr重新抛出异常

解决了C++异常机制，不支持异步的问题，提供了传递和存储异常的统一方式。

在项目中的应用：

1. 在Promise中传递异步任务抛出的异常

2. 在Future中存储异步任务的异常

3. 在获取Future结果时重新抛出异常。

##### 重载 移动/拷贝 赋值运算符

拷贝不适用move

关键点：

1. 首先进行自我赋值检查，避免以我移动

2. 调用析构函数对自身进行析构，这是必须的，否则原有资源不会被释放。

3. 移动赋值状态state_

4. 如果状态是值：
   
   1. placement new，然后用右值的r.value_经过move转换成左值引用进行赋值

5. 如果是异常：
   
   1. placement new，同样使用move进行赋值

6. 最后返回自身引用

这里用到了返回值优化RVO（return *this），返回局部变量和自身引用是返回值优化的常见方式

##### 提供三种隐式类型转换

常量左值引用，左值引用，右值引用三种隐式转换，直接调用getValue，右值引用调用move(getValue)

##### UninitializedResult空结构体的作用

在Result类中,UninitializedResult是一个空的结构体,它被设计为一个特殊的标记类型,用来表示Result未初始化的状态。

其在Result中的用途是在 check() 函数中使用:

```cpp
struct UninitializedResult {};
  void check() const {
    if (state_ == State::Exception)
      std::rethrow_exception(exception_);
    else if (state_ == State::None)
      throw UninitializedResult();
  }
```

 如果调用了 check() 但Result仍未包含值或异常,那么它会抛出这个UninitializedResult对象。

这比直接抛出字符串Exception更符合类型安全的设计。

UninitializedResult 作为一个空标记类型具有以下用途:

1. 把未初始化的状态编码为一种特殊的错误类型
2. 可以被外部捕获并特殊处理
3. 避免使用字符串解释未初始化状态,类型安全
4. 更易于调试,从调用栈可以明确是一个 Result 未初始化导致的错误
5. 作为一种自定义异常类型,可以包含额外的上下文信息

所以 UninitializedResult 是一个用于表示 Result 未初始化状态的标记类型。

它让 Result 可以通过异常的方式来类型安全地传达未初始化的信息。

##### get接口的设计

在Result类中,get()函数的作用是进行结果的提取,其模板参数R定义了如何提取结果。

get()会统一调用getValue()获取结果,然后用std::forward进行完美转发。

get()有两个主要作用:

1. 进行结果提取时 specifying R可以避免拷贝

例如:

```cpp
Result<std::string> result = ...;

// 避免字符串拷贝 
std::string&& s = result.get<std::string&&>();
```

2. 进行类型转换

可以通过指定R为目标类型,将Result中的值转换为另一种类型:

```cpp
Result<int> result = 10;

double d = result.get<double>(); // int -> double
```

所以get()提供了一个可配置的结果提取接口。

程序员可以灵活控制结果的返回方式,减少拷贝,进行类型转换等。

这比单一的getValue()获取左值引用更加灵活。

同时保留了Result对结果的封装,避免了直接暴露值对象。

#### 针对void的特化Result类

1. 避免访问非法值
   
   对于void类型，只包含了exception的接口，避免了对value的非法访问1

2. 保持了接口一致性

3. 将void的特殊细节隔离在特化类中

#### ResultWrapper元函数

1. 对非Result类型进行包装，包装成一个Result<T>

2. 对已经是Result<T>的不进行包装，直接返回

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
