// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

#include "Checks.h"
#include "folly/File.h"
#include "folly/FileUtil.h"
#include "folly/String.h"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <boost/algorithm/string/replace.hpp>

using namespace std;
using namespace folly;
using namespace facebook::flint;
using namespace facebook;

DECLARE_bool(c_mode);

TEST(Lint, testTokenizer) {
  string s = "\
#include <stdio.h>\n                            \
int main() {\n                                  \
  printf(\"hello, world\");\n                   \
}\
";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  EXPECT_EQ(tokens.size(), 18);
  s = ":: () [] . -> ++ -- dynamic_cast static_cast reinterpret_cast \
const_cast typeid ++ -- ~ ! sizeof new delete * & + - .* ->* * / % << >> \
< > <= >= == != & ^ | && || ?: = *= /= %= += -= >>= <<= &= ^= |= ,";
  tokenize(s, filename, tokens);
  const TokenType witness[] = {
    TK_DOUBLE_COLON,
    TK_LPAREN,
    TK_RPAREN,
    TK_LSQUARE,
    TK_RSQUARE,
    TK_DOT,
    TK_ARROW,
    TK_INCREMENT,
    TK_DECREMENT,
    TK_DYNAMIC_CAST,
    TK_STATIC_CAST,
    TK_REINTERPRET_CAST,
    TK_CONST_CAST,
    TK_TYPEID,
    TK_INCREMENT,
    TK_DECREMENT,
    TK_TILDE,
    TK_NOT,
    TK_SIZEOF,
    TK_NEW,
    TK_DELETE,
    TK_STAR,
    TK_AMPERSAND,
    TK_PLUS,
    TK_MINUS,
    TK_DOT_STAR,
    TK_ARROW_STAR,
    TK_STAR,
    TK_DIVIDE,
    TK_REMAINDER,
    TK_LSHIFT,
    TK_RSHIFT,
    TK_LESS,
    TK_GREATER,
    TK_LESS_EQUAL,
    TK_GREATER_EQUAL,
    TK_EQUAL_TO,
    TK_NOT_ASSIGN,
    TK_AMPERSAND,
    TK_XOR,
    TK_BINARY_OR,
    TK_LOGICAL_AND,
    TK_LOGICAL_OR,
    TK_QUESTION,
    TK_COLON,
    TK_ASSIGN,
    TK_STAR_ASSIGN,
    TK_DIVIDE_ASSIGN,
    TK_REMAINDER_ASSIGN,
    TK_PLUS_ASSIGN,
    TK_MINUS_ASSIGN,
    TK_RSHIFT_ASSIGN,
    TK_LSHIFT_ASSIGN,
    TK_AND_ASSIGN,
    TK_XOR_ASSIGN,
    TK_OR_ASSIGN,
    TK_COMMA,
    TK_EOF,
  };

  EXPECT_EQ(tokens.size(), sizeof(witness)/sizeof(*witness));
  size_t j = 0;
  FOR_EACH (i, tokens) {
    EXPECT_EQ(i->type_, witness[j++]);
  }
}

// Test multiline single-line comment
TEST(Lint, testDoubleSlashComment) {
  string s = "\
int x;\
// This is a single-line comment\\\n\
that extends on multiple\\\n\
lines. Nyuk-nyuk...\n\
  float y;";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  //FOR_EACH (t, tokens) LOG(INFO) << t->toString();
  EXPECT_EQ(tokens.size(), 7);
  EXPECT_EQ(tokens[3].type_, TK_FLOAT);
}

// Test numeric literals
TEST(Lint, testNumbers) {
  string s = "\
.123 \
123.234 \
123.234e345 \
123.234e+345 \
0x234p4 \
0xabcde \
0x1.fffffffffffffp1023 \
0x1.0P-1074 \
0x0.0000000000001P-1022 \
";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  // All except the ending TK_EOF are numbers
  FOR_EACH_RANGE (t, tokens.begin(), tokens.end() - 1) {
    LOG(INFO) << t->toString();
    EXPECT_EQ(t->type_, TK_NUMBER);
  }
  EXPECT_EQ(tokens.size(), 10);
}

// Test identifiers
TEST(Lint, testIdentifiers) {
  string s = "\
x \
X \
xy \
Xy \
xY \
_x \
x_ \
x$ \
x_y \
x$y \
xy_ \
xy$ \
";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  // All except the ending TK_EOF are numbers
  FOR_EACH_RANGE (t, tokens.begin(), tokens.end() - 1) {
    LOG(INFO) << t->toString();
    EXPECT_EQ(t->type_, TK_IDENTIFIER);
  }
  EXPECT_EQ(tokens.size(), 13);
}

// Test sequences
TEST(Lint, testCheckBlacklistedSequences) {
  string filename = "nofile.cpp";
  string s = "\
asm volatile('mov eax, 10');\
volatile int foo;\n\
class Foo {\n\
operator+();\n\
}\n\
";
  vector<Token> tokens;
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkBlacklistedSequences(filename, tokens), 1);
}

// Test c_mode=true for sequences
TEST(Lint, testCheckBlacklistedSequencesWithCModeSet) {
  string filename = "nofile.cpp";
  string s = "\
asm volatile('mov eax, 10');\
volatile int foo;\n\
";
  vector<Token> tokens;
  tokenize(s, filename, tokens);
  FLAGS_c_mode = true;
  EXPECT_EQ(checkBlacklistedSequences(filename, tokens), 0);
  FLAGS_c_mode = false;
}

TEST(Lint, testCheckBlacklistedIdentifiers) {
  string filename = "nofile.cpp";

  string s = R"(
int main(int argc, char** argv) {
  auto p = strtok(argv[0], ',');
  while ((p = strtok(nullptr, ','))) {
    sleep(1);
  }
}
)";
  std::vector<Token> tokens;
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkBlacklistedIdentifiers(filename, tokens), 2);

  string s1 = R"(
int main(int argc, char** argv) {
  char* state;
  auto p = strtok_r(argv[0], ',', &state);
  while ((p = strtok_r(nullptr, ',', &state))) {
    sleep(1);
  }
}
)";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkBlacklistedIdentifiers(filename, tokens), 0);
}

