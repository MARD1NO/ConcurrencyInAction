#include "iostream"
#include "thread"
#include "vector"
#include<algorithm>
#include "functional"


void f(int i){
    std::cout<<"i is: "<<i<<std::endl;
}

int main(){
    std::vector<std::thread> threads; 
    for(int i=0; i < 5; i++){
        threads.push_back(std::thread(f, i));
    }
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));    
    return 0;
}