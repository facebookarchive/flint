#include <exception>

using namespace std;

struct Ex1 : exception {};
struct Ex2 : private exception {};
class Ex3 : exception {};
class Ex4 : private exception {};
class Ex5 : public exception {};
struct Ex6 : public exception {};
struct Ex7 : public std::exception {};
struct Ex8 : private std::exception {};
class Ex9 : Ex5, private Ex7, exception {};
