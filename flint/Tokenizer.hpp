#pragma once

#include <cstring>
#include <string>
#include <vector>

using namespace std;

namespace flint {

	/**
	* Higher-order macro that applies "apply" (which itself is usually a
	* macro) to each C++ token that consists of one character and is not
	* a prefix of another token.
	*/
#define CPPLINT_FORALL_ONE_CHAR_TOKENS(apply)						\
	apply('~', TK_TILDE)											\
	apply('(', TK_LPAREN)											\
	apply(')', TK_RPAREN)											\
	apply('[', TK_LSQUARE)											\
	apply(']', TK_RSQUARE)											\
	apply('{', TK_LCURL)											\
	apply('}', TK_RCURL)											\
	apply(';', TK_SEMICOLON)										\
	apply(',', TK_COMMA)											\
	apply('?', TK_QUESTION)

	/**
	* Higher-order macro that applies "apply" (which itself is usually a
	* macro) to each C++ token that consists of one or two characters.
	*/
#define CPPLINT_FORALL_ONE_OR_TWO_CHAR_TOKENS(apply)				\
	apply(':', TK_COLON, ':', TK_DOUBLE_COLON)						\
	apply('%', TK_REMAINDER, '=', TK_REMAINDER_ASSIGN)				\
	apply('=', TK_ASSIGN, '=', TK_EQUAL_TO)							\
	apply('!', TK_NOT, '=', TK_NOT_ASSIGN)							\
	apply('^', TK_XOR, '=', TK_XOR_ASSIGN)							\
	apply('*', TK_STAR, '=', TK_STAR_ASSIGN)

	/**
	* Higher-order macro that applies "apply" (which itself is usually a
	* macro) to each C++ token that consists of one or two characters. In
	* the two-characters case, there are two variants for the last
	* character.
	*/
#define CPPLINT_FORALL_ONE_OR_TWO_CHAR_TOKENS2(apply)               \
	apply('+', TK_PLUS,												\
	'+', TK_INCREMENT,												\
	'=', TK_PLUS_ASSIGN)                                            \
	apply('&', TK_AMPERSAND,										\
	'&', TK_LOGICAL_AND,											\
	'=', TK_AND_ASSIGN)                                             \
	apply('|', TK_BINARY_OR,										\
	'|', TK_LOGICAL_OR,												\
	'=', TK_OR_ASSIGN)

	/**
	* Higher-order macro that applies "apply" (which itself is usually a
	* macro) to each C++ token that consists of one, two, or three
	* characters. Each application introduces four tokens.
	*/
#define CPPLINT_FORALL_ONE_TO_THREE_CHAR_TOKENS(apply)				\
	apply('<', TK_LESS, '=', TK_LESS_EQUAL, '<', TK_LSHIFT,			\
	'=', TK_LSHIFT_ASSIGN)											\
	apply('>', TK_GREATER, '=', TK_GREATER_EQUAL, '>', TK_RSHIFT,	\
	'=', TK_RSHIFT_ASSIGN)

	/**
	* Higher-order macro that applies "apply" (which itself is usually a
	* macro) to each C++ token that needs special handling. For example,
	* '/' is special because it may introduce a comment (comments obey
	* their own grammar and are handled by hand).
	*/
#define CPPLINT_FORALL_ODD_TOKENS(apply)							\
	apply("/", TK_DIVIDE)											\
	apply("/=", TK_DIVIDE_ASSIGN)									\
	apply("-", TK_MINUS)											\
	apply("-=", TK_MINUS_ASSIGN)									\
	apply("--", TK_DECREMENT)										\
	apply("->", TK_ARROW)											\
	apply("->*", TK_ARROW_STAR)										\
	apply(".", TK_DOT)												\
	apply("...", TK_ELLIPSIS)										\
	apply(".*", TK_DOT_STAR)

