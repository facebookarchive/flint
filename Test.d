// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

import std.array, std.conv, std.exception, std.random, std.stdio, std.file;
import Checks, Tokenizer, FileCategories;

unittest {
  EXPECT_EQ(FileCategory.header, getFileCategory("foo.h"));
  EXPECT_EQ(FileCategory.header, getFileCategory("baz/bar/foo.h"));
  EXPECT_EQ(FileCategory.inl_header, getFileCategory("foo-inl.h"));
  EXPECT_EQ(FileCategory.source_cpp, getFileCategory("foo.cpp"));
  EXPECT_EQ(FileCategory.source_c, getFileCategory("foo.c"));
  EXPECT_EQ(FileCategory.unknown, getFileCategory("foo"));

  assert(isHeader("foo.h"));
  assert(!isHeader("foo.cpp"));
  assert(!isHeader("foo.c"));

  assert(!isSource("foo.h"));
  assert(isSource("foo.cpp"));
  assert(isSource("foo.c"));

  assert(!isTestFile("foo.h"));
  assert(isTestFile("testFoo.h"));
  assert(isTestFile("/test/foo.h"));

  EXPECT_EQ("foo", getFileNameBase("foo.h"));
  EXPECT_EQ("foo", getFileNameBase("foo.cpp"));
  EXPECT_EQ("foo", getFileNameBase("foo.c"));
}

unittest {
  string s = "
int main()
  __attribute__((foo))

  // Two on one line:
  __attribute__ ((foo)) __attribute__((foo))

  // Two parameters:
  __attribute__ ( ( foo, bar ) )

  // Here, we ding only the 'format' keyword.
  __attribute__((format (printf, 1, 2)))

  // Here, now with the proper __format__, we ding the format
  // sub-type, 'printf':
  __attribute__((__format__ (printf, 1, 2)))

  // This one is fine.
  __attribute__((__format__ (__printf__, 1, 2)))
{
}
";
  auto tokens = tokenize(s, "nofile.cpp");
  EXPECT_EQ(checkAttributeArgumentUnderscores("nofile.cpp", tokens), 6);
}

unittest {
  string s = "
#include <string>
// Some code does this, (good!), and someday we may want to exempt
// such usage from this lint rule.
#define strncpy(d,s,n) do_not_use_this_function
int main() {
  // Using strncpy(a,b,c) in a comment does not provoke a warning.
  char buf[10];
  strncpy(buf, \"foo\", 3);
  return 0;
}
";
  auto tokens = tokenize(s, "nofile.cpp");
  EXPECT_EQ(checkBlacklistedIdentifiers("nofile.cpp", tokens), 2);
}

unittest {
  string s = "
#include <vector>
#include <list1>
#include <algorith>
#include \"map\"
int main() {
  std::vector<int> s;
  std::map<Foo> s;
  std::list<T> s!@#1;
  std::foo<bar> s;
  std::find();
  std::find_if();
  std::remove();
  vector<int> s;
  map<Foo> s;
  list<T> s!@#1;
  foo<bar> s;
  find();
  find_if();
  remove();
}

#include <algorithm>
void foo() {
  std::find();
  std::find_if();
  std::remove();
}
";
  auto tokens = tokenize(s, "nofile.cpp");
  EXPECT_EQ(checkDirectStdInclude("nofile.cpp", tokens), 5);
}

unittest {
  import std.file;
  string fpath = "linters/flint/test_files/Test1.cpp";
  if (fpath.exists()) {
    string file = fpath.readText;
    auto tokens = tokenize(file, fpath);
    EXPECT_EQ(checkDirectStdInclude(fpath, tokens), 3);
  }
}

unittest {
  string s = "
#include <stdio.h>
int main() {
  printf(\"hello, world\");
}
";
  auto tokens = tokenize(s, "nofile.cpp");
  assert(tokens.length == 19, text(tokens.length));
  s = ":: () [] . -> ++ -- dynamic_cast static_cast reinterpret_cast
const_cast typeid ++ -- ~ ! sizeof new delete * & + - .* ->* * / % << >>
< > <= >= == != & ^ | && || ?: = *= /= %= += -= >>= <<= &= ^= |= ,";
  tokens = tokenize(s, "nofile.cpp");
   const CppLexer.TokenType2[] witness = [
    tk!"::",
    tk!"(",
    tk!")",
    tk!"[",
    tk!"]",
    tk!".",
    tk!"->",
    tk!"++",
    tk!"--",
    tk!"dynamic_cast",
    tk!"static_cast",
    tk!"reinterpret_cast",
    tk!"const_cast",
    tk!"typeid",
    tk!"++",
    tk!"--",
    tk!"~",
    tk!"!",
    tk!"sizeof",
    tk!"new",
    tk!"delete",
    tk!"*",
    tk!"&",
    tk!"+",
    tk!"-",
    tk!".*",
    tk!"->*",
    tk!"*",
    tk!"/",
    tk!"%",
    tk!"<<",
    tk!">>",
    tk!"<",
    tk!">",
    tk!"<=",
    tk!">=",
    tk!"==",
    tk!"!=",
    tk!"&",
    tk!"^",
    tk!"|",
    tk!"&&",
    tk!"||",
    tk!"?",
    tk!":",
    tk!"=",
    tk!"*=",
    tk!"/=",
    tk!"%=",
    tk!"+=",
    tk!"-=",
    tk!">>=",
    tk!"<<=",
    tk!"&=",
    tk!"^=",
    tk!"|=",
    tk!",",
    tk!"\0",
  ];

  assert(tokens.length == witness.length,
      text(tokens.length, " != ", witness.length));
  size_t j = 0;
  foreach (t; tokens) {
    assert(t.type_ == witness[j], text(t.type_, " !is ", witness[j]));
    ++j;
  }
}

// Test multiline single-line comment
unittest {
  string s = "
int x;
// This is a single-line comment\\
that extends on multiple\\
lines. Nyuk-nyuk...
  float y;";
  Token[] tokens = tokenize(s, "nofile.cpp");
  assert(tokens.length == 7);
  assert(tokens[3].type_ == tk!"float");
}

// Test #pragma/#error preprocessor directives are tokenized
unittest {
  string s = "
#error this is an error
#pragma omp parallel for
   #   error with some leading spaces
#pragma   once
#error with line \\
break
#warning warning warning";
  Token[] tokens = tokenize(s, "nofile.cpp");
  assert(tokens.length == 9);
  assert(tokens[0].type_ == tk!"preprocessor_directive" && tokens[0].value() ==
    "#error this is an error");
  assert(tokens[1].type_ == tk!"preprocessor_directive" && tokens[1].value() ==
    "#pragma omp parallel for");
  assert(tokens[2].type_ == tk!"preprocessor_directive" && tokens[2].value() ==
    "#   error with some leading spaces");
  assert(tokens[3].type_ == tk!"#");
  assert(tokens[4].type_ == tk!"identifier" && tokens[4].value() == "pragma");
  assert(tokens[5].type_ == tk!"identifier" && tokens[5].value() == "once");
  assert(tokens[6].type_ == tk!"preprocessor_directive" && tokens[6].value() ==
    "#error with line \\\nbreak");
  assert(tokens[7].type_ == tk!"preprocessor_directive" && tokens[7].value() ==
    "#warning warning warning");
  assert(tokens[8].type_ == tk!"\0");
}

// Test numeric literals
unittest {
  string s = "
.123
123.234
123.234e345
123.234e+345
0x234p4
0xabcde
0x1.fffffffffffffp1023
0x1.0P-1074
0x0.0000000000001P-1022
";
  Token[] tokens = tokenize(s, "nofile.cpp");
  // All except the ending TK_EOF are numbers
  foreach (ref t; tokens[0 .. $ - 1]) {
    assert(t.type_ == tk!"number");
  }
  assert(tokens.length == 10);
}

// Test identifiers
unittest {
  string s = "
x
X
xy
Xy
xY
_x
x_
x$
x_y
x$y
xy_
xy$
";
  Token[] tokens = tokenize(s, "nofile.cpp");
  // All except the ending TK_EOF are numbers
  foreach (ref t; tokens[0 .. $ - 1]) {
    assert(t.type_ == tk!"identifier");
  }
  assert(tokens.length == 13);
}

//Test strings
unittest {
  string s = q"["common\n\"string\"\tliteral\\"
R"(raw"st"r\ni\tng)"
R"())"
]";
  Token[] tokens = tokenize(s, "nofile.cpp");
  //All except the ending TK_EOF are string_literal
  foreach (ref t; tokens[0 .. $ - 1]) {
    assert(t.type_ == tk!"string_literal");
  }
  assert(tokens.length == 4);
}

// Test sequences
unittest {
  string s = "
asm volatile('mov eax, 10');
volatile int foo;
class Foo {
operator+();
}
";
  Token[] tokens = tokenize(s, "nofile.cpp");
  EXPECT_EQ(checkBlacklistedSequences("nofile.cpp", tokens), 1);
}

// Test c_mode=true for sequences
unittest {
  string s = "
asm volatile('mov eax, 10');
volatile int foo;
";
  Token[] tokens = tokenize(s, "nofile.cpp");
  c_mode = true;
  assert(checkBlacklistedSequences("nofile.cpp", tokens) == 0);
  c_mode = false;
}

unittest {
  string filename = "nofile.cpp";

  string s = "(
int main(int argc, char** argv) {
  auto p = strtok(argv[0], ',');
  while ((p = strtok(nullptr, ','))) {
  }
}
)";
  Token[] tokens = tokenize(s, filename);
  assert(checkBlacklistedIdentifiers(filename, tokens) == 2);

  string s1 = "(
int main(int argc, char** argv) {
  char* state;
  auto p = strtok_r(argv[0], ',', &state);
  while ((p = strtok_r(nullptr, ',', &state))) {
  }
}
)";
  tokens = tokenize(s1, filename);
  assert(checkBlacklistedIdentifiers(filename, tokens) == 0);
}

