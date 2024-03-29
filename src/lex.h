// MIT License
//
// Copyright (c) 2020 PG1003
// Copyright (C) 1994-2020 Lua.org, PUC-Rio.
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

#pragma once

#include <cassert>

#include <string>
#include <string_view>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <type_traits>
#include <memory>

#ifdef __has_cpp_attribute
# if __has_cpp_attribute( unlikely )
#  define PG_LEX_UNLIKELY [[unlikely]]
# endif
#endif
#ifndef PG_LEX_UNLIKELY
# define PG_LEX_UNLIKELY
#endif

// Maximum recursion depth for 'match'
#if !defined(MAXCCALLS)
#define MAXCCALLS   200
#endif

// Maximum number of captures that a pattern can do during pattern-matching.
#if !defined(MAXCAPTURES)
#define MAXCAPTURES     32
#endif


namespace pg
{

namespace lex
{

namespace detail
{

template< typename T >                                      struct string_traits               { static constexpr bool is_string = false; };
template<>                                                  struct string_traits< char * >     { static constexpr bool is_string = true; using char_type = char; };
template<>                                                  struct string_traits< wchar_t * >  { static constexpr bool is_string = true; using char_type = wchar_t; };
#if defined( __cpp_lib_char8_t )
template<>                                                  struct string_traits< char8_t * >  { static constexpr bool is_string = true; using char_type = char8_t; };
#endif
template<>                                                  struct string_traits< char16_t * > { static constexpr bool is_string = true; using char_type = char16_t; };
template<>                                                  struct string_traits< char32_t * > { static constexpr bool is_string = true; using char_type = char32_t;};
template< typename T, typename Traits, typename Allocater > struct string_traits< std::basic_string< T, Traits, Allocater > > : string_traits< T * > {};
template< typename T, typename Traits >                     struct string_traits< std::basic_string_view< T, Traits > >       : string_traits< T * > {};
template< typename T >                                      struct string_traits< const T >                                   : string_traits< T >   {};
template< typename T >                                      struct string_traits< const T * >                                 : string_traits< T * > {};
template< typename T >                                      struct string_traits< T [] >                                      : string_traits< T * > {};
template< typename T >                                      struct string_traits< const T [] >                                : string_traits< T * > {};
template< typename T, size_t N >                            struct string_traits< T [ N ] >                                   : string_traits< T * > {};
template< typename T, size_t N >                            struct string_traits< const T [ N ] >                             : string_traits< T * > {};
template< typename T >                                      struct string_traits< T & >                                       : string_traits< T >   {};
template< typename T >                                      struct string_traits< T && >                                      : string_traits< T >   {};


template< typename, typename >
struct match_state;

template< typename CharT >
struct capture
{
    const CharT * init() const  noexcept           { assert( begin ); return begin; }
    void          init( const CharT * i ) noexcept { assert( i ); begin = i; }

    int  len() const  noexcept { assert( length >= 0 ); return length; }
    void len( int l ) noexcept { assert( l >= 0 ); length = l; }

    void mark_unfinished() noexcept     { length = cap_state::unfinished; }
    void mark_position() noexcept       { length = cap_state::position; }
    bool is_unfinished() const noexcept { return length == cap_state::unfinished; }

    operator std::basic_string_view< CharT >() const noexcept
    {
        assert( !is_unfinished() );
        return { begin, static_cast< size_t >( std::max( length, 0 ) ) };
    }

private:
    enum cap_state : int
    {
        unfinished = -1,
        position   = -2
    };

    const CharT * begin  = nullptr;
    int           length = cap_state::unfinished;
};

template< typename CharT >
struct captures
{
    captures() noexcept = default;

    captures( const captures & other ) noexcept
    {
        operator=( other );
    }

    captures( captures && other ) noexcept
        : local{ other.local[ 0 ], other.local[ 1 ] }
        , alloc( std::move( other.alloc ) )
    {}

    captures & operator=( const captures & other ) noexcept
    {
        if( other.alloc )
        {
            alloc = std::unique_ptr< capture< CharT >[] >( new capture< CharT >[ MAXCAPTURES ] );
            std::copy( other.alloc.get(), other.alloc.get() + MAXCAPTURES, alloc.get() );
        }
        else
        {
            std::copy( other.local, other.local + max_local, local );
        }

        return *this;
    }

    capture< CharT > & operator[]( std::size_t idx ) noexcept
    {
        if( alloc )
        {
            return alloc[ idx ];
        }
        else if( idx >= max_local )
        {
            alloc = std::unique_ptr< capture< CharT >[] >( new capture< CharT >[ MAXCAPTURES ] );
            std::copy( local, local + max_local, alloc.get() );
            return alloc[ idx ];
        }

        return local[ idx ];
    }

    const capture< CharT > & operator[]( std::size_t idx ) const noexcept
    {
        return alloc ? alloc[ idx ] : local[ idx ];
    }

    const capture< CharT > * data() const noexcept
    {
        return alloc ? alloc.get() : local;
    }

private:
    static constexpr int                  max_local = 2;
    capture< CharT >                      local[ max_local ];
    std::unique_ptr< capture< CharT >[] > alloc;

