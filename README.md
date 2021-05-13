# lex

A C++ library for Lua style pattern matching.

The standard library of the Lua program language includes a minimalistic pattern matching capability with features you won't find in common regular expressions implementations.
This library provides an easy integration of Lua style pattern matching into your C++ application.

The library is tested by a simple [test program](https://github.com/PG1003/lex/blob/master/test/tests.cpp) that includes tests which are ported from the Lua test suite.
There are additional tests to verify parts that are specific for the implementation of this library.  
You can also use the test program to toy with the library.

This project includes a makefile to build the test program but doesn't include makefiles or project files to build the library.
Integration in your own build environment should be easy since the library consists of only 2 source files ([lex.h](https://github.com/PG1003/lex/blob/master/src/lex.h) and [lex.cpp](https://github.com/PG1003/lex/blob/master/src/lex.cpp)) without any external dependencies.

## Features

The features listed here are specific for this implementation, not about the capabilities of Lua's pattern matching.

* Uses only the C++ standard library, no external dependencies.
* Full compatability with the match patterns as implemented in Lua 5.4. 
* Support for a wide range of string types such as std::string, std::string_view, character arrays and pointers. All of these can be based on the character types as defined by C++17 and C++20.
* Match a string with a pattern.
* Substitute a matching pattern by a replacement pattern or the result of a function which is called for each match.
* Iterate over a string with a pattern.

## Requirements

* Minimal a C++17 compliant compiler.

## Examples

### Match a word at the begin of a string
```c++
// 1 Our input string to match.
std::string str = "Hello world!";

// 2 Match a word at the beginning of a string.
auto result = pg::lex::match( str, "^%a+" );

// 3 Validate if there was a match.
assert( result );

// 4 Print the match.
std::cout << "Match: " << result.at( 0 ) << '\n';
```
Output:
>Match: Hello

### Match with a capture

```c++
// 1 Our input string.
std::wstring str = "Hello PG1003!";

// 2 Match and capture 'PG'.
auto result = pg::lex::match( str, "(%a+)%d+" );

// 3 Validate if there was a match
assert( result );

// 4 Print the capture.
std::cout << "Capture: "<< result.at( 0 ) << '\n';
```
Output:
>Capture: PG

### Iterate with a pattern

```c++
// 1 Our input string.
auto str = u"foo = 42;   bar= 1337; baz = PG =1003 ;";

// 2 Iterate over all key/value pairs.
for( auto match : pg::lex::context( str, "(%a+)%s*=%s*(%d+)%s*;" ) )
{
    // 3 The match should have 2 captures.
    assert( match.size() == 2 );
    
    // 4 Print the key/value pairs.
    std::cout << "Key: " << match.at( 0 ) << ", Value: " << match.at( 1 ) << '\n';
}
```
Output:
>Key: foo, Value: 42  
>Key: bar, Value: 1337  
>Key: PG, Value: 1003

### Substitution with a replacement pattern

```c++
// 1 Our input string.
auto str = "foo =\t42; bar= 1337; pg =1003 ;";

// 2 Match pattern.
auto pat = "(%a+)%s*=%s*(%d+)%s*;";

// 3 Replacement pattern.
auto repl = "%1=%2;";

// 4 Do the global substitution.
auto result = pg::lex::gsub( str, pat, repl );

// 5 Print result.
std::cout << reslult << '\n';
}
```
Output:
>foo=42; bar=1337; pg=1003;

### Substitution with a function

```c++
// 1 Our input string.
auto str = "one two three four";

// 2 Function that generates the replacement.
std::string function( const pg::lex::match_result & mr )
{
    if( mr.at( 0 ) == "one" )
    {
        return "PG";
    }

    return "1003";
}

// 3 Do the substitution for the first 2 matches and print the result.
std::cout << pg::lex::gsub( str, "%s*%w+", function, 2 ) << '\n';
```
Output:
>PG1003 three four

## Documentation

### Lex errors

This library throws exceptions of the `pg::lex:lex_error` type which is derived from `std::runtime_error`.
The errors emitted by Lua regarding to pattern matching are thrown too by this library.

You can get the exception description and number by calling the `what()` and `code()` member functions.

### Match result

A successful match returns a match result that contains one or more [captures](#captures), the position and size of the matched substring. 
The number of captures depends on the pattern; the captures defined in the pattern or one for whole match when the pattern doesn't have captures.
For convenience a match result has een `operator bool` that returns true when it contains at least one capture. 

A match result is templated on the character type of the input string.
The following predefined match result types are made available in the `pg::lex` namespace;

| character type of input string | match result type|
|-----------|------------------|
|  `char`  |  `match_result`  |
|  `wchar`  |  `wmatch_result`  |
|  `char8_t` (C++20) |  `u8match_result` |
|  `char16_t`  |  `u16match_result` |
|  `char32_t`  | `u32match_result` |

You can iterate over the captures of a match result; it has `begin` and `end` member functions that return a random access iterator.
A match result iterator returns a string view when it is dereferenced.  
Match results can also be used with range based for-loops. 

### Match

The `pg::lex::match( string, pattern )` function searches for a [pattern](#patterns) in a string and returns a match result.
An empty match result is returned when no match was found.

### Iteration

To itereate over matches in a string you must create a context.
A context is a `pg::lex::context` object with a reference to a input string and a pattern.

You get a `pg::lex::gmatch_iterator` by calling the `pg::lex::begin` and `pg::lex::end` functions with a context as parameter.
A `pg::lex::gmatch_iterator` behaves like a forward iterator; it can only advance with the `++` operator.
Gmatch iterators return match results when you dereference them. 

The `pg::lex::begin` function creates an iterator and searches for the first match in the input string.
The returned iterator is equal to the iterator returned by `pg::lex::end` when no match was found.

A `pg::lex::context` is compatible with ranged based for-loops as shown in the [Iterate with a pattern](#iterate-with-a-pattern) example.

### Substitute

There are two overloaded functions that substitute a matched substring with a replacement.

The `pg::lex::gsub( string, pattern, replacement, count = -1 )` overload replaces the match with a [replacement pattern](#replacement-pattern).

The second overload `pg::lex::gsub( string, pattern, function, count = -1 )` replaces the match with te result of the function that is called for each match.
The function must accept a match result as parameter that is templated on the same character type as the input string.

The count parameter limits number of substitutes with a negative value for an unlimited count.

The `string.gsub` function in Lua also supports tables as lookup for replacements.
This library does _not_ support this Lua feature.

#### Replacement pattern

A replacement pattern is a string that contains a repacement text which can include [captures](#captures) of the match result.
References to captures are marked as `%d` where d is a number between 1 and 9 to refrence the first up to nineth capture.
`%0` stands for the whole match.
The whole match will handled as one capture when a pattern didn't specified any captures.

```c++
auto a = pg::lex::gsub( "hello world", "(%w+)", "%1 %1" );
assert( a == "hello hello world world" );
     
auto b = pg::lex::gsub("hello world", "%w+", "%0 %0", 1); // Whole match
auto c = pg::lex::gsub("hello world", "%w+", "%1 %1", 1); // Same since there are no captures
assert( b == "hello hello world" );
assert( b == c );
     
auto d = pg::lex::gsub("hello world from Lua", "(%w+)%s*(%w+)", "%2 %1");
assert( d == "world hello Lua from" );
```

### Patterns

_For convenience this paragraph is copied from the patterns paragraph in the [Lua reference manual](http://www.lua.org/manual/5.4/manual.html#6.4.1) and adjusted to match the usage of this library._

Patterns are described by regular strings, which are interpreted when [matching](#match), [iterating](#iteration) and [substituting](#substitute).
This section describes the syntax and the meaning (that is, what they match) of these strings.

#### Character class

A character class is used to represent a set of characters.
The following combinations are allowed in describing a character class:

* `x` (where x is not one of the magic characters ^$()%.[]*+-?) represents the character x itself.
* `.` (a dot) represents all characters.
* `%a` represents all letters.
* `%c` represents all control characters.
* `%d` represents all digits.
* `%g` represents all printable characters except space.
* `%l` represents all lowercase letters.
* `%p` represents all punctuation characters.
* `%s` represents all space characters.
* `%u` represents all uppercase letters.
* `%w` represents all alphanumeric characters.
* `%x` represents all hexadecimal digits.
* `%x` (where x is any non-alphanumeric character) represents the character x.
This is the standard way to escape the magic characters.
Any non-alphanumeric character (including all punctuation characters, even the non-magical) can be preceded by a `%` to represent itself in a pattern.
* `[set]` represents the class which is the union of all characters in set.
A range of characters can be specified by separating the end characters of the range, in ascending order, with a `-`.
All classes %x described above can also be used as components in set.
All other characters in set represent themselves.
For example, `[%w_]` (or `[_%w]`) represents all alphanumeric characters plus the underscore, `[0-7]` represents the octal digits, and `[0-7%l%-]` represents the octal digits plus the lowercase letters plus the `-` character.  
You can put a closing square bracket in a set by positioning it as the first character in the set.
You can put a hyphen in a set by positioning it as the first or the last character in the set. (You can also use an escape for both cases.)  
The interaction between ranges and classes is not defined.
Therefore, patterns like `[%a-z]` or `[a-%%]` have no meaning.
* `[^set]` represents the complement of set, where set is interpreted as above.

For all classes represented by single letters (`%a`, `%c`, etc.), the corresponding uppercase letter represents the complement of the class.
For instance, `%S` represents all non-space characters.

The definitions of letter, space, and other character groups depend on the current locale.
In particular, the class `[a-z]` may not be equivalent to `%l`.

#### Pattern item

A pattern item can be:

* a single character class, which matches any single character in the class.
* a single character class followed by `*`, which matches sequences of zero or more characters in the class. These repetition items will always match the longest possible sequence.
* a single character class followed by `+`, which matches sequences of one or more characters in the class. These repetition items will always match the longest possible sequence.
* a single character class followed by `-`, which also matches sequences of zero or more characters in the class. Unlike `*`, these repetition items will always match the shortest possible sequence.
* a single character class followed by `?`, which matches zero or one occurrence of a character in the class. It always matches one occurrence if possible.
* `%n`, for n between 1 and 9; such item matches a substring equal to the n-th captured string (see [captures](#captures) below).
* `%bxy`, where x and y are two distinct characters; such item matches strings that start with x, end with y, and where the x and y are balanced.
This means that, if one reads the string from left to right, counting +1 for an x and -1 for a y, the ending y is the first y where the count reaches 0.
For instance, the item `%b()` matches expressions with balanced parentheses.
* `%f[set]`, a frontier pattern; such item matches an empty string at any position such that the next character belongs to set and the previous character does not belong to set.
The set set is interpreted as previously described.
The beginning and the end of the subject are handled as if they were the character `\0`.

#### Pattern

A pattern is a sequence of pattern items.
A caret `^` at the beginning of a pattern anchors the match at the beginning of the subject string.
A `$` at the end of a pattern anchors the match at the end of the subject string.
At other positions, `^` and `$` have no special meaning and represent themselves.

#### Captures

A pattern can contain sub-patterns enclosed in parentheses; they describe captures.
When a match succeeds, the substrings of the subject string that match captures are stored (captured) for future use.
Captures are numbered according to their left parentheses.
For instance, in the pattern `(a*(.)%w(%s*))`, the part of the string matching `a*(.)%w(%s*)` is stored as the first capture, the character matching `.` is second capture and finally the part matching `%s*` is the third capture.

As a special case, the capture `()` captures the current string position (a number).
For instance, if we apply the pattern `()aa()` on the string `flaaap`, there will be two captures: 2 and 4.

#### Multiple matches

The function `pg::lex::gsub` and iterator `pg::lex::gmatch_iterator` match multiple occurrences of the given pattern in the subject.
For these functions, a new match is considered valid only if it ends at least one character after the end of the previous match.
In other words, the pattern machine never accepts the empty string as a match immediately after another match.

As an example, consider the results of the following code:

```c++
for( const auto & match : pg::lex::context( "abc", "()a*()" ) )
{
    const auto position = match.position();
    std::cout << position.first << " - " << position.second << '\n';
}
```
Which outputs:
>0 - 1  
>2 - 2  
>3 - 3  

The second and third results come from matching an empty string after 'b' and another one after 'c'.
The iterator does not match an empty string after 'a', because it would end at the same position of the previous match.
