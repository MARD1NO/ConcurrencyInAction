# 3.1 线程之间共享数据的问题
所有线程之间共享数据的问题，都是**修改数据导致的**。如果共享数据都是只读的，就不存在问题

## 3.1.1 竞争条件
在并发性中，竞争条件就是结果取决于两个或更多线程上的操作执行的相对顺序的一切事物

竞争条件通常用来表示有问题的竞争条件， 数据竞争表示因单个对象的并发修改而产生的特定类型的竞争条件

## 3.1.2 避免有问题的竞争条件
1. 封装数据结构，确保只有实际执行修改的线程能够在不变量损坏的地方看到中间数据
2. 修改数据结构的设计及其不变量，令修改作为一系列不可分割的变更来完成，每个修改均保留其不变量（也叫无锁编程）
3. 将数据结构的更新作为一个事务，在单个步骤进行提交。若提交因为数据结构被另一个线程修改而无法进行，则事务重新启动

# 3.2 用互斥元mutex保护共享数据
在访问共享数据之前，锁定与该数据相关的互斥元，当访问数据结构完成后，解锁互斥元。

线程库会确保一旦一个线程已经锁定某个互斥元，所有其他试图锁定相同互斥元的线程必须等待，直到成功锁定了该互斥元的线程解锁此互斥元。

## 3.2.1 使用C++中的互斥元
通过构造`std::mutex`实例创建互斥元，调用函数`lock`锁定，`unlock`解锁

调用成员函数比较繁琐，可能遗漏。C++标准库提供了`std::lock_guard`类模板
```cpp
#include "list"
#include "mutex"
#include "algorithm"

std::list<int> some_list; 
std::mutex some_mutex; 

void add_to_list(int new_value){
    std::lock_guard<std::mutex> guard(some_mutex); 
    some_list.push_back(new_value);
}

bool list_contains(int value_to_find){
    std::lock_guard<std::mutex> guard(some_mutex); 
    return std::find(some_list.begin(), some_list.end(), value_to_find) != some_list.end();
}
```
这两个函数都使用了`lock_guard`来保护，所以函数中的访问是互斥的。`list_contains`无法在`add_to_list`执行过程中看到`some_list`变量

## 3.2.2 为保护共享数据精心组织代码
```cpp
class some_data{
    int a; 
    std::string b;
public: 
    void do_something();
};
class data_wrapper{
private: 
    some_data data; 
    std::mutex m; 
public: 
    template<typename Function>
    void process_data(Function func){
        std::lock_guard<std::mutex> l(m);
        func(data);
    }
};
some_data* unprotected; 

void malicious_function(some_data& protected_data){
    unprotected = &protected_data;
}
data_wrapper x; 
void foo(){
    x.process_data(malicious_function); // 传入恶意函数
    unprotected->do_something();
}
```
这里通过`malicious_function`绕过了`lock_guard`的保护。它无需锁定互斥元即可`do_something`

准则： 
- **不要将对受保护数据的指针和引用传递到锁的范围之外，无论是通过从函数中返回它们，将其存放在外部可见的内存中，还是作为参数传递给用户提供的函数**

## 3.2.3 发现接口中固有的竞争条件
考虑一个stack容器适配器的接口
```cpp
template<typename T, typename Container=std::deque<T>>
class Stack{
public: 
    bool empty() const; 
    size_t size() const; 
    void push(); 
    void pop();
    void swap();
    T& top();
    ...
}
```
问题在于`empty`和`size`接口并不可靠，返回的时候，可能别的线程将元素入栈或出栈

此外某些API粒度过小，如top，导致锁并没有很好的保护整个过程（见p40，表3.1）

一个线程安全堆栈的示范定义
```cpp
#include "exception"
#include "memory"

struct empty_stack: std:exception{
    const char* what() const throw();
};

template<typename T>
class threadsafe_stack{
public: 
    threadsafe_stack(); 
    threadsafe_stack(const threadsafe_stack&); 
    threadsafe_stack& operator=(const threadsafe_stack&) = delete; 

    void push(T new_value);
    std::shared_ptr<T> pop(); 
    void pop(T& value);
    bool empty() const; 
}
```
原来的接口被削减为`pop`, `push`。接口的简化可以更好控制数据，你可以确保互斥元为了操作的整体而被锁定

```cpp
#include "exception"
#include "memory"
#include "mutex"
#include "stack" 

struct empty_stack: std::exception{
    const char* what() const throw();
};
template<typename T>
class threadsafe_stack{
private: 
    std::stack<T> data; 
    mutable std::mutex m; 
public: 
    threadsafe_stack(){}
    threadsafe_stack(const threadsafe_stack& other){
        std::lock_guard<std::mutex> lock(other.m);
        data = other.data;
    }
    threadsafe_stack& operator=(const threadsafe_stack&) = delete; 

    void push(T new_value){
        std::lock_guard<std::mutex> lock(m); 
        data.push(new_value);
    }
    std::shared_ptr<T> pop(){
        std::lock_guard<std::mutex> lock(m);
        if(data.empty()) throw empty_stack(); 
        std::shared_ptr<T> const res(std::make_shared<T>(data.top()));
        data.pop(); 
        return res; 
    }
    void pop(T& value){
        std::lock_guard<std::mutex> lock(m); 
        if(data.empty()) throw empty_stack(); 
        value = data.top(); 
        data.pop();
    }
    bool empty() const{
        std::lock_guard<std::mutex> lock(m); 
        return data.empty();
    }
};
```
**上述的例子表明，锁的粒度过小，会产生有问题的竞争条件。然而锁的粒度过大，在极端情况下，单个全局互斥元保护所有共享的数据，会消除并发的优势**

