# 1.1 什么是并发
## 1.1.1 计算机系统中的并发
在计算机只有一个处理器的时代，这种计算机某一时刻只可以执行一个任务。通过做一点这个任务，再做一点别的任务。让任务看起来像是在并行发生。即**任务切换**，这是一种并发的“假象”

当出现了多核处理器，才能够**真正并行运行多个任务**，称为**硬件并发**

单核处理器执行并发任务时，需要交替切换任务。切换时会执行一次**上下文切换**。为了执行上下文切换，操作系统需要
- 保存当前运行的任务保存CPU的状态和指令指针
- 算出切换到哪个任务，并为要切换的任务重新加载处理器状态。
- CPU可能还要将新任务的指令缓存载入 （造成延迟）

**硬件线程**是指硬件可以真正并发运行多少独立的任务

## 1.1.2 并发的途径
### 多进程并发
将应用程序分为多个**独立的，单线程的进程**

缺点是
1. 进程之间的通信通常比较复杂，速度较慢。因为操作系统在进程间提供大量的保护，以避免进程相互干扰。
2. 运行多个进程产生的固有开销

### 多线程并发
另一个途径是在单个进程中运行多个线程，每个线程可以运行不同的指令序列，但**共享相同的地址空间**。

相比于多进程，多线程并发开销较小，更为灵活。但缺点是多个线程运行时访问的数据可能不一致（可能另外一个线程修改了），需要程序员确保程序正确性

# 1.2 为什么使用并发
## 1.2.1 为了划分关注点而使用并发
使用并发来分割不同的功能，让每个线程逻辑更加简单，而不是不同任务逻辑混杂在一起。
## 1.2.2 为了性能而使用并发
具体来说分两种方式
1. 任务并行，让不同线程分别执行算法的不同部分
2. 数据并行，让不同线程在不同的数据部分上执行相同的操作
## 1.2.3 什么时候不使用并发
1. 并发代码过于复杂，难以维护
2. 若线程运行任务很快完成，那么任务实际占据的时间与启动线程的开销时间就显得微不足道
3. 线程是有限的资源，过多线程会消耗操作系统资源

# 一个简单示例
```cpp
#include "iostream"
#include <thread>

void hello(){
    std::cout<<"Hello Concurrent World \n";
}

int main(){
    std::thread t(hello);
    t.join();
}
```
首先我们定义了一个hello函数，然后在初始线程main中，启动了额外的一个新线程t。初始线程继续执行，而新线程执行hello函数。

如果不等待新线程，那么初始线程可能自顾自地运行到main函数结束。所以最后调用了`join`方法，让调用线程`main`等待`t`线程执行完毕

然后进行编译
```shell
g++ hello_world.cpp -o hello_world -lpthread
```

