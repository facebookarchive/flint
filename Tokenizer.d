// Copyright (c) 2014- Facebook
// License: Boost License 1.0, http://boost.org/LICENSE_1_0.txt
// @author Andrei Alexandrescu (andrei.alexandrescu@facebook.com)

import std.algorithm, std.array, std.ascii, std.conv, std.exception, std.regex,
  std.stdio, std.typecons, std.typetuple;

struct TokenizerGenerator(alias tokens, alias reservedTokens) {
  /**
   * All token types include regular and reservedTokens, plus the null
   * token ("") and the end-of-stream token ("\0").
   */
  private enum totalTokens = tokens.length + reservedTokens.length + 2;

  /**
   * Representation for token types.
   */
  static if (totalTokens <= ubyte.max)
    alias TokenIDRep = ubyte;
  else static if (totalTokens <= ushort.max)
    alias TokenIDRep = ushort;
  else // seriously?
    alias TokenIDRep = uint;

  struct TokenType2 {
    TokenIDRep id;

    string sym() const {
      assert(id <= tokens.length + reservedTokens.length + 2, text(id));
      if (id == 0) return "";
      auto i = id - 1;
      if (i >= tokens.length + reservedTokens.length) return "\0";
      return i < tokens.length
        ? tokens[i]
        : reservedTokens[i - tokens.length];
    }
  }

  template tk(string symbol) {
    import std.range;
    static if (symbol == "") {
      // Token ID 0 is reserved for "unrecognized token".
      enum tk = TokenType2(0);
    } else static if (symbol == "\0") {
      // Token ID max is reserved for "end of input".
        enum tk = TokenType2(
          cast(TokenIDRep) (1 + tokens.length + reservedTokens.length));
    } else {
        //enum id = chain(tokens, reservedTokens).countUntil(symbol);
      // Find the id within the regular tokens realm
      enum idTokens = tokens.countUntil(symbol);
      static if (idTokens >= 0) {
        // Found, regular token. Add 1 because 0 is reserved.
        enum id = idTokens + 1;
      } else {
        // not found, only chance is within the reserved tokens realm
        enum idResTokens = reservedTokens.countUntil(symbol);
        enum id = idResTokens >= 0 ? tokens.length + idResTokens + 1 : -1;
      }
      static assert(id >= 0 && id < TokenIDRep.max,
                    "Invalid token: " ~ symbol);
      enum tk = TokenType2(cast(ubyte) id);
    }
  }

/**
 * Defines one token together with file and line information. The
 * precedingComment_ is set if there was one comment before the token.
 */
  struct Token {
    TokenType2 type_;
    string value_;
    string precedingWhitespace_;
    size_t line_;
    string file_;

    string value() const {
      return value_ ? value_ : type_.sym;
    }
  };

  static string generateCases(string[] tokens, size_t index = 0,
                             bool* mayFallThrough = null) {
    assert(tokens.length > 1);

    static bool mustEscape(char c) {
      return c == '\\' || c == '"' || c == '\'';
    }

    static string escape(string s) {
      string result;
      foreach (c; s) {
        if (mustEscape(c)) result ~= "\\";
        result ~= c;
      }
      return result;
    }

    string result;
    for (size_t i = 0; i < tokens.length; ++i) {
      if (index >= tokens[i].length) {
        result ~= "default: t = tk!\""
          ~ tokens[i] ~ "\"; break token_search;\n";
      } else {
        result ~= "case '" ~ escape(tokens[i][index .. index + 1]) ~ "': ";
        auto j = i + 1;
        while (j < tokens.length && tokens[j][index] == tokens[i][index]) {
          ++j;
        }
        // i through j-1 are tokens with the same prefix
        if (j == i + 1) {
          if (tokens[i].length > index + 1) {
            result ~= "if (";
            result ~= tokens[i].length == index + 2
              ? ("pc["~to!string(index + 1)~"] == '"
                 ~ escape(tokens[i][index + 1 .. index + 2]) ~ "') ")
              : ("pc["~to!string(index + 1)~" .. $].startsWith(\""
                 ~ escape(tokens[i][index + 1 .. $]) ~ "\")) ");
            result ~= "{ t = tk!\""
              ~ escape(tokens[i]) ~
              "\"; break token_search; } else break;\n";
            if (mayFallThrough) *mayFallThrough = true;
          } else {
            result ~= "t = tk!\"" ~ escape(tokens[i])
              ~ "\"; break token_search;\n";
          }
          continue;
        }
        auto endOfToken = false;
        foreach (k; i .. j) {
          if (index + 1 >= tokens[k].length) {
            endOfToken = true;
            break;
          }
        }
        result ~= "switch (pc["~to!string(index + 1)~"]) {\n";
        if (!endOfToken) result ~= "default: break;\n";
        bool mft;
        result ~= generateCases(tokens[i .. j], index + 1, &mft)
          ~ "}";
        if (!endOfToken || mft) {
          result ~= " break;\n";
          if (mayFallThrough) *mayFallThrough = true;
        }
        else {
          result ~= "\n";
        }
        i = j - 1;
      }
    }
    return result;
  }

//pragma(msg, generateCases([NonAlphaTokens, Keywords]));

