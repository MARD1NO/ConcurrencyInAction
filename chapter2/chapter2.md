# 2.1 基本线程管理
每个c++程序拥有至少一个线程，该线程运行着main函数。程序可以继续启动具有其他函数入口的线程，连同初始线程一起运行
## 2.1.1 启动线程
开始一个线程总是要构造一个`thread`对象。
```cpp
void do_some_work(); 
std::thread my_thread(do_some_work);
```
`std::thread`可以与任何可调用类型一同工作。所以我们可以将带有`函数调用操作符`的类的实例传递给`std::thread`的构造函数来进行代替
```cpp
class background_task
{
public: 
    void operator()() const{
        do_something();
        do_something_else();
    }
};
background_task f; 
std::thread my_thread(f);
```
所提供的函数对象被复制（copy）到属于新创建的执行线程的存储器中，并从那里调用。

如果传递一个临时的，且未命名的变量，那么其语法可能与函数声明一样，如
```cpp
std::thread my_thread(background_task());
```
**声明了函数`my_thread`，接受单个参数，并返回`std::thread`对象，而不是启动一个线程。**
为了避免这种情况，我们有新的统一初始化语法
```cpp
std::thread my_thread((background_task())); // 使用额外的括号避免其解释为函数声明
std::thread my_thread{background_task()}; // 使用新的统一初始化方法
```

一旦开始线程，需要显式地决定要等待它完成（join），还是任它自行运行(detach)

如果不等待线程完成，那么需要确保通过该线程访问的数据是有效的，直到该线程完成为止
下面看一个例子
```cpp
struct func{
    int &i; 
    func(int& i_): i(i_){}
    void operator()(){
        for(unsigned j=0; j < 1000000; ++j){
            do_something(i); // 对悬空引用可能的访问
        }
    }
}；

void oops(){
    int some_local_state = 0; 
    func my_func(some_local_state);
    std::thread my_thread(my_func);
    my_thread.detach(); 不等待线程完成
}
```
当`oops`退出时，与`my_thread`相关的线程可能仍在运行（因为我们调用了`detach`，选择不等待该线程）。

仔细看，`func`里面的变量`i`是一个引用。而我们在`oops`函数里，初始化了一个局部变量，并给func构造。当`oops`退出时候，该局部变量销毁，`func`此时访问到的`i`是一个悬空引用

一个常见的处理方式，是让线程函数自己包含局部变量，即**把数据拷贝到该线程中，而不是共享数据**

另外可以通过`join`，确保函数退出前，该线程执行完毕

## 2.1.2 等待线程完成
即在上面的代码将detach替换成`join`

调用`join`的行为也会清理所有与该线程相关联的存储器。意味着我们**只能对一个给定的线程调用一次join()**,一旦调用`join`则该thread对象不再是可连接的，并且`joinable()`返回false

## 2.1.3 在异常环境下的等待
在调用`join`的时候，需要仔细地选择在代码的哪个位置进行调用。在异常环境下，如果在线程开始之后，但在调用`join`之前引发异常，那对`join`的调用很容易被跳过

```cpp
struct func; 

void f(){
    int some_local_state = 0; 
    func my_func(some_local_state);
    std::thread t(my_func);
    try{
        do_something_in_current_thread();
    }
    catch(...){
        t.join(); // 异常中断
        throw;
    }
    t.join(); // 正常退出
}
```
我们可以提供一个类，在它的析构函数进行join
```cpp
class thread_guard{
    std::thread& t; 
public: 
    explicit thread_guard(std::thread& t_): t(t_){}
    ~thread_guard(){
        if(t.joinable()){
            t.join()
        }
    }
    thread_guard(thread_guard const&) = delete; 
    thread_guard& operator=(thread_guard const&)=delete;
};

void f(){
    int some_local_state = 0; 
    func my_func(some_local_state);
    std::thread t(my_func); 
    thread_guard g(t); 
    do_something_in_current_thread();
}
```
当执行到f函数末尾时，局部对象会按照构造的逆序销毁。因此`thread_guard g`对象最先被销毁，调用了其析构函数，其中析构函数根据线程的属性`joinable`，并调用了`join`方法。

另外该`thread_guard`类将**拷贝构造函数**和**拷贝赋值函数**标记为delete，以防止复制或赋值这样一个对象，因为它可能比要结合的线程的作用域存在得更久

## 2.1.4 在后台运行线程
调用`detach()`后，线程会在后台运行。没有直接的方法与之通信，也不再可能等待该线程完成。

为了从一个thread分离，必须有一个线程可供分离。同样我们可以使用`t.joinable()`来检查

假设我们有一个文档编辑的应用程序，不同文档的编辑互不干扰。意味着打开一个新的文档就需要启动一个新的线程，并且不在乎其他线程的完成情况，我们可以直接分离
```cpp
void edit_document(std::string const& filename){
    open_document_and_display_gui(filename); 
    while(!done_editing()){
        user_command cmd = get_user_input(); 
        if(cmd.type == open_new_document){
            std::string const new_name = get_filename_from_user();
            std::thread t(edit_document, new_name);
            t.detach();
        }
        else{
            process_user_input(cmd);
        }
    }
}
```
# 2.2 传递参数给线程函数
额外的参数会以默认的方式被复制（copy）到内部存储空间
```cpp
void f(int i, std::string const& s);
std::thread t(f, 3, "hello");
```

考虑以下代码
```cpp
void f(int i, std::string const& s);
void oops(int some_param){
    char buffer[1024];
    std::thread t(f, 3, buffer); 
    t.detach();
}
```
可能存在一个时机，函数oops，在局部变量`buffer`被转换为string之前退出，导致未定义的行为。
一个解决的方法是，在**将变量`buffer`传递给构造函数前，先转成string**
```cpp
std::thread t(f, 3, std::string(buffer));
```

考虑另外一个代码
```cpp
void update_data_for_widget(widget_id w, widget_data& data); // 1 

void oops_again(widget_id w){
    widget_data data; 
    std::thread t(update_data_for_widget, w, data); // 2
    display_status();
    t.join();
    process_widget_data(data); // 3
}
```
我们的函数`update_data_for_widget`的第二个参数是传入引用，但是thread的构造函数默认是复制传进去的，因此当线程完成后，副本销毁，给函数`process_widget_data`传入的参数data仍为修改。

我们需要`std:ref`来包装
```cpp
std::thread t(update_data_for_widget, w, std::ref(data));
```

此外我们可以传递一个成员函数的指针作为函数，前提是提供一个合时的对象指针
```cpp
class X{
public: 
    void do_lengthy_work();
};
X my_x; 
std::thread t(&X::do_lengthy_work, &my_x);
```
这段代码将在新线程调用函数`do_lengthy_work`，如果要给该函数传递参数，可以在thread的第三个参数开始传入成员函数的第一个参数

我们还可以以`移动move`的方式提供参数
```cpp
void process_big_object(std::unique_ptr<big_object>);
std::unique_ptr<big_object> p(new big_object);
p->prepare_data(42);
std::thread t(process_big_object, std::move(p));
```
在thread构造函数指定`std::move(p)`，让big_object的所有权转移到**新线程的内部存储中**，然后进入`process_big_object`