// Test catch clauses in exceptions
unittest {
  string s = "
try {} catch (Exception &) {}
try {} catch (Exception & e) {}
try {} catch (ns::Exception &) {}
try {} catch (const ns::Exception & e) {}
try {} catch (::ns::Exception &) {}
try {} catch (const ::ns::Exception & e) {}
try {} catch (typename ::ns::Exception &) {}
try {} catch (const typename ns::Exception & e) {}
try {} catch (Exception<t, (1 + 1) * 2> &) {}
try {} catch (const Exception<(1)> & x) {}
try {} catch (...) {}
";
  auto tokens = tokenize(s, "nofile.cpp");
  assert(checkCatchByReference("nofile.cpp", tokens) == 0);
}

// testCheckIfEndifBalance
unittest {
  string s = "
#ifndef A
#if B
#endif
#endif
";
  auto tokens = tokenize(s);
  assert(tokens.atSequence(tk!"#", tk!"identifier"));
  EXPECT_EQ(checkIfEndifBalance("nofile.cpp", tokens), 0);

string s2 = "
#ifndef A
#if B
  #ifdef C
  #endif
#else
#endif
";
  tokens = tokenize(s2);
  assert(checkIfEndifBalance("nofile.cpp", tokens) == 1);

string s3 = "
#if B
  #ifdef C
  #endif
#else
#endif
#endif
";
  tokens = tokenize(s3);
  assert(checkIfEndifBalance("nofile.cpp", tokens) == 1);

string s4 = "
#ifndef A
#endif
#else
";
  tokens = tokenize(s4);
  assert(checkIfEndifBalance("nofile.cpp", tokens) == 1);
}

unittest {
  string code;

  // Non-explicit single-arg constructors may inadvertently be used for
  // type conversion
  code = "
class AA {
  AA(int bad);
  AA(AA *bad);
};
struct BB {
  BB(int bad);
};
";
  Token[] tokens;
  tokens = tokenize(code);
  assert(checkConstructors("nofile.cpp", tokens) == 3);

  // Default arguments are a special case of the previous problem
  code = "
class AA {
  AA(int bad = 42);
  AA(int bad, int j = 42);
  AA(int bad = 42, int j = 42);
};
";
  tokens = tokenize(code);
  EXPECT_EQ(checkConstructors("nofile.cpp", tokens), 3);

  // Constructors with zero or 2+ required arguments are safe
  code = "
class AA {
  AA();
  AA(void);
  AA(int safe, int safe2);
  AA(AA safe, AA safe2);
  void dosomething(const int & safe, vector<AA> & safe2);
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 0);

  // Single-arg constructors marked "explicit" are safe
  code = "
class AA {
  explicit AA(int safe);
  explicit AA(int safe, int j = 42);
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 0);

  // Could be "explicit constexpr" or "constexpr explicit" too
  code = "
class AA {
  explicit constexpr AA(int safe);
  constexpr explicit AA(int* safe);
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 0);

  // Suppress warnings when single-arg constructors are marked as implicit
  code = "
class CC {
  /* implicit */ CC(int acceptable);
  /* implicit */ CC(int acceptable, int j = 42);
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 0);

  // These copy/move constructors are reasonable
  code = "
class AA {
  AA(const AA& acceptable);
  AA(AA&& acceptable) noexcept;
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 0);

  // Move constructors should be declared noexcept
  code = "
class AA {
  AA(const AA& acceptable);
  AA(AA&& mightThrow);
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 1);

  // Move constructors that are deleted doesn't have to be
  // declared noexcept
  code = "
class AA {
  AA(const AA& acceptable);
  AA(AA&& mightThrow) = delete;
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 0);

  // Move constructors that throw should be explicitly marked as such
  code = "
class AA {
  AA(const AA& acceptable);
  AA(AA&& mightThrow) /* may throw */;
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 0);

  // These move/copy constructors don't make sense
  code = "
class AA {
  AA(AA& shouldBeConst);
  AA(const AA&& shouldNotBeConst);
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 3);

  // Don't warn for single-argument std::initializer_list<...>
  code = "
class AA {
  AA(std::initializer_list<CC> args);
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 0);

  // Verify that the parser handles nested blocks correctly
  code = "
namespace AA { namespace BB {
  class CC {
    class DD {
      DD(int bad);
      void CC(int safe);
    };
    void DD(int safe);
    CC(int bad);
  };
  void AA(int safe);
} }
void CC(int safe);
void DD(int safe);
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 2);

  // Some special cases that are tricky to parse
  code = "
template < class T, class Allocator = allocator<T> >
class AA {
  using namespace std;
  static int i = 0;
  typedef AA<T, Allocator> this_type;
  friend class ::BB;
  struct {
    int x;
    struct DD {
      DD(int bad);
    };
  } pt;
  struct foo foo_;
  AA(std::vector<CC> bad);
  AA(T bad, Allocator jj = NULL);
  AA* clone() const { return new AA(safe); }
  void foo(std::vector<AA> safe) { AA(this->i); }
  void foo(int safe);
};
class CC : std::exception {
  CC(const string& bad) {}
  void foo() {
    CC(localizeString(MY_STRING));
    CC(myString);
    CC(4);
    throw CC(\"ok\");
  }
};
";
  assert(checkConstructors("nofile.cpp", tokenize(code)) == 4);
}

