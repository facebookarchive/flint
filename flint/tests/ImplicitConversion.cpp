// DO NOT COMPILE ME!!!

namespace GRR {
	
	class Foo {
	public:

		struct FooU {

			explicit operator char();

			/* implicit */ operator int();
		};

		operator bool();
	};

	
};