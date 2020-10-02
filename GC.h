// https://v8.dev/blog/high-performance-cpp-gc
#include <stdio.h>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <future>
#include <mutex>
#include <condition_variable>
#include <cassert>
#include <thread>
#include <chrono>
//#include "cppgc/allocation.h"
//#include "cppgc/member.h"
//#include "cppgc/garbage-collected.h"

struct Visitor;

struct GarbageCollectedBase {
    virtual int size() { return 0; }
    int markVersion = 0;
    GarbageCollectedBase *next = nullptr;

    virtual void Trace(Visitor* visitor) const {}

    GarbageCollectedBase() {
        #if __TRACE_GC
        std::cout << "Created GarbageCollectedBase - " << this << "\n";
        #endif
    }

    virtual ~GarbageCollectedBase() {
        #if __TRACE_GC
        std::cout << "Deleted GarbageCollectedBase - " << this << "\n";
        #endif
    }
};

template <typename T>
struct GarbageCollected : public GarbageCollectedBase {
    int size() override {
    return sizeof(T);
    }
};

template <typename T>
struct BaseMember {
    BaseMember() { this->raw_ = nullptr; }
    BaseMember(const T* x) { this->raw_ = (T *)x; }
    BaseMember& operator= (const T* x) { return *this; }
    operator T*() const { return raw_; }
    mutable T* raw_ = nullptr;
};

template <typename T>
struct Member : public BaseMember<T> {
    Member() : BaseMember<T>() {}
    Member(const T* x) : BaseMember<T>(x) {}
};

template <typename T>
struct WeakMember : public BaseMember<T> {
    WeakMember() : BaseMember<T>() {}
    WeakMember(const T* x) : BaseMember<T>(x) {}
};

struct Visitor {
    int version = 1;

    template <typename T>
    void Trace(const Member<T>& member) {
        Trace((GarbageCollectedBase *)(T *)member);
    }

    virtual void Trace(GarbageCollectedBase *obj) {
        if (obj == nullptr) return;
        if (obj->markVersion == version) return;
        obj->markVersion = version;
        obj->Trace(this);
        //std::cout << "Visitor.Trace: " << obj;
    }
};

struct Stack {
    void **start = nullptr;
    std::mutex mutex;
    std::mutex mutex2;
    std::condition_variable cv;
    std::condition_variable cv2;
    std::promise<int> locked;
    Stack(void **start) : start(start) { }
};

struct Heap {
    int allocatedSize = 0;
    int allocatedCount = 0;
    std::unordered_set<GarbageCollectedBase*> allocated;
    std::unordered_set<GarbageCollectedBase*> roots;
    std::unordered_map<std::thread::id, Stack*> threads_to_stacks;
    GarbageCollectedBase* head = nullptr;
    Visitor visitor;
    std::atomic<bool> sweepingStop;

    void ShowStats() {
        std::cout << "Heap Stats. Object Count: " << allocatedCount << ", TotalSize: " << allocatedSize << "\n";
    }

    void AddRoot(GarbageCollectedBase* root) {
        roots.insert(root);
    }

    void RemoveRoot(GarbageCollectedBase* root) {
        roots.erase(root);
    }

    void RegisterCurrentThread() {
        void *ptr = nullptr;
        RegisterThreadInternal(new Stack(&ptr));
    }

    void UnregisterCurrentThread() {
        auto current_thread_id = std::this_thread::get_id();
        #if __TRACE_GC
        std::cout << "UnregisterCurrentThread:" << current_thread_id << "\n";
        #endif
        threads_to_stacks.erase(current_thread_id);
    }

    // Call at some points to perform a stop-the-world
    void CheckCurrentThread() {
        if (sweepingStop) {
            auto stack = threads_to_stacks[std::this_thread::get_id()];
            stack->cv.notify_all();
            std::unique_lock<std::mutex> lk(stack->mutex2);
            stack->cv2.wait(lk);
            //stack->cv.wait(stack->mutex);
        }
    }

    void RegisterThreadInternal(Stack *stack) {
        auto current_thread_id = std::this_thread::get_id();
        #if __TRACE_GC
        std::cout << "RegisterThreadInternal:" << current_thread_id << "\n";
        #endif
        threads_to_stacks[current_thread_id] = stack;
    }
    

    void Mark() {
        visitor.version++;
        for (auto root : roots)  {
            visitor.Trace(root);
        }
        for (auto tstack : threads_to_stacks) {
            CheckStack(tstack.second);
        }
    }

    void Sweep() {
        sweepingStop = true;
        auto currentThreadId = std::this_thread::get_id();
        for (auto tstack : threads_to_stacks) {
            if (tstack.first != currentThreadId) {
                std::unique_lock<std::mutex> lk(tstack.second->mutex);
                if (tstack.second->cv.wait_for(lk, std::chrono::seconds(10)) == std::cv_status::timeout) {
                    std::cout << "Thread was locked for too much time. Aborting.\n";
                    abort();
                }
            }
        }

        int version = visitor.version;
        bool reset = version >= 1000;
        GarbageCollectedBase* prev = nullptr;
        GarbageCollectedBase* current = head;
        GarbageCollectedBase* todelete = nullptr;
        while (current != nullptr) {
            if (current->markVersion != version) {
                todelete = current;
                allocated.erase(todelete);
                //std::cout << "delete unreferenced object!\n";
                if (prev != nullptr) {
                    prev->next = current->next;
                    current = prev;
                } else {
                    //std::cout << "head=" << head << ", current=" << current << "\n";
                    assert(head == current);
                    head = current->next;
                }
                current = current->next;
                allocatedSize -= todelete->size();
                allocatedCount--;
                delete todelete;
            } else {
                if (reset) {
                   current->markVersion = 0;
                }
                prev = current;
                current = current->next;
            }
        }
        if (reset) {
            version = 0;
        }
        prev = nullptr;
        current = nullptr;
        todelete = nullptr;

        // Resume all threads
        {
            sweepingStop = false;
            for (auto tstack : threads_to_stacks) {
                if (tstack.first != currentThreadId) {
                    tstack.second->cv2.notify_all();
                }
            }
        }
    }

    void CheckStack(Stack *stack) {
        auto start = stack->start;
        void *value = nullptr;

        for (void **ptr = &value; ptr < start; ptr++) {
            void *value = *ptr;
            if (value > (void *)0x10000) {
                if (allocated.find((GarbageCollectedBase*)value) != allocated.end()) {
                    visitor.Trace((GarbageCollectedBase*)value);
                }
                //std::cout << value << "\n";
            }
        }
    }

    void GC() {
        Mark();
        Sweep();
    }

    template <typename T, typename... Args>
    T* MakeGarbageCollected(Args&&... args) {
        void *memory = malloc(sizeof(T));
        T *result = ::new (memory) T(std::forward<Args>(args)...);
        allocated.insert(result);
        this->allocatedSize += sizeof(T);
        this->allocatedCount++;
        result->next = (GarbageCollectedBase*)this->head;
        this->head = result;
        return result;
    };
};

Heap heap;

struct GcThread {
    GcThread() {
        heap.RegisterCurrentThread();
        #if __TRACE_GC
        std::cout << "GcThread\n";
        #endif
    }
    ~GcThread() {
        heap.UnregisterCurrentThread();
        #if __TRACE_GC
        std::cout << "~GcThread\n";
        #endif
    }
};

#define GC_REGISTER_THREAD() GcThread __current_gc_thread;