// testCheckImplicitCast
unittest {
  Token[] tokens;
  string filename = "nofile.cpp";
  string code;

  // Test if it captures implicit bool cast
  code = "
class AA {
  operator bool();
};
struct BB {
  operator bool();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 2);

  // Test if explicit and /* implicit */ whitelisting works
  code = "
class AA {
  explicit operator bool();
};
struct BB {
  /* implicit */ operator bool();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 0);

  // It's ok to delete an implicit bool operator.
  code = "(
class AA {
  operator bool() = delete;
  operator bool() const = delete;
};
)";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 0);

  // Test if it captures implicit char cast
  code = "
class AA {
  operator char();
};
struct BB {
  operator char();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 2);

  // Test if explicit and /* implicit */ whitelisting works
  code = "
struct Foo;
class AA {
  explicit operator Foo();
};
struct BB {
  /* implicit */ operator Foo();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 0);

  // Test if it captures implicit uint8_t cast
  code = "
class AA {
  operator uint8_t();
};
struct BB {
  operator uint8_t();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 2);

  // Test if it captures implicit uint8_t * cast
  code = "
class AA {
  operator uint8_t *();
};
struct BB {
  operator uint8_t *();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 2);

  // Test if it captures implicit void * cast
  code = "
class AA {
  operator void *();
};
struct BB {
  operator void *();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 2);

  // Test if it captures implicit class cast
  code = "
class AA {
};
class BB {
  operator AA();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 1);

  // Test if it captures implicit class pointer cast
  code = "
class AA {
};
class BB {
  operator AA *();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 1);

  // Test if it captures implicit class reference cast
  code = "
class AA {
};
class BB {
  operator AA&();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 1);

  // Test if it captures template classes
  code = "
template <class T>
class AA {
  T bb;
  operator T();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 1);

  // Test if it avoids operator overloading
  code = "
class AA {
  int operator *()
  int operator+(int i);
  void foo();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 0);

  // Test if it captures template casts
  code = "
template <class T>
class AA {
  operator std::unique_ptr<T>();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 1);

  // Test if it avoids definitions
  code = "
class AA {
  int bb
  operator bool();
};
AA::operator bool() {
  return bb == 0;
}
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 1);

  // explicit constexpr is still explicit
  code = "
class Foo {
  explicit constexpr operator int();
};
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 0);

  // operator in any unrelated context
  code = "
#include <operator.hpp>
#include \"operator.hpp\"
";
  tokens = tokenize(code);
  assert(checkImplicitCast(filename, tokens) == 0);
}

// Test non-virtual destructor detection
unittest {
  string s ="
class AA {
public:
  ~AA();
  virtual int foo();
  void aa();
};";
  Token[] tokens;
  string filename = "nofile.cpp";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  s = "
class AA {
public:
private:
  virtual void bar();
public:
  ~AA();
};";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  s = "
class AA {
public:
  virtual void aa();
};";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  // Namespace
  s = "
class AA::BB {
  public:
    virtual void foo();
  ~BB();
};";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  // Struct
  s = "
struct AA {
  virtual void foo() {}
  ~AA();
};";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  // Nested class
  s = "
class BB {
public:
  ~BB();
  class CC {
    virtual int bar();
  };
  virtual void foo();
};";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 2);

  // Not a base class; don't know if AA has a virtual dtor
  s = "
class BB : public AA{
  virtual foo() {}
};";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 0);

  // Protected dtor
  s = "
class BB {
  protected:
    ~BB();
  public:
    virtual void foo();
};";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 0);

  // Private dtor
  s = "
class BB {
  ~BB();
  public:
    virtual void foo();
};";
  tokens = tokenize(s);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 0);
}

void EXPECT_EQ(T, U, string f = __FILE__, size_t n = __LINE__)
    (auto ref T lhs, auto ref U rhs) {
  if (lhs == rhs) return;
  stderr.writeln(text(f, ":", n, ": ", lhs, " != ", rhs));
  assert(0);
}

// Test include guard detection
unittest {
  string s = "
//comment
/*yet\n anothern one\n*/
#ifndef TEST_H
#define TEST_H
class A { };
#if TRUE
  #ifdef TEST_H
  class B { };
  #endif
class C { };
#else
#endif
class D { };
#endif
";
  Token[] tokens;
  string filename = "nofile.h";
  tokens = tokenize(s);
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 0);

string s2 = "
#ifndef TEST_H
#define TeST_h
class A { };
#if TRUE
  #ifdef TEST_H
  class B { };
  #endif
class C { };
#else
#endif
class D { };
#endif
#ifdef E_H
class E { };
#endif
";
  tokens = tokenize(s2);
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 2);

string s3 = "
class A { };
#if TRUE
  #ifdef TEST_H
  class B { };
  #endif
class C { };
#else
#endif
class D { };
";
  tokens = tokenize(s3);
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 1);
  filename = "nofile.cpp";
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 0);

  string s4 = "
#pragma once
class A { };
#if TRUE
  #ifdef TEST_H
  class B { };
  #endif
class C { };
#else
#endif
class D { };
";
  tokens = tokenize(s4);
  filename = "nofile.h";
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 0);
}

// testCheckInitializeFromItself
unittest {
  string s = "
namespace whatever {
ClassFoo::ClassFoo(int memberBar, int memberOk, int memberBaz)
  : memberBar_(memberBar_)
  , memberOk_(memberOk)
  , memberBaz_(memberBaz_) {
}
}
";
  auto tokens = tokenize(s);
  EXPECT_EQ(checkInitializeFromItself("nofile.cpp", tokens), 2);

  string s1 = "
namespace whatever {
ClassFooPOD::ClassFooPOD(int memberBaz) :
  memberBaz(memberBaz) {
}
}
";
  tokens = tokenize(s1);
  EXPECT_EQ(checkInitializeFromItself("nofile.cpp", tokens), 0);
}

// testCheckUsingDirectives
unittest {
  string s = "
namespace whatever {
  void foo() {
    using namespace ok;
  }
}
namespace facebook {
  namespace whatever {
    class Bar {
      void baz() {
        using namespace ok;
    }};
}}
void qux() {
  using namespace ok;
}
";
  string filename = "nofile.h";
  auto tokens = tokenize(s);
  EXPECT_EQ(checkUsingDirectives(filename, tokens), 0);

  string s1 = "
  namespace facebook {

    namespace { void unnamed(); }
    namespace fs = boost::filesystem;

    void foo() { }
    using namespace not_ok;
    namespace whatever {
      using namespace warn;
      namespace facebook {
      }
    }
  }
  using namespace not_ok;
";
  tokens = tokenize(s1);
  EXPECT_EQ(checkUsingDirectives(filename, tokens), 4);

  string s2 = "
void foo() {
  namespace fs = boost::filesystem;
}";
  tokens = tokenize(s2);
  EXPECT_EQ(checkUsingDirectives(filename, tokens), 0);
}

// test that we allow 'using x=y::z; but not 'using abc::def'
unittest {
  string s1 = "
    using not_ok::nope;
    using not_ok::nope::stillnotok;
    using SomeAlias1 = is::ok;
    using SomeAlias2 = is::ok::cool;
  ";
  string filename = "nofile.h";
  auto tokens = tokenize(s1);
  EXPECT_EQ(checkUsingDirectives(filename, tokens), 2);

  string s2 = "
    void foo() { using is::ok; }
    namespace facebook {
      using not_ok::nope;
      using alias3 = is::ok;
    }";
  auto tokens2 = tokenize(s2);
  EXPECT_EQ(checkUsingDirectives(filename, tokens2), 1);

  string s3 = "
    namespace notfacebook {
      using is::warn;
      using namespace warn;
    }";
  auto tokens3 = tokenize(s3);
  EXPECT_EQ(checkUsingDirectives(filename, tokens3), 2);

  string s4 = "
    /* using override */ using ok::good;
    /* using override */ using namespace ok;
    namespace facebook {
      /* using override */ using ok::good;
    }";

  auto tokens4 = tokenize(s4);
  EXPECT_EQ(checkUsingDirectives(filename, tokens4), 0);
}

