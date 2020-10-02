// https://v8.dev/blog/high-performance-cpp-gc
#include <stdio.h>
#include <iostream>
#include <vector>
#include <unordered_set>
#include <memory>
#include <cassert>
//#include "cppgc/allocation.h"
//#include "cppgc/member.h"
//#include "cppgc/garbage-collected.h"

class Visitor;

class GarbageCollectedBase {
    public:
        virtual int size() { return 0; }
        int markVersion = 0;
        GarbageCollectedBase *next = nullptr;

        virtual void Trace(Visitor* visitor) const {}

        GarbageCollectedBase() {
            std::cout << "Created GarbageCollectedBase - " << this << "\n";
        }

        virtual ~GarbageCollectedBase() {
            std::cout << "Deleted GarbageCollectedBase - " << this << "\n";
        }
};

template <typename T>
class GarbageCollected : public GarbageCollectedBase {
    public:
        int size() override {
        return sizeof(T);
        }
};

template <typename T>
class BaseMember {
    public:
        BaseMember() { this->raw_ = nullptr; }
        BaseMember(const T* x) { this->raw_ = (T *)x; }
        BaseMember& operator= (const T* x) { return *this; }
        operator T*() const { return raw_; }
        mutable T* raw_ = nullptr;
};

template <typename T>
class Member : public BaseMember<T> {
    public:
        Member() : BaseMember<T>() {}
        Member(const T* x) : BaseMember<T>(x) {}
};

template <typename T>
class WeakMember : public BaseMember<T> {
    public:
        WeakMember() : BaseMember<T>() {}
        WeakMember(const T* x) : BaseMember<T>(x) {}
};

class Visitor {
    public:
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

class Stack {
    public:
        void **start = nullptr;
        Stack(void **start) : start(start) { }
};

class Heap {
    public:
        int allocatedSize = 0;
        int allocatedCount = 0;
        std::unordered_set<GarbageCollectedBase*> allocated;
        std::unordered_set<Stack*> stacks;
        std::unordered_set<GarbageCollectedBase*> roots;
        GarbageCollectedBase* head = nullptr;
        Visitor visitor;

        void ShowStats() {
            std::cout << "Heap Stats. Object Count: " << allocatedCount << ", TotalSize: " << allocatedSize << "\n";
        }

        void AddRoot(GarbageCollectedBase* root) {
            roots.insert(root);
        }

        void RemoveRoot(GarbageCollectedBase* root) {
            roots.erase(root);
        }

        void AddStack(Stack* root) {
            stacks.insert(root);
        }

        void RemoveStack(Stack* root) {
            stacks.erase(root);
        }

        void Mark() {
            visitor.version++;
            for (auto root : roots)  {
                visitor.Trace(root);
            }
            for (auto stack : stacks) {
                CheckStack(stack);
            }
        }

        void Sweep() {
            int version = visitor.version;
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
                    prev = current;
                    current = current->next;
                }
            }
            prev = nullptr;
            current = nullptr;
            todelete = nullptr;
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
