# lex

This is a C++ port of Lua's functions for pattern matching.

Lua comes with a minimalistic pattern matching capability with features you won't find in common regular expressions implementations.
This library provides an easy integration of Lua style pattern matching into your program.

The library is tested by a simple [test program](https://github.com/PG1003/lex/blob/master/test/tests.cpp) that includes test cases for pattern matching from the Lua test suite.
There are dditional tests to verify parts that are specific for the implementation of this library.  
You can also use the test program to toy with the library.

This project doesn't include makefiles or project files to build the library.
Integration in your own build environment should be easy since the library consists of only 2 source files ([lex.h](https://github.com/PG1003/lex/blob/master/src/lex.h) and [lex.cpp](https://github.com/PG1003/lex/blob/master/src/lex.cpp)) without any external dependencies.

## Features

The features listed here are specific for this implementation, not about the capabilities of Lua's pattern matching.  
You can read more about the patterns and its features in the [Lua reference manual](http://www.lua.org/manual/5.4/manual.html#6.4.1).

* Uses only the C++ standard library, no external dependencies.
* Full compatability with the match patterns as implemented in Lua 5.4. 
* Support for a wide range of string types;
  * All the ```std::string``` variants.
  * All the ```std::string_view``` variants.
  * Character pointers and arrays of type ```char```, ```wchar_t```, ```char16_t``` and ```char32_t```.
* Matching a string with a pattern.
* Substitute matching patterns with a replacement pattern or the result of a function.
* Iterate over a string with a pattern.

## Requirements

* A C++17 compliant compiler.

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

// 4 Do the substitution for the first 2 matches and print the result.
std::cout << pg::lex::gsub( str, "%s*%w+", function, 2 ) << '\n';
```
Output:
>PG1003 three four

## Documentation

The documentation here describes the usage of this library.  
Details about the patterns are documented [here](http://www.lua.org/manual/5.4/manual.html#6.4.1) in the Lua reference manual.

### Match result

A successful match returns a match result that contains one or more captures, the position and size of the matched substring. 
The number of captures depends on the pattern; the captures defined in the pattern or one for whole match when the pattern doesn't have captures.

A match result is templated on the character type of the input string.
The following predefined match result types are made available in the ```pg::lex``` namespace;

| character type of input string | match result type|
|-----------|------------------|
| ```char``` | ```match_result``` |
| ```wchar``` | ```wmatch_result``` |
| ```char16_t``` | ```u16match_result``` |
| ```char32_t``` | ```u32match_result``` |

You can iterate over the captures of a match result; it has ```begin``` and ```end``` member functions that return a random access iterator.
A match result iterator returns a string view when it is dereferenced.
Match results can also be used with range based for-loops. 

### Match

The ```pg::lex::match( str, pat )``` function searches for a pattern in a string and returns a match result.
An empty match result is returned when no match was found.    

### Iteration

To itereate over matches in a string you must create a context.
A context is a ```pg::lex::context``` object with a reference to a input string and a pattern.

You get a ```pg::lex::gmatch_iterator``` by calling the ```pg::lex::begin``` and ```pg::lex::end``` free functions with a context as parameter.
A ```pg::lex::gmatch_iterator``` behaves as a forward iterator; it can only advance with the ```++``` operator.
Gmatch iterators return match results when you dereference them. 

The ```pg::lex::begin``` function creates an iterator and searches for the first match in the input string.
The returned iterator is equal to the end iterator returned by ```pg::lex::end``` when no match was found.

A context also works with a ranged based for-loop.

### Substitute

There are two overloaded functions that substitutes a matched substring with a replacement.

The ```pg::lex::gsub( str, pat, repl, count = -1 )``` overload replaces the match with a replacement pattern.
A replacement pattern can have references to the captured substrings.
You can find more details about the replacement pattern [here](http://www.lua.org/manual/5.4/manual.html#pdf-string.gsub) in the Lua reference manual.

The second overload ```pg::lex::gsub( str, pat, function, count = -1 )``` replaces the match with te result of the function that is called for each match.
The function must accept a match result, that is templated on the character type of the input string, as parameter.

The count parameter limits number of substitutes with a negative value for an unlimited count.

The ```string.gsub``` function in Lua supports also tables as lookup for replacements.
This library doesn't have an overload to support this Lua feature since there is no equivalent in C++ for Lua like tables.