// testCheckUsingNamespaceDirectives
unittest {
  string filename = "nofile.cpp";
  Token[] tokens;

  void RUN_THIS_TEST(string code, uint expectedResult) {
    auto tokens = tokenize(code);
    EXPECT_EQ(checkUsingNamespaceDirectives(filename, tokens), expectedResult);
  }

  string nothing = "";
  RUN_THIS_TEST(nothing, 0);

  string simple1 = "using namespace std;";
  RUN_THIS_TEST(simple1, 0);

  string simple2 = "using ok::good;";
  RUN_THIS_TEST(simple2, 0);

  string simple3 = "using alias = ok::good;";
  RUN_THIS_TEST(simple3, 0);

  string simpleFail = "
using namespace std;
using namespace boost;
";
  RUN_THIS_TEST(simpleFail, 1);

  // 2 directives in separate scopes should not conflict
  string separateBlocks = "
{
  using namespace std;
}
{
  using namespace boost;
}
";
  RUN_THIS_TEST(separateBlocks, 0);

  // need to catch directives if they are nested in scope of other directive
  string nested = "
{
  using namespace std;
  {
    using namespace boost;
  }
}
";
  RUN_THIS_TEST(nested, 1);

  // same as last case, but without outer braces
  string nested2 = "
using namespace std;
{
  using namespace boost;
}
";
  RUN_THIS_TEST(nested, 1);

  // make sure doubly nesting things is not a problem
  string doubleNested = "
{
  using namespace std;
  {
    {
      using namespace boost;
    }
  }
}
";
  RUN_THIS_TEST(doubleNested, 1);

  // same as last case, but without outer braces
  string doubleNested2 = "
using namespace std;
{
  {
    using namespace boost;
  }
}
";
  RUN_THIS_TEST(doubleNested, 1);

  // duplicates using namespace declaratives
  string dupe = "
using namespace std;
using namespace std;
";
  RUN_THIS_TEST(dupe, 1);

  // even though "using namespace std;" appears twice above "using namespace
  // boost;", there should only be 1 error from that line (and 1 from the
  // duplication)
  string dupe2 = "
using namespace std;
using namespace std;
using namespace boost;
";
  RUN_THIS_TEST(dupe2, 2);

  // even though the third line has 2 errors (it's a duplicate and it
  // conflicts with the 2nd line), it should only result in the duplicate
  // errorand short circuit out of the other
  string dupe3 = "
using namespace std;
using namespace boost;
using namespace std;
";
  RUN_THIS_TEST(dupe3, 2);

  // only 2 errors total should be generated even though the 3rd line
  // conflicts with both of the first 2
  string threeWay = "
using namespace std;
using namespace boost;
using namespace std;
";
  RUN_THIS_TEST(threeWay, 2);

  // make sure nesting algorithm realizes second using namespace std; is a
  // dupe and doesn't remove it from the set of visible namespaces
  string nestedDupe = "
using namespace std;
{
  using namespace std;
}
using namespace std;
";
  RUN_THIS_TEST(nestedDupe, 2);

  string other1 = "
{
using namespace std;
using namespace std;
}
using namespace boost;
";
  RUN_THIS_TEST(other1, 1);
}

// testThrowsSpecification
unittest {
  string s1 = "struct foo { void function() throw(); };";
  Token[] tokens;
  string filename = "nofile.cpp";
  tokens = tokenize(s1);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 1);

  string s2 = "
void func() {
  throw (std::runtime_error(\"asd\");
}
";
  tokens = tokenize(s2);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s3 = "
struct something {
  void func() throw();
  void func2() noexcept();
  void func3() throw(char const* const*);
  void wat() const {
    throw(12);
  }
};
";
  tokens = tokenize(s3);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 2);

  string s4 = "struct A { void f1() const throw();
                          void f2() volatile throw();
                          void f3() const volatile throw();
                          void f4() volatile const throw();
                          void f5() const;
                          void f6() volatile;
                          void f7() volatile const;
                          void f8() volatile const {}
                        };";
  tokens = tokenize(s4);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 4);

  string s5 = "void f1();";
  tokens = tokenize(s5);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s6 = "void f1(void(*)(int,char)) throw();";
  tokens = tokenize(s6);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 1);

  string s7 = "void f() { if (!true) throw(12); }";
  tokens = tokenize(s7);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s8 = "namespace foo {
                 struct bar {
                   struct baz {
                     void f() throw(std::logic_error);
                   };
                   struct huh;
                 };
                 using namespace std;
                 struct bar::huh : bar::baz {
                   void f2() const throw() { return f(); }
                 };
                 namespace {
                   void func() throw() {
                     if (things_are_bad()) {
                       throw 12;
                     }
                   }
                 }
                 class one_more { void eh() throw(); };
               }";
  tokens = tokenize(s8);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 4);

  string s9 = "struct foo : std::exception {
    virtual void what() const throw() = 0; };";
  tokens = tokenize(s9);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s10 = "template<class A> void f() { throw(12); }
                template<template<class> T, class Y> void g()
                  { throw(12); }
                const int a = 2; const int b = 12;
                template<class T, bool B = (a < b)> void h()
                  { throw(12); }";
  tokens = tokenize(s10);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s11 = "const int a = 2; const int b = 12;
                template<bool B = (a < b)> void f() throw() {}
                void g() throw();";
  tokens = tokenize(s11);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 2);

  string s12 = "struct Foo : std::exception { ~Foo() throw(); };";
  tokens = tokenize(s12);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s13 = "struct Foo : std::exception { ~Foo() throw() {} };";
  tokens = tokenize(s13);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s14 = "struct Foo : std::exception { ~Foo() throw() {} " ~
    "virtual const char* what() const throw() {} };";
  tokens = tokenize(s14);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s15 = "struct Foo { const char* what() const throw() {}" ~
    "~Foo() throw() {} };";
  tokens = tokenize(s15);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);
}

// testProtectedInheritance
unittest {
  string s1 = "class foo { }";
  Token[] tokens;
  string filename = "nofile.cpp";
  tokens = tokenize(s1);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 0);

  string s2 = "class foo : public bar { }";
  tokens = tokenize(s2);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 0);

  string s3 = "class foo : protected bar { }";
  tokens = tokenize(s3);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 1);

  string s4 = "class foo : public bar { class baz : protected bar { } }";
  tokens = tokenize(s4);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 1);

  string s5 = "class foo : protected bar { class baz : public bar { } }";
  tokens = tokenize(s5);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 1);

  string s6 = "class foo : protected bar { class baz : protected bar { } }";
  tokens = tokenize(s6);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 2);

  string s7 = "class foo; class bar : protected foo {};";
  tokens = tokenize(s7);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 1);
}

