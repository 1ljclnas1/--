# Golang 内存分配原理

[Golang 内存分配原理_哔哩哔哩_bilibili](https://www.bilibili.com/video/BV16B4y1m7PC/?spm_id_from=333.337.search-card.all.click&vd_source=6624455d15d4e57b52b279d7da88fa40)

为什么栈比对快？栈中出栈入栈只需要两个指令一个push一个pop。堆内存分配比较复杂

借鉴了TCMalloc，尽量减少在多线程模型下的，线程开销

TC快的原因是：

1. 减少锁的争用：每个有ThreadCache不需要加锁

2. 减少了系统调用，上下文的切换

![](C:\Users\ljc\Documents\GitHub\--\看视频啦\硬核课堂\图片\golang内存管理.png)

每一个处理器(P)都会分配一个mcache用于处理为对象和小对象的分配，他们持有mspan

每个类型的内存管理单元都会管理特定大小的对象，当内存管理单元不存在空闲对象的时候他们会从mcentral申请新的内存单元，中心缓存属于全局的对结构体mheap，mheap会从操作系统申请。

## 内存管理单元 mspan

内存管理的基本单元

每个mspan都对应一个大小等级(67)，小对象类型的堆对象会根据其大小分配到相应设定好大小等级mspan上分配内存。

- 微对象(0,16B): 先使用微型分配器，再依次尝试县城缓存、中心缓存和堆分配内存

- 小对象[16B,32KB]: 依次尝试线程缓存，中心缓存和堆分配内存

- 大对象(32KB, +++):直接在堆上分配内存

该结构体包含next和prev两个指针，形成双向链表。

### 页和内存

Span----跨度

一段连续的内存空间

每个mspan都管理npages个大小为8KB的页，这里的页不是操作系统的内存页，是操作系统内存页的整数倍。

```go
type mspan struct {
    startAddr uintptr
    npages    uintptr
    freeindex uintptr

    allocBits    *gcBits    // 用于标记内存的占用情况
    gcmarkBits    *gcBits    // 用于标记内存的回收情况
    allocCache    uint64    // allocBits的补码，可以用于快速查找未被使用的内存
}
```

mspan会以两种不同的视角看待管理的内存，

当结构体管理的内存不足时，运行时会以页为单位向堆申请内存

当用户程序或线程向mspan申请内存时，它会使用allocCache字段以对象为单位在管理的内存中快速查找待分配的空间

如果我们能在内存中找到空闲的内存单元会直接返回，当内存中不包含空闲的内存时，上一级的组件mcache会为调用mcache.refill更新内存管理单元以满足为更多的对象分配内存的需求

### 跨度类 Spanclass

标记mspan的类型 就是ID int8

Go中一共有67种跨度类，每一个跨度类都会存储特定大小的对象并且包含特定数量的页数以及对象，所有的数据都会被预先计算好并存储在runtime.class_to_size和runtime.class_to_allocnpages等变量中。

为什么预先计算？因为用空间换时间

运行时还会包含id为0的特殊跨度类，用于管理大于32KB的特殊对象

除了存储id之外还会存储noscan标记，标识这个对象是否包含指针，gc过程中会对于指针往下scan，有noscan就可以不扫描，节省时间消耗。

是一个uint8了剋行的整数，前七位存储跨度类ID，最后一位标识是否为noscan

## 线程缓存mcache

线程缓存，与P一一绑定，每个线程分配一个mcache用于处理为对象和小对象的分配，因为是每个线程独有的，所以不用加锁

mcache在刚初始化的时候是不包含mspan的，只有当用户程序申请内存时才会从上一级组件获取新的mspan满足内存分配的需求

- mcache会持有tiny相关字段用于微对象内存分配

- mcache会持有mspan用于小对象内存分配

- alloc用于分配内存的mspan数组

数组大小为span类型总数的2倍，即每种span类型都有两个mspan，一个标识对象中包含了指针，一个表示对象不含有指针。

mcache在刚被初始化时，alloc中mspan时空的占位符emptymspan。当mcache中mspan的空闲内存不足时，会向mcentral组件请求获取mspan

![](C:\Users\ljc\Documents\GitHub\--\看视频啦\硬核课堂\图片\alloc.png)

#### 微分配器 TinyAllocator

线程缓存中还包含几个用于分配微对象的字段，下面的这三个字段组成了微对象分配器，专门管理16字节以下的对象

```go
type mcache struct{
    ...
    tiny    uintptr
    tinyoffset    uintptr
    local_tinyallocs    uintptr

}
```

tiny指向堆中的一片内存

tinyOffset是下一个空闲内存所在的偏移量

local_tinyallocs会记录内存分配器中分配的对象个数

## 中心缓存mcentral

多个线程公用，需要互斥锁

每个中心缓存都会管理某个跨度类的内存管理单元，他会同时持有两个runtime.spanSet，分别存储包含空闲对象和不包含空闲对象的内存管理单元

- partial: 有空闲空间的span列表

- full：没有空闲空间的span列表

从mcentral中申请资源的时候，会先按照有空闲空间的列表中申请，申请到了之后，替换掉mcache中相应跨度类的mspan，如果没有申请到就去无空闲空间链表申请（GC之后，这里可能出现了可以再用的mspan），如果还是失败，就去mheap申请mspan

申请到了就会更新mspan的  allocBits和allocCache字段，被替换掉的由mheap和mcentral管理，所以mcache不需要记录

## 页堆mheap

内存分配的核心组件包含mcentral和heapArena，堆上所有mspan都是从mheap结构分配来的。

![](C:\Users\ljc\Documents\GitHub\--\看视频啦\硬核课堂\图片\mheap.png)

- allspans：已经分配的所有mspan

- arenas：heapArena数组，用于管理一个个内存块

- central：mcentral数组，用于管理对应spanClass的mspan

# 内存分配

堆上所有的对象都会通过调用runtime.newobject函数分配内存，该函数会调用runtime.mallocgc分配指定大小的内存空间，这也是用户程序向堆上申请内存空间的必经函数

### 微对象

使用mcache的微分配器来提高性能，主要用来分配较小的字符串以及逃逸的临时变量，为分配器可以将多个较小的内存分配合入一个微分配器，但是gc的时候，必须全部被标为可回收才能整体回收。

不能是指针类型，管理多个对象的内存块大小maxTinySize是可以调整的，在默认情况下，内存块的大小为16字节。maxTinySize的值越大，组合多个对象的可能性就越高，内存浪费也就越严重；maxTinySize越小，内存浪费就越少，不过无论如何调整，8的倍数都是好的选择 。

### 小对象

大于16字节小于32KB和小于16字节的指针类型的对象，

分为三个步骤

5. 确定分配对象的大小以及跨度类

6. 从线程缓存、中心缓存或者堆中获取内存管理单元并从内存管理单元找到空闲的内存空间

7. 调用runtime.memclrNoHeapPointers清空空闲内存中的所有数据。

### 大对象

对于大于32KB的大对象，单独处理，直接调用mcache.allocLarge

按照8KB的倍数在堆上申请内存。

# 逃逸分析

是一种静态分析，在编译期间执行，决定变量是在栈上分配还是在堆上分配：

遵循两个不变性：

1. 指向栈对象的指针不能存在于堆中

2. 指向栈对象的指针不能在栈对象回收后存活

主要策略：

1. 如果函数外部没有引用，则优先放到栈中

2. 如果函数外部存在引用，则必定放到堆中

场景：

1. 函数返回局部变量指针引起的逃逸

2. 动态类型引起的逃逸

3. 栈空间不足引起的逃逸

4. 闭包引用的逃逸：闭包函数中没有定义变量i的，而是引用了它所在函数f中的变量i，变量i发生逃逸。

### 如何利用逃逸分析提高性能？

#### 传值vs传指针

传指针可以减少对象拷贝，但是会引发逃逸到堆，增加垃圾回收的负担

因此对于大对象才传指针，小对象传值更好
