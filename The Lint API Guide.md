The Lint API Guide
==================

When writing a Lint Check to add to `Flint++` you are given a Token Stream of the form `vector<Token>` with which you can traverse and analyse in order to detect potential areas for review. To traverse the Token Stream there are several predefined functions to help you move around. The purpose of this document is to enumerate those functions and their purposes to avoid the scenario where contributors feel they have to reinevent functionality which they did not know was already present.

## 1. The structure of a Lint Check

A complete Lint Check consists of (atleast) **3** file modifications. These are:

* A new header definition in the `Checks.hpp` file, of the form

	X(DescriptiveNameOfCheck)

* An implementation of the aforementioned Check in the `Checks.cpp` file, of the form

	void checkDescriptiveNameOfCheck(ErrorFile &errors, const string &path, const vector<Token> &tokens);

* And finally, an addition to the list of Checks to be run in the `Main.cpp` file, where the function call to the new Lint Check is placed in the approriate scope.

## 2. Reporting Errors

As your function Lints through the given token stream you'll want a way to announce that an error has been found. This is done using one of three functions which reflect the severity of the Lint Error.

[Referenced from Lines 25-46 of Checks.cpp](https://github.com/L2Program/FlintPlusPlus/blob/master/flint/Checks.cpp#L25-L46) the three report functions are `lintError`, `lintWarning`, and `lintAdvice`. These functions have the form:

	lintError(ErrorFile &errors, const Token &token, const string title, const string desc = "")

Where `errors` is the report struct given to your function to be passed along, `token` is the first token of the sequence which broke the rules of your Lint Check, and `title` which refers to the main message of the error with an optional `desc` field for an extended description. 

## 3. How to traverse the Token Stream

### atSequence
	
```cpp
/**
* Returns whether the current token is at the start of a given sequence
*
* @param tokens
*		The token list for the file
* @param pos
*		The current index position inside the token list
* @param list
*		The token list for the desired sequence
* @return
*		Returns true if we were at the start of a given sequence
*/
bool atSequence(const vector<Token> &tokens, size_t pos, const vector<TokenType> &list)
```

### atBuiltinType

```cpp
/**
* Returns whether the current token is a built in type
*
* @param tokens
*		The token list for the file
* @param pos
*		The current index position inside the token list
* @return
*		Returns true is the token as pos is a built in type
*/
bool atBuiltinType(const vector<Token> &tokens, size_t pos)
```

### skipToToken

```cpp
/**
* Moves pos to the next position of the target token
*
* @param tokens
*		The token list for the file
* @param pos
*		The current index position inside the token list
* @param target
*		The token to match
* @return
*		Returns true if we are at the given token
*/
bool skipToToken(const vector<Token> &tokens, size_t &pos, TokenType target)
```

### skipTemplateSpec

```cpp
/**
* Traverses the token list until the whole template sequence has been passed
*
* @param tokens
*		The token list for the file
* @param pos
*		The current index position inside the token list
* @param containsArray
*		Optional parameter to return a bool of whether an array was found inside
*		the template list
* @return
*		Returns the position of the closing angle bracket
*/
size_t skipTemplateSpec(const vector<Token> &tokens, size_t pos, bool *containsArray = nullptr)
```

### getIncludedPath

```cpp
/**
* Strips the ""'s or <>'s from an #include path
*
* @param path
*		The string to trim
* @return
*		Returns the include path without it's wrapping quotes/brackets
*/
string getIncludedPath(const string &path)
```

#### There is more to come but I'm busy. Sorry