	/**
	* Higher-order macro that applies "apply" (which itself is usually a
	* macro) to each C++ alphanumeric token.
	*/
#define CPPLINT_FORALL_KEYWORDS(apply)								\
	apply("auto", TK_AUTO)                              			\
	apply("const", TK_CONST)                              			\
	apply("constexpr", TK_CONSTEXPR)                      			\
	apply("double", TK_DOUBLE)                            			\
	apply("float", TK_FLOAT)                              			\
	apply("int", TK_INT)                                  			\
	apply("short", TK_SHORT)                              			\
	apply("struct", TK_STRUCT)                            			\
	apply("unsigned", TK_UNSIGNED)                        			\
	apply("break", TK_BREAK)                              			\
	apply("continue", TK_CONTINUE)                        			\
	apply("else", TK_ELSE)                                			\
	apply("for", TK_FOR)                                  			\
	apply("long", TK_LONG)                                			\
	apply("signed", TK_SIGNED)                            			\
	apply("switch", TK_SWITCH)                            			\
	apply("void", TK_VOID)                                			\
	apply("case", TK_CASE)                                			\
	apply("default", TK_DEFAULT)                          			\
	apply("enum", TK_ENUM)                                			\
	apply("goto", TK_GOTO)                                			\
	apply("register", TK_REGISTER)                        			\
	apply("sizeof", TK_SIZEOF)                            			\
	apply("typedef", TK_TYPEDEF)                          			\
	apply("volatile", TK_VOLATILE)                                  \
	apply("char", TK_CHAR)                                          \
	apply("do", TK_DO)                                              \
	apply("extern", TK_EXTERN)                                      \
	apply("if", TK_IF)                                              \
	apply("return", TK_RETURN)                                      \
	apply("static", TK_STATIC)                                      \
	apply("union", TK_UNION)                                        \
	apply("while", TK_WHILE)                                        \
	apply("asm", TK_ASM)                                            \
	apply("dynamic_cast", TK_DYNAMIC_CAST)                          \
	apply("namespace", TK_NAMESPACE)                                \
	apply("reinterpret_cast", TK_REINTERPRET_CAST)                  \
	apply("try", TK_TRY)                                            \
	apply("bool", TK_BOOL)                                          \
	apply("explicit", TK_EXPLICIT)                                  \
	apply("new", TK_NEW)                                            \
	apply("static_cast", TK_STATIC_CAST)                            \
	apply("typeid", TK_TYPEID)                                      \
	apply("catch", TK_CATCH)                                        \
	apply("false", TK_FALSE)                                        \
	apply("operator", TK_OPERATOR)                                  \
	apply("template", TK_TEMPLATE)                                  \
	apply("typename", TK_TYPENAME)                                  \
	apply("class", TK_CLASS)                                        \
	apply("friend", TK_FRIEND)                                      \
	apply("private", TK_PRIVATE)                                    \
	apply("this", TK_THIS)                                          \
	apply("using", TK_USING)                                        \
	apply("const_cast", TK_CONST_CAST)                              \
	apply("inline", TK_INLINE)                                      \
	apply("public", TK_PUBLIC)                                      \
	apply("throw", TK_THROW)                                        \
	apply("virtual", TK_VIRTUAL)                                    \
	apply("delete", TK_DELETE)                                      \
	apply("mutable", TK_MUTABLE)                                    \
	apply("protected", TK_PROTECTED)                                \
	apply("true", TK_TRUE)                                          \
	apply("wchar_t", TK_WCHAR_T)                                    \
	apply("and", TK_AND)                                            \
	apply("bitand", TK_BITAND)                                      \
	apply("compl", TK_COMPL)                                        \
	apply("not_eq", TK_NOT_EQ_CLEARTEXT)                            \
	apply("or_eq", TK_OR_EQ)                                        \
	apply("xor_eq", TK_XOR_ASSIGN_CLEARTEXT)                        \
	apply("and_eq", TK_AND_EQ)                                      \
	apply("bitor", TK_BITOR)                                        \
	apply("not", TK_NOT_CLEARTEXT)                                  \
	apply("or", TK_OR)                                              \
	apply("xor", TK_XOR_CLEARTEXT)