// Test catch clauses in exceptions
TEST(Lint, testCatch) {
  string s = "\
try {} catch (Exception &) {}\n\
try {} catch (Exception & e) {}\n\
try {} catch (ns::Exception &) {}\n\
try {} catch (const ns::Exception & e) {}\n\
try {} catch (::ns::Exception &) {}\n\
try {} catch (const ::ns::Exception & e) {}\n\
try {} catch (typename ::ns::Exception &) {}\n\
try {} catch (const typename ns::Exception & e) {}\n\
try {} catch (Exception<t, (1 + 1) * 2> &) {}\n\
try {} catch (const Exception<(1)> & x) {}\n\
try {} catch (...) {}\n\
";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkCatchByReference(filename, tokens), 0);
}

TEST(Lint, testCheckIfEndifBalance) {
  string s = "\
#ifndef A\n\
#if B\n\
  #ifdef C\n\
  #endif\n\
  #if D || E\n\
  #else\n\
  #endif\n\
#else\n\
#endif\n\
#endif\n\
";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkIfEndifBalance(filename, tokens), 0);

string s2 = "\
#ifndef A\n\
#if B\n\
  #ifdef C\n\
  #endif\n\
#else\n\
#endif\n\
";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkIfEndifBalance(filename, tokens), 1);

string s3 = "\
#if B\n\
  #ifdef C\n\
  #endif\n\
#else\n\
#endif\n\
#endif\n\
";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkIfEndifBalance(filename, tokens), 1);

string s4 = "\
#ifndef A\n\
#endif\n\
#else\n\
";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkIfEndifBalance(filename, tokens), 1);
}

TEST(Lint, testCheckConstructors) {
  vector<Token> tokens;
  string filename = "nofile.h";
  string code;

  // Non-explicit single-arg constructors may inadvertently be used for
  // type conversion
  code = "\
class AA { \n\
  AA(int bad); \n\
  AA(AA *bad); \n\
}; \n\
struct BB { \n\
  BB(int bad); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 3);

  // Default arguments are a special case of the previous problem
  code = "\
class AA { \n\
  AA(int bad = 42); \n\
  AA(int bad, int j = 42); \n\
  AA(int bad = 42, int j = 42); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 3);

  // Constructors with zero or 2+ required arguments are safe
  code = "\
class AA { \n\
  AA(); \n\
  AA(void); \n\
  AA(int safe, int safe2); \n\
  AA(AA safe, AA safe2); \n\
  void dosomething(const int & safe, vector<AA> & safe2); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 0);

  // Single-arg constructors marked "explicit" are safe
  code = "\
class AA { \n\
  explicit AA(int safe); \n\
  explicit AA(int safe, int j = 42); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 0);

  // Suppress warnings when single-arg constructors are marked as implicit
  code = "\
class CC { \n\
  /* implicit */ CC(int acceptable); \n\
  /* implicit */ CC(int acceptable, int j = 42); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 0);

  // Could be "explicit constexpr" or "constexpr explicit" too
  code = "\
class AA { \n\
  explicit constexpr AA(int safe); \n\
  constexpr explicit AA(int* safe); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 0);

  // These copy/move constructors are reasonable
  code = "\
class AA { \n\
  AA(const AA& acceptable); \n\
  AA(AA&& acceptable); \n\
  AA(AA &&) = default; \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 0);

  // These move/copy constructors don't make sense
  code = "\
class AA { \n\
  AA(AA& shouldBeConst); \n\
  AA(const AA&& shouldNotBeConst); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 2);

  // Don't warn for single-argument std::initializer_list<...>
  code = "\
class AA { \n\
  AA(std::initializer_list<CC> args); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 0);

  // Verify that the parser handles nested blocks correctly
  code = "\
namespace AA { namespace BB { \n\
  class CC { \n\
    class DD { \n\
      DD(int bad);\n\
      void CC(int safe); \n\
    }; \n\
    void DD(int safe);\n\
    CC(int bad);\n\
  }; \n\
  void AA(int safe); \n\
} } \n\
void CC(int safe); \n\
void DD(int safe); \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 2);

  // Some special cases that are tricky to parse
  code = "\
template < class T, class Allocator = allocator<T> > \n\
class AA { \n\
  using namespace std; \n\
  static int i = 0; \n\
  typedef AA<T, Allocator> this_type; \n\
  friend class ::BB; \n\
  struct { \n\
    int x; \n\
    struct DD { \n\
      DD(int bad); \n\
    }; \n\
  } pt; \n\
  struct foo foo_; \n\
  AA(std::vector<CC> bad); \n\
  AA(T bad, Allocator jj = NULL); \n\
  AA* clone() const { return new AA(safe); } \n\
  void foo(std::vector<AA> safe) { AA(this->i); } \n\
  void foo(int safe); \n\
}; \n\
class CC : std::exception { \n\
  CC(const string& bad) {} \n\
  void foo() { \n\
    CC(localizeString(MY_STRING)); \n\
    CC(myString); \n\
    CC(4); \n\
    throw CC(\"ok\"); \n\
  } \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkConstructors(filename, tokens), 4);
}