  static Tuple!(size_t, size_t, TokenType2)
  match(R)(R r) {
    size_t charsBefore = 0, linesBefore = 0;
    TokenType2 t;
    auto pc = r;

    token_search:
    for (;;) {
      assert(pc.length);
      switch (pc[0]) {
        case '\n':
          ++linesBefore;
          goto case; // fall through to also increment charsBefore
        case ' ': case '\t': case '\r':
          ++charsBefore;
          pc = pc[1 .. $];
          continue;
        case '\0':
          // done!
          t = tk!"\0";
          break token_search;
        default:
          break;
          // Bunch of cases generated here
          mixin(generateCases(tokens));
      }
      // Couldn't match any token
      t = tk!"";
      break;
    }
    return tuple(linesBefore, charsBefore, t);
  }
}

alias NonAlphaTokens = TypeTuple!(
  "~", "(", ")", "[", "]", "{", "}", ";", ",", "?",
  "<", "<=", "<<", "<<=", ">", ">=", ">>", ">>=", "%", "%=", "=", "==",
  "!", "!=", "^", "^=", "*", "*=",
  ":", "::", "+", "++", "+=", "&", "&&", "&=", "|", "||", "|=",
  "-", "--", "-=", "->", "->*",
  "/", "/=", "//", "/*",
  "\\",
  ".", ".*", "...",
  "'",
  "\"",
  "#", "##",
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
  "@", "$", // allow as extensions
  "R\"(",
);

alias Keywords = TypeTuple!(
  "and", "and_eq", "asm", "auto",
  "bitand", "bitor", "bool", "break",
  "case", "catch", "char", "class", "compl", "const", "const_cast", "constexpr",
    "continue",
  "default", "delete", "do", "double", "dynamic_cast",
  "else", "enum", "explicit", "extern", "false", "float", "for", "friend",
  "goto",
  "if", "inline", "int",
  "long",
  "mutable",
  "namespace", "new", "noexcept", "not", "not_eq",
  "operator", "or", "or_eq",
  "private", "protected", "public",
  "register", "reinterpret_cast", "return",
  "short", "signed", "sizeof", "static", "static_cast", "struct", "switch",
  "template", "this", "throw", "true", "try", "typedef", "typeid", "typename",
  "union", "unsigned", "using",
  "virtual", "void", "volatile",
  "wchar_t", "while",
  "xor", "xor_eq",
);

static immutable string[] specialTokens = [
  "identifier", "number", "string_literal", "char_literal",
  "preprocessor_directive"
];

alias TokenizerGenerator!([NonAlphaTokens, Keywords], specialTokens)
  CppLexer;
alias tk = CppLexer.tk;
alias Token = CppLexer.Token;
alias TokenType = CppLexer.TokenType2;

/**
 * This is the quintessential function. Given a string containing C++
 * code and a filename, fills output with the tokens in the
 * file. Warning - don't use temporaries for input and filename
 * because the resulting tokens contain StringPiece objects pointing
 * into them.
 */
CppLexer.Token[] tokenize(string input, string initialFilename = null) {
  input ~= '\0'; // TODO revisit this
  CppLexer.Token[] output;
  auto file = initialFilename;
  size_t line = 1;

  for (;;) {
    auto t = nextToken(input, line);
    t.file_ = initialFilename;
    //writeln(t);
    output ~= t;
    if (t.type_ is CppLexer.tk!"\0") break;
  }

  return output;
}

void tokenize(string input, string initialFilename, ref CppLexer.Token[] t) {
  t = tokenize(input, initialFilename);
}

/**
 * Helper function, gets next token and updates pc and line.
 */