    static_assert( MAXCAPTURES > max_local );
};

}

template< typename >
struct basic_match_result;

enum error_type : int
{
    pattern_too_complex,
    pattern_ends_with_percent,
    pattern_missing_closing_bracket,
    balanced_no_arguments,
    frontier_no_open_bracket,
    capture_too_many,
    capture_invalid_pattern,
    capture_invalid_index,
    capture_not_finished,
    capture_out_of_range,
    percent_invalid_use_in_replacement
};

class lex_error : public std::runtime_error
{
    const error_type error_code;

public:
    lex_error( const std::string & ) = delete;
    lex_error( const char * ) = delete;

    lex_error( pg::lex::error_type ec ) noexcept;
    [[nodiscard]] error_type code() const noexcept { return error_code; }
};

/**
 * \brief The base template for a match result
 *
 * \tparam CharT The character type of the match result.
 */
template< typename CharT >
struct basic_match_result
{
private:
    template< typename, typename >
    friend struct detail::match_state;

    std::pair< int, int >     pos   = { -1, -1 };  // The indices where the match starts and ends
    int                       level = 0;           // Total number of captures (finished or unfinished)
    detail::captures< CharT > captures;

public:

    /**
     * \brief Iterator for the captures
     */
    struct iterator
    {
        template< typename >
        friend struct basic_match_result;

        iterator() noexcept : cap( nullptr ) {}

        [[nodiscard]] const std::basic_string_view< CharT > & operator *() const noexcept { assert( cap ); return sv; }
        [[nodiscard]] const std::basic_string_view< CharT > * operator ->() const noexcept { assert( cap ); return &sv; }

                      iterator & operator ++() noexcept { move( 1 ); return *this; }
                      iterator & operator --() noexcept { move( -1 ); return *this; }
                      iterator   operator ++( int ) noexcept { const auto tmp = iterator( cap + 1 ); move( 1 ); return tmp; }
                      iterator   operator --( int ) noexcept { const auto tmp = iterator( cap - 1 ); move( -1 ); return tmp; }
                      iterator & operator +=( int i ) noexcept { move( i ); return *this; }
                      iterator & operator -=( int i ) noexcept { move( -i ); return *this; }
        [[nodiscard]] iterator   operator +( int i ) const noexcept { assert( cap ); return iterator( cap + i ); }
        [[nodiscard]] iterator   operator -( int i ) const noexcept { assert( cap ); return iterator( cap - i ); }
        [[nodiscard]] bool       operator ==( const iterator &other ) const noexcept { return cap == other.cap; }
        [[nodiscard]] bool       operator !=( const iterator &other ) const noexcept { return cap != other.cap; }

    private:
        const detail::capture< CharT > * cap = nullptr;
        std::basic_string_view< CharT >  sv;

        iterator( const detail::capture< CharT > * c ) noexcept
            : cap( c )
        {
            assert( cap );
            sv = *cap;
        }

        void move( int i ) noexcept
        {
            assert( cap );
            cap += i;
            sv = *cap;
        }
    };

    /**
     * \brief Returns an iterator to the begin of the capture list.
     */
    [[nodiscard]] iterator begin() const noexcept { return { captures.data() }; }

    /**
     * \brief Returns an iterator to the end of the capture list.
     */
    [[nodiscard]] iterator end() const noexcept { return { captures.data() + size() }; }

    /**
     * \brief Returns the number of captures.
     */
    [[nodiscard]] size_t size() const noexcept { assert( level >= 0 ); return level; }

    /**
     * \brief Conversion to bool for easy evaluation if the match result contains match data.
     */
    [[nodiscard]] operator bool() const noexcept { return level > 0; }

    /**
     * \brief Returns a std::string_view of the requested capture.
     *
     * This function throws a 'capture_out_of_range' when match result doesn't have a capture at the requested index.
     */
    [[nodiscard]] std::basic_string_view< CharT > at( size_t i ) const
    {
        if( static_cast< int >( i ) >= level ) PG_LEX_UNLIKELY
        {
            throw lex_error( capture_out_of_range );
        }

        return captures[ i ];
    }

    /**
     * \brief Returns a pair of indices that tells the position of the match in the string.
     *
     * First is the start index of the match and second one past the last character of the match.
     *
     * \note first and second are -1 when the match result doesn't contain match data.
     */
    [[nodiscard]] const std::pair< int, int > & position() const noexcept { return pos; }

