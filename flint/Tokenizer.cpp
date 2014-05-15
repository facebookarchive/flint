#include "Tokenizer.hpp"

#include <algorithm>
#include <unordered_map>
#include <cassert>
#include <stdexcept>

#include <numeric>
#include <cinttypes>

#include "Polyfill.hpp"

namespace std {
	template<>
    	struct hash<flint::StringFragment>
    	{
        	typedef flint::StringFragment argument_type;
        	typedef size_t value_type;
 
        	inline value_type operator()(const argument_type &fragment) const {
				return accumulate(fragment.begin(), fragment.end(), 5381, [](uint64_t curr, char next) {
					return ((curr << 5) + curr) + next;
				});
			}
    	};
}

namespace flint {

	namespace { // Anonymous Namespace for Tokenizing and munching functions
		
		// Black magic code expansion from Token Definitions
		// See header...
		static unordered_map<StringFragment, TokenType> initializeKeywords() {
			static unordered_map<string, TokenType> root; // Will own all the strings
			unordered_map<StringFragment, TokenType> result;

		#define CPPLINT_ASSIGN(s, tk) root[string(s)] = tk;
			CPPLINT_FORALL_KEYWORDS(CPPLINT_ASSIGN)
		#undef CPPLINT_ASSIGN

			for (const auto& item : root) {
				auto& key = item.first;				
				result[StringFragment{key.begin(), key.end()}] = item.second;
			}

			return result;
		};

		// Oh good lord... Keep this for now,
		// then code review and find a cleaner method
		#define FBEXCEPTION(e) \
		do { throw runtime_error(string((e))); } while (0)

		#define ENFORCE(e, m) \
		if (e) {} else { FBEXCEPTION(m); }

		/**
		* Map containing mappings of the kind "virtual" -> TK_VIRTUAL.
		*/
		static unordered_map<StringFragment, TokenType> keywords = initializeKeywords();

		/**
		* Eats howMany characters out of pc, advances pc appropriately, and
		* returns the eaten portion.
		*/
		static StringFragment munchChars(string::const_iterator &pc, size_t howMany) {
			//assert(pc.size() >= howMany);
			assert(howMany > 0);
			auto result = StringFragment{pc, pc + howMany};
			advance(pc, howMany);
			return result;
		};

		/**
		* Assuming pc is positioned at the start of an identifier, munches it
		* from pc and returns it.
		*/
		static StringFragment munchIdentifier(string::const_iterator &pc, string::const_iterator inputEnd) {
			
			size_t size = distance(pc, inputEnd);
			for (size_t i = 0; i < size; ++i) {
				assert(i < size);
				const char c = pc[i];
				// g++ allows '$' in identifiers. Also, some crazy inline
				// assembler uses '@' in identifiers, see e.g.
				// fbcode/external/cryptopp/rijndael.cpp, line 527
				if (!isalnum(c) && c != '_' && c != '$' && c != '@') {
					// done
					ENFORCE(i > 0, "Invalid identifier: " + string(&*pc));
					return munchChars(pc, i);
				}
			}

			return munchChars(pc, size);
		};

		/**
		* Assuming pc is positioned at the start of a C-style comment,
		* munches it from pc and returns it.
		*/
		static StringFragment munchComment(string::const_iterator &pc, size_t &line) {
			assert(pc[0] == '/' && pc[1] == '*');
			for (size_t i = 2;; ++i) {
				//assert(i < pc.size());
				auto c = pc[i];
				if (c == '\n') {
					++line;
				}
				else if (c == '*') {
					if (pc[i + 1] == '/') {
						// end of comment
						return munchChars(pc, i + 2);
					}
				}
				else if (!c) {
					// end of input
					FBEXCEPTION("Unterminated comment: " + string(&*pc));
				}
			}
			assert(false);
		};

