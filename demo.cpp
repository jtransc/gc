// g++ demo.cpp && ./a.out

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

        ~GarbageCollectedBase() {
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

Stack *mainStack;

class Heap {
    public:
        int allocatedSize = 0;
        int allocatedCount = 0;
        std::unordered_set<GarbageCollectedBase*> allocated;
        std::vector<GarbageCollectedBase*> roots;
        GarbageCollectedBase* head = nullptr;
        Visitor visitor;

        void ShowStats() {
            std::cout << "Heap Stats. Object Count: " << allocatedCount << ", TotalSize: " << allocatedSize << "\n";
        }

        void AddRoot(GarbageCollectedBase* root) {
            roots.push_back(root);
        }

        void Mark() {
            visitor.version++;
            for (auto root : roots)  {
                visitor.Trace(root);
            }
            CheckStack(mainStack);
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

class LinkedNode final : public GarbageCollected<LinkedNode> {
 public:
  LinkedNode(LinkedNode* next, int value) : next_(next), value_(value) {
    std::cout << "Created LinkedNode - " << this << "\n";
  }
  ~LinkedNode() {
    std::cout << "Deleted LinkedNode - " << this << "\n";
  }
  void Trace(Visitor* visitor) const {
    visitor->Trace(next_);
  }
  Member<LinkedNode> next_;
  int value_;
 private:
};

LinkedNode* CreateNodes() {
  LinkedNode* first_node = heap.MakeGarbageCollected<LinkedNode>(nullptr, 1);
  LinkedNode* second_node = heap.MakeGarbageCollected<LinkedNode>(first_node, 2);
  //LinkedNode* first_node = new LinkedNode(nullptr, 1);
  return second_node;
}

void demo() {
    heap.MakeGarbageCollected<LinkedNode>(nullptr, 3);
    heap.MakeGarbageCollected<LinkedNode>(nullptr, 3);
    heap.MakeGarbageCollected<LinkedNode>(nullptr, 3);
}

void main2() {

    LinkedNode* a = heap.MakeGarbageCollected<LinkedNode>(nullptr, 3);
    LinkedNode* b = heap.MakeGarbageCollected<LinkedNode>(nullptr, 3);
    auto value = CreateNodes();
    auto value2 = (LinkedNode *)value->next_;
    value->Trace(new Visitor());

    LinkedNode* c = heap.MakeGarbageCollected<LinkedNode>(nullptr, 3);
    LinkedNode* d = heap.MakeGarbageCollected<LinkedNode>(nullptr, 3);

    //heap.AddRoot(value);

    std::cout << value << "\n";
    std::cout << value2 << "\n";
    std::cout << sizeof(Member<LinkedNode>) << "\n";
    std::cout << sizeof(LinkedNode*) << "\n";
    heap.ShowStats();
    printf("HELLO\n");

    demo();

    heap.ShowStats();
    heap.GC();
    heap.ShowStats();

}

int main() {
    void *ptr = nullptr;
    mainStack = new Stack(&ptr);
    std::cout << "stackStart: " << mainStack->start << "\n";

    main2();

    heap.GC();
    heap.ShowStats();
    heap.GC();
    heap.ShowStats();

    return 0;
}