TEST(Lint, testCheckImplicitCast) {
  vector<Token> tokens;
  string filename = "nofile.h";
  string code;

  // Test if it captures implicit bool cast
  code = "\
class AA { \n\
  operator bool(); \n\
}; \n\
struct BB { \n\
  operator bool(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 2);

  // Test if explicit and /* implicit */ whitelisting works
  code = "\
class AA { \n\
  explicit operator bool(); \n\
}; \n\
struct BB { \n\
  /* implicit */ operator bool(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 1);

  // It's ok to delete an implicit bool operator.
  code = R"(
class AA {
  /* implicit */ operator bool() = delete;
  /* implicit */ operator bool() const = delete;
};
)";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 0);

  // Test if it captures implicit char cast
  code = "\
class AA { \n\
  operator char(); \n\
}; \n\
struct BB { \n\
  operator char(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 2);

  // Test if explicit and /* implicit */ whitelisting works
  code = "\
struct Foo;\n\
class AA { \n\
  explicit operator Foo(); \n\
}; \n\
struct BB { \n\
  /* implicit */ operator Foo(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 0);

  // Test if it captures implicit uint8_t cast
  code = "\
class AA { \n\
  operator uint8_t(); \n\
}; \n\
struct BB { \n\
  operator uint8_t(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 2);

  // Test if it captures implicit uint8_t * cast
  code = "\
class AA { \n\
  operator uint8_t *(); \n\
}; \n\
struct BB { \n\
  operator uint8_t *(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 2);

  // Test if it captures implicit void * cast
  code = "\
class AA { \n\
  operator void *(); \n\
}; \n\
struct BB { \n\
  operator void *(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 2);

  // Test if it captures implicit class cast
  code = "\
class AA { \n\
}; \n\
class BB { \n\
  operator AA(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 1);

  // Test if it captures implicit class pointer cast
  code = "\
class AA { \n\
}; \n\
class BB { \n\
  operator AA *(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 1);

  // Test if it captures implicit class reference cast
  code = "\
class AA { \n\
}; \n\
class BB { \n\
  operator AA&(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 1);

  // Test if it captures template classes
  code = "\
template <class T> \n\
class AA { \n\
  T bb; \n\
  operator T(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 1);

  // Test if it avoids operator overloading
  code = "\
class AA { \n\
  int operator *() \n\
  int operator+(int i); \n\
  void foo(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 0);

  // Test if it captures template casts
  code = "\
template <class T> \n\
class AA { \n\
  operator std::unique_ptr<T>(); \n\
}; \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 1);

  // Test if it avoids definitions
  code = "\
class AA { \n\
  int bb \n\
  operator bool(); \n\
}; \n\
AA::operator bool() { \n\
  return bb == 0;\n\
} \n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 1);

  // explicit constexpr is still explicit
  code = "\
class Foo {\n\
  explicit constexpr operator int();\n\
};\n\
";
  tokenize(code, filename, tokens);
  EXPECT_EQ(checkImplicitCast(filename, tokens), 0);
}

// Test non-virtual destructor detection
TEST(Lint, testCheckVirtualDestructors) {

  string s ="\
class AA {\
public:\
  ~AA();\
  virtual int foo();\
  void aa();\
};";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  s = "\
class AA {\
public:\
private:\
  virtual void bar();\
public:\
  ~AA();\
};";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  s = "\
class AA {\
public:\
  virtual void aa();\
};";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  // Namespace
  s = "\
class AA::BB {\
  public:\
    virtual void foo();\
  ~BB();\
};";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  // Struct
  s = "\
struct AA {\
  virtual void foo() {}\
  ~AA();\
};";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 1);

  // Nested class
  s = "\
class BB {\
public:\
  ~BB();\
  class CC {\
    virtual int bar();\
  };\
  virtual void foo();\
};";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 2);

  // Not a base class; don't know if AA has a virtual dtor
  s = "\
class BB : public AA{\
  virtual foo() {}\
};";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 0);

  // Protected dtor
  s = "\
class BB {\
  protected:\
    ~BB();\
  public:\
    virtual void foo();\
};";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 0);

  // Private dtor
  s = "\
class BB {\
  ~BB();\
  public:\
    virtual void foo();\
};";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkVirtualDestructors(filename, tokens), 0);
}

// Test include guard detection
TEST(Lint, testCheckIncludeGuard) {
  string s = "\
//comment\n\
/*yet\n another\n one\n*/\n\
#ifndef TEST_H\n\
#define TEST_H\n\
class A { };\n\
#if TRUE\n\
  #ifdef TEST_H\n\
  class B { };\n\
  #endif\n\
class C { };\n\
#else\n\
#endif\n\
class D { };\n\
#endif\n\
";
  vector<Token> tokens;
  string filename = "nofile.h";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 0);

string s2 = "\
#ifndef TEST_H\n\
#define TeST_h\n\
class A { };\n\
#if TRUE\n\
  #ifdef TEST_H\n\
  class B { };\n\
  #endif\n\
class C { };\n\
#else\n\
#endif\n\
class D { };\n\
#endif\n\
#ifdef E_H\n\
class E { };\n\
#endif\n\
";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 2);

string s3 = "\
class A { };\n\
#if TRUE\n\
  #ifdef TEST_H\n\
  class B { };\n\
  #endif\n\
class C { };\n\
#else\n\
#endif\n\
class D { };\n\
";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 1);
  filename = "nofile.cpp";
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 0);

  string s4 = "\
#pragma once\n\
class A { };\n\
#if TRUE\n\
  #ifdef TEST_H\n\
  class B { };\n\
  #endif\n\
class C { };\n\
#else\n\
#endif\n\
class D { };\n\
";
  tokenize(s4, filename, tokens);
  filename = "nofile.h";
  EXPECT_EQ(checkIncludeGuard(filename, tokens), 0);
}

TEST(Lint, testCheckInitializeFromItself) {
  string s = "\
namespace whatever {\n\
ClassFoo::ClassFoo(int memberBar, int memberOk, int memberBaz)\n\
  : memberBar_(memberBar_)\n\
  , memberOk_(memberOk)\n\
  , memberBaz_(memberBaz_) {\n\
}\n\
}\n\
";
  vector<Token> tokens;
  string filename = "nofile.h";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkInitializeFromItself(filename, tokens), 2);

  string s1 = "\
namespace whatever {\n\
ClassFooPOD::ClassFooPOD(int memberBaz) :\n\
  memberBaz(memberBaz) {\n\
}\n\
}\n\
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkInitializeFromItself(filename, tokens), 0);
}

TEST(Lint, testCheckUsingDirectives) {
  string s = "\
namespace whatever {\n\
  void foo() {\n\
    using namespace ok;\n\
  }\n\
}\n\
namespace facebook {\n\
  namespace whatever {\n\
    class Bar {\n\
      void baz() {\n\
        using namespace ok;\n\
    }};\n\
}}\n\
void qux() {\n\
  using namespace ok;\n\
}\n\
";
  vector<Token> tokens;
  string filename = "nofile.h";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkUsingDirectives(filename, tokens), 0);

  string s1 = "\
namespace facebook {\n\
\
  namespace { void unnamed(); }\
  namespace fs = boost::filesystem;\
\
  void foo() { }\n\
  using namespace not_ok;\n\
  namespace whatever {\n\
    using namespace not_ok;\n\
    namespace facebook {\n\
    }\n\
  }\n\
}\n\
using namespace not_ok;\n\
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkUsingDirectives(filename, tokens), 4);

  string s2 = "\
void foo() {\n\
  namespace fs = boost::filesystem;\n\
}";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkUsingDirectives(filename, tokens), 0);
}