// testExceptionInheritance
unittest {
  Token[] tokens;
  string filename = "nofile.cpp", s;

  s = "class foo { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "class foo: exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: std::exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private std::exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: protected exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: protected std::exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: public exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "class foo: public std::exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: std::exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: private exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "struct foo: private std::exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "struct foo: protected exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: protected std::exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: public exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: public std::exception { }";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "class bar: public std::exception {class foo: exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception {class foo: std::exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception {class foo: private exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception " ~
    "{class foo: private std::exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception " ~
    "{class foo: protected exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception " ~
    "{class foo: protected std::exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception {class foo: public exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "class bar: std::exception {class foo { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: std::exception {class foo: exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: std::exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: private exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: private std::exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: protected exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: protected std::exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: public exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: std::exception {class foo: public std::exception { } c;}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo; class bar: std::exception {}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: public bar, std::exception {}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: public bar, private std::exception {}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private bar, std::exception {}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private bar, public baz, std::exception {}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private bar::exception {}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: public bar, std::exception {}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: public bar, private std::exception {}";
  tokens = tokenize(s);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);
}

// testCxxReplace
unittest {
  if (!std.file.exists("_bin/linters/flint/cxx_replace")) {
    // No prejudice if the file is missing (this may happen e.g. if
    // running fbmake runtests with -j)
    stderr.writeln("_bin/linters/flint/cxx_replace is missing" ~
      " so it cannot be tested this time around. This may happen" ~
      " during a parallel build.");
    return;
  }
  string s1 = "
          tokenLen = 0;
          pc = pc1;
          pc.advance(strlen(\"line\"));
          line = parse<size_t>(pc);
          munchSpaces(pc);
          if (pc[0] != '\n') {
            for (size_t i = 0; ; ++i) {
              ENFORCE(pc[i]);
              if (pc[i] == '\n') {
                file = munchChars(pc, i);
                ++line;
                break;
              }
            }
          }
";
  auto f = "/tmp/cxx_replace_testing"
    ~ to!string(uniform(0,int.max)) ~ to!string(uniform(0,int.max));
  std.file.write(f, s1);
  scope(exit) std.file.remove(f);
  import std.process;
  EXPECT_EQ(executeShell("_bin/linters/flint/cxx_replace 'munch' 'crunch' "
                    ~ f).status,
            0);
  EXPECT_EQ(std.file.readText(f), s1);
  EXPECT_EQ(executeShell("_bin/linters/flint/cxx_replace 'break' 'continue' "
                    ~ f).status,
            0);
  string s2 = std.array.replace(s1, "break", "continue");
  EXPECT_EQ(std.file.readText(f), s2);
}

// testThrowsHeapAllocException
unittest {
  string s1 = "throw new MyException(\"error\");";
  string filename = "nofile.cpp";
  auto tokens = tokenize(s1);
  EXPECT_EQ(checkThrowsHeapException(filename, tokens), 1);

  string s2 = "throw new (MyException)(\"error\");";
  tokens = tokenize(s2);
  EXPECT_EQ(checkThrowsHeapException(filename, tokens), 1);

  string s3 = "throw new MyTemplatedException<arg1, arg2>();";
  tokens = tokenize(s3);
  EXPECT_EQ(checkThrowsHeapException(filename, tokens), 1);

  string s4 = "throw MyException(\"error\")";
  tokens = tokenize(s4);
  EXPECT_EQ(checkThrowsHeapException(filename, tokens), 0);
}

// testHPHPCalls
unittest {
  Token[] tokens;
  string filename = "nofile.cpp";

  // basic pattern
  string s1 = "using namespace HPHP;
               void f() {
                 f_require_module(\"soup\");
                 f_someother_func(1,2,3);
               };";
  tokens = tokenize(s1);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 0);

  // alternative namespace syntax
  string s2 = "using namespace ::HPHP;
               void f() {
                 f_require_module(\"soup\");
                 f_someother_func(1,2,3);
               };";
  tokens = tokenize(s2);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 0);

  // reversed order of usage
  string s3 = "using namespace ::HPHP;
               void f() {
                 f_someother_func(1,2,3);
                 f_require_module(\"soup\");
               };";
  tokens = tokenize(s3);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 1);

  // all ways to fail without namespace open
  string s4 = "HPHP::f_someother_func(1,2,3);
               ::HPHP::f_someother_func(1,2);
               HPHP::c_className::m_mfunc(1,2);
               ::HPHP::c_className::m_mfunc(1,2);
               HPHP::k_CONSTANT;
               ::HPHP::k_CONSTANT;
               HPHP::ft_sometyped_func(1,2);
               ::HPHP::ft_sometyped_func(1,2);";
  tokens = tokenize(s4);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 8);

  // all ways to fail with namespace open
  string s5 = "using namespace HPHP;
               HPHP::f_someother_func(1,2,3);
               ::HPHP::f_someother_func(1,2);
               HPHP::c_className::m_mfunc(1,2);
               ::HPHP::c_className::m_mfunc(1,2);
               HPHP::k_CONSTANT;
               ::HPHP::k_CONSTANT;
               HPHP::ft_sometyped_func(1,2);
               ::HPHP::ft_sometyped_func(1,2);
               f_someother_func(1,2);
               c_className::m_mfunc(1,2);
               k_CONSTANT;
               ft_sometyped_func(1,2);";
  tokens = tokenize(s5);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 12);

  // all checks pass after f_require_module
  string s6 = "using namespace HPHP;
               f_require_module(\"some module\");
               HPHP::f_someother_func(1,2,3);
               ::HPHP::f_someother_func(1,2);
               HPHP::c_className::m_mfunc(1,2);
               ::HPHP::c_className::m_mfunc(1,2);
               HPHP::k_CONSTANT;
               ::HPHP::k_CONSTANT;
               HPHP::ft_sometyped_func(1,2);
               ::HPHP::ft_sometyped_func(1,2);
               f_someother_func(1,2);
               c_className::m_mfunc(1,2);
               k_CONSTANT;
               ft_sometyped_func(1,2);";
  tokens = tokenize(s6);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 0);

  // check that c_str is exempt
  string s7 = "using namespace HPHP;
               string c(\"hphp\"); c.c_str();";
  tokens = tokenize(s7);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 0);

  // check incorrect syntax
  string s8 = "using namespace ? garbage";
  tokens = tokenize(s8);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 1);

  // check that nested namespace usage doesn't confuse us
  string s9 = "using namespace ::HPHP;
               {
                 { using namespace HPHP; }
                 f_someother_func(1,2,3);
               };";
  tokens = tokenize(s9);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 1);

  // check that f_require_module is valid only in HPHP namespace
  string s10 = "f_require_module(\"meaningless\");
               using namespace HPHP;
               {
                 std::f_require_module(\"cake\");
                 f_someother_func(1,2,3);
                 HPHP::c_classOops::mf_func(1);
               };";
  tokens = tokenize(s10);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 2);

  // check that leaving HPHP scope re-requires module
  // this is arguable as a point of correctness, but better to
  // conservatively warn than silently fail
  string s11 = "int f1() { using namespace HPHP; f_require_module(\"foo\");}
                int second_entry_point() { using namespace HPHP; f_oops(); }";
  tokens = tokenize(s11);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 1);
}

