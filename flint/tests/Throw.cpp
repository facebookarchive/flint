// Throw new checks
class MyException {

};

int main() {
	
	throw new MyException();

	throw new (MyException)();

	throw new MyException;
};