TEST(Lint, testCheckUsingNamespaceDirectives) {
  string filename = "nofile.cpp";
  vector<Token> tokens;

#define RUN_THIS_TEST(string, expectedResult)                                 \
do {                                                                          \
  tokenize(string, filename, tokens);                                         \
  EXPECT_EQ(checkUsingNamespaceDirectives(filename, tokens), expectedResult); \
} while (0);

  string nothing = "";
  RUN_THIS_TEST(nothing, 0);

  string simple = "using namespace std;";
  RUN_THIS_TEST(simple, 0);

  string simpleFail = "\
using namespace std;\n\
using namespace boost;\n\
";
  RUN_THIS_TEST(simpleFail, 1);

  // 2 directives in separate scopes should not conflict
  string separateBlocks = "\
{\n\
  using namespace std;\n\
}\n\
{\n\
  using namespace boost;\n\
}\n\
";
  RUN_THIS_TEST(separateBlocks, 0);

  // need to catch directives if they are nested in scope of other directive
  string nested = "\
{\n\
  using namespace std;\n\
  {\n\
    using namespace boost;\n\
  }\n\
}\n\
";
  RUN_THIS_TEST(nested, 1);

  // same as last case, but without outer braces
  string nested2 = "\
using namespace std;\n\
{\n\
  using namespace boost;\n\
}\n\
";
  RUN_THIS_TEST(nested, 1);

  // make sure doubly nesting things is not a problem
  string doubleNested = "\
{\n\
  using namespace std;\n\
  {\n\
    {\n\
      using namespace boost;\n\
    }\n\
  }\n\
}\n\
";
  RUN_THIS_TEST(doubleNested, 1);

  // same as last case, but without outer braces
  string doubleNested2 = "\
using namespace std;\n\
{\n\
  {\n\
    using namespace boost;\n\
  }\n\
}\n\
";
  RUN_THIS_TEST(doubleNested, 1);

  // duplicates using namespace declaratives
  string dupe = "\
using namespace std;\n\
using namespace std;\n\
";
  RUN_THIS_TEST(dupe, 1);

  // even though "using namespace std;" appears twice above "using namespace
  // boost;", there should only be 1 error from that line (and 1 from the
  // duplication)
  string dupe2 = "\
using namespace std;\n\
using namespace std;\n\
using namespace boost;\n\
";
  RUN_THIS_TEST(dupe2, 2);

  // even though the third line has 2 errors (it's a duplicate and it
  // conflicts with the 2nd line), it should only result in the duplicate
  // errorand short circuit out of the other
  string dupe3 = "\
using namespace std;\n\
using namespace boost;\n\
using namespace std;\n\
";
  RUN_THIS_TEST(dupe3, 2);

  // only 2 errors total should be generated even though the 3rd line
  // conflicts with both of the first 2
  string threeWay = "\
using namespace std;\n\
using namespace boost;\n\
using namespace std;\n\
";
  RUN_THIS_TEST(threeWay, 2);

  // make sure nesting algorithm realizes second using namespace std; is a
  // dupe and doesn't remove it from the set of visible namespaces
  string nestedDupe = "\
using namespace std;\n\
{\n\
  using namespace std;\n\
}\n\
using namespace std;\n\
";
  RUN_THIS_TEST(nestedDupe, 2);

  string other1 = "\
{\n\
using namespace std;\n\
using namespace std;\n\
}\n\
using namespace boost;\n\
";
  RUN_THIS_TEST(other1, 1);

#undef RUN_THIS_TEST
}

TEST(Lint, testThrowsSpecification) {
  string s1 = "struct foo { void function() throw(); };";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 1);

  string s2 = "\
void func() {\n\
  throw (std::runtime_error(\"asd\");\n\
}\n\
";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s3 = "\
struct something {\n\
  void func() throw();\n\
  void func2() noexcept();\n\
  void func3() throw(char const* const*);\n\
  void wat() const {\n\
    throw(12);\n\
  }\n\
};\n\
";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 2);

  string s4 = "struct A { void f1() const throw();\n\
                          void f2() volatile throw();\n\
                          void f3() const volatile throw();\n\
                          void f4() volatile const throw();\n\
                          void f5() const;\n\
                          void f6() volatile;\n\
                          void f7() volatile const;\n\
                          void f8() volatile const {}\n\
                        };";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 4);

  string s5 = "void f1();";
  tokenize(s5, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s6 = "void f1(void(*)(int,char)) throw();";
  tokenize(s6, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 1);

  string s7 = "void f() { if (!true) throw(12); }";
  tokenize(s7, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s8 = "namespace foo {\n\
                 struct bar {\n\
                   struct baz {\n\
                     void f() throw(std::logic_error);\n\
                   };\n\
                   struct huh;\n\
                 };\n\
                 using namespace std;\n\
                 struct bar::huh : bar::baz {\n\
                   void f2() const throw() { return f(); }\n\
                 };\n\
                 namespace {\n\
                   void func() throw() {\n\
                     if (things_are_bad()) {\n\
                       throw 12;\n\
                     }\n\
                   }\n\
                 }\n\
                 class one_more { void eh() throw(); };\n\
               }";
  tokenize(s8, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 4);

  string s9 = "struct foo : std::exception {\
    virtual void what() const throw() = 0; };";
  tokenize(s9, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s10 = "template<class A> void f() { throw(12); }\n\
                template<template<class> T, class Y> void g()\n\
                  { throw(12); }\n\
                const int a = 2; const int b = 12;\n\
                template<class T, bool B = (a < b)> void h()\n\
                  { throw(12); }\n";
  tokenize(s10, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s11 = "const int a = 2; const int b = 12;\n\
                template<bool B = (a < b)> void f() throw() {}\n\
                void g() throw();\n";
  tokenize(s11, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 2);

  string s12 = "struct Foo : std::exception { ~Foo() throw(); };";
  tokenize(s12, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s13 = "struct Foo : std::exception { ~Foo() throw() {} };";
  tokenize(s13, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s14 = "struct Foo : std::exception { ~Foo() throw() {} "
    "virtual const char* what() const throw() {} };";
  tokenize(s14, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);

  string s15 = "struct Foo { const char* what() const throw() {}"
    "~Foo() throw() {} };";
  tokenize(s15, filename, tokens);
  EXPECT_EQ(checkThrowSpecification(filename, tokens), 0);
}

TEST(Lint, testProtectedInheritance) {
  string s1 = "class foo { }";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 0);

  string s2 = "class foo : public bar { }";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 0);

  string s3 = "class foo : protected bar { }";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 1);

  string s4 = "class foo : public bar { class baz : protected bar { } }";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 1);

  string s5 = "class foo : protected bar { class baz : public bar { } }";
  tokenize(s5, filename, tokens);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 1);

  string s6 = "class foo : protected bar { class baz : protected bar { } }";
  tokenize(s6, filename, tokens);
  EXPECT_EQ(checkProtectedInheritance(filename, tokens), 2);
}

