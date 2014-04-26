// DO NOT COMPILE ME!!!

#include <string>

using namespace std;

int main() {
	
	// Memset Checks
	int *ptr;
	memset(ptr, 4, 0);

	memset(ptr, sizeof(int), 1);

	memset(ptr, 0, sizeof(int));

	return 0;
};