    /**
     * \brief Returns the length of the match.
     */
    [[nodiscard]] size_t length() const noexcept { return pos.second - pos.first; }
};

extern template struct basic_match_result< char >;
extern template struct basic_match_result< wchar_t >;
#if defined( __cpp_lib_char8_t )
extern template struct basic_match_result< char8_t >;
#endif
extern template struct basic_match_result< char16_t >;
extern template struct basic_match_result< char32_t >;

using match_result    = basic_match_result< char >;
using wmatch_result   = basic_match_result< wchar_t >;
#if defined( __cpp_lib_char8_t )
using u8match_result  = basic_match_result< char8_t >;
#endif
using u16match_result = basic_match_result< char16_t >;
using u32match_result = basic_match_result< char32_t >;


namespace detail
{

template< typename CharT >
using pos_result = std::pair< const CharT *, bool >;

template< typename StrCharT, typename PatCharT >
using common_unsigned_char = typename std::make_unsigned< typename std::common_type< StrCharT, PatCharT >::type >;

template< typename StrCharT, typename PatCharT>
struct match_state
{
    match_state( const StrCharT * str_begin, const StrCharT * str_end, const PatCharT * pat_end, basic_match_result< StrCharT> & mr ) noexcept
        : s_begin( str_begin )
        , s_end( str_end )
        , p_end( pat_end )
        , level( mr.level )
        , captures( mr.captures )
        , pos( mr.pos )
    {
        reprepstate();
    }

    void reprepstate() noexcept
    {
        assert( matchdepth == MAXCCALLS );

        level = 0;
        pos   = { -1, -1 };
    }

    const StrCharT * const s_begin;
    const StrCharT * const s_end;
    const PatCharT * const p_end;
    int                    matchdepth = MAXCCALLS;  // Control for recursive depth (to avoid stack overflow)

