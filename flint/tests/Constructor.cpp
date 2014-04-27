// DO NOT COMPILE ME!!!

// Function Throw Checks
void func();
void funcTest() throw () {};

namespace GRR {
	
	class Foo : protected Bar {
	private:

		void Bar() throw () {};
		void Bar2() {};

	public:

		struct FooU {
			void Bar() throw () {};

			/* implicit */ FooU(int i) {};
			FooU(char i) {};
		};

		Foo(int i) {};
		Foo() {};

		Foo(void) {};

		Foo(const Foo&& other);
		Foo(Foo &other);
	};

};