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
	
	std::shared_ptr<Foo> sh1;

	std::lock_guard<std::mutex> lock(g_i_mutex);
	lock_guard<mutex> lock2(g_i_mutex);

	// The bad
	std::unique_ptr<int[]> c(new int);
	unique_ptr<Foo> d(new Foo[8]);

	// The ugly...
	std::unique_ptr<int[]> c(new int(42));
	unique_ptr<Foo> d(new Foo[]());

	shared_ptr<int> ptr3(new Foo, &CustomDeleter, &CustomAllocator);
	std::shared_ptr<Foo> sh2(new Foo);
	boost::shared_ptr<Foo> sh3(new sh2);
	std::shared_ptr<Foo> sh4(new Foo, D(), A());

	std::lock_guard<std::mutex>(g_i_mutex);
	lock_guard<mutex> (g_i_mutex);

	return 0;
};