# 设计模式----Golang

[Easy搞定Golang设计模式(Go语言设计模式，如此简单)_哔哩哔哩_bilibili](https://www.bilibili.com/video/BV1Eg411m7rV/?spm_id_from=333.999.0.0&vd_source=6624455d15d4e57b52b279d7da88fa40)

## 在一定环境下，用固定套路解决问题

## 一、设计模式原则

### 1.1 单一职责原则

每个类的职责单一，对外只提供一种功能，而引起类变化的原因都应该只有一个

```go
package main

import "fmt"

type ClotheShop struct{}

func (cs *ClotheShop) Style() {
    fmt.Println("逛街的装扮")
}

type ClotheWork struct{}

func (cs *ClotheWork) Style() {
    fmt.Println("工作的装扮")
}

func main() {
    //工作的业务
    cw := ClotheShop{}
    cw.Style()
    //逛街的业务
    cs := ClotheShop{}
    cs.Style()
}
```

### ！！开闭原则（彻底理解）

**类的改动是通过增加代码进行的，而不是修改源代码**

违反开闭原则的设计

```go
package main

import "fmt"

// 无开闭原则的

type Banker struct{}

// 存款
func (b *Banker) Save() {
    fmt.Println("进行了 存款")
}

// 转账
func (b *Banker) Tansfer() {
    fmt.Println("进行了 转账")
}

// 支付
func (b *Banker) Pay() {
    fmt.Println("进行了 支付")
}

// 当添加代码的时候，给这个类添加了一个方法----违反了开闭原则，不应该修改这个类的源代码

func main() {
    banker := &Banker{}

    banker.Save()
    banker.Tansfer()
    banker.Pay()
}
```

正确的设计

```go
// 抽象的Banker
type AbstractBanker interface {
    DoBusi() //抽象的处理业务接口
}

// 存款的业务员
type SaveBanker struct {
    // AbstractBanker
}

func (sb *SaveBanker) DoBusi() {
    fmt.Println("进行了存款业务")
}

// 转账2的业务员
type TansferBanker struct {
    // AbstractBanker
}

func (sb *TansferBanker) DoBusi() {
    fmt.Println("进行了转账业务")
}

// 实现一个架构层（基于抽象层进行业务封装-针对interface接口进行封装）
func BankBusiness(banker AbstractBanker) {
    //通过接口向下来调用（多态的现象）
    banker.DoBusi()
}
func main() {
    // 存款的业务
    sb := &SaveBanker{}
    sb.DoBusi()

    BankBusiness(&SaveBanker{})
    BankBusiness(&TansferBanker{})
}
    sb.DoBusi()
}
```

### 里氏代换原则

任何抽象类（interface接口）出现的地方都可以用他的实现类进行替换，实际就是虚拟机制，语言级别实现面向对象功能

### ！！依赖倒转原则（理解并会应用）

**依赖于抽象（接口），不要依赖具体的实现（类），也就是针对接口编程**

耦合度极高的代码如下

```go
//耦合度极高的模块设计

//司机张三，李四，汽车 宝马，奔驰
//1. 张三  开奔驰
//2. 李四 开宝马

// 奔驰汽车
type Benz struct {
}

func (b *Benz) Run() {
    fmt.Println("Benz is running....")
}

type BMW struct{}

func (b *BMW) Run() {
    fmt.Println("BMW is running....")
}

// 司机张三

type Zhang3 struct{}

func (z3 *Zhang3) DriveBenz(benz *Benz) {
    fmt.Println("zhang3 Drive Benz")
    benz.Run()
}

func (z3 *Zhang3) DriveBMW(bwm *BMW) {
    fmt.Println("zhang3 Drive BMW")
    bwm.Run()
}

type Li4 struct{}

func (z3 *Li4) DriveBMW(bmw *BMW) {
    fmt.Println("Li4 Drive BMW")
    bmw.Run()
}

//...M*N个方法

func main() {
    benz := &Benz{}
    bwm := &BMW{}
    zhang3 := &Zhang3{}
    li4 := &Li4{}
    zhang3.DriveBenz(benz)
    li4.DriveBMW(bwm)

}
```

依赖倒转如下：

面向抽象层依赖倒转将程序分为三层

实现层-抽象层-业务逻辑层

业务逻辑层依赖于抽象层而不依赖于实现层

```go
//依赖倒转原则改进
// --->抽象层<---
type Car interface {
    Run()
}

type Driver interface {
    Drive(car Car)
}
// --->实现层<---
type Benz struct {
    //.....
}

func (b *Benz) Run(){
    fmt.Println("Benz is running...")
}

type Bmw struct {
    //.....
}

func (b *Bmw) Run(){
    fmt.Println("Bmw is running...")
}

type Zhang3 struct{
    //...
}

func(zhang3 Zhang3) Driver(car Car){
    fmt.Println("Zhang3 is driving...")
    car.Run()
}

type Li4 struct{
    //...
}

func(li4 Li4) Driver(car Car){
    fmt.Println("Li4 is driving...")
    car.Run()
}
// --->业务逻辑层<---
func main(){
    // zhang3->benz
    var benz Car
    benz = new(Benz)
    var zhang3 Driver
    zhang3.Drive(benz)

    // li4->bmw
    var bmw Car
    bmw = new(Bmw)
    var li4 Driver
    li4.Drive(bmw)


}
```

### 接口隔离原则

不应该强迫用户的程序依赖他们不需要的接口方法。

一个接口应该只提供一种对外功能，不应该把所有操作都封装到一个接口中去。

### 合成复用原则

如果使用继承，会导致父类的任何变换都可能影响到子类的行为。如果使用对象组合，就降低了这种依赖关系。对于继承和组合，优先使用组合。

```go
package main

import "fmt"

type Cat struct{}

func (c *Cat) Eat(){
    fmt.Println("小猫吃饭")
}


//给小猫添加一个可以睡觉的方法（使用继承）
type CatB struct {
    Cat
}

func (cb *CatB)Sleep(){
    fmt.Println("小猫睡觉")
}

//将耦合从类转到具体函数，耦合度更小
type CatC struct {
    //C *Cat // 组合进来一个Cat类
}

func (cc *CatC)Eat(c *Cat){
    c.Eat()
}

func (cc *CatC)Sleep(){
    fmt.Println("小猫睡觉")
}
func main(){
    cc := &CatC{}
    cc.Eat(&Cat{})
    cc.Sleep()
}
```

### ！！迪米特法则

一个对象应该对其它对象尽可能减少了解，从而降低各个对象之间的耦合，提高系统的可维护性。例如在一个程序中，各个模块之间相互调用时，通常会提供一个统一的接口来实现。这样其他模块不需要了解另外一个模块的内部实现细节，这样当一个模块内部的实现发生改变时，不会影响其他模块的使用

## 创建型

如何创建对象

### 为什么需要工厂模式

业务逻辑层与工厂模块打交道，由工厂模块来关注对基础类模块相关的耦合，从而降低业务逻辑层对基础类模块的耦合

### 1. 简单工厂模式

**通过专门定义一个类来负责创建其他类的实例，被创建的实例通常都具有共同的父类**

**工厂角色：** 它负责实现创建所有实例的内部逻辑。工厂类可以被外界直接使用，创建u宋旭的产品对象

**抽象产品角色：** 简单工厂模式所创建的所有对象的父类，它负责描述所有实例所共有的公共接口

**具体产品角色：** 简单工厂模式所需创建的具体实例对象

![](C:\Users\ljc\Documents\GitHub\--\看视频啦\刘丹冰Aceld\设计模式\图片\简单工厂.PNG)

优点：

1. 实现了对象创建和使用的分离

2. 不需要记住具体类名，记住参数就好

缺点：

1. 工厂职责过重，一旦不能工作，系统受到影响

2. 增加系统中类的个数，复杂度增加

3. 违反开闭原则，添加新产品需要修改工厂逻辑，工厂越来越复杂

### 2. 工厂方法模式

**定义一个创建产品对象的工厂接口，将实际创建工作推迟到子类中**

**简单工厂加开闭原则**

抽象产品类---->具体的产品类，抽象工厂类--->具体产品的工厂，业务逻辑只与抽象类打交道

优点：

1. 不用记类名和参数

2. 实现了对象创建和使用的分离

3. 扩展性好，符合开闭

缺点：

1. 客户端不知道它所需要的对象的类

2. 抽象工厂类通过其子类来指定创建哪个对象

### 3. ！！抽象工厂模式

**提供一个创建一系列相关或者相互依赖的接口，而无需指定它们具体的类**

#### 产品族和产品等级结构

![](C:\Users\ljc\Documents\GitHub\--\看视频啦\刘丹冰Aceld\设计模式\图片\产品族和产品等级结构.PNG)

按产品族进行划分工厂，产品族里面的东西应该是稳定的

#### 抽象工厂方法

抽象的产品族工厂，提供所拥有的具体产品数量的创建方法（基本不变所以这样）

![](C:\Users\ljc\Documents\GitHub\--\看视频啦\刘丹冰Aceld\设计模式\图片\抽象产品族与具体产品族.PNG)

多个抽象的产品类，每个里偏右产品族数量的产品创建方法

### 4. ！！单例模式

**是保证一个类仅有一个实例，并提供一个访问它的全局访问点**

## 结构型

如何实现类或对象的组合

### 1. ！！代理模式

### 2. 装饰模式

### 3. ！！外观模式

### 4. 适配器模式

## 行为型

类或对象怎样交互以及怎样分配职责

### 1. 模板模式

### 2. 命令模式

### 3. ！！策略模式

### 4. ！！观察者模式
