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
        void Trace(const BaseMember<T>& member) {
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
        std::vector<GarbageCollectedBase*> roots;
        GarbageCollectedBase* head = nullptr;
        Visitor visitor;

        void AddRoot(GarbageCollectedBase* root) {
            roots.push_back(root);
        }

        void Mark() {
            visitor.version++;
            for (auto root : roots)  {
                visitor.Trace(root);
            }
        }

        void Sweep(std::unordered_set<void *>& pointers) {
            int version = visitor.version;
            GarbageCollectedBase* prev = nullptr;
            GarbageCollectedBase* current = head;
            GarbageCollectedBase* todelete = nullptr;
            while (current != nullptr) {
                if (current->markVersion != version && pointers.find((void *)current) == pointers.end()) {
                    todelete = current;
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
                    delete todelete;
                } else {
                    prev = current;
                    current = current->next;
                }
            }
        }

        void CheckStack(Stack *stack, std::unordered_set<void *>& pointers) {
            auto start = stack->start;
            void *value = nullptr;

            for (void **ptr = &value; ptr < start; ptr++) {
                void *value = *ptr;
                if (value > (void *)0x10000) {
                    std::cout << value << "\n";
                    pointers.insert(value);
                }
            }
        }

        void GC() {
            std::unordered_set<void *> pointers;
            CheckStack(mainStack, pointers);

            Mark();
            Sweep(pointers);
        }
};

template <typename T, typename... Args>
T* MakeGarbageCollected(Heap &heap, Args&&... args) {
    void *memory = malloc(sizeof(T));
    T *result = ::new (memory) T(std::forward<Args>(args)...);
    heap.allocatedSize += sizeof(T);
    result->next = (GarbageCollectedBase*)heap.head;
    heap.head = result;
    return result;
};

Heap heap;

class LinkedNode final : public GarbageCollected<LinkedNode> {
 public:
  LinkedNode(LinkedNode* next, int value) : next_(next), value_(value) {}
  void Trace(Visitor* visitor) const {
    visitor->Trace(next_);
  }
  Member<LinkedNode> next_;
  int value_;
 private:
};

LinkedNode* CreateNodes() {
  LinkedNode* first_node = MakeGarbageCollected<LinkedNode>(heap, nullptr, 1);
  LinkedNode* second_node = MakeGarbageCollected<LinkedNode>(heap, first_node, 2);
  //LinkedNode* first_node = new LinkedNode(nullptr, 1);
  return second_node;
}

void demo() {
    MakeGarbageCollected<LinkedNode>(heap, nullptr, 3);
    MakeGarbageCollected<LinkedNode>(heap, nullptr, 3);
    MakeGarbageCollected<LinkedNode>(heap, nullptr, 3);
}

void main2() {

    LinkedNode* a = MakeGarbageCollected<LinkedNode>(heap, nullptr, 3);
    LinkedNode* b = MakeGarbageCollected<LinkedNode>(heap, nullptr, 3);
    auto value = CreateNodes();
    auto value2 = (LinkedNode *)value->next_;
    value->Trace(new Visitor());

    LinkedNode* c = MakeGarbageCollected<LinkedNode>(heap, nullptr, 3);
    LinkedNode* d = MakeGarbageCollected<LinkedNode>(heap, nullptr, 3);

    heap.AddRoot(value);

    std::cout << value << "\n";
    std::cout << value2 << "\n";
    std::cout << sizeof(Member<LinkedNode>) << "\n";
    std::cout << sizeof(LinkedNode*) << "\n";
    std::cout << "allocatedSize: " << heap.allocatedSize << "\n";
    printf("HELLO\n");

    demo();

    heap.GC();
    std::cout << "allocatedSize: " << heap.allocatedSize << "\n";

}

int main() {
    void *ptr = nullptr;
    mainStack = new Stack(&ptr);
    std::cout << "stackStart: " << mainStack->start << "\n";

    main2();

    return 0;
}
