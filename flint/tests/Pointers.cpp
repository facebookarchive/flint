// DO NOT COMPILE ME!!!

#include <memory>

using namespace std;

struct Foo {
	Foo() {};
	explicit Foo(int i) {};
};

int main() {

	// The good
	std::unique_ptr<int> a(new int);
	unique_ptr<Foo> b(new Foo);

	std::unique_ptr<int[]> c(new int[2]);
	unique_ptr<Foo[]> d(new Foo[8]);
	
	// The bad
	std::unique_ptr<int[]> c(new int);
	unique_ptr<Foo> d(new Foo[8]);

	// The ugly...
	std::unique_ptr<int[]> c(new int(42));
	unique_ptr<Foo> d(new Foo[]());

	return 0;
};