// PR c++/9782

extern "C" void printf(char *, ...);

template <int>
struct A {
  A() {printf("A::A()\n");}
};


struct B {
  B() {printf("B::B()\n");}
};


int main () {
  new A<0>[1][1];
  new B   [1][1];
}