## 3.2.4 死锁
简单来说，死锁就是一对线程中 **一对线程中的每一个都需要锁住一对互斥锁来执行某些操作，其中每个线程都有一个互斥锁，并且正在等待另一个。这样没有线程能继续进行，因为他们都在等待对方释放互斥锁**

常见的避免死锁的建议是**始终使用相同的顺序锁定这两个互斥元**

考虑一个场景，我们交换**同一个类，两个不同实例**的数据。为保证数据被正确交换，我们需要给两个实例都上锁
但是，如果选择一个固定的顺序(例如，先是第一个参数对应的互斥锁，然后是第二个参数的互斥锁)，这可能会适得其反：只要交换一下参数的位置，两个线程试图在相同的两个实例间交换数据，就会发生死锁！

C++标准库提供了`std::lock`，它可以一次性锁住多个互斥锁，且没有死锁的风险
```cpp
class some_big_object;
void swap(some_big_object& lhs, some_big_object& rhs); 

class X{
private: 
    some_big_object some_detail; 
    std::mutex m; 
public: 
    X(some_big_object const& sd): some_detail(sd){}
    friend void swap(X& lhs, X& rhs){
        if(&lhs == &rhs)
            return; 
        std::lock(lhs.m, rhs.m); 
        std::lock_guard<std::mutex> lock_a(lhs.m, std::adopt_lock); 
        std::lock_guard<std::mutex> lock_b(rhs.m, std::adopt_lock); 
        swap(lhs.some_detail, rhs.some_detail);
    }
};
```
- 首先检查参数，确保是不同的实例
- 调用`std::lock`锁定这两个实例的互斥元，并分别构造两个`lock_guard`实例
- 额外提供一个参数`std::adopt_lock`，告知`lock_guard`该互斥元已被锁定

## 3.2.5 避免死锁的进一步指南
1. 避免嵌套锁
2. 在持有锁时，避免调用用户提供的代码
3. 以固定顺序获取锁
4. 使用锁层次。如果在**较低层**已持有锁定，则不允许锁定

```cpp
hierarchical_mutex high_level_mutex(10000); 
hierarchical_mutex low_level_mutex(5000); 

int do_low_level_stuff(); 

int low_level_func(){
    std:lock_guard<hierarchical_mutex> lk(low_level_mutex); 
    return do_low_level_stuff(); 
}
void high_level_stuff(int some_param);

void high_level_func(){
    std::lock_guard<hierarchical_mutex> lk(high_level_mutex);
    high_level_stuff(low_level_func());
}

void thread_a(){
    high_level_func();
}
hierarchical_mutex other_mutex(100);
void do_other_stuff();

void other_stuff(){
    high_level_func(); 
    do_other_stuff();
}
void thread_b(){
    std::lock_guard<hierarchical_mutex> lk(other_mutex);
    other_stuff();
}
```
`thread_b`没有遵守规则，获取了一个层次为100的锁，意味着保护着低级别的数据。但是`other_stuff`里调用的`high_level_func`具有更高层次，违反了层次。

`hierarchical_mutex`的相关代码如下，我们实现`lock`, `unlock`和`try_lock`。`try_lock`表示如果互斥元上的锁已被另一个线程所持有，则返回false

```cpp
class hierarchical_mutex{
    std::mutex internal_mutex; 
    unsigned long const hierarchy_value;
    unsigned long previous_hierarchy_value; 
    static thread_local unsigned long this_thread_hierarchy_value;

    void check_for_hierarchy_violation(){
        if(this_thread_hierarchy_value <= hierarchy_value){
            throw std::logic_error("mutex hierarchy violated");
        }
    }
    void update_hierarchy_value(){
        previous_hierarchy_value = this_thread_hierarchy_value;
        this_thread_hierarchy_value = hierarchy_value;
    }
public: 
    explicit hierarchical_mutex(unsigned long value): hierarchy_value(value), previous_hierarchy_value(0){}
    void lock(){
        check_for_hierarchy_violation();
        internal_mutex.lock();
        update_hierarchy_value();
    }
    void unlock(){
        this_thread_hierarchy_value = previous_hierarchy_value; 
        internal_mutex.unlock();
    }
    bool try_lock(){
        check_for_hierarchy_violation();
        if(!internal_mutex.try_lock())
            return false; 
        update_hierarchy_value();
        return true;
    }
};

thread_local unsigned long hierarchical_mutex::this_thread_hierarchy_value(ULONG_MAX);
```
- 首先将`this_thread_hierarchy_value`值初始化为最大值，刚开始任意互斥元都可以被锁定
- 当锁定的时候，对层次进行检查。通过后让内部的互斥元进行锁定，并更新当前层次值

## 3.2.6 用std::unique_lock 灵活锁定
一个`std::unique_lock`实例并不总是拥有与之相关联的互斥元，我们可以把`std::defer_lock`作为第二参数传递，来表示该互斥元在构造时应保持未被锁定

一个小问题：`std::unique_lock`占用更多空间，且相比`lock_guard`稍慢。因为它必须存储这个信息，并在必要的时刻更新

```cpp
class some_big_object;
void swap(some_big_object& lhs, some_big_object& rhs); 

class X{
private: 
    some_big_object some_detail; 
    std::mutex m; 
public: 
    X(some_big_object const& sd): some_detail(sd){}
    friend void swap(X& lhs, X& rhs){
        if(&lhs == &rhs)
            return; 
        std::unique_lock<std::mutex> lock_a(lhs.m, std::defer_lock); 
        std::unique_lock<std::mutex> lock_b(rhs.m, std::defer_lock); 
        std::lock(lock_a, lock_b);
        swap(lhs.some_detail, rhs.some_detail);
    }
};
```