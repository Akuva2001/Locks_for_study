#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdio>
#include <time.h>
#include <mutex>
#include <unistd.h>
#include <string>

//compile: g++ main.cpp -lpthread

class Locker{
public:
    virtual void lock() = 0;
    virtual void unlock() = 0;
};

class Mutex_locker:public Locker{
    std::mutex mut;
public:
    void lock(){mut.lock();}
    void unlock(){mut.unlock();}
};

class TAS:public Locker{
    std::atomic<bool> locked {false};
public:
    void lock() {
        bool flag = false;
        while (!locked.compare_exchange_strong(flag, true, std::memory_order_acquire, std::memory_order_relaxed)){
            flag = false;
        }
    }
    void unlock() {
        locked.store(false, std::memory_order_release);
    }
};

class TTAS:public Locker{
    std::atomic<bool> locked{false};
public:
    void lock() {
        bool flag = false;
        do {
            while (locked.load(std::memory_order_acquire)) {
            }
            flag = false;
        } while (!locked.compare_exchange_weak(flag, true, std::memory_order_acquire, std::memory_order_relaxed));
    }
    void unlock() {
        locked.store(false, std::memory_order_release);
    }
};


class Backoff{
    unsigned long long yield_quantity = 1;
public:
    void operator ()(){
        for (int i=0; i < yield_quantity; ++i)
            std::this_thread::yield();
        yield_quantity*=2;
    }
    void ok(){
        yield_quantity = 1;
    }

};

class TTAS_based_Spinlock_with_short_backoff: public Locker{
    std::atomic<bool> locked {false};
    Backoff backoff;
public:
    void lock() {
        bool flag = false;
        do {
            while (locked.load(std::memory_order_acquire)) {
                backoff();
                backoff.ok();
            }
            flag = false;
        } while (!locked.compare_exchange_weak(flag, true, std::memory_order_acquire, std::memory_order_relaxed));
    }
    void unlock() {
        locked.store(false, std::memory_order_release);
    }
};


class TTAS_based_Spinlock_with_short_exp_backoff:public Locker{
    std::atomic<bool> locked {false};
    Backoff backoff;
public:
    void lock() {
        bool flag = false;
        do {
            while (locked.load(std::memory_order_acquire)) {
                backoff();
            }
            flag = false;
            backoff.ok();
        } while (!locked.compare_exchange_weak(flag, true, std::memory_order_acquire, std::memory_order_relaxed));
        backoff.ok();
    }
    void unlock() {
        locked.store(false, std::memory_order_release);
    }
};

class TTAS_based_Spinlock_with_long_exp_backoff:public Locker{
    std::atomic<bool> locked {false};
    Backoff backoff;
public:
    void lock() {
        bool flag = false;
        do {
            while (locked.load(std::memory_order_acquire)) {
                backoff();
            }
            flag = false;
        } while (!locked.compare_exchange_weak(flag, true, std::memory_order_acquire, std::memory_order_relaxed));
        backoff.ok();
    }
    void unlock() {
        locked.store(false, std::memory_order_release);
    }
};

class Ticket_Lock_without_backoff:public Locker{
    std::atomic<unsigned long long> current {0}, next {0};
public:
    void lock() {
        unsigned long long my_ticket = next.fetch_add(1);
        while (current.load(std::memory_order_acquire) != my_ticket);
    }
    void unlock() {
        current.fetch_add(1, std::memory_order_release);
    }
};


class Backoff_for_ticket{
    unsigned long long yield_quantity = 1;
public:
    void operator ()(){
        for (int i=0; i < yield_quantity; ++i) {
            std::this_thread::yield();
            __asm volatile ("pause");
        }
        yield_quantity*=2;
    }
    void ok(){
        yield_quantity = 1;
    }

};


class Ticket_Lock_with_short_backOff:public Locker{
    std::atomic<unsigned long long> current {0}, next{0};
    Backoff_for_ticket backoff;
public:
    void lock() {
        unsigned long long my_ticket = next.fetch_add(1);
        while (current.load(std::memory_order_acquire) != my_ticket){
            backoff();
            backoff.ok();
        }
    }
    void unlock() {
        current.fetch_add(1, std::memory_order_release);
    }
};