    int &                          level;           // Total number of captures (finished or unfinished)
    detail::captures< StrCharT > & captures;
    std::pair< int, int > &        pos;
};


template< typename StrCharT, typename PatCharT >
auto match( match_state< StrCharT, PatCharT > & ms, const StrCharT * s, const PatCharT * p ) noexcept -> pos_result< StrCharT >;

bool match_class( int c, int cl ) noexcept;


template< typename PatCharT >
auto find_bracket_class_end( const PatCharT * p ) noexcept
{
    do
    {
        if( *p == '%' )
        {
            p += 2;
        }
    }
    while( *p++ != ']' );

    return p;
}

template< typename StrCharT, typename PatCharT >
auto matchbracketclass( const match_state< StrCharT, PatCharT > & ms, const StrCharT c, const PatCharT * p ) noexcept -> pos_result< PatCharT >
{
    using uchar_t = typename common_unsigned_char< StrCharT, PatCharT >::type;

    const auto uc = static_cast< uchar_t >( c );

    bool ret = true;
    if( *( ++p ) == '^' )
    {
        ret = false;
        ++p;    // Skip the '^'
    }

    do
    {
        if( *p == '%' )
        {
            ++p;    // Skip escapes (e.g. '%]')
            if( match_class( uc, *p ) )
            {
                return { p + 1, ret };
            }
        }
        else if( auto ec = p + 2 ; *( p + 1 ) == '-' && ec < ms.p_end && *ec != ']' )
        {
            const auto min = static_cast< uchar_t >( *p );
            const auto max = static_cast< uchar_t >( *ec );
            if( min <= uc && uc <= max )
            {
                return { ec + 1, ret };
            }
            p = ec;
        }
        else if( static_cast< uchar_t >( *p ) == uc )
        {
            return { p + 1, ret };
        }

        ++p;
    }
    while( *p != ']' );

    return { p, !ret };
}


template< typename StrCharT, typename PatCharT >
auto single_match_pr( const match_state< StrCharT, PatCharT > & ms,  const StrCharT * s, const PatCharT * p ) noexcept -> pos_result< PatCharT >
{
    using uchar_t = typename common_unsigned_char< StrCharT, PatCharT >::type;

    const bool not_end = s < ms.s_end;

    switch( *p )
    {
    case '.':
        return { p + 1, not_end }; // Matches any char

    case '%':
        return { p + 2, not_end && match_class( static_cast< uchar_t >( *s ), *( p + 1 ) ) };

    case '[':
        if( not_end )
        {
            auto [ ep, res ] = matchbracketclass( ms, *s, p );
            return { find_bracket_class_end( ep ), res };
        }
        return { find_bracket_class_end( p ), false };

    default:
        return { p + 1, not_end && static_cast< uchar_t >( *s ) == static_cast< uchar_t >( *p ) };
    }
}


template< typename StrCharT, typename PatCharT >
bool single_match( const match_state< StrCharT, PatCharT > & ms, StrCharT c, const PatCharT * p ) noexcept
{
    using uchar_t = typename common_unsigned_char< StrCharT, PatCharT >::type;

    switch( *p )
    {
    case '.':
        return true; // Matches any char

    case '%':
        return match_class( static_cast< uchar_t >( c ), *( p + 1 ) );

    case '[':
        return matchbracketclass( ms, c, p ).second;

    default:
        return static_cast< uchar_t >( *p ) == static_cast< uchar_t >( c );
    }
}


template< typename StrCharT, typename PatCharT >
const StrCharT * matchbalance( const match_state< StrCharT, PatCharT > & ms, const StrCharT * s, const PatCharT * p ) noexcept
{
    using uchar_t = typename common_unsigned_char< StrCharT, PatCharT >::type;

    if( static_cast< uchar_t >( *s ) != static_cast< uchar_t >( *p ) )
    {
        return nullptr;
    }
    else
    {
        const auto b = static_cast< uchar_t >( *p );
        const auto e = static_cast< uchar_t >( *( p + 1 ) );
        int count    = 1;
        while( ++s < ms.s_end )
        {
            const auto uc = static_cast< uchar_t >( *s );
            if( uc == e )
            {
                if( --count == 0 )
                {
                    return s + 1;
                }
            }
            else if( uc == b )
            {
              ++count;
            }
        }
    }
    return nullptr;
}


template< typename StrCharT, typename PatCharT >
auto max_expand( match_state< StrCharT, PatCharT > & ms, const StrCharT * s, const PatCharT * p, const PatCharT * ep ) noexcept -> pos_result< StrCharT >
{
    ptrdiff_t i = 0;
    while( ( s + i ) < ms.s_end && single_match( ms, *( s + i ), p ) )
    {
        ++i;
    }
    // Keeps trying to match with the maximum repetitions
    while( i >= 0 )
    {
        if( auto res = match( ms, s + i, ep + 1 ) ; res.second )
        {
            return res;
        }
        --i;
    }

    return { s, false };
}


template< typename StrCharT, typename PatCharT >
auto min_expand( match_state< StrCharT, PatCharT > & ms, const StrCharT * s, const PatCharT * p, const PatCharT * ep ) noexcept -> pos_result< StrCharT >
{
    for( ; ; )
    {
        if( auto res = match( ms, s, ep + 1 ) ; res.second  )
        {
            return res;
        }
        else if( s != ms.s_end && single_match( ms, *s, p ) )
        {
            ++s;
        }
        else
        {
            return { s, false };
        }
    }
}


template< typename StrCharT, typename PatCharT >
auto start_capture( match_state< StrCharT, PatCharT > & ms, const StrCharT * s, const PatCharT * p ) noexcept
{
    ms.captures[ ms.level ].init( s );

    if( *p == ')' )
    {
        ms.captures[ ms.level ].mark_position();
        ++p;
    }
    else
    {
        ms.captures[ ms.level ].mark_unfinished();
    }

    ++ms.level;

    auto res = match( ms, s, p );
    if( !res.second )
    {
        // Undo capture when the match has failed
        --ms.level;
        ms.captures[ ms.level ].mark_unfinished();
    }
    return res;
}


template< typename StrCharT, typename PatCharT >
auto end_capture( match_state< StrCharT, PatCharT > & ms, const StrCharT * s, const PatCharT * p ) noexcept -> pos_result< StrCharT >
{
    int i = ms.level;
    for( --i ; i >= 0 ; --i )
    {
        auto& cap = ms.captures[ i ];
        if( cap.is_unfinished() )
        {
            cap.len( static_cast< int >( s - cap.init() ) );

            auto res = match( ms, s, p );
            if( !res.second )
            {
                // Undo capture when the match has failed
                cap.mark_unfinished();
            }
            return res;
        }
    }

    return { s, false };
}


template< typename StrCharT, typename PatCharT >
auto match_capture( match_state< StrCharT, PatCharT > & ms, const StrCharT * s, PatCharT c ) noexcept -> pos_result< StrCharT >
{
    const int i              = c - '1';
    const int len            = ms.captures[ i ].len();
    const StrCharT * c_begin = ms.captures[ i ].init();
    const StrCharT * c_end   = c_begin + len;
    if( ( ms.s_end - s ) >= len &&
        std::equal( c_begin, c_end, s ) )
    {
        return { s + len, true };
    }

    return { s, false };
}


template< typename StrCharT, typename PatCharT >
auto match( match_state< StrCharT, PatCharT > & ms, const StrCharT * s, const PatCharT * p ) noexcept -> pos_result< StrCharT >
{
    while( p != ms.p_end )
    {
        switch( *p )
        {
        case '(':  // Start capture
            return start_capture( ms, s, p + 1 );

        case ')':  // End capture
            return end_capture( ms, s, p + 1 );

        case '$':
            if( ( p + 1 ) != ms.p_end )  // Is the '$' the last char in pattern?
            {
                break;
            }
            return { s, s == ms.s_end };

        case '%':  // Escaped sequences not in the format class[*+?-]?
            switch( *( p + 1 ) )
            {
            case 'b':  // Balanced string?
                if( auto res = matchbalance( ms, s, p + 2 ) )
                {
                    s  = res;
                    p += 4;
                    continue;
                }
                return { s, false };

            case 'f':  // Frontier?
                p += 2;
                if( matchbracketclass( ms, *s, p ).second )
                {
                    const StrCharT previous = ( s == ms.s_begin ) ? '\0' : *( s - 1 );
                    if( auto [ ep, res ] = matchbracketclass( ms, previous, p ) ; !res )
                    {
                        assert( *ep == ']' );
                        p = ep + 1;
                        continue;
                    }
                }
                return { s, false };

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':  // Capture results (%0-%9)?
                if( auto res = match_capture( ms, s, *( p + 1 ) ) ; res.second )
                {
                    s  = res.first;
                    p += 2;
                    continue;
                }
                return { s, false };
            }
        }

        const auto [ ep, r ] = single_match_pr( ms, s, p );
        if( r )
        {
            switch( *ep )  // Handle optional suffix
            {
            case '+':   // 1 or more repetitions
                ++s;    // 1 match already done
            [[fallthrough]];
            case '*':   // 0 or more repetitions
                return max_expand( ms, s, p, ep );

            case '-':   // 0 or more repetitions (minimum)
                return min_expand( ms, s, p, ep );

            default:    // No suffix
                if( auto res = match( ms, s + 1, ep ) ; res.second )
                {
                    return res;
                }
                return { s, false };

            case '?':  // Optional
                if( auto res = match( ms, s + 1, ep + 1 ) ; res.second )
                {
                    return res;
                }
            }
        }
        else if( *ep != '*' && *ep != '?' && *ep != '-' )  // Accept empty?
        {
            return { s, false };
        }

        p = ep + 1;
    }

    return { s, true };
}


template< typename CharT >
void append_number( std::basic_string< CharT >& str, ptrdiff_t number ) noexcept
{
    assert( number >= 0 );

    if( number > 9 )
    {
        append_number( str, number / 10 );
    }
    str.append( 1, '0' + ( number % 10 ) );
}


template< typename CharT >
struct string_context
{
    string_context( const CharT * s ) noexcept
        : string_context( s, std::char_traits< CharT >::length( s ) )
    {}

