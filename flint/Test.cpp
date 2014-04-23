// DO NOT COMPILE ME!!!

using namespace std;

// Define Warning Checks
#define __BAD_DEFINE
#define OKAY_DEFINE

// Function Throw Checks
void funcTest() throw () {};

namespace GRR {
	
	class Foo {
	private:

		void Bar() throw () {};
		void Bar2() {};

	public:

		Foo(int i) {};
		Foo() {};

		Foo(void) {};

		Foo(const Foo &other);
		Foo(Foo &other);
	};

	struct FooU {
		void Bar() throw () {};
	};
};

int main() {
	
	// Postfix Advice Checks
	int i = 0;
	i--;
	++i;
	for (i = 0; i < 10; i++) {}

	// Try Catch Checks
	try {

	}
	catch (int e) {

	}

	// #if Checks
#if

#ifdef A

#endif

#else

	return 0;
};