class Ticket_Lock_with_exp_backOff:public Locker{
    std::atomic<unsigned long long> current {0}, next{0};
    Backoff_for_ticket backoff;
public:
    void lock() {
        unsigned long long my_ticket = next.fetch_add(1);
        while (current.load(std::memory_order_acquire) != my_ticket){
            backoff();
        }
        backoff.ok();
    }
    void unlock() {
        current.fetch_add(1, std::memory_order_release);
    }
};


void fun(long long int * val, Locker *mut, int k = 10000){
    for (int i=0; i<k; ++i){
        mut->lock();
        (*val)++;
        mut->unlock();
    }
}

unsigned long test(Locker& mut, unsigned long long N = 100, unsigned long long k = 10000){
    std::vector<std::thread> th_arr(N);
    long long int val = 0;

    mut.lock();
    for (int i = 0; i < N; ++i) {
        th_arr[i] = std::thread(fun, &val, &mut, k);
    }

    unsigned long begin = clock();
    mut.unlock();


    for (int i = 0; i < N; ++i) {
        th_arr[i].join();
    }

    unsigned long end = clock();
    printf("%lu ms\n", (end - begin) * 1000 / CLOCKS_PER_SEC);

    if (val!=k*N) printf("Wrong, val = %lld,\n right = %lld\n", val, (long long)k*N);
    return (end - begin) * 1000 / CLOCKS_PER_SEC;
}

int main() {
    std::cout << "Hello, World!" << std::endl;
    if (true) {
        printf("At first, test working of all lockers (except of two too long).\n");
        printf("100 threads do ++val, each thread 10 000 times.\n");
        {
            Mutex_locker mut;
            printf("Mutex_locker\n");
            test(mut);
        }
        {
            TAS mut;
            printf("TAS\n");
            test(mut);
        }
        {
            TTAS mut;
            printf("TTAS\n");
            test(mut);
        }
        {
            TTAS_based_Spinlock_with_short_backoff mut;
            printf("TTAS_based_Spinlock_with_short_backoff\n");
            test(mut);
        }
        {
            TTAS_based_Spinlock_with_short_exp_backoff mut;
            printf("TTAS_based_Spinlock_with_short_exp_backoff\n");
            test(mut);
        }
        {
            TTAS_based_Spinlock_with_long_exp_backoff mut;
            printf("TTAS_based_Spinlock_with_long_exp_backoff\n");
            test(mut);
        }
        /*{//it works too long
            Ticket_Lock_without_backoff mut;
            printf("Ticket_Lock_without_backoff\n");
            test(mut);
        }*/
        {//it's still too long
            Ticket_Lock_with_short_backOff mut;
            printf("Ticket_Lock_with_short_backOff\n");
            printf("Yes, it's very long\n");
            test(mut);
        }
        /*{//this is very long
            Ticket_Lock_with_exp_backOff mut;
            printf("Ticket_Lock_with_exp_backOff\n");
            test(mut);
        }*/
    }
    printf("##############################\n");
    printf("Let's plot graph\n");
    printf("the best are TTAS_based_Spinlock_with_short_exp_backoff and Mutex\n");
    std::string output = std::string("[");
    for (int i=0; i<202; i+=10){
        unsigned long long N = (i==0)?1:i;
        unsigned long long k = 7560*2*100/N;
        {
            Mutex_locker mut;
            printf("Mutex_locker\n");
            unsigned long res = test(mut, N, k);
            output+=std::string("(")+std::to_string(N)+std::string(",")+std::to_string(res)+std::string(",");
        }
        {
            TTAS mut;
            printf("TTAS\n");
            unsigned long res = test(mut, N, k);
            output+=std::to_string(res)+",";
        }
        {
            TTAS_based_Spinlock_with_short_exp_backoff mut;
            printf("TTAS_based_Spinlock_with_short_exp_backoff\n");
            unsigned long res = test(mut, N, k);
            output += std::to_string(res)+std::string("),");
        }

    }
    output+=std::string("]");
    printf("##############################\n");
    std::cout<<output<<std::endl;
    printf("that array could be past to plot.py\n");
    return 0;
}
