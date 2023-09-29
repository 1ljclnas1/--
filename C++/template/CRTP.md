# CRTP 曲折继承 (  Curiously Recurring Template Pattern奇怪的重复模板模式)

[Curiously Recurring Template Pattern - cppreference.com](https://en.cppreference.com/w/cpp/language/crtp)

```cpp
template<class Z>
class Y{};



class X : public Y<X>{};
```

当一个基类声明了一个接口的时候，而派生类实ra现这个接口的时候，CRTP可以派上用场。

```cpp
#include <cstdio>

#ifndef __cpp_CRTP_DEMO
#define __cpp_CRTP_DEMO

template <class Derived>
struct Base { 
    void name() { 
        (static_cast<Derived*>(this))->impl();
    }
};
struct D1 : public Base<D1> { 
    void impl() { 
        std::puts("D1::impl()"); 
    } 
};
struct D2 : public Base<D2> { 
    void impl() { 
        std::puts("D2::impl()"); 
    } 
};

void test()
{
    // Base<D1> b1; b1.name(); //undefined behavior
    // Base<D2> b2; b2.name(); //undefined behavior
    D1 d1; d1.name();
    D2 d2; d2.name();
}

#else // C++23修改了语法，https://godbolt.org/z/s1o6qTMnP

struct Base { 
    void name(this auto&& self) { 
        self.impl(); 
    } 
};
struct D1 : public Base {
    void impl() { 
        std::puts("D1::impl()"); 
    } 
};
struct D2 : public Base { 
    void impl() { 
        std::puts("D2::impl()"); 
    } 
};

void test()
{
    D1 d1; d1.name();
    D2 d2; d2.name();
}

#endif //__cpp_CRTP_DEMO

int main(){
    test();
}
```

## stl中的应用

[view_interface](view_interface.md)