    string_context( const CharT * s, size_t l ) noexcept
        : begin( s )
        , end( s + l )
    {}

    template< size_t N >
    string_context( const CharT( & s )[ N ] ) noexcept
        : string_context( s, N )
    {}

    template< typename Traits, typename Allocator >
    string_context( const std::basic_string< CharT, Traits, Allocator > & s ) noexcept
        : string_context( s.data(), s.size() )
    {}

    template< typename Traits >
    string_context( const std::basic_string_view< CharT, Traits > & s ) noexcept
        : string_context( s.data(), s.size() )
    {}

    const CharT * const begin = nullptr;
    const CharT * const end   = nullptr;
};

extern template struct string_context< char >;
extern template struct string_context< wchar_t >;
#if defined( __cpp_lib_char8_t )
extern template struct string_context< char8_t >;
#endif
extern template struct string_context< char16_t >;
extern template struct string_context< char32_t >;


template< typename CharT >
auto find_bracket_class_end( const CharT * p, const CharT * ep )
{
    do
    {
        if( p == ep ) PG_LEX_UNLIKELY
        {
            throw lex_error( pattern_missing_closing_bracket );
        }
        else if( *p == '%' )
        {
            if( ++p == ep ) PG_LEX_UNLIKELY  // Skip escapes (e.g. '%]')
            {
                throw lex_error( pattern_ends_with_percent );
            }
            if( ++p == ep ) PG_LEX_UNLIKELY
            {
                throw lex_error( pattern_missing_closing_bracket );
            }
        }
    }
    while( *p++ != ']' );

    return p;
}

}


template< typename CharT  >
struct pattern
{
    pattern( const CharT * p )
        : pattern( p, std::char_traits< CharT >::length( p ) )
    {}

    pattern( const CharT * const p, size_t l )
        : end( p + l )
        , anchor( p < end && *p == '^' )
        , begin( anchor ? p + 1 : p )
    {
        enum class capture_state { available, unfinished, finished };

        int           depth                   = 0;
        capture_state captures[ MAXCAPTURES ] = {};
        int           capture_level           = 0;

        auto q = begin;
        while( q < end )
        {
            switch( *q )
            {
            case '(':
                if( capture_level > MAXCAPTURES ) PG_LEX_UNLIKELY
                {
                    throw lex_error( capture_too_many );
                }
                captures[ capture_level++ ] = capture_state::unfinished;
                ++q;
                ++depth;
                continue;

            case ')':
            {
                auto level = capture_level;
                while( --level >= 0 )
                {
                    if( captures[ level ] == capture_state::unfinished )
                    {
                        captures[ level ] = capture_state::finished;
                        break;
                    }
                }
                if( level < 0 ) PG_LEX_UNLIKELY
                {
                    throw lex_error( capture_invalid_pattern );
                }
                ++q;
                ++depth;
                continue;
            }

            case '$':
                ++q;
                continue;

            case '%':
                if( ++q == end ) PG_LEX_UNLIKELY
                {
                    throw lex_error( pattern_ends_with_percent );
                }
                switch( *q )
                {
                case 'b':
                    q += 3;
                    if( q > end ) PG_LEX_UNLIKELY
                    {
                        throw lex_error( balanced_no_arguments );
                    }
                    continue;

                case 'f':
                    if( *( ++q ) != '[' ) PG_LEX_UNLIKELY
                    {
                        throw lex_error( frontier_no_open_bracket );
                    }
                    q = detail::find_bracket_class_end( q, end );
                    continue;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    if( const int i = *q - '1' ; i < 0 ||
                                                 i >= capture_level ||
                                                 captures[ i ] != capture_state::finished ) PG_LEX_UNLIKELY
                    {
                        throw lex_error( capture_invalid_index );
                    }
                    ++q;
                    continue;
                }
            }

            if( q != end && *q == '[' )
            {
                q = detail::find_bracket_class_end( q, end );
            }
            else
            {
                ++q;
            }
            if( q != end && ( *q == '*' || *q == '+' || *q == '?' || *q == '-' ) )
            {
                ++q;
            }

            ++depth;
        }

        if( std::any_of( captures, captures + capture_level,
                         []( const auto cap ){ return cap != capture_state::finished; } ) ) PG_LEX_UNLIKELY
        {
            throw lex_error( capture_not_finished );
        }

        if( depth > MAXCCALLS ) PG_LEX_UNLIKELY
        {
            throw lex_error( pattern_too_complex );
        }
    }