// testCheckDeprecatedIncludes
unittest {
  Token[] tokens;
  string filename = "dir/TestFile.cpp";
  // sanity
  string s0 = "#include \"TestFile.h\"
              #include \"foo.h\"";
  tokens = tokenize(s0);

  EXPECT_EQ(checkQuestionableIncludes(filename, tokens), 0);

  // error case with one deprecated include
  string s1 = "#include \"TestFile.h\"
              #include \"common/base/Base.h\"";
  tokens = tokenize(s1);

  EXPECT_EQ(checkQuestionableIncludes(filename, tokens), 1);

  // error case with two deprecated includes
  string s2 = "#include \"TestFile.h\"
              #include \"common/base/Base.h\"
              #include \"common/base/StringUtil.h\"";
  tokens = tokenize(s2);

  EXPECT_EQ(checkQuestionableIncludes(filename, tokens), 2);

  // No errors if expensive header in .cpp
  string s3 = "#include \"TestFile.h\"
               #include \"multifeed/aggregator/gen-cpp/aggregator_types.h\"";
  tokens = tokenize(s3);

  EXPECT_EQ(checkQuestionableIncludes(filename, tokens), 0);

  // Two expensive header include from a header
  string headerFile = "dir/TestFile.h";
  string s4 = "#include \"multifeed/aggregator/gen-cpp/aggregator_types.h\"
               #include \"multifeed/shared/gen-cpp/multifeed_types.h\"";
  tokens = tokenize(s4);

  EXPECT_EQ(checkQuestionableIncludes(headerFile, tokens), 2);
}


// testCheckIncludeAssociatedHeader
unittest {
  Token[] tokens;
  string filename = "dir/TestFile.cpp";
  // sanity
  string s0 = "#include \"TestFile.h\"
              #include \"SomeOtherFile.h\"";
  tokens = tokenize(s0);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  filename = "TestFile.cpp";
  // sanity hpp
  string s1 = "#include \"TestFile.hpp\"
              #include \"SomeOtherFile\"";
  tokens = tokenize(s1);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // defines and pragmas before
  string s2 = "#pragma option -O2
              #define PI 3.14
              #include \"TestFile.h\"
              #include \"SomeOtherFile.h\"";
  tokens = tokenize(s2);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // no associated header
  string s3 = "#include \"<vector>\"
              #include \"SomeOtherFile.h\"";
  tokens = tokenize(s3);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // case sensitiveness
  string s4 = "#include \"testfile.h\"
              #include \"SomeOtherFile.h\"";
  tokens = tokenize(s4);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // no good
  string s5 = "#include \"<vector>\"
              #include \"SomeOtherFile.h\"
              #pragma option -O2
              #include \"TestFile.h\"";
  tokens = tokenize(s5, filename);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // no good
  string s6 = "#include \"<vector>\"
              #include \"SomeOtherFile.h\"
              #pragma option -O2
              #include \"TestFile.hpp\"
              #include \"Dijkstra.h\"";
  tokens = tokenize(s6);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // erroneous file
  string s7 = "#include #include #include";
  tokens = tokenize(s7);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // no good using relative path
  filename = "../TestFile.cpp";
  string s8 = "#include \"<vector>\"
              #include \"SomeOtherFile.h\"
              #pragma option -O2
              #include \"TestFile.h\"";
  tokens = tokenize(s8);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // good using relative path
  string s9 = "#include <vector>
              #include \"SomeOtherFile.h\"
              #pragma option -O2
              #include \"../TestFile.h\"";
  tokens = tokenize(s9, filename);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // good when header having same name under other directory
  filename = "dir/TestFile.cpp";
  string s10 = "#include \"SomeOtherDir/TestFile.h\"";
  tokens = tokenize(s10);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // testing not breaking relative path
  filename = "TestFile.cpp";
  string s11 = "#include <vector>
                #include \"TestFile.h\"";
  tokens = tokenize(s11);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // no good using absolute path for filename
  filename = "/home/philipp/fbcode/test/testfile.cpp";
  string s12 = "#include <vector>
                #include \"file.h\"
                #include \"test/testfile.h\"";
  tokens = tokenize(s12);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // good using absolute path for filename
  string s13 = "#include <vector>
                #include \"file.h\"
                #include \"othertest/testfile.h\"";
  tokens = tokenize(s13);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // good with 'nolint' comments and PRECOMPILED
  filename = "A.cpp";
  string s14 = "#include <q/r/s> // nolint
                #include \"abracadabra\" // more than nolint comment
                #include PRECOMPILED
                #include \"A.h\"";
  tokens = tokenize(s14);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);
}


// testMemset
unittest {
  string filename = "nofile.cpp";
  Token[] tokens;

  // good calls
  string s1 = "memset(foo, 0, sizeof(foo));
               memset(foo, 1, sizeof(foo));
               memset(foo, 12, 1)
               memset(T::bar(this, is, a pointers),(12+3)/5, 42);
               memset(what<A,B>(12,1), 0, sizeof(foo));
               memset(this->get<A,B,C>(12,1), 0, sizeof(foo));
               memset(&foo, 0, sizeof(foo));";
  tokens = tokenize(s1);
  EXPECT_EQ(checkMemset(filename, tokens), 0);

  // funky call that would terminate all subsequent check
  string s1b = "memset(SC, a < b ? 0 : 1, sizeof(foo));\n";
  tokens = tokenize(s1b);
  EXPECT_EQ(checkMemset(filename, tokens), 0);

  // bad call with 0
  string s2 = "memset(foo, 12, 0);";
  tokens = tokenize(s2);
  EXPECT_EQ(checkMemset(filename, tokens), 1);

  // bad call with 1
  string s3 = "memset(foo, sizeof(bar), 1);";
  tokens = tokenize(s3);
  EXPECT_EQ(checkMemset(filename, tokens), 1);

  // combination of bad calls
  string s4 = s2 ~ s3;
  s4 ~= "memset(T1::getPointer(), sizeof(T2), 0);
         memset(&foo, sizeof(B), 1);";
  tokens = tokenize(s4);
  EXPECT_EQ(checkMemset(filename, tokens), 4);

  // combination of good and bad calls
  string s5 = s1 ~ s4;
  tokens = tokenize(s5);
  EXPECT_EQ(checkMemset(filename, tokens), 4);

  // combination of bad calls and funky calls
  string s5b = s1b ~ s4;
  tokens = tokenize(s5b);
  EXPECT_EQ(checkMemset(filename, tokens), 0);

  // combination of bad calls and funky calls, different way
  string s6 = s4 ~ s1b;
  tokens = tokenize(s6);
  EXPECT_EQ(checkMemset(filename, tokens), 4);

  // process only function calls
  string s7 = "using std::memset;";
  tokens = tokenize(s7);
  EXPECT_EQ(checkMemset(filename, tokens), 0);
}

// testCheckInlHeaderInclusions
unittest {
  Token[] tokens;

  string s1 = "
#include \"Foo.h\"
#include \"Bar-inl.h\"
#include \"foo/baz/Bar-inl.h\"

int main() {}
";

  auto filename = "...";
  tokens = tokenize(s1, filename);
  EXPECT_EQ(checkInlHeaderInclusions("Foo.h", tokens), 2);
  EXPECT_EQ(checkInlHeaderInclusions("Bar.h", tokens), 0);

  string s2 = "
#include \"Foo-inl.h\"
";
  auto filename2 = "FooBar.h";
  tokens = tokenize(s2, filename2);
  EXPECT_EQ(checkInlHeaderInclusions(filename2, tokens), 1);
}

// testUpcaseNull
unittest {
  Token[] tokens;

  string s1 = "
#include <stdio.h>
int main() { int x = NULL; }
";
  tokens = tokenize(s1);
  EXPECT_EQ(checkUpcaseNull("filename", tokens), 1);

  string s2 = "int main() { int* x = nullptr; }\n";
  tokens = tokenize(s2);
  EXPECT_EQ(checkUpcaseNull("filename", tokens), 0);
}

