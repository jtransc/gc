// https://v8.dev/blog/high-performance-cpp-gc
#include <stdio.h>
#include <iostream>
#include <vector>
#include <memory>
//#include "cppgc/allocation.h"
//#include "cppgc/member.h"
//#include "cppgc/garbage-collected.h"

namespace cppgc {
    class Visitor;

    class GarbageCollectedBase {
        public:
            int markVersion = 0;
            GarbageCollectedBase *next = nullptr;

            virtual void Trace(cppgc::Visitor* visitor) const {}
    };

    template <typename T>
    class GarbageCollected : public GarbageCollectedBase {
    };

    template <typename T>
    class BaseMember {
        public:
            BaseMember() { this->raw_ = nullptr; }
            BaseMember(const T* x) { this->raw_ = (T *)x; }
            BaseMember& operator= (const T* x) { return *this; }
            operator T*() { return raw_; }
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

    class Heap {
        public:
            int allocatedSize = 0;
            std::vector<GarbageCollectedBase*> roots;
            GarbageCollectedBase* head = nullptr;

            void AddRoot(GarbageCollectedBase* root) {
                roots.push_back(root);
            }

            void Sweep() {
                for (auto root : roots)  {
                    
                }
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

    class Visitor {
        public:
            template <typename T>
            void Trace(const BaseMember<T>& member) {
                std::cout << "Visitor.Trace: " << member.raw_;
            }
    };

};

cppgc::Heap heap;

class LinkedNode final : public cppgc::GarbageCollected<LinkedNode> {
 public:
  LinkedNode(LinkedNode* next, int value) : next_(next), value_(value) {}
  void Trace(cppgc::Visitor* visitor) const {
    visitor->Trace(next_);
  }
  cppgc::Member<LinkedNode> next_;
  int value_;
 private:
};

LinkedNode* CreateNodes() {
  LinkedNode* first_node = cppgc::MakeGarbageCollected<LinkedNode>(heap, nullptr, 1);
  LinkedNode* second_node = cppgc::MakeGarbageCollected<LinkedNode>(heap, first_node, 2);
  //LinkedNode* first_node = new LinkedNode(nullptr, 1);
  return second_node;
}

int main() {
    auto value = CreateNodes();
    auto value2 = (LinkedNode *)value->next_;
    value->Trace(new cppgc::Visitor());

    heap.AddRoot(value);

    std::cout << value << "\n";
    std::cout << value2 << "\n";
    std::cout << sizeof(cppgc::Member<LinkedNode>) << "\n";
    std::cout << sizeof(LinkedNode*) << "\n";
    std::cout << heap.allocatedSize << "\n";
    printf("HELLO\n");
    return 0;
}