TEST(Lint, testExceptionInheritance) {
  vector<Token> tokens;
  string filename = "nofile.cpp", s;

  s = "class foo { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "class foo: exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: std::exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private std::exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: protected exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: protected std::exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: public exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "class foo: public std::exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: std::exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: private exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "struct foo: private std::exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "struct foo: protected exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: protected std::exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: public exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: public std::exception { }";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "class bar: public std::exception {class foo: exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception {class foo: std::exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception {class foo: private exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception "
    "{class foo: private std::exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception "
    "{class foo: protected exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception "
    "{class foo: protected std::exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: public std::exception {class foo: public exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "class bar: std::exception {class foo { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: std::exception {class foo: exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: std::exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: private exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: private std::exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: protected exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: protected std::exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 2);

  s = "class bar: std::exception {class foo: public exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class bar: std::exception {class foo: public std::exception { } c;}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo; class bar: std::exception {}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: public bar, std::exception {}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: public bar, private std::exception {}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private bar, std::exception {}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private bar, public baz, std::exception {}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);

  s = "class foo: private bar::exception {}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: public bar, std::exception {}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 0);

  s = "struct foo: public bar, private std::exception {}";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkExceptionInheritance(filename, tokens), 1);
}

static bool regularFileExists(const char* filename) {
    struct stat st;
    return !stat(filename, &st) && S_ISREG(st.st_mode);
}

static bool writeStringToFile(const StringPiece data, const char* filename) {
  auto f = File(filename, O_CREAT | O_WRONLY | O_TRUNC);
  return writeFull(f.fd(), data.data(), data.size()) == data.size();
}

TEST(Lint, DISABLED_testCxxReplace) {
  if (!regularFileExists("_bin/linters/flint/cxx_replace")) {
    // No prejudice if the file is missing (this may happen e.g. if
    // running fbmake runtests with -j)
    LOG(WARNING) << "_bin/linters/flint/cxx_replace is missing"
      " so it cannot be tested this time around. This may happen"
      " during a parallel build.";
    return;
  }
  string s1 = "\
          tokenLen = 0;\n\
          pc = pc1;\n\
          pc.advance(strlen(\"line\"));\n\
          line = parse<size_t>(pc);\n\
          munchSpaces(pc);\n\
          if (pc[0] != '\n') {\n\
            for (size_t i = 0; ; ++i) {\n\
              ENFORCE(pc[i]);\n\
              if (pc[i] == '\n') {\n\
                file = munchChars(pc, i);\n\
                ++line;\n\
                break;\n\
              }\n\
            }\n\
          }\n\
";
  const char* f = "/tmp/cxx_replace_testing";
  EXPECT_TRUE(writeStringToFile(s1, f));
  EXPECT_EQ(system(("_bin/linters/flint/cxx_replace 'munch' 'crunch' "
                    + string(f)).c_str()),
            0);
  string witness;
  EXPECT_TRUE(readFile(f, witness));
  EXPECT_EQ(witness, s1);
  EXPECT_EQ(system(("_bin/linters/flint/cxx_replace 'break' 'continue' "
                    + string(f)).c_str()),
            0);
  string s2 = s1;
  boost::replace_all(s2, "break", "continue");
  EXPECT_TRUE(readFile(f, witness));
  EXPECT_EQ(witness, s2);
}

TEST(Lint, testThrowsHeapAllocException) {
  string s1 = "throw new MyException(\"error\");";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkThrowsHeapException(filename, tokens), 1);

  string s2 = "throw new (MyException)(\"error\");";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkThrowsHeapException(filename, tokens), 1);

  string s3 = "throw new MyTemplatedException<arg1, arg2>();";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkThrowsHeapException(filename, tokens), 1);

  string s4 = "throw MyException(\"error\")";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkThrowsHeapException(filename, tokens), 0);
}