    template< size_t N >
    pattern( const CharT( & p )[ N ] )
        : pattern( p, N )
    {}

    template< typename Traits, typename Allocator >
    pattern( const std::basic_string< CharT, Traits, Allocator > & s )
        : pattern( s.data(), s.size() )
    {}

    template< typename Traits >
    pattern( const std::basic_string_view< CharT, Traits > & s )
        : pattern( s.data(), s.size() )
    {}

    const CharT * const end    = nullptr;
    const bool          anchor = false;
    const CharT * const begin  = nullptr;
};

extern template struct pattern< char >;
extern template struct pattern< wchar_t >;
#if defined( __cpp_lib_char8_t )
extern template struct pattern< char8_t >;
#endif
extern template struct pattern< char16_t >;
extern template struct pattern< char32_t >;


/**
 * \brief A lex gmatch_context is an input string combined with a pattern.
 *
 * You can get a pg::lex::gmatch_iterator using the pg::lex::begin and pg::lex::end functions and
 * iterate with it over all matches in a lex context.
 *
 * A lex gmatch_context also works with ranged based for-loops but using the
 * pg::lex::gmatch function is prefered.
 *
 * \note A lex context keeps a reference to the input string and pattern.
 *
 * \tparam StrCharT The char type of the input string.
 * \tparam PatCharT The char type of the pattern.
 *
 * \see pg::lex::gmatch
 * \see pg::lex::gmatch_iterator
 * \see pg::lex::begin
 * \see pg::lex::end
 */
template< typename StrCharT, typename PatCharT >
struct gmatch_context
{
    const detail::string_context< StrCharT > s;
    const pattern< PatCharT >                p;

    template< typename StrT, typename PatT >
    gmatch_context( StrT && s, PatT && p ) noexcept
        : s( std::forward< StrT >( s ) )
        , p( std::forward< PatT >( p ) )
    {}

    template< typename StrT, typename PatCharT_ >
    gmatch_context( StrT && s, const pattern< PatCharT_ > & p ) noexcept
        : s( std::forward< StrT >( s ) )
        , p( p )
    {}

    [[nodiscard]] bool operator ==( const gmatch_context< StrCharT, PatCharT >& other ) const noexcept
    {
        return s.begin == other.s.begin && s.end == other.s.end &&
               p.begin == other.p.begin && p.end == other.p.end;
    }
};

template< typename StrT, typename PatT >
gmatch_context( StrT &&, PatT && ) noexcept ->
gmatch_context< typename detail::string_traits< StrT >::char_type,
                typename detail::string_traits< PatT >::char_type >;

template< typename StrT, typename PatCharT >
gmatch_context( StrT &&, const pattern< PatCharT > & ) noexcept ->
gmatch_context< typename detail::string_traits< StrT >::char_type, PatCharT >;


/**
 * \brief An iterator for matches in pg::lex::context objects.
 *
 * The constructor does not perform the first match so the match result is not yet valid.
 * First you must increase the iterator before dereferencing it to search for the first match result.
 *
 * The iterator behaves as a forward iterator and can only advance with the `++` operator.
 *
 * \see pg::lex::gmatch
 * \see pg::lex::gmatch_context
 * \see pg::lex::begin
 * \see pg::lex::end
 */
template< typename StrCharT, typename PattternT >
struct gmatch_iterator
{
    gmatch_iterator( const gmatch_context< StrCharT, PattternT > & ctx, const StrCharT * start ) noexcept
        : c( ctx )
        , pos( start )
    {}

    /**
     * \brief Iterates to the next match in the context.
     *
     * The match result is empty when the end is reached.
     */
    gmatch_iterator& operator ++()
    {
        detail::match_state ms = { c.s.begin, c.s.end, c.p.end, mr };

        while( pos <= c.s.end )
        {
            auto [ e, r ] = detail::match( ms, pos, c.p.begin );
            if( !r || e == last_match )
            {
                pos = e + 1;
                ms.reprepstate();
            }
            else
            {
                ms.pos = { static_cast< int >( pos - c.s.begin ), static_cast< int >( e - c.s.begin ) };
                if( ms.level == 0 )
                {
                    ms.captures[ ms.level ].init( pos );
                    ms.captures[ ms.level ].len( static_cast< int >( e - pos ) );
                    ++ms.level;
                }
                last_match = e;
                pos        = e;

                return *this;
            }
        }

        return *this;
    }