CppLexer.Token nextToken(ref string pc, ref size_t line) {
  size_t charsBefore;
  string value;
  CppLexer.TokenType2 tt;
  auto initialPc = pc;
  auto initialLine = line;
  size_t tokenLine;

  for (;;) {
    auto t = CppLexer.match(pc);
    line += t[0];
    tokenLine = line;
    charsBefore += t[1];
    tt = t[2];

    // Position pc to advance past the leading whitespace
    pc = pc[t[1] .. $];

    // Unrecognized token?
    if (tt is tk!"") {
      auto c = pc[0];
      if (std.ascii.isAlpha(c) || c == '_' || c == '$' || c == '@') {
        value = munchIdentifier(pc);
        //writeln("sym: ", value);
        tt = tk!"identifier";
        break;
      } else {
        writeln("Illegal character: ", cast(uint) c, " [", c, "]");
        throw new Exception("Illegal character");
      }
    }

    assert(tt.sym.length >= 1, "'" ~ tt.sym ~ "'");
    // Number?
    if (isDigit(tt.sym[0]) || tt is tk!"." && isDigit(pc[1])) {
      tt = tk!"number";
      value = munchNumber(pc);
      break;
    }

    // Single-line comment?
    if (tt is tk!"//") {
      charsBefore += munchSingleLineComment(pc, line).length;
      continue;
    }

    // Multi-line comment?
    if (tt is tk!"/*") {
      charsBefore += munchComment(pc, line).length;
      continue;
    }

    // #pragma/#error/#warning preprocessor directive (except #pragma once)?
    if (tt == tk!"#" && match(pc, ctRegex!(`^#\s*(error|warning|pragma)\s`))
          && !match(pc, ctRegex!(`^#\s*pragma\s+once`))) {
      value = munchPreprocessorDirective(pc, line);
      tt = tk!"preprocessor_directive";
      break;
    }

    // Literal string?
    if (tt is tk!"\"") {
      value = munchString(pc, line);
      tt = tk!"string_literal";
      break;
    }

    //Raw string?
    if (tt is tk!"R\"(") {
      value = munchRawString(pc, line);
      tt = tk!"string_literal";
      break;
    }

    // Literal char?
    if (tt is tk!"'") {
      value = munchCharLiteral(pc, line);
      tt = tk!"char_literal";
      break;
    }

    // Keyword or identifier?
    char c = tt.sym[0];
    if (std.ascii.isAlpha(c) || c == '_' || c == '$' || c == '@') {
      // This is a keyword, but it may be a prefix of a longer symbol
      assert(pc.length >= tt.sym.length, text(tt.sym, ": ", pc));
      c = pc[tt.sym.length];
      if (isAlphaNum(c) || c == '_' || c == '$' || c == '@') {
        // yep, longer symbol
        value = munchIdentifier(pc);
        // writeln("sym: ", tt.symbol, symbol);
        tt = tk!"identifier";
      } else {
        // Keyword, just update the pc
        pc = pc[tt.sym.length .. $];
      }
      break;
    }

    // End of stream?
    if (tt is tk!"\0") {
      break;
    }

    // We have something!
    pc = pc[tt.sym.length .. $];
    break;
  }

  version (unittest) {
    // make sure the we munched the right number of characters
    auto delta = initialPc.length - pc.length;
    if (tt is tk!"\0") delta += 1;
    auto tsz = charsBefore + (value ? value.length : tt.sym().length);
    if (tsz != delta) {
      stderr.writeln("Flint tokenization error: Wrong size for token type '",
          tt.sym(), "': '", initialPc[0 .. charsBefore], "'~'", value, "' ",
          "of size ", tsz, " != '", initialPc[0 .. delta], "' of size ",
          delta);
      throw new Exception("Internal flint error");
    }

    // make sure that line was incremented the correct number of times
    auto lskip = std.algorithm.count(initialPc[0 .. delta], '\n');
    if (initialLine + lskip != line) {
      stderr.writeln("Flint tokenization error: muched '",
          initialPc[0 .. delta], "' (token type '", tt.sym(), "'), "
          "which contains ", lskip, " newlines, "
          "but line has been incremented by ", line - initialLine);
      throw new Exception("Internal flint error");
    }
  }

  return CppLexer.Token(
    tt, value,
    initialPc[0 .. charsBefore],
    tokenLine);
}

/**
 * Eats howMany characters out of pc, avances pc appropriately, and
 * returns the eaten portion.
 */
static string munchChars(ref string pc, size_t howMany) {
  assert(pc.length >= howMany);
  auto result = pc[0 .. howMany];
  pc = pc[howMany .. $];
  return result;
}

/**
 * Assuming pc is positioned at the start of a single-line comment,
 * munches it from pc and returns it.
 */
static string munchSingleLineComment(ref string pc, ref size_t line) {
  for (size_t i = 0; ; ++i) {
    assert(i < pc.length);
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
    if (!c) {
      // single-line comment at end of file, meh
      return munchChars(pc, i);
    }
  }
  assert(false);
}

/**
 * Assuming pc is positioned at the start of a C-style comment,
 * munches it from pc and returns it.
 */
static string munchComment(ref string pc, ref size_t line) {
  //assert(pc[0] == '/' && pc[1] == '*');
  for (size_t i = 0; ; ++i) {
    assert(i < pc.length);
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
      enforce(false, "Unterminated comment: ", pc);
    }
  }
  assert(false);
}