TEST(Lint, testHPHPCalls)
{
  vector<Token> tokens;
  string filename = "nofile.cpp";

  // basic pattern
  string s1 = "using namespace HPHP; \n\
               void f() {\n\
                 f_require_module(\"soup\");\n\
                 f_someother_func(1,2,3);\n\
               };";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 0);

  // alternative namespace syntax
  string s2 = "using namespace ::HPHP; \n\
               void f() {\n\
                 f_require_module(\"soup\");\n\
                 f_someother_func(1,2,3);\n\
               };";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 0);

  // reversed order of usage
  string s3 = "using namespace ::HPHP; \n\
               void f() {\n\
                 f_someother_func(1,2,3);\n\
                 f_require_module(\"soup\");\n\
               };";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 1);

  // all ways to fail without namespace open
  string s4 = "HPHP::f_someother_func(1,2,3);\n\
               ::HPHP::f_someother_func(1,2);\n\
               HPHP::c_className::m_mfunc(1,2);\n\
               ::HPHP::c_className::m_mfunc(1,2);\n\
               HPHP::k_CONSTANT;\n\
               ::HPHP::k_CONSTANT;\n\
               HPHP::ft_sometyped_func(1,2);\n\
               ::HPHP::ft_sometyped_func(1,2);";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 8);

  // all ways to fail with namespace open
  string s5 = "using namespace HPHP;\n\
               HPHP::f_someother_func(1,2,3);\n\
               ::HPHP::f_someother_func(1,2);\n\
               HPHP::c_className::m_mfunc(1,2);\n\
               ::HPHP::c_className::m_mfunc(1,2);\n\
               HPHP::k_CONSTANT;\n\
               ::HPHP::k_CONSTANT;\n\
               HPHP::ft_sometyped_func(1,2);\n\
               ::HPHP::ft_sometyped_func(1,2);\n\
               f_someother_func(1,2);\n\
               c_className::m_mfunc(1,2);\n\
               k_CONSTANT;\n\
               ft_sometyped_func(1,2);";
  tokenize(s5, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 12);

  // all checks pass after f_require_module
  string s6 = "using namespace HPHP;\n\
               f_require_module(\"some module\");\n\
               HPHP::f_someother_func(1,2,3);\n\
               ::HPHP::f_someother_func(1,2);\n\
               HPHP::c_className::m_mfunc(1,2);\n\
               ::HPHP::c_className::m_mfunc(1,2);\n\
               HPHP::k_CONSTANT;\n\
               ::HPHP::k_CONSTANT;\n\
               HPHP::ft_sometyped_func(1,2);\n\
               ::HPHP::ft_sometyped_func(1,2);\n\
               f_someother_func(1,2);\n\
               c_className::m_mfunc(1,2);\n\
               k_CONSTANT;\n\
               ft_sometyped_func(1,2);";
  tokenize(s6, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 0);

  // check that c_str is exempt
  string s7 = "using namespace HPHP;\n\
               string c(\"hphp\"); c.c_str();";
  tokenize(s7, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 0);

  // check incorrect syntax
  string s8 = "using namespace ? garbage";
  tokenize(s8, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 1);

  // check that nested namespace usage doesn't confuse us
  string s9 = "using namespace ::HPHP; \n\
               {\n\
                 { using namespace HPHP; }\n\
                 f_someother_func(1,2,3);\n\
               };";
  tokenize(s9, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 1);

  // check that f_require_module is valid only in HPHP namespace
  string s10 = "f_require_module(\"meaningless\");\n\
               using namespace HPHP; \n\
               {\n\
                 std::f_require_module(\"cake\");\n\
                 f_someother_func(1,2,3);\n\
                 HPHP::c_classOops::mf_func(1);\n\
               };";
  tokenize(s10, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 2);

  // check that leaving HPHP scope re-requires module
  // this is arguable as a point of correctness, but better to
  // conservatively warn than silently fail
string s11 = "int f1() { using namespace HPHP; f_require_module(\"foo\");}\n\
                int second_entry_point() { using namespace HPHP; f_oops(); }";
  tokenize(s11, filename, tokens);
  EXPECT_EQ(checkHPHPNamespace(filename, tokens), 1);
}

TEST(Lint, testCheckDeprecatedIncludes) {
  vector<Token> tokens;
  string filename = "dir/TestFile.cpp";
  // sanity
  string s0 = "#include \"TestFile.h\"\
              #include \"foo.h\"";
  tokenize(s0, filename, tokens);

  EXPECT_EQ(checkDeprecatedIncludes(filename, tokens), 0);

  // error case with one deprecated include
  string s1 = "#include \"TestFile.h\"\
              #include \"common/base/Base.h\"";
  tokenize(s1, filename, tokens);

  EXPECT_EQ(checkDeprecatedIncludes(filename, tokens), 1);

  // error case with two deprecated includes
  // (since only Base.h is deprecated, we're replicating
  // it twice here
  string s2 = "#include \"TestFile.h\"\
              #include \"common/base/Base.h\"\
              #include \"common/base/Base.h\"";
  tokenize(s2, filename, tokens);

  EXPECT_EQ(checkDeprecatedIncludes(filename, tokens), 2);
}

