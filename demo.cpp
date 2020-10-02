// g++ demo.cpp && ./a.out

#include "GC.h"

class LinkedNode final : public GarbageCollected<LinkedNode> {
 public:
  LinkedNode(LinkedNode* next, int value) : next_(next), value_(value) {
    //std::cout << "Created LinkedNode - " << this << "\n";
  }
  ~LinkedNode() {
    //std::cout << "Deleted LinkedNode - " << this << "\n";
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
    std::cout << sizeof(LinkedNode) << "\n";
    heap.ShowStats();
    printf("HELLO\n");

    demo();

    heap.ShowStats();
    heap.GC();
    heap.ShowStats();

}

int main() {
    {
        GC_REGISTER_THREAD();
        main2();
        heap.GC();
        heap.ShowStats();
        heap.GC();
        heap.ShowStats();
    }

    std::cout << std::thread::hardware_concurrency() << " concurrent threads are supported.\n";

    return 0;
}