/**
 * Assuming pc is positioned at the start of a specified preprocessor directive,
 * munches it from pc and returns it.
 */
static string munchPreprocessorDirective(ref string pc, ref size_t line) {
  for (size_t i = 0; ; ++i) {
    assert(i < pc.length);
    auto c = pc[i];
    if (c == '\n') {
      if (i > 0 && pc[i - 1] == '\\') {
        // multiline directive
        ++line;
        continue;
      }
      // end of directive
      return munchChars(pc, i);
    }
    if (!c) {
      // directive at end of file
      return munchChars(pc, i);
    }
  }
}

/**
 * Assuming pc is positioned at the start of an identifier, munches it
 * from pc and returns it.
 */
static string munchIdentifier(ref string pc) {
  for (size_t i = 0; ; ++i) {
    assert(i < pc.length);
    const c = pc[i];
    // g++ allows '$' in identifiers. Also, some crazy inline
    // assembler uses '@' in identifiers, see e.g.
    // fbcode/external/cryptopp/rijndael.cpp, line 527
    if (!isAlphaNum(c) && c != '_' && c != '$' && c != '@') {
      // done
      enforce(i > 0, "Invalid identifier: ", pc);
      return munchChars(pc, i);
    }
  }
  assert(false);
}

/**
 * Assuming pc is positioned at the start of a string literal, munches
 * it from pc and returns it. A reference to line is passed in order
 * to track multiline strings correctly.
 */
static string munchString(ref string pc, ref size_t line) {
  assert(pc[0] == '"');
  for (size_t i = 1; ; ++i) {
    const c = pc[i];
    if (c == '"') {
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
    enforce(c, "Unterminated string constant: ", pc);
  }
}

/**
  * Assuming pc is positioned at the start og a raw string, munches
  * it from pc and returns it.
  */
static string munchRawString(ref string pc, ref size_t line) {
  assert(pc.startsWith(`R"(`));
  for (size_t i = 3; ; ++i) {
    const c = pc[i];
    if (c == ')') {
      if (pc[i + 1] == '"') {
        //End of a raw string literal
        return munchChars(pc, i + 2);
      }
    }
    enforce(c, "Unterminated raw string: ", pc);
  }
}

/**
 * Assuming pc is positioned at the start of a number (be it decimal
 * or floating-point), munches it off pc and returns it. Note that the
 * number is assumed to be correct so a number of checks are not
 * necessary.
 */
static string munchNumber(ref string pc) {
  bool sawDot = false, sawExp = false, sawX = false, sawSuffix = false;
  for (size_t i = 0; ; ++i) {
    assert(i < pc.length);
    auto const c = pc[i];
    if (c == '.' && !sawDot && !sawExp && !sawSuffix) {
      sawDot = true;
    } else if (isDigit(c)) {
      // Nothing to do
    } else if (sawX && !sawExp && c && "AaBbCcDdEeFf".canFind(c)) {
      // Hex digit; nothing to do. The condition includes !sawExp
      // because the exponent is decimal even in a hex floating-point
      // number!
    } else if (c == '+' || c == '-') {
      // Sign may appear at the start or right after E or P
      if (i > 0 && !"EePp".canFind(pc[i - 1])) {
        // Done, the sign is the next token
        return munchChars(pc, i);
      }
    } else if (!sawExp && !sawSuffix && !sawX && (c == 'e' || c == 'E')) {
      sawExp = true;
    } else if (sawX && !sawExp && !sawSuffix && (c == 'p' || c == 'P')) {
      sawExp = true;
    } else if ((c == 'x' || c == 'X') && i == 1 && pc[0] == '0') {
      sawX = true;
    } else if (c && "FfLlUu".canFind(c)) {
      // It's a suffix. There could be several of them (including
      // repeats a la LL), so let's not return just yet
      sawSuffix = true;
    } else {
      // done
      enforce(i > 0, "Invalid number: ", pc);
      return munchChars(pc, i);
    }
  }
  assert(false);
}

/**
 * Assuming pc is positioned at the start of a character literal,
 * munches it from pc and returns it. A reference to line is passed in
 * order to track multiline character literals (yeah, that can
 * actually happen) correctly.
 */
static string munchCharLiteral(ref string pc, ref size_t line) {
  assert(pc[0] == '\'');
  for (size_t i = 1; ; ++i) {
    auto const c = pc[i];
    if (c == '\n') {
      ++line;
    }
    if (c == '\'') {
      // That's about it
      return munchChars(pc, i + 1);
    }
    if (c == '\\') {
      ++i;
      continue;
    }
    enforce(c, "Unterminated character constant: ", pc);
  }
}