TEST(Lint, testCheckIncludeAssociatedHeader){
  vector<Token> tokens;
  string filename = "dir/TestFile.cpp";
  // sanity
  string s0 = "#include \"TestFile.h\"\
              #include \"SomeOtherFile.h\"";
  tokenize(s0, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  filename = "TestFile.cpp";
  // sanity hpp
  string s1 = "#include \"TestFile.hpp\"\
              #include \"SomeOtherFile\"";
  tokenize(s1, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // defines and pragmas before
  string s2 = "#pragma option -O2\
              #define PI 3.14\
              #include \"TestFile.h\"\
              #include \"SomeOtherFile.h\"";
  tokenize(s2, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // no associated header
  string s3 = "#include \"<vector>\"\
              #include \"SomeOtherFile.h\"";
  tokenize(s3, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // case sensitiveness
  string s4 = "#include \"testfile.h\"\
              #include \"SomeOtherFile.h\"";
  tokenize(s4, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // no good
  string s5 = "#include \"<vector>\"\
              #include \"SomeOtherFile.h\"\
              #pragma option -O2\
              #include \"TestFile.h\"";
  tokenize(s5, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // no good
  string s6 = "#include \"<vector>\"\
              #include \"SomeOtherFile.h\"\
              #pragma option -O2\
              #include \"TestFile.hpp\"\
              #include \"Dijkstra.h\"";
  tokenize(s6, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // erroneous file
  string s7 = "#include #include #include";
  tokenize(s7, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // no good using relative path
  filename = "../TestFile.cpp";
  string s8 = "#include \"<vector>\"\
              #include \"SomeOtherFile.h\"\
              #pragma option -O2\
              #include \"TestFile.h\"";
  tokenize(s8, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // good using relative path
  string s9 = "#include <vector>\
              #include \"SomeOtherFile.h\"\
              #pragma option -O2\
              #include \"../TestFile.h\"";
  tokenize(s9, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // good when header having same name under other directory
  filename = "dir/TestFile.cpp";
  string s10 = "#include \"SomeOtherDir/TestFile.h\"";
  tokenize(s10, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);

  // testing not breaking relative path
  filename = "TestFile.cpp";
  string s11 = "#include <vector>\
                #include \"TestFile.h\"";
  tokenize(s11, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // no good using absolute path for filename
  filename = "/home/philipp/fbcode/test/testfile.cpp";
  string s12 = "#include <vector>\
                #include \"file.h\"\
                #include \"test/testfile.h\"";
  tokenize(s12, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 1);

  // good using absolute path for filename
  string s13 = "#include <vector>\
                #include \"file.h\"\
                #include \"othertest/testfile.h\"";
  tokenize(s13, filename, tokens);

  EXPECT_EQ(checkIncludeAssociatedHeader(filename, tokens), 0);
}

TEST(Lint, testMemset) {
  string filename = "nofile.cpp";
  vector<Token> tokens;

  // good calls
  string s1 = "memset(foo, 0, sizeof(foo));\n\
               memset(foo, 1, sizeof(foo));\n\
               memset(foo, 12, 1)\n\
               memset(T::bar(this, is, a pointers),(12+3)/5, 42);\n\
               memset(what<A,B>(12,1), 0, sizeof(foo));\n\
               memset(this->get<A,B,C>(12,1), 0, sizeof(foo));\n\
               memset(&foo, 0, sizeof(foo));";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 0);

  // funky call that would terminate all subsequent check
  string s1b = "memset(SC, a < b ? 0 : 1, sizeof(foo));\n";
  tokenize(s1b, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 0);

  // bad call with 0
  string s2 = "memset(foo, 12, 0);";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 1);

  // bad call with 1
  string s3 = "memset(foo, sizeof(bar), 1);";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 1);

  // combination of bad calls
  string s4 = s2 + s3;
  s4 += "memset(T1::getPointer(), sizeof(T2), 0);\n\
         memset(&foo, sizeof(B), 1);";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 4);

  // combination of good and bad calls
  string s5 = s1 + s4;
  tokenize(s5, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 4);

  // combination of bad calls and funky calls
  string s5b = s1b + s4;
  tokenize(s5b, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 0);

  // combination of bad calls and funky calls, different way
  string s6 = s4 + s1b;
  tokenize(s6, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 4);

  // process only function calls
  string s7 = "using std::memset;";
  tokenize(s7, filename, tokens);
  EXPECT_EQ(checkMemset(filename, tokens), 0);
}

TEST(Lint, testCheckInlHeaderInclusions) {
  std::vector<Token> tokens;

  string s1 = "\
#include \"Foo.h\"\n\
#include \"Bar-inl.h\"\n\
#include \"foo/baz/Bar-inl.h\"\n\
\n\
int main() {}\n\
";

  std::string filename = "...";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkInlHeaderInclusions("Foo.h", tokens), 2);
  EXPECT_EQ(checkInlHeaderInclusions("Bar.h", tokens), 0);

  string s2 = "\
#include \"Foo-inl.h\"\n\
";
  std::string filename2 = "FooBar.h";
  tokenize(s2, filename2, tokens);
  EXPECT_EQ(checkInlHeaderInclusions(filename2, tokens), 1);
}

TEST(Lint, testUpcaseNull) {
  std::vector<Token> tokens;
  std::string filename = "...";

  std::string s1 = "\
#include <stdio.h>\n\
int main() { int x = NULL; }\n\
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkUpcaseNull(filename, tokens), 1);

  std::string s2 = "int main() { int* x = nullptr; }\n";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkUpcaseNull(filename, tokens), 0);
}


TEST(Lint, testSmartPtrUsage) {
  std::vector<Token> tokens;
  std::string filename = "...";

  // test basic pattern with known used namespaces
  std::string s1 = "\
std::shared_ptr<Foo> p(new Foo(whatever));\
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  std::string s2 = "\
boost::shared_ptr<Foo> p(new Foo(whatever));\
";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  std::string s3 = "\
facebook::shared_ptr<Foo> p(new Foo(whatever)); }\
";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  std::string s4 = "\
shared_ptr<Foo> p(new Foo(whatever)); }\
";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  // this matches and suggests using allocate_shared
  std::string s10 = "\
shared_ptr<Foo> p(new Foo(whatever), d, a); }\
";
  tokenize(s10, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  // this matches without any special suggestion but using
  // make_shared, Deleter are not well supported in make_shared
  std::string s11 = "\
shared_ptr<Foo> p(new Foo(whatever), d); }\
";
  tokenize(s11, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  // test in a more "complex" code
  std::string s12 = "\
int main() { std::shared_ptr<Foo> p(new Foo(whatever)); }\
";
  tokenize(s12, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 1);

  // function declarations are ok
  std::string s14 = "\
std::shared_ptr<Foo> foo(Foo foo);\
";
  tokenize(s14, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 0);

  // assignments are fine
  std::string s16 = "\
std::shared_ptr<Foo> foo = foo();\
";
  tokenize(s16, filename, tokens);
  EXPECT_EQ(checkSmartPtrUsage(filename, tokens), 0);
}


TEST(Lint, testUniquePtrUsage) {
  std::vector<Token> tokens;
  std::string filename = "...";

  std::string s1 = "\
std::unique_ptr<Foo> p(new Foo(whatever));\
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

//Should skip this, since its not of namespace std
  std::string s2 = "\
boost::unique_ptr<Foo> p(new Foo[5]);\
";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  // this matches and suggests using array in the template
  std::string s3 = "\
unique_ptr<Foo> p(new Foo[5]);\
";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  // this should not match
  std::string s4 = "\
shared_ptr<Foo> p(new Foo(Bar[5]));\
";
  tokenize(s4, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  // test in a more "complex" code
  std::string s5 = "\
int main() { std::unique_ptr<Foo> p(new Foo[5]);\
 std::unique_ptr<Foo[]> p(new Foo[5]);\
 unique_ptr<Bar> q(new Bar[6]);}";
  tokenize(s5, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 2);

  std::string s6 = "\
std::unique_ptr< std::unique_ptr<int[]> >\
  p(new std::unique_ptr<int[]>(new int[2]));\
";
  tokenize(s6, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  std::string s7 = "\
std::unique_ptr< std::unique_ptr<int[]> > p(new std::unique_ptr<int[]>[6]);";
  tokenize(s7, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  // temporaries
  std::string s8 = "\
std::unique_ptr<int[]>(new int());";
  tokenize(s8, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  std::string s9 = "\
std::unique_ptr<int>(new int());";
  tokenize(s9, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  std::string s10 = "\
std::unique_ptr<int>(new int[5]);";
  tokenize(s10, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  std::string s11 = "\
std::unique_ptr<int[]>(new int[5]);";
  tokenize(s11, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  std::string s12 = R"(
std::unique_ptr<Foo[]> function() {
  std::unique_ptr<Foo[]> ret;
  return ret;
}
)";
  tokenize(s12, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  std::string s13 = R"(
std::unique_ptr<
  std::unique_ptr<int> > foo(new std::unique_ptr<int>[12]);
)";
  tokenize(s13, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 1);

  std::string s14 = R"(
void function(std::unique_ptr<int> a,
              std::unique_ptr<int[]> b = std::unique_ptr<int[]>()) {
}
)";
  tokenize(s14, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  std::string s15 = R"(
int main() {
  std::vector<char> args = something();
  std::unique_ptr<char*[]> p(new char*[args.size() + 1]);
}
)";
  tokenize(s15, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

  std::string s16 = R"(
int main() {
  std::vector<char> args = something();
  std::unique_ptr<char const* volatile**[]> p(
    new char const* volatile*[args.size() + 1]
  );
}
)";
  tokenize(s16, filename, tokens);
  EXPECT_EQ(checkUniquePtrUsage(filename, tokens), 0);

}

TEST(Lint, testThreadSpecificPtr) {
  std::vector<Token> tokens;
  std::string filename = "...";

  std::string s1 = "\
int main() {\
  boost::thread_specific_ptr<T> p;\
}\
";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkBannedIdentifiers(filename, tokens), 1);

  std::string s2 = "\
int main() {\
  folly::ThreadLocalPtr<T> p;\
}\
}";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkBannedIdentifiers(filename, tokens), 0);
}

TEST(Lint, testNamespaceScopedStatics) {
  std::vector<Token> tokens;
  std::string filename = "somefile.h";

  std::string s1 = "\
  namespace bar {\
    static const int x = 42;\
    static inline getX() { return x; }\
    static void doFoo();\
    int getStuff() {\
      static int s = 22;\
      return s;\
    }\
  }";
  tokenize(s1, filename, tokens);
  EXPECT_EQ(checkNamespaceScopedStatics(filename, tokens), 3);

  std::string s2 = "\
  class bar;\
  namespace foo {\
    static const int x = 42;\
  }";
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkNamespaceScopedStatics(filename, tokens), 1);

  std::string s3 = "\
  namespace bar {\
    class Status {\
    public:\
      static Status OK() {return 22;}\
    };\
    static void doBar();\
  }\
  static void doFoo();";
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkNamespaceScopedStatics(filename, tokens), 2);
}

TEST(Lint, testCheckMutexHolderHasName) {
  string s = "\
    void foo() {\n\
      lock_guard<x> ();\n\
      std::lock_guard<std::mutex>(m_lock);\n\
      lock_guard<std::mutex>(m_lock);\n\
      std::unique_lock<std::mutex>(m_lock);\n\
      unique_lock<mutex>(m_lock);\n\
    }\n\
   ";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkMutexHolderHasName(filename, tokens), 3);

  string s2 = "\
    void foo() {\n\
      vector<int> ();\n\
      lock_guard<x> s(thing);\n\
      unique_lock<std::mutex> l(m_lock);\n\
    }\n\
   ";
  tokens.clear();
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkMutexHolderHasName(filename, tokens), 0);

  string s3 = "\
    void foo(std::lock_guard<std::mutex>& m, lock_guard<x>* m2) {\n\
    }\n\
   ";
  tokens.clear();
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkMutexHolderHasName(filename, tokens), 0);
}

TEST(Lint, testOSSIncludes) {
  string s =
    "#include <super-safe>\n"
    "#include <braces/are/safe>\n"
    "#include \"no-slash-is-safe\"\n"
    "#include \"folly/is/safe\"\n"
    "#include \"hphp/is/safe/in/hphp\"\n"
    "#include \"hphp/facebook/hphp-facebook-is-safe-subdirectory\"\n"
    "#include \"random/unsafe/in/oss\"\n"
    "#include \"oss-is-safe\" // nolint\n"
    "#include \"oss/is/safe\" // nolint\n"
    "#include \"oss-at-eof-should-be-safe\" // nolint";

  pair<string, int> filenamesAndErrors[] = {
    { "anyfile.cpp", 0 },
    { "non-oss-project/anyfile.cpp", 0 },
    { "folly/anyfile.cpp", 3 },
    { "hphp/anyfile.cpp", 1 },
    { "hphp/facebook/anyfile.cpp", 0 },
  };
  for (const auto& p : filenamesAndErrors) {
    cout << p.first << endl;
    vector<Token> tokens;
    tokenize(s, p.first, tokens);
    EXPECT_EQ(p.second, checkOSSIncludes(p.first, tokens));
  }
}

TEST(Lint, testCheckBreakInSynchronized) {
  string s = "\
    int foo() {\n\
      int i = 1;\n\
      Synchronized<vector<int>> v;\n\
      while(i > 0) {\n\
        SYNCHRONIZED (v) {\n\
          if(v.size() > 0) break; \n\
        }\n\
        i--;\n\
      };\n\
    }\n\
   ";
  vector<Token> tokens;
  string filename = "nofile.cpp";
  tokenize(s, filename, tokens);
  EXPECT_EQ(checkBreakInSynchronized(filename, tokens), 1);

  string s2 = "\
    void foo() {\n\
      Synchronized<vector<int>> v;\n\
      for(int i = 10; i < 0; i--) { \n\
        if(i < 1) break; \n\
        SYNCHRONIZED(v) { \n\
          if(v.size() > 5) break; \n\
          while(v.size() < 5) { \n\
            v.push(1); \n\
            if(v.size() > 3) break; \n\
          } \n\
          if(v.size() > 4) break; \n\
        } \n\
        i--; \n\
        if(i > 2) continue; \n\
      }\n\
    }\n\
   ";
  tokens.clear();
  tokenize(s2, filename, tokens);
  EXPECT_EQ(checkBreakInSynchronized(filename, tokens), 2);

  string s3 = "\
    void foo() {\n\
      Synchronized<vector<int>> v;\n\
      SYNCHRONIZED_CONST(v) {\n\
        for(int i = 0; i < v.size(); i++) {\n\
          if(v[i] == 5) break\n\
        }\n\
      }\n\
    }\n\
   ";

  tokens.clear();
  tokenize(s3, filename, tokens);
  EXPECT_EQ(checkBreakInSynchronized(filename, tokens), 0);
}