		/**
		* Assuming pc is positioned at the start of a single-line comment,
		* munches it from pc and returns it.
		*/
		static StringFragment munchSingleLineComment(string::const_iterator &pc, string::const_iterator inputEnd, size_t &line) {
			assert(pc[0] == '/' && pc[1] == '/');

			size_t size = distance(pc, inputEnd);
			for (size_t i = 2; i < size; ++i) {
				//assert(i < pc.size());
				auto c = pc[i];
				if (c == '\n') {
					++line;
					if (i > 0 && pc[i - 1] == '\\') {
						// multiline single-line comment (sic)
						continue;
					}
					// end of comment
					return munchChars(pc, i + 1);
				}
			}

			return munchChars(pc, size);
		};

		/**
		* Assuming pc is positioned at the start of a number (be it decimal
		* or floating-point), munches it off pc and returns it. Note that the
		* number is assumed to be correct so a number of checks are not
		* necessary.
		*/
		static StringFragment munchNumber(string::const_iterator &pc) {
			bool sawDot = false, sawExp = false, sawX = false, sawSuffix = false;
			for (size_t i = 0;; ++i) {
				//assert(i < pc.size());
				auto const c = pc[i];
				if (c == '.' && !sawDot && !sawExp && !sawSuffix) {
					sawDot = true;
				}
				else if (isdigit(c)) {
					// Nothing to do
				}
				else if (sawX && !sawExp && c && strchr("AaBbCcDdEeFf", c)) {
					// Hex digit; nothing to do. The condition includes !sawExp
					// because the exponent is decimal even in a hex floating-point
					// number!
				}
				else if (c == '+' || c == '-') {
					// Sign may appear at the start or right after E or P
					if (i > 0 && !strchr("EePp", pc[i - 1])) {
						// Done, the sign is the next token
						return munchChars(pc, i);
					}
				}
				else if (!sawExp && !sawSuffix && !sawX && (c == 'e' || c == 'E')) {
					sawExp = true;
				}
				else if (sawX && !sawExp && !sawSuffix && (c == 'p' || c == 'P')) {
					sawExp = true;
				}
				else if ((c == 'x' || c == 'X') && i == 1 && pc[0] == '0') {
					sawX = true;
				}
				else if (c && strchr("FfLlUu", c)) {
					// It's a suffix. There could be several of them (including
					// repeats a la LL), so let's not return just yet
					sawSuffix = true;
				}
				else {
					// done
					ENFORCE(i > 0, "Invalid number: " + string(&*pc));
					return munchChars(pc, i);
				}
			}
			assert(false);
		};

		/**
		* Assuming pc is positioned at the start of a character literal,
		* munches it from pc and returns it. A reference to line is passed in
		* order to track multiline character literals (yeah, that can
		* actually happen) correctly.
		*/
		static StringFragment munchCharLiteral(string::const_iterator &pc, size_t &line) {
			assert(pc[0] == '\'');
			for (size_t i = 1;; ++i) {
				auto const c = pc[i];
				if (c == '\'') {
					// That's about it
					return munchChars(pc, i + 1);
				}
				if (c == '\\') {
					++i;
					if (pc[i] == '\n') {
						++line;
					}
					continue;
				}
				ENFORCE(c, "Unterminated character constant: " + string(&*pc));
			}
		};

		/**
		* Assuming pc is positioned at the start of a string literal, munches
		* it from pc and returns it. A reference to line is passed in order
		* to track multiline strings correctly.
		*/
		static StringFragment munchString(string::const_iterator &pc, size_t &line, bool isIncludeLiteral = false) {
			char stringEnd = (isIncludeLiteral ? '>' : '"');

			assert(pc[0] == (isIncludeLiteral ? '<' : '"'));			
			for (size_t i = 1;; ++i) {
				auto const c = pc[i];
				if (c == stringEnd) {
					// That's about it
					return munchChars(pc, i + 1);
				}
				if (c == '\\') {
					++i;
					if (pc[i] == '\n') {
						++line;
					}
					continue;
				}
				ENFORCE(c, "Unterminated string constant: " + string(&*pc));
			}
		};