    [[nodiscard]] bool operator ==( const gmatch_iterator & other ) const noexcept
    {
        return c == other.c && pos == other.pos;
    }

    [[nodiscard]] bool operator !=( const gmatch_iterator & other ) const noexcept
    {
        return !( *this == other );
    }

    /**
     * \brief Dereferences to a match result.
     */
    [[nodiscard]] const auto & operator *() const noexcept
    {
        return mr;
    }

    /**
     * \brief Returns a pointer to a match result.
     */
    [[nodiscard]] auto operator ->() const noexcept
    {
        return &mr;
    }

private:
    const gmatch_context< StrCharT, PattternT > c;
    const StrCharT *                            pos        = nullptr;
    const StrCharT *                            last_match = nullptr;
    basic_match_result< StrCharT >              mr;
};

/**
 * \brief Returns a pg::lex::gmatch_iterator from a lex context object.
 *
 * When the context contains matches, the iterator has initial the result of the first match.
 * The iterator is equal to the end iterator when the context has no matches.
 *
 * \see pg::lex::gmatch_iterator
 * \see pg::lex::end
 */
template< typename StrCharT, typename PatCharT >
[[nodiscard]] auto begin( const gmatch_context< StrCharT, PatCharT > & c )
{
    auto it = gmatch_iterator( c, c.s.begin );
    return ++it;
}

/**
 * \brief Returns a pg::lex::gmatch_iterator that indicates the end of a lex context.
 *
 * The result of the end iterator is always an empty match result.
 *
 * \see pg::lex::gmatch_iterator
 * \see pg::lex::begin
 */
template< typename StrCharT, typename PatCharT >
[[nodiscard]] auto end( const gmatch_context< StrCharT, PatCharT > & c ) noexcept
{
    return gmatch_iterator( c, c.s.end + 1 );
}

/**
 * \brief Returns a pg::lex::gmatch_context that is used to iterate with a pattern over string.
 *
 * This function is most likely to be used with a range-based for.
 *
 * \see pg::lex::gmatch_context
 */
template< typename StrT, typename PatT >
[[nodiscard]] auto gmatch( StrT && str, PatT && pat ) noexcept
{
    return gmatch_context( std::forward< StrT >( str ), std::forward< PatT >( pat ) );
}

/**
 * \brief Searches for the first match of a pattern in an input string.
 *
 * \return Returns a match result based on the character type of the input string.
 */
template< typename StrT, typename PatCharT >
[[nodiscard]] auto match( StrT && str, const pattern< PatCharT > & pat )
{
    using str_char_type = typename detail::string_traits< StrT >::char_type;

    basic_match_result< str_char_type > mr;
    const auto                          c   = gmatch( std::forward< StrT >( str ), pat );
    detail::match_state                 ms  = { c.s.begin, c.s.end, c.p.end, mr };
    const str_char_type *               pos = c.s.begin;

    do
    {
        auto [ e, r ] = detail::match( ms, pos, c.p.begin );
        if( r )
        {
            ms.pos = { static_cast< int >( pos - c.s.begin ), static_cast< int >( e - c.s.begin ) };
            if( ms.level == 0 )
            {
                ms.captures[ ms.level ].init( pos );
                ms.captures[ ms.level ].len( static_cast< int >( e - pos ) );
                ++ms.level;
            }

            return mr;
        }
        else
        {
            pos = e + 1;
        }
    }
    while( pos <= c.s.end && !c.p.anchor );

    return mr;
}

template< typename StrT, typename PatT,
          typename detail::string_traits< PatT >::char_type = 0 >  // Terminate recursion of PatT
[[nodiscard]] auto match( StrT && str, PatT && pat )
{
    using PatCharT = typename detail::string_traits< PatT >::char_type;

    return match( std::forward< StrT >( str ), pattern< PatCharT >( pat ) );
}

/**
 * \brief Substitutes a replacement pattern for a match found in the input string.
 *
 * \param str   The input string.
 * \param pat   The pattern used to find matches in the input string.
 * \param repl  The replacement pattern that substitutes the match.
 * \param count The maximum number of substitutes; negative for unlimited an unlimited count.
 *
 * \return Returns a std::string based on the character type of the input string.
 */
template< typename StrT, typename PatCharT, typename ReplT,
          typename std::enable_if< detail::string_traits< ReplT >::is_string, int >::type = 0 >
[[nodiscard]] auto gsub( StrT && str, const pattern< PatCharT > & pat, ReplT && repl, int count = -1 )
{
    static_assert( detail::string_traits< ReplT >::is_string, "Replacement pattern is not one of the supported string-like types!" );

    using str_char_type  = typename detail::string_traits< StrT >::char_type;
    using repl_char_type = typename detail::string_traits< ReplT >::char_type;
    using iterator_type  = gmatch_iterator< str_char_type, PatCharT >;

    const detail::string_context< repl_char_type > r            = { std::forward< ReplT >( repl ) };
    const auto                                     c            = gmatch( std::forward< StrT >( str ), pat );
    auto                                           match_it     = iterator_type( c, c.s.begin );
    const auto                                     match_end_it = end( c );

    std::basic_string< str_char_type > result;
    result.reserve( c.s.end - c.s.begin );

    const str_char_type * copy_begin = c.s.begin;
    while( ++match_it != match_end_it && count != 0 )
    {
        --count;

        const auto & mr                = *match_it;
        const str_char_type * copy_end = c.s.begin + mr.position().first;

        result.append( copy_begin, copy_end );

        auto r_begin = r.begin;
        for( auto find = std::find( r_begin, r.end, '%' ) ;
            find != r.end ;
            r_begin = find + 1, find = std::find( r_begin, r.end, '%' ) )
        {
            result.append( r_begin, find );     // Copy pattern before '%'
            ++find;                             // skip ESC

            const str_char_type cap_char = *find;
            if( cap_char == '%' )                // %%
            {
                result.append( 1u, cap_char );
            }
            else if( cap_char == '0' )           // %0
            {
                const str_char_type * const match_begin = c.s.begin + mr.position().first;
                const str_char_type * const match_end   = c.s.begin + mr.position().second;
                result.append( match_begin ,match_end );
            }
            else if( std::isdigit( cap_char ) )  // %n
            {
                const auto cap_index = static_cast< size_t >( cap_char - '1' );
                if( cap_index >= mr.size() ) PG_LEX_UNLIKELY
                {
                    throw lex_error( capture_invalid_index );
                }
                const auto cap = mr.at( cap_index );
                if( cap.size() == 0 )   // Position capture
                {
                    const ptrdiff_t pos = 1 + cap.data() - c.s.begin;
                    detail::append_number( result, pos );
                }
                else
                {
                    result.append( cap.data(), cap.size() );
                }
            }
            else
            {
                throw lex_error( percent_invalid_use_in_replacement );
            }
        }

        result.append( r_begin, r.end );

        copy_begin = c.s.begin + mr.position().second;
    }

    result.append( copy_begin, c.s.end );

    return result;
}

template< typename StrT, typename PatT, typename ReplT,
          typename std::enable_if< detail::string_traits< ReplT >::is_string, int >::type = 0,
          typename detail::string_traits< PatT >::char_type = 0 >  // Terminate recursion of PatT
[[nodiscard]] auto gsub( StrT&& str, PatT && pat, ReplT && repl, int count = -1 )
{
    using PatCharT = typename detail::string_traits< PatT >::char_type;

    return gsub( std::forward< StrT >( str ), pattern< PatCharT >( pat ), std::forward< ReplT >( repl ), count );
}

/**
 * \brief Substitutes a replacement for a match found in the input string.
 *
 * \param str   The input string.
 * \param pat   The pattern used to find matches in the input string.
 * \param repl  A function that accepts a match result and returns the replacement.
 * \param count The maximum number of substitutes; negative for unlimited an unlimited count.
 *
 * \return Returns a std::string based on the character type of the input string.
 */
template< typename StrT, typename PatCharT, typename Function,
          typename std::enable_if< !detail::string_traits< Function >::is_string, int >::type = 0 >
[[nodiscard]] auto gsub( StrT && str, const pattern< PatCharT > & pat, Function && func, int count = -1 )
{
    using str_char_type = typename detail::string_traits< StrT >::char_type;
    using iterator_type = gmatch_iterator< str_char_type, PatCharT >;

    const auto c            = gmatch( std::forward< StrT >( str ), pat );
    auto       match_it     = iterator_type( c, c.s.begin );
    const auto match_end_it = end( c );

    std::basic_string< str_char_type > result;
    result.reserve( c.s.end - c.s.begin );

    const str_char_type * copy_begin = c.s.begin;
    while( ++match_it != match_end_it && count != 0 )
    {
        --count;

        const auto & mr                = *match_it;
        const str_char_type * copy_end = c.s.begin + mr.position().first;

        result.append( copy_begin, copy_end );
        result.append( func( mr ) );

        copy_begin = c.s.begin + mr.position().second;
    }

    result.append( copy_begin, c.s.end );

    return result;
}

template< typename StrT, typename PatT, typename Function,
          typename std::enable_if< !detail::string_traits< Function >::is_string, int >::type = 0,
          typename detail::string_traits< PatT >::char_type = 0 >  // Terminate recursion of PatT
[[nodiscard]] auto gsub( StrT && str, PatT && pat, Function && func, int count = -1 )
{
    using PatCharT = typename detail::string_traits< PatT >::char_type;

    return gsub( std::forward< StrT >( str ), pattern< PatCharT >( pat ), std::forward< Function >( func ), count );
}

}

}
