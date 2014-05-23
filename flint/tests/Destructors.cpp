struct A {
        virtual void foo();
};

class B {
        ~B() {}
        virtual void foo();
};

class C {
public:
        ~C() {}
        virtual void foo();
};

struct D {};
struct E : virtual D {
  void foo();
};