// testSmartPtrUsage
unittest {
  Token[] tokens;
  string filename = "...";

  // test basic pattern with known used namespaces
  string s1 = "
std::shared_ptr<Foo> p(new Foo(whatever));
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  string s2 = "
boost::shared_ptr<Foo> p(new Foo(whatever));
";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  string s3 = "
facebook::shared_ptr<Foo> p(new Foo(whatever)); }
";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  string s4 = "
shared_ptr<Foo> p(new Foo(whatever)); }
";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  // this matches and suggests using allocate_shared
  string s10 = "
shared_ptr<Foo> p(new Foo(whatever), d, a); }
";
  tokenize(s10, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  // this matches without any special suggestion but using
  // make_shared, Deleter are not well supported in make_shared
  string s11 = "
shared_ptr<Foo> p(new Foo(whatever), d); }
";
  tokenize(s11, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  // test in a more "complex" code
  string s12 = "
int main() { std::shared_ptr<Foo> p(new Foo(whatever)); }
";
  tokenize(s12, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  // function declarations are ok
  string s14 = "
std::shared_ptr<Foo> foo(Foo foo);
";
  tokenize(s14, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 0);

  // assignments are fine
  string s16 = "
std::shared_ptr<Foo> foo = foo();
";
  tokenize(s16, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 0);
}

// testUniquePtrUsage
unittest {
  Token[] tokens;
  string filename = "...";

  string s1 = "
unique_ptr<Foo> p(new Foo(whatever));
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

//Should skip this, since its not of namespace std
  string s2 = "
boost::unique_ptr<Foo> p(new Foo[5]);
";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  // this matches and suggests using array in the template
  string s3 = "
unique_ptr<Foo> p(new Foo[5]);
";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  // this should not match
  string s4 = "
shared_ptr<Foo> p(new Foo(Bar[5]));
";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  // test in a more "complex" code
  string s5 = "
int main() { unique_ptr<Foo> p(new Foo[5]);
 unique_ptr<Foo[]> p(new Foo[5]);
 unique_ptr<Bar> q(new Bar[6]);}";
  tokenize(s5, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 2);

  string s6 = "
unique_ptr< unique_ptr<int[]> >
  p(new unique_ptr<int[]>(new int[2]));
";
  tokenize(s6, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  string s7 = "
unique_ptr< unique_ptr<int[]> > p(new unique_ptr<int[]>[6]);";
  tokenize(s7, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  // temporaries
  string s8 = "
unique_ptr<int[]>(new int());";
  tokenize(s8, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  string s9 = "
unique_ptr<int>(new int());";
  tokenize(s9, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  string s10 = "
unique_ptr<int>(new int[5]);";
  tokenize(s10, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  string s11 = "
unique_ptr<int[]>(new int[5]);";
  tokenize(s11, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  string s12 = "
unique_ptr<Foo[]> function() {
  unique_ptr<Foo[]> ret;
  return ret;
}
";
  tokenize(s12, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  string s13 = "
unique_ptr<
  unique_ptr<int> > foo(new unique_ptr<int>[12]);
";
  tokenize(s13, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  string s14 = "
void function(unique_ptr<int> a,
              unique_ptr<int[]> b = unique_ptr<int[]>()) {
}
";
  tokenize(s14, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  string s15 = "
int main() {
  vector<char> args = something();
  unique_ptr<char*[]> p(new char*[args.size() + 1]);
}
";
  tokenize(s15, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  string s16 = "
int main() {
  vector<char> args = something();
  unique_ptr<char const* volatile**[]> p(
    new char const* volatile*[args.size() + 1]
  );
}
";
  tokenize(s16, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  string s17 = "
int main() {
  unique_ptr<const volatile char*[]> p(
    new const volatile char*[1]
  );
}
";
  tokenize(s17, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);
}

// testThreadSpecificPtr
unittest {
  Token[] tokens;
  string filename = "...";

  string s1 = "
int main() {
  boost::thread_specific_ptr<T> p;
}
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkBannedIdentifiers(filename, tokens), 1);

  string s2 = "
int main() {
  folly::ThreadLocalPtr<T> p;y
}
}";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkBannedIdentifiers(filename, tokens), 0);
}

// testNamespaceScopedStatics
unittest {
  Token[] tokens;
  string filename = "somefile.h";

  string s1 = "
  namespace bar {
    static const int x = 42;
    static inline getX() { return x; }
    static void doFoo();
    int getStuff() {
      static int s = 22;
      return s;
    }
  }";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkNamespaceScopedStatics(filename, tokens), 3);

  string s2 = "
  class bar;
  namespace foo {
    static const int x = 42;
  }";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkNamespaceScopedStatics(filename, tokens), 1);

  string s3 = "
  namespace bar {
    class Status {
    public:
      static Status OK() {return 22;}
    };
    static void doBar();
  }
  static void doFoo();";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkNamespaceScopedStatics(filename, tokens), 2);
}

// testCheckMutexHolderHasName
unittest {
  string s = "
    unique_lock<mutex> foo() {
      lock_guard<x> ();
      lock_guard<mutex>(m_lock);
      lock_guard<mutex>(m_lock);
    }
   ";
  Token[] tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkMutexHolderHasName(filename, tokens), 3);

  string s2 = "
    unique_lock<mutex> foo() {
      vector<int> ();
      lock_guard<x> s(thing);
      unique_lock<mutex> l(m_lock);

      bar(unique_lock<mutex>(m_lock));
      return unique_lock<mutex>(m_lock);
    }
   ";
  tokens.destroy();
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkMutexHolderHasName(filename, tokens), 0);

  string s3 = "y
    void foo(lock_guard<mutex>& m, lock_guard<x>* m2) {
    }
   ";
  tokens.destroy();
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkMutexHolderHasName(filename, tokens), 0);
}

// testOSSIncludes
unittest {
  string s =
    "#include <super-safe>\n" ~
    "#include <braces/are/safe>\n" ~
    "#include \"no-slash-is-safe\"\n" ~
    "#include \"folly/is/safe\"\n" ~
    "#include \"hphp/is/safe/in/hphp\"\n" ~
    "#include \"hphp/facebook/hphp-facebook-is-safe-subdirectory\"\n" ~
    "#include \"random/unsafe/in/oss\"\n" ~
    "#include \"oss-is-safe\" // nolint\n" ~
    "#include \"oss/is/safe\" // nolint\n" ~
    "#include \"oss-at-eof-should-be-safe\" // nolint";

  import std.typecons;
  Tuple!(string, int)[] filenamesAndErrors = [
    tuple("anyfile.cpp", 0),
    tuple("non-oss-project/anyfile.cpp", 0),
    tuple("folly/anyfile.cpp", 3),
    tuple("hphp/anyfile.cpp", 1),
    tuple("hphp/facebook/anyfile.cpp", 0),
    tuple("proxygen/lib/anyfile.cpp", 3),
  ];

  foreach (ref p; filenamesAndErrors) {
    Token[] tokens;
    tokenize(s, p[0], tokens);
    EXPECT_EQ(p[1], checkOSSIncludes(p[0], tokens));
  }
}

// testMultipleIncludes

unittest {
  import std.typecons;
  Tuple!(string, int)[] includesAndErrors = [
    tuple("#include <single_file>\n" ~
          "#include <includedTwice>\n" ~
          "#include <includedTwice>\n", 1),
    tuple("#include <single_file>\n" ~
          "#include \"includedTwice\"\n" ~
          "#include \"includedTwice\"\n", 1),
    tuple("#include <includedTwice>\n" ~
          "include \"includedTwice\"\n", 0),
    tuple("#include <firstInclude>\n" ~
          "#include <secondInclude>\n", 0),
    tuple("#include <includedTwice>\n" ~
          "#include <includedTwice> /* nolint */\n", 0)
  ];

  foreach (ref p; includesAndErrors) {
    Token[] tokens;
    tokenize(p[0], "testFile.cpp", tokens);
    EXPECT_EQ(p[1], checkMultipleIncludes("testFile.cpp", tokens));
    tokenize(p[0], "testFile.c", tokens);
    EXPECT_EQ(p[1], checkMultipleIncludes("testFile.c", tokens));
  }
}

// testCheckBreakInSynchronized
unittest {
  string s = "
    int foo() {\n
      int i = 1;\n
      Synchronized<vector<int>> v;\n
      while(i > 0) {\n
        SYNCHRONIZED (v) {\n
          if(v.size() > 0) break; \n
        }\n
        i--;\n
      };\n
    }\n
   ";
  Token[] tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkBreakInSynchronized(filename, tokens), 1);

  string s2 = "
    void foo() {\n
      Synchronized<vector<int>> v;\n
      for(int i = 10; i < 0; i--) { \n
        if(i < 1) break; \n
        SYNCHRONIZED(v) { \n
          if(v.size() > 5) break; \n
          while(v.size() < 5) { \n
            v.push(1); \n
            if(v.size() > 3) break; \n
          } \n
          if(v.size() > 4) break; \n
        } \n
        i--; \n
        if(i > 2) continue; \n
      }\n
    }\n
   ";
  tokens.destroy();
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkBreakInSynchronized(filename, tokens), 2);

  string s3 = "
    void foo() {\n
      Synchronized<vector<int>> v;\n
      SYNCHRONIZED_CONST(v) {\n
        for(int i = 0; i < v.size(); i++) {\n
          if(v[i] == 5) break;\n
        }\n
      }\n
    }\n
   ";

  tokens.destroy();
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkBreakInSynchronized(filename, tokens), 0);
}

// testFollyStringPieceByValue
unittest {
  string filename = "nofile.cpp";
  Token[] tokens;

  // const & not ok
  string s = "const folly::StringPiece& f(const StringPiece&)\n";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkFollyStringPieceByValue(filename, tokens), 2);

  // non-const & is ok
  string s2 = "folly::StringPiece& g(StringPiece&)\n";
  tokens.destroy();
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkFollyStringPieceByValue(filename, tokens), 0);

  // by value is ok
  string s3 = "folly::StringPiece g(StringPiece)\n";
  tokens.destroy();
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkFollyStringPieceByValue(filename, tokens), 0);
}

// testCheckRandomUsage
unittest {
  string filename = "nofile.cpp";
  Token[] tokens;

  // random_device
  string s = "
    random_device rd;
    std::uniform_int_distribution<int> dist;
    std::random_device rd2;
    r = dist(rd);
  ";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkRandomUsage(filename, tokens), 2);

  // rand()
  string s1 = "
    int randomInt = rand();
    tr_rand();
  ";
  tokens.destroy();
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkRandomUsage(filename, tokens), 1);

  // RandomInt32
  string s2 = "
    RandomInt32 r;
    GetRandomInt32();
    uint32_t rand = r();
  ";
  tokens.destroy();
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkRandomUsage(filename, tokens), 1);

  // RandomInt64
  string s3 = "
    RandomInt64 r;
    GetRandomInt64();
    uint64_t rand = r();
  ";
  tokens.destroy();
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkRandomUsage(filename, tokens), 1);

  // std::random_shuffle
  string s4 = "
    random_shuffle(x, y, z);
  ";
  tokens.destroy();
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkRandomUsage(filename, tokens), 1);
}

