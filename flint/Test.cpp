// DO NOT COMPILE ME!!!

#include <string>
#include <cstdlib>

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

		struct FooU {
			void Bar() throw () {};

			/* implicit */ FooU(int i) {};
			FooU(string i) {};
		};

		Foo(int i) {};
		Foo() {};

		Foo(void) {};

		Foo(const Foo&& other);
		Foo(Foo &other);
	};

	
};

int main() {
	
	// Memset Checks
	GRR::Foo *ptr;
	memset(ptr, sizeof(GRR::Foo), 1);

	memset(ptr, 0, sizeof(GRR::Foo));

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