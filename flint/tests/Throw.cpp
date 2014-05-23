// Throw new checks
class MyException {

};

int main() {
	
	throw new MyException();

	throw new (MyException)();

	throw new MyException;

	try {} catch (int i) {}
	try {} catch (Foo i) {}

	try {}
	catch (Foo &i) {}

	try {}
	catch (Bar) {}
};