#ifndef NAMESPACE_HPP
#define NAMESPACE_HPP

#include <iostream>

using namespace std;

namespace Named {
  static int x = 5;
  static const int y = 9;

  namespace Good {
    int x = 6;
    const int y = 15;
  }
};

namespace {
  static int a = 11;
  static const int b = 15;

  namespace InnerNamed {
    static int s = 8;

    static bool testFunction() {
      using namespace std; // This one is safe
      return false;
    };
  }
};

namespace {
  int c = 13;
  const int d = 7;
};

int foo()
{
  static int p = 7;

  return Named::x + Named::y + a + b + Named::Good::x + Named::Good::y + c + d + p + InnerNamed::s;
};

#endif