	/**
	* Applies macros a1 (two args), a2 (four args), a3 (six args), or a4
	* (eight args) appropriately to groups of tokens as introduced above.
	*/
#define CPPLINT_FOR_ALL_TOKENS(a1, a2, a3, a4)							  \
	/* Keywords */                                                        \
	CPPLINT_FORALL_KEYWORDS(a1)                                           \
	/* One character tokens */                                            \
	CPPLINT_FORALL_ONE_CHAR_TOKENS(a1)                                    \
	/* One- and two-character tokens that are not keywords */             \
	CPPLINT_FORALL_ONE_OR_TWO_CHAR_TOKENS(a2)                             \
	/* One- and two-character tokens that all start with the same char */ \
	CPPLINT_FORALL_ONE_OR_TWO_CHAR_TOKENS2(a3)                            \
	CPPLINT_FORALL_ONE_TO_THREE_CHAR_TOKENS(a4)                           \
	/* Odd tokens; they will be parsed by hand */                         \
	CPPLINT_FORALL_ODD_TOKENS(a1)                                         \
	/* Others */                                                          \
	a1("", TK_IDENTIFIER)                                                 \
	a1("", TK_NUMBER)                                                     \
	a1("", TK_CHAR_LITERAL)                                               \
	a1("", TK_STRING_LITERAL)                                             \
	a1("#include", TK_INCLUDE)                                            \
	a1("#if", TK_POUNDIF)                                                 \
	a1("#ifdef", TK_IFDEF)                                                \
	a1("#ifndef", TK_IFNDEF)                                              \
	a1("#undef", TK_UNDEF)                                                \
	a1("#", TK_POUND)                                                     \
	a1("##", TK_DOUBLEPOUND)                                              \
	a1("#else", TK_POUNDELSE)                                             \
	a1("#endif", TK_ENDIF)                                                \
	a1("#pragma", TK_PRAGMA)                                              \
	a1("#error", TK_ERROR)                                                \
	a1("#line", TK_HASHLINE)                                              \
	a1("#define", TK_DEFINE)                                              \
	a1("", TK_EOF)

	/**
	* Defines all token types TK_XYZ.
	* Basically black magic... Stand in awe of it's prowess.
	*/
	enum TokenType {
#define CPPLINT_X1(c1, t1) t1,
#define CPPLINT_X2(c1, t1, c2, t2) t1, t2,
#define CPPLINT_X3(c1, t1, c2, t2, c3, t3) t1, t2, t3,
#define CPPLINT_X4(c1, t1, c2, t2, c3, t3, c4, t4) t1, t2, t3, t4,
		CPPLINT_FOR_ALL_TOKENS(CPPLINT_X1, CPPLINT_X2, CPPLINT_X3, CPPLINT_X4)
#undef CPPLINT_X1
#undef CPPLINT_X2
#undef CPPLINT_X3
#undef CPPLINT_X4
		NUM_TOKENS
	};

	/**
	* Converts e.g. TK_VIRTUAL to "TK_VIRTUAL".
	*/
	string toString(TokenType t);

	/**
	* Defines one token together with file and line information. The
	* precedingComment_ is set if there was one comment before the token.
	*/
	struct Token {
		TokenType type_;
		string value_;
		string precedingWhitespace_;
		size_t line_;

		Token(TokenType type, string value, size_t line, string whitespace)
			: type_(type), value_(move(value)), precedingWhitespace_(move(whitespace)), line_(line) {};

		string toString() const {
			return string("Line:" + to_string(line_) + ":" + value_);
		};
	};

	/**
	* This is the quintessential function. Given a string containing C++
	* code and a filename, fills output with the tokens in the
	* file.
	*/
	size_t tokenize(const string &input, const string &initialFilename, vector<Token> &output);

	/**
	* Prevent the use of temporaries for input and filename
	* because the resulting tokens contain StringPiece objects pointing
	* into them.
	*/
	size_t tokenize(string&&, const string &, vector<Token> &) = delete;
	size_t tokenize(const string&, string &&, vector<Token> &) = delete;
};
