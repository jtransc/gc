// g++ demo.cpp && ./a.out

//#define __TRACE_GC 1

#include "GC.h"

class LinkedNode final : public __GC {
 public:
  LinkedNode(LinkedNode* next, int value) : next_(next), value_(value) {
    //std::cout << "Created LinkedNode - " << this << "\n";
  }
  ~LinkedNode() {
    //std::cout << "Deleted LinkedNode - " << this << "\n";
  }
  int __GC_Size() { return sizeof(LinkedNode); }
  void __GC_Trace(__GCVisitor* visitor) const {
    visitor->Trace(next_);
  }
  __GCMember<LinkedNode> next_;
  int value_;
 private:
};

LinkedNode* CreateNodes() {
  LinkedNode* first_node = __GC_ALLOC<LinkedNode>(nullptr, 1);
  LinkedNode* second_node = __GC_ALLOC<LinkedNode>(first_node, 2);
  //LinkedNode* first_node = new LinkedNode(nullptr, 1);
  return second_node;
}

void demo() {
    __GC_ALLOC<LinkedNode>(nullptr, 3);
    __GC_ALLOC<LinkedNode>(nullptr, 3);
    __GC_ALLOC<LinkedNode>(nullptr, 3);
    std::cout << "ALLOCATED 3 more\n";
}

void main2() {
    LinkedNode* a = __GC_ALLOC<LinkedNode>(nullptr, 3);
    LinkedNode* b = __GC_ALLOC<LinkedNode>(nullptr, 3);
    auto value = CreateNodes();
    auto value2 = (LinkedNode *)value->next_;
    //value->__GC_Trace(new Visitor());
    LinkedNode* c = __GC_ALLOC<LinkedNode>(nullptr, 3);
    LinkedNode* d = __GC_ALLOC<LinkedNode>(nullptr, 3);
    std::cout << "ALLOCATED 6\n";

    int allocationCount = 1000000;
    for (int n = 0; n < allocationCount; n++) {
        __GC_ALLOC<LinkedNode>(nullptr, 3);
    }
    std::cout << "ALLOCATED-DISCARDED " << allocationCount << "\n";

    //__GC_ADD_ROOT(value);

    /*
    std::cout << value << "\n";
    std::cout << value2 << "\n";
    std::cout << sizeof(Member<LinkedNode>) << "\n";
    std::cout << sizeof(LinkedNode*) << "\n";
    std::cout << sizeof(LinkedNode) << "\n";
    */
   __GC_SHOW_STATS();

    demo();

   __GC_SHOW_STATS();
   __GC_GC();
    printf("Pointer to D: %p\n", d);
   __GC_SHOW_STATS();

}

int main() {
    __GC_REGISTER_THREAD();
    {
        main2();
        __GC_GC();
        __GC_SHOW_STATS();
        __GC_GC();
        __GC_SHOW_STATS();
    }
    {
        __GC_GC();
        __GC_SHOW_STATS();
    }

    std::cout << std::thread::hardware_concurrency() << " concurrent threads are supported.\n";

    return 0;
}
