# GMP

## G

go语言中的goroutine的缩写，相当于操作系统中的进程控制块。其中存着goroutine的运行时栈信息，CPU的一些寄存器的值以及执行的函数指令等。sched字段保存了goroutine的上下文。goroutine切换的时候不同于线程有OS来负责这部分数据，而是由一个gobuf结构体来保存

```go
type g struct{
    stack            stack    //描述真实的栈内存，包括上下界

    m                *m       //当前的m
    sched            gobuf    //goroutine切换时，用于保存g的上下文
    param            unsafe.Pointer    //用于传递参数，睡眠时其他goroutine可以设置param，唤醒时，该goroutine可以获取
    atomicstatus     uint32
    stackLock        uint32
    goid             int64    //goroutine的ID
    waitsince        int64    //g被阻塞的大体时间
    lockedm          *m       //G被锁定只在这个m上运行
}
```

gobuf保存了当前的栈指针，计数器，还有g自身，这里记录自身g的指针是为了能快速的访问到goroutine中的信息。gobuf的结构如下：

```go
type gobuf struct{
    sp    uintptr
    pc    uintptr
    g     guintptr
    ctxt  unsafe.Pointer
    ret   sys.Uintreg
    lr    uintptr
    bp    uintptr    //for goEXPEROMENT=framepointer
}
```

## M(machine)

M代表一个操作系统的主线程，对内核级线程的封装，数量对应真实的CPU数。一个M直接关联一个os内核线程，用于执行G。M会优先从关联的P的本地队列中直接获取待执行的G。M保存了M自身使用的栈信息、当前正在M上执行的G信息、与之绑定的P信息。

结构体M中，curg代表结构体M当前绑定的结构体G；g0是带有调度栈的goroutine，普通的goroutinr的栈是在堆上分配的可增长的栈，但是g0的栈是M对应的线程的栈。与调度相关的代码，会先切换到该goroutine的栈中再执行。

```go
type m struct{
    g0             *g            //带有调度栈的goroutine

    gsignal        *g            //处理信号的goroutine
    tls            [6]uintptr    //thread-Local Storage
    mstartfn       func()        
    curg           *g            //当前运行的goroutine
    caughtsig      guintptr      
    p              puintptr      //关联p和执行的go代码
    nextp          puintptr
    id             int32
    mallocing      int32         //状态

    spinning       bool          //m是否out of work
    blocked        bool          //m是否被阻塞
    inwb           bool          //m是否在执行写屏蔽

    printlock      int8
    incgo          bool
    fastrand       uint32
    ncgocall       uint64        //cgo调用的总数
    ncgo           int32         //当前cgo调用的数目
    park           note
    alllink        *m            //用于链接allm
    schedlink      muintptr
    mcache         *mcache       //当前m的内存缓存
    lockedg        *g            //锁定g在当前m上执行，而不会切换到其他m
    createstack    [32]uintptr   //thread创建的栈


}
```

## P(Process)

Processor代表了M所需的上下文环境，代表M运行G所需要的资源。是处理用户级代码逻辑的处理器，可以将其看作一个局部调度器使go代码在一个线程上跑。当P有任务时，就需要创建或者唤醒一个系统线程来执行它队列里的任务，所以P和M使相互绑定的。P可以根据实际情况开启协程去工作，它包含了运行goroutine的资源，如果线程想运行goroutine，必须先获取P，P中还包含了可运行的G队列。

```go
type p struct{
    lock                mutex
    id                  int32
    status              uint32    //状态，可以为pidle/prunning/...
    link                puingptr
    schedtick           uint32    //每调度一次加一
    syscalltick         uint32    //每一次系统调用加一
    sysmontick          sysmontick
    m                   muintptr  //回链到关联的m
    mcache              *mcache
    racectx             uintptr

    goidcache           uint64    //goroutine的ID缓存
    goidcached          uint64

    //可运行的goroutine的队列
    runqhead            uint32
    runqtail            uint32
    runq                [256]guintptr

    runnext             guintptr    //下一个运行的g

    sudogcache           []*sudog
    sudogbuf             [128]*sudog

    palloc               persistentAlloc    //per-P to avoid mutex

    pad                   [sys.CacheLineSize]byte
}
```

# GMP调度流程

- 每个P有个局部队列，局部队列保存待执行的goroutine（流程2），当M绑定的P的局部队列已经满了之后就会把goroutine放到全局队列（流程2-1）

- 每个P和一个M绑定，M使真正的执行P中goroutine的实体（流程3），M从绑定的P中的局部队列获取G来执行

- 当M绑定的P的局部队列为空时，M会从全局队列获取到本地队列来执行G（流程3.1），当从全局队列中没有获取到可执行的G时候，M会从其他P的局部队列中偷取G来执行（流程3.2），这种从其他P偷的方式称为work stealing