		/**
		* Munches horizontal spaces from pc. If we want to disallow tabs in
		* sources, here is the place. No need for end=of-input checks as the
		* input always has a '\0' at the end.
		*/
		static StringFragment munchSpaces(string::const_iterator &pc) {
			size_t i;
			for (i = 0; pc[i] == ' ' || pc[i] == '\t'; ++i) {}

			const auto result = StringFragment{pc, pc + i};
			advance(pc, i);
			return result;
		};

	}; // Anonymous Namespace

	/**
	* Given the contents of a C++ file and a filename, tokenizes the
	* contents and places it in output.
	*/
	size_t tokenize(const string &input, const string &file, vector<Token> &output, vector<size_t> &structures) {
		output.clear();
		structures.clear();
		
		static const string eof("\0");

		auto pc = input.begin();
		size_t line = 1;

		TokenType t;
		size_t tokenLen;
		string whitespace = "";

		while (pc != input.end()) {
			const char c = pc[0];

			if (output.size() > 0) {
				const TokenType tok = output.back().type_;
				if (tok == TK_CLASS || tok == TK_STRUCT || tok == TK_UNION) {
					// If the last token added was the start of a structure push it onto
					// the list of structures
					structures.push_back(output.size() - 1);
				}
				else if (c == '<' && tok == TK_INCLUDE) {
					// Special case for parsing #include <...>
					// Previously the include path would not be captured as a string literal
					auto str = munchString(pc, line, true);
					output.push_back(Token(TK_STRING_LITERAL, move(str), line,
						whitespace));
					whitespace.clear();
					continue;
				}


			}

			switch (c) {
				// *** One-character tokens that don't require lookahead (comma,
				// *** semicolon etc.)
#define CPPLINT_INTRODUCE_CASE(c0, t0)													\
			case (c0) : t = (t0); tokenLen = 1; goto INSERT_TOKEN;
				CPPLINT_FORALL_ONE_CHAR_TOKENS(CPPLINT_INTRODUCE_CASE);
#undef CPPLINT_INTRODUCE_CASE
				// *** One- or two-character tokens
#define CPPLINT_INTRODUCE_CASE(c1, t1, c2, t2)											\
			case c1:																	\
				if (pc[1] == (c2)) {													\
					t = (t2); tokenLen = 2;												\
				}																		\
				else {																	\
					t = (t1); tokenLen = 1;												\
				}																		\
				goto INSERT_TOKEN;
					// Insert a bunch of code here
					CPPLINT_FORALL_ONE_OR_TWO_CHAR_TOKENS(CPPLINT_INTRODUCE_CASE);
#undef CPPLINT_INTRODUCE_CASE
#define CPPLINT_INTRODUCE_CASE(c1, t1, c2, t2, c3, t3)									\
			case c1:																	\
					if (pc[1] == (c2)) {												\
						t = (t2); tokenLen = 2;											\
					}																	\
					else if (pc[1] == (c3)) {											\
						t = (t3); tokenLen = 2;											\
					}																	\
					else {																\
						t = (t1); tokenLen = 1;											\
					}																	\
					goto INSERT_TOKEN;
						// Insert a bunch of code here
						CPPLINT_FORALL_ONE_OR_TWO_CHAR_TOKENS2(CPPLINT_INTRODUCE_CASE);
#undef CPPLINT_INTRODUCE_CASE
#define CPPLINT_INTRODUCE_CASE(c1, t1, c2, t2, c3, t3, c4, t4)							\
			case c1:																	\
						if (pc[1] == (c2)) {											\
							t = (t2); tokenLen = 2;										\
						}																\
						else if (pc[1] == (c3)) {										\
							if (pc[2] == (c4)) {										\
								t = (t4); tokenLen = 3;                                 \
							}															\
							else {														\
								t = (t3); tokenLen = 2;                                 \
							}															\
						}																\
						else {															\
							t = (t1); tokenLen = 1;					                    \
						}												                \
						goto INSERT_TOKEN;
							// Insert a bunch of code here
							CPPLINT_FORALL_ONE_TO_THREE_CHAR_TOKENS(CPPLINT_INTRODUCE_CASE);
#undef CPPLINT_INTRODUCE_CASE
							// *** Everything starting with a slash: /, /=, single- and
							// *** multi-line comments
			case '/':
				if (pc[1] == '*') {
					const auto& comment = munchComment(pc, line); 
					whitespace.append(comment.begin(), comment.end());
					break;
				}
				if (pc[1] == '/') {
					const auto &single = munchSingleLineComment(pc, input.end(), line);
					whitespace.append(single.begin(), single.end());
					break;
				}
				if (pc[1] == '=') {
					t = TK_DIVIDE_ASSIGN; tokenLen = 2;
				}
				else {
					t = TK_DIVIDE; tokenLen = 1;
				}
				goto INSERT_TOKEN;
				// *** Backslash
			case '\\':
				ENFORCE(pc[1] == '\n' || pc[1] == '\r', "Misplaced backslash in " + file + ":" + to_string(line));
				++line;
				whitespace.append(pc, pc + 2);
				advance(pc, 2);
				break;
				// *** Newline
			case '\n':
				whitespace += *(pc++);
				++line;
				break;
				// *** Part of a DOS newline; ignored
			case '\r':
				whitespace += *(pc++);
				break;
				// *** ->, --, -=, ->*, and -
			case '-':
				if (pc[1] == '-') {
					t = TK_DECREMENT; tokenLen = 2;
					goto INSERT_TOKEN;
				}
				if (pc[1] == '=') {
					t = TK_MINUS_ASSIGN; tokenLen = 2;
					goto INSERT_TOKEN;
				}
				if (pc[1] == '>') {
					if (pc[2] == '*') {
						t = TK_ARROW_STAR; tokenLen = 3;
					}
					else {
						t = TK_ARROW; tokenLen = 2;
					}
					goto INSERT_TOKEN;
				}
				t = TK_MINUS; tokenLen = 1;
				goto INSERT_TOKEN;
				// *** Whitespace
			case ' ': case '\t':
				{
					const auto &spaces = munchSpaces(pc);
					whitespace.append(spaces.begin(), spaces.end());
				}
				break;
				// *** Done parsing!
			case '\0':
				//assert(pc.size() == 0);
				// Push last token, the EOF
				output.push_back(Token(TK_EOF, StringFragment{eof.begin(), eof.end()}, line, whitespace));
				return line;
				// *** Verboten characters (do allow '@' and '$' as extensions)
			case '`':
				FBEXCEPTION("Invalid character: " + string(1, c) + " in " + string(file + ":" + to_string(line)));
				break;
				// *** Numbers
			case '0': case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9':
			ITS_A_NUMBER : {
				auto symbol = munchNumber(pc);
				assert(symbol.size() > 0);
				output.push_back(Token(TK_NUMBER, move(symbol), line, whitespace));
				whitespace.clear();
			}
				break;
				// *** Number, member selector, ellipsis, or .*
			case '.':
				if (isdigit(pc[1])) {
					goto ITS_A_NUMBER;
				}
				if (pc[1] == '*') {
					t = TK_DOT_STAR; tokenLen = 2;
				}
				else if (pc[1] == '.' && pc[2] == '.') {
					t = TK_ELLIPSIS; tokenLen = 3;
				}
				else {
					t = TK_DOT; tokenLen = 1;
				}
				goto INSERT_TOKEN;
				// *** Character literal
			case '\'': {
				auto charLit = munchCharLiteral(pc, line);
				output.push_back(Token(TK_CHAR_LITERAL, move(charLit), line,
					whitespace));
				whitespace.clear();
			}
				break;
				// *** String literal
			case '"': {
				auto str = munchString(pc, line);
				output.push_back(Token(TK_STRING_LITERAL, move(str), line,
					whitespace));
				whitespace.clear();
			}
				break;
			case '#': {
				// Skip ws
				auto pc1 = pc + 1;
				tokenLen = 1 + munchSpaces(pc1).size();
				// define, include, pragma, or line
				if (startsWith(pc1, "line")) {
					t = TK_HASHLINE; tokenLen += distance(pc, find(pc1, input.end(), '\n'));
				}
				else if (startsWith(pc1, "error")) {
					// The entire #error line is the token value
					t = TK_ERROR; tokenLen += distance(pc, find(pc1, input.end(), '\n'));
					ENFORCE(tokenLen > 0, "Unterminated #error message");
				}
				else if (startsWith(pc1, "include")) {
					t = TK_INCLUDE; tokenLen += 7; // strlen("include");
				}
				else if (startsWith(pc1, "ifdef")) {
					t = TK_IFDEF; tokenLen += 5; // strlen("ifdef");
				}
				else if (startsWith(pc1, "ifndef")) {
					t = TK_IFNDEF; tokenLen += 6; // strlen("ifndef");
				}
				else if (startsWith(pc1, "if")) {
					t = TK_POUNDIF; tokenLen += 2; // strlen("if");
				}
				else if (startsWith(pc1, "undef")) {
					t = TK_UNDEF; tokenLen += 5; // strlen("undef");
				}
				else if (startsWith(pc1, "else")) {
					t = TK_POUNDELSE; tokenLen += 4; // strlen("else");
				}
				else if (startsWith(pc1, "endif")) {
					t = TK_ENDIF; tokenLen += 5; // strlen("endif");
				}
				else if (startsWith(pc1, "define")) {
					t = TK_DEFINE; tokenLen += 6; // strlen("define");
				}
				else if (startsWith(pc1, "pragma")) {
					t = TK_PRAGMA; tokenLen += 6; // strlen("pragma");
				}
				else if (startsWith(pc1, "#")) {
					t = TK_DOUBLEPOUND; tokenLen += 2; // strlen("##");
				}
				else {
					// We can only assume this is inside a macro definition
					t = TK_POUND; tokenLen += 1;
				}
			}
				goto INSERT_TOKEN;
				// *** Everything else
			default:
				if (iscntrl(c)) {
					whitespace += *(pc++);
				}
				else if (isalpha(c) || c == '_' || c == '$' || c == '@') {
					// it's a word
					auto symbol = munchIdentifier(pc, input.cend());
					auto iter = keywords.find(symbol);
					if (iter != keywords.end()) {
						// keyword, baby
						output.push_back(Token(iter->second, move(symbol), line,
							whitespace));
						whitespace.clear();
					}
					else {
						// Some identifier
						assert(symbol.size() > 0);
						output.push_back(Token(TK_IDENTIFIER, move(symbol), line,
							whitespace));
						whitespace.clear();
					}
				}
				else {
					// what could this be? (BOM?)
					FBEXCEPTION("Unrecognized character in " + file + ":" + to_string(line));
				}
				break;
				// *** All
			INSERT_TOKEN:
				output.push_back(Token(t, munchChars(pc, tokenLen), line,
					whitespace));
				whitespace.clear();
				break;
			}
		}

		output.push_back(Token(TK_EOF, StringFragment{eof.begin(), eof.end()}, line, ""));

		return line;
	};

	/**
	* Converts e.g. TK_VIRTUAL to "TK_VIRTUAL".
	* More black magic...
	*/
	string toString(TokenType t) {

#define CPPLINT_X1(c1, t1) if ((t1) == t) return (#t1);
#define CPPLINT_X2(c1, t1, c2, t2) CPPLINT_X1(c1, t1) CPPLINT_X1(c2, t2)
#define CPPLINT_X3(c1, t1, c2, t2, c3, t3)      \
		CPPLINT_X1(c1, t1) CPPLINT_X2(c2, t2, c3, t3)

#define CPPLINT_X4(c1, t1, c2, t2, c3, t3, c4, t4)               \
		CPPLINT_X2(c1, t1, c2, t2) CPPLINT_X2(c3, t3, c4, t4)

		// Expansion
		CPPLINT_FOR_ALL_TOKENS(CPPLINT_X1, CPPLINT_X2, CPPLINT_X3, CPPLINT_X4)

#undef CPPLINT_X1
#undef CPPLINT_X2
#undef CPPLINT_X3
#undef CPPLINT_X4

			FBEXCEPTION("Unknown token type: " + toString(t));
	};

};
