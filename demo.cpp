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
  void __GC_Trace(Visitor* visitor) const {
    visitor->Trace(next_);
  }
  Member<LinkedNode> next_;
  int value_;
 private:
};

LinkedNode* CreateNodes() {
  LinkedNode* first_node = heap.Alloc<LinkedNode>(nullptr, 1);
  LinkedNode* second_node = heap.Alloc<LinkedNode>(first_node, 2);
  //LinkedNode* first_node = new LinkedNode(nullptr, 1);
  return second_node;
}

void demo() {
    heap.Alloc<LinkedNode>(nullptr, 3);
    heap.Alloc<LinkedNode>(nullptr, 3);
    heap.Alloc<LinkedNode>(nullptr, 3);
    std::cout << "ALLOCATED 3 more\n";
}

void main2() {
    LinkedNode* a = heap.Alloc<LinkedNode>(nullptr, 3);
    LinkedNode* b = heap.Alloc<LinkedNode>(nullptr, 3);
    auto value = CreateNodes();
    auto value2 = (LinkedNode *)value->next_;
    //value->__GC_Trace(new Visitor());
    LinkedNode* c = heap.Alloc<LinkedNode>(nullptr, 3);
    LinkedNode* d = heap.Alloc<LinkedNode>(nullptr, 3);
    std::cout << "ALLOCATED 6\n";

    for (int n = 0; n < 1000000; n++) {
        heap.Alloc<LinkedNode>(nullptr, 3);
    }

    //heap.AddRoot(value);

    /*
    std::cout << value << "\n";
    std::cout << value2 << "\n";
    std::cout << sizeof(Member<LinkedNode>) << "\n";
    std::cout << sizeof(LinkedNode*) << "\n";
    std::cout << sizeof(LinkedNode) << "\n";
    */
    heap.ShowStats();

    demo();

    heap.ShowStats();
    heap.GC();
    printf("Pointer to D: %p\n", d);
    heap.ShowStats();

}

int main() {
    GC_REGISTER_THREAD();
    {
        main2();
        heap.GC();
        heap.ShowStats();
        heap.GC();
        heap.ShowStats();
    }
    {
        heap.GC();
        heap.ShowStats();
    }

    std::cout << std::thread::hardware_concurrency() << " concurrent threads are supported.\n";

    return 0;
}
