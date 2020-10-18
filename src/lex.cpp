// MIT License
//
// Copyright (c) 2020 PG1003
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "lex.h"
#include <locale.h>


template struct pg::lex::basic_match_result< char >;
template struct pg::lex::basic_match_result< wchar_t >;
template struct pg::lex::basic_match_result< char16_t >;
template struct pg::lex::basic_match_result< char32_t >;

template struct pg::lex::detail::string_context< char >;
template struct pg::lex::detail::string_context< wchar_t >;
template struct pg::lex::detail::string_context< char16_t >;
template struct pg::lex::detail::string_context< char32_t >;

template struct pg::lex::detail::pattern_context< char >;
template struct pg::lex::detail::pattern_context< wchar_t >;
template struct pg::lex::detail::pattern_context< char16_t >;
template struct pg::lex::detail::pattern_context< char32_t >;


pg::lex::detail::matchdepth_sentinel::matchdepth_sentinel( int &counter )
    : m_counter( counter )
{
    if( --m_counter < 0 )
    {
        throw lex_error( pattern_too_complex );
    }
}

pg::lex::detail::matchdepth_sentinel::~matchdepth_sentinel()
{
    ++m_counter;
}

static const char * lex_error_text( pg::lex::error_type code ) noexcept
{
    switch( code )
    {
    case pg::lex::pattern_too_complex:                  return "pattern too complex";
    case pg::lex::pattern_ends_with_percent:            return "malformed pattern (ends with '%%')";
    case pg::lex::pattern_missing_closing_bracket:      return "malformed pattern (missing ']')";
    case pg::lex::balanced_no_arguments:                return "malformed pattern (missing arguments to '%b')";
    case pg::lex::frontier_no_open_bracket:             return "missing '[' after '%%f' in pattern";
    case pg::lex::capture_too_many:                     return "too many captures";
    case pg::lex::capture_invalid_pattern:              return "invalid pattern capture";
    case pg::lex::capture_invalid_index:                return "invalid capture index";
    case pg::lex::capture_not_finished:                 return "unfinished capture";
    case pg::lex::capture_out_of_range:                 return "capture out of range";
    case pg::lex::percent_invalid_use_in_replacement:   return "invalid use of '%' in replacement string";
    default:                                            return "lex error";
    }
}


pg::lex::lex_error::lex_error( pg::lex::error_type ec ) noexcept
    : runtime_error( lex_error_text( ec ) )
    , error_code( ec )
{}


bool pg::lex::detail::match_class( int c, int cl ) noexcept
{
    bool res;
    switch( std::tolower( cl ) )
    {
        case 'a' : res = std::isalpha( c ); break;
        case 'c' : res = std::iscntrl( c ); break;
        case 'd' : res = std::isdigit( c ); break;
        case 'g' : res = std::isgraph( c ); break;
        case 'l' : res = std::islower( c ); break;
        case 'p' : res = std::ispunct( c ); break;
        case 's' : res = std::isspace( c ); break;
        case 'u' : res = std::isupper( c ); break;
        case 'w' : res = std::isalnum( c ); break;
        case 'x' : res = std::isxdigit( c ); break;
        case 'z' : res = (c == 0); break;  /* deprecated option */
        default: return cl == c;
    }
    return std::islower( cl ) ? res : !res;
}