- 当G因系统调用（syscall）阻塞时会阻塞M，此时P会和M解绑即hand off，并寻找新的idle的M，若没有idle的M就会新建一个M（流程5.1）

- 当G因channel或者network IO阻塞时，不会阻塞M，M会寻找其他runnable的G，当阻塞的G恢复后会重新进入runnable进入P队列等待执行（流程5.3）

# P和M的个数？

- P：有启动时环境变量￥GOMAXPROCS或者是由runtime的方法GOMAXPROCS()决定。这意味着在程序执行的任意时刻都只有￥GOMAXPROCS个goroutine在执行

- M：
  
  - Go语言本身的限制：Go程序启动时，会设置M的最大数量，默认10000，但是内核很难支持这么多的线程数，所以这个限制可以忽略。
  
  - runtime/debug中的SetMaxThreads函数，设置M的最大数量
  
  - 一个M阻塞了，会创建新的M

M和P的数量没有绝对关系，一个M阻塞，P就回去创建或切换到另一个M，所以，即时P的默认数量是1，也可能会有很多个M出来。

# P和M何时会被创建

P：在确定了P的最大数量n后，运行时系统会根据这个数量创建n个P

M：没有足够的M来关联P并运行其中的可与运行的G时创建。比如所有的M此时都阻塞了，而P中还有很多个就绪任务，就会去寻找空闲的M，而没有空闲的，就会去创建新的M。

# goroutine创建流程

在调用go func()的时候，会调用runtime.newproc来创建一个goroutine，这个goroutine会新建一个自己的栈空间，同时在G的sched中维护栈地址与程序计数器这些信息（备注：这些数据在goroutine被调度的时候会被用到。准确的说该goroutine在放弃CPU之后，下一次在重新获取cpu的时候，这些信息会被重新加载到cpu的寄存器中）

创建好的这个goroutine会被放到它所对应的内核线程M所使用的上下文中的run_queue中，等待调度器来决定何时取出该goroutine并执行，通常调度是按时间顺序被调度的，这个队列是一个先进先出的队列。

# goroutine什么时候会被挂起？

- waitReasonChanReceiveNilChan: 对未初始化的channel进行读操作

- waitReasonChanSendNilChan: 对未初始化的channel进行写操作

- 在main goroutine发生panic时，会触发

- 在调用关键字select时会触发

- 在调用关键字select时，若一个case都没有会直接触发

- 在channel进行读操作，会触发

- 在channel进行写操作，会触发

- sleep行为，会触发

- IO阻塞等待时，例如：网络请求等

- 在垃圾回收时，主要场景是GC标记终止和标记阶段时触发

- GC清扫阶段中的结束行为，会触发

- 信号量处理结束时，会触发

# 同时启动了一万个goroutine，会如何调度？

一万个G会按照P的个数，尽量平均的分配到每个P的本队列中。如果所有的本地队列都满了，剩余的G则会分配到GMP的全局队列上，接下来便开始执行GMP模型的调度策略：

- 本地队列轮转：每个P维护着一个包含G的队列，不考虑G进入系统调用或IO操作的情况下，P周期性的将G调度到M中执行，执行一小段时间，将上下文保存起来，然后将G放到队列尾部，然后从队首中重新取出一个G进行调度。

- 系统调用：P的个数默认等于CPU核数，每个M必须持有一个P才可以执行G，一般情况下M的个数会略大于P的个数，这多出来的M将会在G产生系统调用时发挥作用。当该G即将进入系统调用时，对应的M由于陷入系统调用而被阻塞，将释放P，进而某个空闲的M1获取P，继续执行P队列中剩下的G。

- 工作量窃取：多个P中维护的G队列有可能是不均衡的，当某个P已经将G全部执行完，然后去查询全局队列，全局队列中也没有新的G，而另一个M中队列中还有很多G待运行。此时，空闲的P会将其他P中的G偷取一部分过来，一般每次偷取一半。

# goroutine内存泄漏和处理

原因：

Goroutine是轻量级线程，需要维护执行用户代码的上下文信息。在运行过程中也需要消耗一定的内存来保存这类信息，而这些内存在目前版本的Go中是不会被释放的。因此，如果一个程序持续不断的产生新的goroutine、且不结束已经创建的goroutine并服用这部分内存就会造成内存泄露的现象。造成泄露的大多数原因有以下三种：

- goroutine内正在进行channel/mutex等读写操作。但由于逻辑问题，某些情况下被一致阻塞

- goroutine内的业务逻辑进入死循环，资源无法释放

- goroutine内的业务逻辑进入长时间等待，有不断新增的goroutine进入等待。

解决方法：

- 使用channel
  
  1. 使用channel接受业务完成的通知
  
  2. 业务执行阻塞超过设定的超时时间，就会触发超时退出

- 使用pprof排查
  
  - 由Go官方提供的可用于收集程序运行时报告的工具，其中包含CPU、内存等信息。也可以获取运行时goroutine堆栈信息