// testCheckSleepUsage
unittest {
  string filename = "testFile.cpp";
  Token[] tokens;

  // sleep
  string s = "
    sleep(1);
    //Nothing
    sleep(5);
  ";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkSleepUsage(filename, tokens), 2);

  // usleep()
  string s1 = "
    usleep(200);
  ";
  tokens.destroy();
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkSleepUsage(filename, tokens), 1);

  // sleep_for
  string s2 = "
    std::this_thread::sleep_for(std::chrono::seconds(3));
  ";
  tokens.destroy();
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkSleepUsage(filename, tokens), 1);

  // sleep_until
  string s3 = "
    this_thread::sleep_until(chrono::system_clock::now());
  ";
  tokens.destroy();
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkSleepUsage(filename, tokens), 1);

  // No false positives
  string s4 = `
    //sleep comment
    sleepy_code();
    DEFINE_int32(sleep, 0, "trolololol");
  `;
  tokens.destroy();
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkSleepUsage(filename, tokens), 0);

  // Override lint rule mechanism
  // Scope test: one lint error will apply for this test (the last line).
  stderr.writeln("-------------------------------------TESTASFASDFASDASDASD");
  string s5 = "
    /* sleep override */ sleep();
    /* sleep override */ this_thread::sleep_for(std::chrono::milliseconds(200));
    /* sleep override */ std::this_thread::sleep_for(std::chrono::milliseconds(200));
    /* sleep override */ appliesToThisInstead(); sleep();
  ";
  tokens.destroy();
  tokenize(s5, filename, tokens);
  EXPECT_EQ(checkSleepUsage(filename, tokens), 1);
}

version(facebook)  {
  unittest {
    Token[] tokens;
    string filename = "somefileforAngleBrackets.cpp";

    string s1 = "
    #include PRECOMPILED
    #include \"folly/Foo.h\"  // not ok
    #include \"folly/Bar.h\"  // ok because nolint
    #include <folly/Baz.h>    // ok because folly likes angle brackets
    ";
    tokenize(s1, filename, tokens);

    // Only a warning if used outside of folly or thrift
    EXPECT_EQ(checkAngleBracketIncludes(filename, tokens), 0);

    filename = "thrift/somefileForAngleBrackets.cpp";
    tokenize(s1, filename, tokens);

    EXPECT_EQ(checkAngleBracketIncludes(filename, tokens), 1);
  }
}

// testSkipTemplateSpec
unittest {
  Token[] tokens;
  string filename = "src.cpp";

  string content = "<A<B>>C";
  tokenize(content, filename, tokens);

  auto r = skipTemplateSpec(tokens);
  EXPECT_EQ(r.length != 0, true);
  if (r.length > 0) {
    EXPECT_EQ(r.front.type_, tk!">>");
  }
}

// allow /* may throw */ before constructor
unittest {
  string code = "
    struct Foo {
      Foo() {}
      /* may throw */ Foo(Foo &&) {}
    };
  ";
  string filename = "nofile.cpp";
  auto tokens = tokenize(code, filename);
  EXPECT_EQ(checkConstructors(filename, tokens), 0);
}

// testCheckExitStatus
unittest {
  string code = "
    exit(-1);
  ";
  string filename = "nofile.cpp";
  auto tokens = tokenize(code, filename);
  EXPECT_EQ(checkExitStatus(filename, tokens), 1);
  code = "
    _exit(-1);
  ";
  tokens = tokenize(code, filename);
  EXPECT_EQ(checkExitStatus(filename, tokens), 1);
  code = "
    ::exit(-1);
  ";
  tokens = tokenize(code, filename);
  EXPECT_EQ(checkExitStatus(filename, tokens), 1);
  code = "
    _exit(EXIT_FAILURE);
  ";
  tokens = tokenize(code, filename);
  EXPECT_EQ(checkExitStatus(filename, tokens), 0);
  code = "
    _exit(0);
  ";
  tokens = tokenize(code, filename);
  EXPECT_EQ(checkExitStatus(filename, tokens), 0);
  code = "
    validFunction(-1);
  ";
  tokens = tokenize(code, filename);
  EXPECT_EQ(checkExitStatus(filename, tokens), 0);
}

void main(string[] args) {
  enforce(c_mode == false);
}
