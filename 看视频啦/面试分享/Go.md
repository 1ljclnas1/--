# 面试分享

## 模拟3年20-30K Go基础岗

[【模拟Go面试】模拟3年20K-30KGo岗位基础面_哔哩哔哩_bilibili](https://www.bilibili.com/video/BV1tF41147cK/?spm_id_from=333.337.search-card.all.click&vd_source=6624455d15d4e57b52b279d7da88fa40)

项目设计了mysql和redis，不整理相关

### go相关

1. 如果有内存泄漏，怎么去处理？
   
   用pprof查看火焰图和函数调用，分析是哪个函数导致的问题

2. 为什么go语言使用CSP模型来实现呢？用通信进行共享，而不是通过共享内存，使用channel进行通信

3. channel是不是线程安全的？怎么保证的
   
   是线程安全的，将channel的底层结构，sendq，recvq，有缓冲和无缓冲....

4. Go中的Mutex是悲观锁还是乐观锁呢？

5. Go中的RW锁？

对于4，5这个可以看Go高性能编程那里对Mutex和RWLock的介绍

6. map是不是线程安全的？
   
   不是，线程安全的，可以加一个读写锁或者用sync下面那个map

7. 如果想设计一个无锁保护的线程安全的map如何去设计？
   
   可以用一些乐观锁，虽然他是乐观锁，但他是cas的一个操作

8. 结构体可不可以进行一个比较？
   
   deepequal（），可比较字段，不可比较字段

9. 空结构体的作用？
   
   可以表示一个占位符，或者表示一个信号，可以用来实现Set，
   
   仅包含方法的结构体

10. 为什么要给channel用空结构体？
    
    节省空间

11. 字符串转byte数组的时候会进行内存拷贝么？
    
    标准转换会涉及拷贝，而且如果s大于32还会进行一次内存分配
    
    ```go
    type stringStruct struct {
        str unsafe.Pointer
        len int
    }
    ```

12. 字符串转bytes的时候如果不想发生内存拷贝怎么办？
    
    因为byte[]只多了一个cap所以是可以内存对齐的，使用unsafe.Pointer()，改变指针的指向。

13. 在多核的情况下cache如何保持一致？cpu缓存一致性

14. 讲讲goroutine

15. 一道常见的编程题，各打印一百次
    
    ```go
    package main
    
    import (
        "fmt"
        "sync"
    )
    func main(){
        for i := 0;i<100;i++{
            var wg sync.WaitGroup
            ch := make(chan int)
            ch2 := make(chan int)
            wg.Add(3)
            go func(){
                fmt.Print("Cat")
                ch<-1
                wg.Done()
            }()
            go func(){
                <-ch
                fmt.Print(" Dog")
                wg.Done()
                ch2<-2
            }()
            go func(){
                <-ch2
                fmt.Println(" Fish")
                wg.Done()
            }()
            wg.Wait()
        } 
    }
    ```

```go
package main

import (
	"fmt"
	"sync"
	"sync/atomic"
)

const MAX_COUNT uint64 = 100

func main() {
	
	var wg sync.WaitGroup
	wg.Add(3)
	var dogcounter uint64
	var fishcounter uint64
	var catcounter uint64
	
	dogch := make(chan struct{})
	fishch := make(chan struct{})
	catch := make(chan struct{})
	
	
	go dog(&wg, dogcounter, dogch, fishch)
	go fish(&wg, fishcounter, fishch, catch)
	go cat(&wg, catcounter, catch, dogch)
	dogch<-struct{}{}
	wg.Wait()
}

func dog(wg *sync.WaitGroup, counter uint64, dogch, fishch chan struct{}){
	for{
		if counter >= MAX_COUNT {
			wg.Done()
			return
		}
		<-dogch
		fmt.Print("dog ")
		atomic.AddUint64(&counter, 1)
		fishch<-struct{}{}
	}
}

func fish(wg *sync.WaitGroup, counter uint64, fishch, catch chan struct{}){
	for{
		if counter >= MAX_COUNT {
			wg.Done()
			return
		}
		<-fishch
		fmt.Print("fish ")
		atomic.AddUint64(&counter, 1)
		catch<-struct{}{}
	}
}
```

## ShowWeBug

### 1. 如何保证多核CPU的缓存一致性？

#### bus snooping 机制

当CPU修改自己私有的Cache时，硬件就会广播通知到总线上其他所有的CPU。

这样的话，不管别的CPU上是否缓存了相同数据都会广播一次，加重了总线负载，也增加了读写延迟

#### MESI Protocol

可以看作是一个状态机，将每一个cache line标记状态，并且维护状态的切换。cache line的状态可以像tag，modify等类似存储。

1. 当CPU0读取0x40数据，数据被缓存到CPU0私有Cache，此时CPU1没有缓存0x40数据，所以我们标记cache line状态为Exclusive。Exclusive代表cache line对应的数据仅在数据只在一个CPU的私有Cache中缓存，并且其在缓存中的内容与主存的内容一致。
2. 然后CPU1读取0x40数据，发送消息给其他CPU，发现数据被缓存到CPU0私有Cache，数据从CPU0 Cache返回给CPU1。此时CPU0和CPU1同时缓存0x40数据，此时cache line状态从Exclusive切换到Shared状态。Shared代表cache line对应的数据在"多"个CPU私有Cache中被缓存，并且其在缓存中的内容与主存的内容一致。
3. 继续CPU0修改0x40地址数据，发现0x40内容所在cache line状态是Shared。CPU0发出invalid消息传递到其他CPU，这里是CPU1。CPU1接收到invalid消息。将0x40所在的cache line置为Invalid状态。Invalid状态表示表明当前cache line无效。然后CPU0收到CPU1已经invalid的消息，修改0x40所在的cache line中数据。并更新cache line状态为Modified。Modified表明cache line对应的数据仅在一个CPU私有Cache中被缓存，并且其在缓存中的内容与主存的内容不一致，代表数据被修改。
4. 如果CPU0继续修改0x40数据，此时发现其对应的cache line的状态是Modified。因此CPU0不需要向其他CPU发送消息，直接更新数据即可。
5. 如果0x40所在的cache line需要替换，发现cache line状态是Modified。所以数据应该先写回主存。

以上是cache line状态改变的举例。我们可以知道cache line具有4中状态，分别是Modified、Exclusive、Shared和Invalid。取其首字母简称MESI。当cache line状态是Modified或者Exclusive状态时，修改其数据不需要发送消息给其他CPU，这在一定程度上减轻了带宽压力。

### 2. rune？

### 3. Mutex可以用作自旋锁么？

正常模式下可以，饥饿模式不可以，会导致拿不到锁的情况恶化

### 4. RWMutex？

### 5. 线程安全的共享内存？

## 2年 Golang

```go
package main

import (
    "fmt"
    "log"
)

type LetterFreq map[rune]int

func CountLetters(strs []string) LetterFreq{
    m := make(LetterFreq, 0)
    for _, str := range strs{
        for _, c := range str{
            m[r]++
        }
    }
    return m
}

func main(){
    input := []string{"abcd", "abc,"ab"}
    res := CountLetter(input)

    if res['a] != 3{
        log.Fatalf("want 3, get %d\n", res['a'])
    } else{
        fmt.Println("SUCCESS")
    }
}
```


