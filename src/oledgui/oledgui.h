/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

// See README.md for a brief code overview
namespace og {
enum class Visibility {
        visible,    // Widget drew something on the screen.
        outside,    // Widget is invisible because is outside the active region.
        nonDrawable // Widget is does not draw anything by itself.
};

struct Empty {};

using Coordinate = int16_t;
using Dimension = uint16_t;
using Focus = uint16_t;
using Selection = uint8_t;
using LineOffset = int;

namespace style {
        enum class Text { regular, highlighted };
        enum class Focus { disabled, enabled };
        enum class Editable { no, yes };

        /// Style tag. Use negative numbers for custom user widgets.
        using Tag = int;
        constexpr Tag button = 1;
        constexpr Tag check = 2;
        constexpr Tag combo = 3;
        constexpr Tag group = 4;
        constexpr Tag label = 5;
        constexpr Tag layout = 6;
        constexpr Tag line = 7;
        constexpr Tag number = 8;
        constexpr Tag radio = 9;
        constexpr Tag item = 10;
        constexpr Tag text = 11;
        constexpr Tag window = 12;

        /// Default empty style
        template <Tag tag> struct Style {};
        using Empty = void;

        /* clang-format off */
        namespace getter {
                template <typename T> struct Focus {};
                template <typename T> requires requires { T::focus; } struct Focus<T> { static constexpr auto value = T::focus; };

                template <typename T> struct Editable {};
                template <typename T> requires requires { T::editable; } struct Editable<T> { static constexpr auto value = T::editable; };

                template <typename T> struct Checked {};
                template <typename T> requires requires { T::checked; } struct Checked<T> { static constexpr auto value = T::checked; };

                template <typename T> struct Unchecked {};
                template <typename T> requires requires { T::unchecked; } struct Unchecked<T> { static constexpr auto value = T::unchecked; };

                template <typename T> struct HLine {};
                template <typename T> requires requires { T::hline; } struct HLine<T> { static constexpr auto value = T::hline; };

                template <typename T> struct VLine {};
                template <typename T> requires requires { T::vline; } struct VLine<T> { static constexpr auto value = T::vline; };

                template <typename T> struct NWCorner {};
                template <typename T> requires requires { T::nwcorner; } struct NWCorner<T> { static constexpr auto value = T::nwcorner; };

                template <typename T> struct NECorner {};
                template <typename T> requires requires { T::necorner; } struct NECorner<T> { static constexpr auto value = T::necorner; };

                template <typename T> struct SWCorner {};
                template <typename T> requires requires { T::swcorner; } struct SWCorner<T> { static constexpr auto value = T::swcorner; };

                template <typename T> struct SECorner {};
                template <typename T> requires requires { T::secorner; } struct SECorner<T> { static constexpr auto value = T::secorner; };

                template <typename T> struct Frame {};
                template <typename T> requires requires { T::frame; } struct Frame<T> { static constexpr auto value = T::frame; };

                template <typename T> struct RadioVisible {};
                template <typename T> requires requires { T::radioVisible; } struct RadioVisible<T> { static constexpr auto value = T::radioVisible; };

                template <typename T> struct Interspace {};
                template <typename T> requires requires { T::interspace; } struct Interspace<T> { static constexpr auto value = T::interspace; };
        } // namespace getter
        /* clang-format on */

        /// Style helper
        template <style::Tag globalStyle, typename LocalStyle, template <typename T> typename Getter> constexpr auto get (auto defult)
        {
                if constexpr (requires { Getter<LocalStyle>::value; }) {
                        return Getter<LocalStyle>::value;
                }

                if constexpr (requires { Getter<Style<globalStyle>>::value; }) {
                        return Getter<Style<globalStyle>>::value;
                }

                return defult;
        }

} // namespace style

class Point {
public:
        constexpr Point (Coordinate x = 0, Coordinate y = 0) : x_{x}, y_{y} {}
        Point (Point const &) = default;
        Point &operator= (Point const &p) = default;
        Point (Point &&) noexcept = default;
        Point &operator= (Point &&) noexcept = default;
        ~Point () = default;

        Point &operator+= (Point const &p)
        {
                x_ += p.x_;
                y_ += p.y_;
                return *this;
        }

        Point &operator-= (Point const &p)
        {
                x_ -= p.x_;
                y_ -= p.y_;
                return *this;
        }

        Point operator+ (Point p) const
        {
                p += *this;
                return p;
        }

        Point operator- (Point const &p)
        {
                Point ret = *this;
                ret -= p;
                return ret;
        }

        Coordinate &x () { return x_; }
        Coordinate x () const { return x_; }
        Coordinate &y () { return y_; }
        Coordinate y () const { return y_; }

private:
        Coordinate x_;
        Coordinate y_;
};

struct Dimensions {
        constexpr Dimensions (Dimension w = 0, Dimension h = 0) : width{w}, height{h} {}

        Dimension width{};
        Dimension height{};
};

/**
 * Runtime context for all the recursive loops.
 */
struct Context {
        Point const origin{};
        Dimensions const dimensions{};
        Focus currentFocus{};
        Coordinate currentScroll{};
};

enum class Key { unknown, incrementFocus, decrementFocus, select };

struct EmptyDisplay;

namespace c {

        /**
         * Layer 1 widget.
         */
        template <typename T>
        concept widget = requires (T const t, EmptyDisplay &d, Context const &c) {
                                 {
                                         T::height
                                         } -> std::same_as<Dimension const &>;
                                 {
                                         t.template operator()<int> (d, c)
                                         } -> std::same_as<Visibility>;

                                 // t.template input<int> (d, c, 'a'); // input is not required. Some widget has it, some hasn't
                         };

        template <typename S>
        concept string = std::copyable<S> && requires (S s) {
                                                     {
                                                             s.size ()
                                                             } -> std::convertible_to<std::size_t>;
                                             };

        template <typename S>
        concept text_buffer = string<S> && requires (S s) {
                                                   typename S::value_type;

                                                   {
                                                           s.cbegin ()
                                                           } -> std::input_iterator;

                                                   {
                                                           s.cend ()
                                                           } -> std::input_iterator;
                                           };

} // namespace c

/**
 * A helper.
 */
template <typename... T> struct First {
        using type = std::tuple_element_t<0, std::tuple<T...>>;
};

template <typename... T> using First_t = typename First<T...>::type;

// Helpers, utilities
namespace detail {

        // Converts a given integer x to string
        // digits is the number of digits required in the output.
        // If d is more than the number of digits in x,
        // then 0s are added at the beginning.
        // return value : number of characters used (without '\0')
        template <std::integral Int, std::output_iterator<char> Iter> // TODO concept
        int itoa (Int num, Iter iter, int digits)
        {
                Iter start = iter;
                Int sign{};

                if constexpr (std::is_signed_v<Int>) {
                        if ((sign = num) < 0) { /* record sign */
                                num = -num;     /* make n positive */
                        }
                }

                do { // Does not work when num == 0 and digits == 0
                        *iter++ = (num % 10) + '0';
                } while ((num /= 10) > 0);

                // If number of digits required is more, then
                // add 0s at the beginning
                while (std::distance (start, iter) < digits) {
                        *iter++ = '0';
                }

                if constexpr (std::is_signed_v<Int>) {
                        if (sign < 0) {
                                *iter++ = '-';
                        }
                }

                std::reverse (start, iter);
                *iter++ = '\0';
                return std::distance (start, iter) - 1;
        }

        template <typename String>
        concept out_string = requires (String str) {
                                     {
                                             str.begin ()
                                             } -> std::output_iterator<char>;
                             };

        template <std::integral Int, out_string String> // TODO concept
        int itoa (Int num, String &str, int digits = 1)
        {
                return itoa (num, str.begin (), digits);
        }

        template <std::integral Int> constexpr Int pow (Int value, unsigned int exp)
        {
                if (exp == 0) {
                        return 1;
                }

                Int tmp{value};

                for (unsigned int i = 1; i < exp; ++i) {
                        tmp *= value;
                }

                return tmp;
        }

        // Converts a floating-point/double number to a string.
        // Taken from https://www.geeksforgeeks.org/convert-floating-point-number-string/
        template <std::floating_point Float, out_string String> void ftoa (Float n, String &res, int afterpoint)
        {
                // Extract integer part
                int ipart = n;

                // Extract floating part
                Float fpart = n - Float (ipart);

                // convert integer part to string
                int i = itoa (ipart, res, 0);

                // check for display option after point
                if (afterpoint != 0) {
                        res[i] = '.'; // add dot

                        // Get the value of fraction part upto given no.
                        // of points after dot. The third parameter
                        // is needed to handle cases like 233.007
                        fpart = std::round (fpart * pow (10, afterpoint));

                        itoa (static_cast<int> (fpart), std::next (res.begin (), i + 1), afterpoint);
                }
        }

} // namespace detail

/**
 * Display interface
 */
struct IDisplay {
        IDisplay () = default;
        IDisplay (IDisplay const &) = default;
        IDisplay &operator= (IDisplay const &) = default;
        IDisplay (IDisplay &&) noexcept = default;
        IDisplay &operator= (IDisplay &&) noexcept = default;
        virtual ~IDisplay () = default;

        virtual void print (std::span<const char> const &str) = 0;
        virtual void clear () = 0;
        virtual void textStyle (style::Text clr) = 0;
        virtual void refresh () = 0;

        constexpr virtual Dimension width () const = 0;
        constexpr virtual Dimension height () const = 0;

        virtual Point &cursor () = 0;
        virtual Point const &cursor () const = 0;
};

namespace detail {

        // TODO export this mess to a named concept, and use a placeholder.
        template <typename String>
                requires requires (String str) {
                                 {
                                         // TODO not acurate
                                         str.data ()
                                         } -> std::convertible_to<const char *>;
                         }
        void print (IDisplay &disp, String const &str)
        {
                disp.print (std::span<const char> (str.begin (), str.end ()));
        }

} // namespace detail

/**
 * Abstract
 */
template <typename ConcreteClass, Dimension widthV, Dimension heightV> class AbstractDisplay : public IDisplay {
public:
        static constexpr Dimension width_ = widthV;   // Dimensions in characters
        static constexpr Dimension height_ = heightV; // Dimensions in characters

        ~AbstractDisplay () noexcept override = default;

        constexpr Dimension width () const final { return width_; }
        constexpr Dimension height () const final { return height_; }

        Point &cursor () final { return cursor_; }
        Point const &cursor () const final { return cursor_; }

private:
        Point cursor_{};
};

struct EmptyDisplay : public AbstractDisplay<EmptyDisplay, 0, 0> {

        void print (std::span<const char> const &str) final {}
        void clear () final {}
        void textStyle (style::Text clr) final {}
        void refresh () final {}
};

/****************************************************************************/

namespace detail {

        // Warning it moves the cursor!
        template <typename String = std::string_view> // No concept, no requirements
        void line (auto &disp, uint16_t len, String const &chr = std::string_view ("-"))
        {
                for (uint16_t i = 0; i < len; ++i) {
                        detail::print (disp, chr);
                        disp.cursor ().x () += 1;
                }
        }

} // namespace detail

namespace detail {
        template <std::signed_integral C, std::unsigned_integral D> constexpr bool heightsOverlap (C y1, D height1, C y2, D height2)
        {
                auto y1d = y1 + height1 - 1;
                auto y2d = y2 + height2 - 1;
                return y1 <= C (y2d) && y2 <= C (y1d);
        }

        static_assert (!heightsOverlap (-1, 1U, 0, 2U));
        static_assert (heightsOverlap (0, 1U, 0, 2U));
        static_assert (heightsOverlap (1, 1U, 0, 2U));
        static_assert (!heightsOverlap (2, 1U, 0, 2U));

} // namespace detail

/****************************************************************************/

template <Dimension len, typename LocalStyle> struct Line {
        static constexpr Dimension height = 1;
        static constexpr Dimension width = len;

        template <typename> Visibility operator() (auto &disp, Context const & /* ctx */) const
        {
                detail::line (disp, len, style::get<style::line, LocalStyle, style::getter::HLine> (std::string_view ("-")));
                return Visibility::visible;
        }
};

template <Dimension len, typename LocalStyle = style::Empty> Line<len, LocalStyle> const line;

template <Dimension widthV, Dimension heightV> struct Space {
        static constexpr Dimension height = heightV;
        static constexpr Dimension width = widthV;

        template <typename> Visibility operator() (auto &disp, Context const & /* ctx */) const
        {
                disp.cursor () += {width, std::max (height - 1, 0)};
                return Visibility::visible;
        }
};

template <Dimension widthV, Dimension heightV> Space<widthV, heightV> const space;
template <Dimension widthV> Space<widthV, 1> const hspace;
template <Dimension heightV> Space<1, heightV> const vspace;

/****************************************************************************/
/* Check                                                                    */
/****************************************************************************/

template <typename Callback, typename ChkT, typename String, typename LocalStyle>
        requires c::string<std::remove_reference_t<String>>
class Check {
public:
        static constexpr style::Focus focus = style::get<style::check, LocalStyle, style::getter::Focus> (style::Focus::enabled);
        static constexpr Dimension height = 1;

        constexpr Check (Callback clb, ChkT const &chk, String const &lbl) : label_{lbl}, checked_{chk}, callback{std::move (clb)} {}

        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;
        template <typename Wrapper> void input (auto & /* d */, Context const & /* ctx */, Key key);

        bool &checked () { return checked_; }
        bool const &checked () const { return checked_; }

        String const &label () { return label_; }
        String &label () const { return label_; }

private:
        String label_;
        ChkT checked_{};
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ChkT, typename String, typename LocalStyle>
        requires c::string<std::remove_reference_t<String>>
template <typename Wrapper>
Visibility Check<Callback, ChkT, String, LocalStyle>::operator() (auto &disp, Context const &ctx) const
{
        using namespace std::string_view_literals;

        if constexpr (focus == style::Focus::enabled) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.textStyle (style::Text::highlighted);
                }
        }

        if (checked_) {
                detail::print (disp, style::get<style::check, LocalStyle, style::getter::Checked> ("x"sv));
        }
        else {
                detail::print (disp, style::get<style::check, LocalStyle, style::getter::Unchecked> ("."sv));
        }

        disp.cursor () += {style::get<style::check, LocalStyle, style::getter::Interspace> (1), 0};

        detail::print (disp, label_);

        if constexpr (focus == style::Focus::enabled) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.textStyle (style::Text::regular);
                }
        }

        disp.cursor () += {Coordinate (label_.size ()), 0};
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ChkT, typename String, typename LocalStyle>
        requires c::string<std::remove_reference_t<String>>
template <typename Wrapper>
void Check<Callback, ChkT, String, LocalStyle>::input (auto & /* d */, Context const & /* ctx */, Key key)
{
        static constexpr style::Editable editable = style::get<style::check, LocalStyle, style::getter::Editable> (style::Editable::yes);

        if constexpr (focus == style::Focus::enabled && editable == style::Editable::yes) {
                if (key == Key::select) {
                        bool tmp = !static_cast<bool> (checked_);
                        checked_ = tmp;
                        callback (tmp);
                }
        }
}

/*--------------------------------------------------------------------------*/

namespace c {

        template <typename ValueT>
        concept checkValueContainer = requires (std::unwrap_ref_decay_t<ValueT> t, bool b) {
                                              b = t;
                                              t = b;
                                      };

}

template <typename T> struct EmptyUnaryInvocable {
        void operator() (T /*arg*/) {}
};

// template <std::invocable<bool> Callback, c::string String>
// Check (Callback clb, String const &lbl) -> Check<std::remove_cvref_t<Callback>, bool, std::unwrap_ref_decay_t<String>, void>;

template <typename LocalStyle = style::Empty, std::invocable<bool> Callback, c::string String>
constexpr auto check (Callback &&clb, String &&str)
{
        return Check<std::remove_cvref_t<Callback>, bool, std::unwrap_ref_decay_t<String>, LocalStyle> (std::forward<Callback> (clb), {},
                                                                                                        std::forward<String> (str));
}

template <typename LocalStyle = style::Empty, c::checkValueContainer ChkT, c::string String>
        requires (!std::invocable<ChkT, bool>)
constexpr auto check (ChkT &&chk, String &&str)
{
        return Check<EmptyUnaryInvocable<bool>, std::unwrap_ref_decay_t<ChkT>, std::unwrap_ref_decay_t<String>, LocalStyle> (
                {}, std::forward<ChkT> (chk), std::forward<String> (str));
}

// template <std::invocable<bool> Callback, std::convertible_to<bool> ChkT, c::string String>
// Check (Callback clb, ChkT chk, String const &str)
//         -> Check<std::remove_cvref_t<Callback>, std::remove_reference_t<ChkT>, std::unwrap_ref_decay_t<String>>;

// TODO change ChkT to ValueT - make this naming consistent in every widget
template <typename LocalStyle = style::Empty, std::invocable<bool> Callback, c::checkValueContainer ChkT, c::string String>
constexpr auto check (Callback &&clb, ChkT &&chk, String &&str)
{
        return Check<std::remove_cvref_t<Callback>, std::unwrap_ref_decay_t<ChkT>, std::unwrap_ref_decay_t<String>, LocalStyle> (
                std::forward<Callback> (clb), std::forward<ChkT> (chk), std::forward<String> (str));
}

/****************************************************************************/
/* Radio                                                                    */
/****************************************************************************/

template <typename String, std::regular ValueT, typename LocalStyle, style::Tag styleTag = style::radio>
        requires c::string<std::remove_reference_t<String>>
class Radio {
public:
        static constexpr bool radioVisible = style::get<styleTag, LocalStyle, style::getter::RadioVisible> (true);
        static constexpr style::Focus focus = style::get<styleTag, LocalStyle, style::getter::Focus> (style::Focus::enabled);
        static constexpr style::Editable editable = style::get<styleTag, LocalStyle, style::getter::Editable> (style::Editable::yes);
        static constexpr Dimension height = 1;
        using Value = ValueT;

        constexpr Radio (Value value, String const &lbl) : id_{std::move (value)}, label_{lbl} {}
        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx, ValueT const &value = {}) const;
        template <typename Wrapper, typename Callback> void input (auto &disp, Context const &ctx, Key key, Callback &clb, ValueT &value);

        // Value &id () { return id_; }
        Value const &id () const { return id_; }

        // String &label () { return label_; }
        String const &label () const { return label_; }

private:
        Value id_;
        String label_;
};

/*--------------------------------------------------------------------------*/

template <typename String, std::regular ValueT, typename LocalStyle, style::Tag styleTag>
        requires c::string<std::remove_reference_t<String>>
template <typename Wrapper>
Visibility Radio<String, ValueT, LocalStyle, styleTag>::operator() (auto &disp, Context const &ctx, ValueT const &value) const
{
        using namespace std::string_view_literals;

        if constexpr (focus == style::Focus::enabled) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.textStyle (style::Text::highlighted);
                }
        }

        if constexpr (radioVisible && styleTag != style::item) {
                if (id () == value) {
                        detail::print (disp, style::get<styleTag, LocalStyle, style::getter::Checked> ("o"sv));
                }
                else {
                        detail::print (disp, style::get<styleTag, LocalStyle, style::getter::Unchecked> ("."sv));
                }

                disp.cursor () += {style::get<styleTag, LocalStyle, style::getter::Interspace> (1), 0};
        }

        detail::print (disp, label_);

        if constexpr (focus == style::Focus::enabled) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.textStyle (style::Text::regular);
                }
        }

        disp.cursor () += {Coordinate (label_.size ()), 0};
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename String, std::regular ValueT, typename LocalStyle, style::Tag styleTag>
        requires c::string<std::remove_reference_t<String>>
template <typename Wrapper, typename Callback>
void Radio<String, ValueT, LocalStyle, styleTag>::input (auto & /* disp */, Context const & /* ctx */, Key key, Callback &clb, ValueT &value)
{
        if constexpr (focus == style::Focus::enabled && editable == style::Editable::yes) {
                if (key == Key::select) {
                        value = id ();
                        clb (id ());
                }
        }
}

/*--------------------------------------------------------------------------*/

template <typename LocalStyle = style::Empty, typename String, std::regular Id> constexpr auto radio (Id &&cid, String &&label)
{
        return Radio<std::unwrap_ref_decay_t<String>, std::decay_t<Id>, LocalStyle> (std::forward<Id> (cid), std::forward<String> (label));
}

template <typename LocalStyle = style::Empty, typename String, std::regular Id> constexpr auto item (Id &&cid, String &&label)
{
        return Radio<std::unwrap_ref_decay_t<String>, std::decay_t<Id>, LocalStyle, style::item> (std::forward<Id> (cid),
                                                                                                  std::forward<String> (label));
}

/*--------------------------------------------------------------------------*/

template <typename T> struct is_groupable : public std::bool_constant<false> {};

template <typename String, typename Id, typename LocalStyle, style::Tag styleTag>
struct is_groupable<Radio<String, Id, LocalStyle, styleTag>> : public std::bool_constant<true> {};

// template <typename String, typename Id, typename LocalStyle>
// struct is_groupable<Item<String, Id, LocalStyle>> : public std::bool_constant<true> {};

/****************************************************************************/
/* Text                                                                     */
/****************************************************************************/

/**
 * Multiline text
 * TODO do not allow scrolling so the text inside is not visible. Stop scrolling when the last line is reached.
 */
template <Dimension widthV, Dimension heightV, typename Buffer>
        requires c::text_buffer<std::remove_reference_t<Buffer>>
class Text {
public:
        using BufferType = std::remove_reference_t<Buffer>;
        static constexpr Dimension height = heightV;
        static constexpr Dimension width = widthV;

        /// Tip use std::ref
        constexpr explicit Text (Buffer const &b) : buffer_{b} {}

        template <typename /* Wrapper */> Visibility operator() (auto &d, Context const & /* ctx */) const;

        typename BufferType::const_iterator skipToLine (LineOffset line)
        {
                size_t charactersToSkip = line * widthV;
                return std::next (buffer_.cbegin (), std::min (charactersToSkip, buffer_.size ()));
        }

        LineOffset setStartLine (LineOffset line)
        {
                startLine = std::max (line, 0);

                size_t charNoInLine{};
                LineOffset linesCounted{};
                start = std::find_if (buffer_.cbegin (), buffer_.cend (), [&charNoInLine, &linesCounted, this] (auto c) {
                        if (startLine == linesCounted) {
                                return true;
                        }

                        if (c != '\n') {
                                ++charNoInLine;
                        }

                        if (c == '\n' || charNoInLine >= width) {
                                ++linesCounted;
                                charNoInLine = 0;
                        }

                        return false;
                });

                if (start == buffer_.cend ()) {
                        startLine = linesCounted;
                }

                return startLine;
        }

        Buffer const &buffer () { return buffer_; }
        Buffer &buffer () const { return buffer_; }

private:
        Buffer buffer_;
        typename BufferType::const_iterator start = buffer_.cbegin (); // start points to the start of what's visible
        LineOffset startLine{};
        bool scrollToBottom{};
};

template <Dimension widthV, Dimension heightV, typename Buffer>
        requires c::text_buffer<std::remove_reference_t<Buffer>>
template <typename Wrapper>
Visibility Text<widthV, heightV, Buffer>::operator() (auto &d, Context const &ctx) const
{
        size_t widgetScroll = std::max (ctx.currentScroll - Wrapper::getY (), 0);
        size_t heightToPrint = heightV - widgetScroll;

        Dimension len = buffer_.size ();
        std::array<char, widthV + 1> lineA;
        std::string_view line{lineA.data (), lineA.size ()};
        size_t totalCharactersCopied{};
        size_t linesPrinted{};
        size_t charactersToSkip = widgetScroll * widthV;
        auto iter = std::next (start, std::min (charactersToSkip, buffer_.size ()));

        while (totalCharactersCopied < len && linesPrinted++ < heightToPrint) {
                if (linesPrinted > 1) {
                        d.cursor () += {0, 1}; // New line
                }

                // Depending on the iterator category (input or random), distance can perform an iteration or simple subtraction respectively.
                auto lineCharactersToCopy = std::min (size_t (std::distance (iter, buffer_.cend ())), size_t (widthV));

                // We're looking for \n-s not from the buffer start, but from the visible contents start
                bool newLine{};
                auto end = std::next (iter, lineCharactersToCopy + 1); // 1 past the end
                if (auto lineEndIter = std::find (iter, end, '\n'); lineEndIter != end) {
                        lineCharactersToCopy = std::distance (iter, lineEndIter);
                        newLine = true;
                }

                auto i = std::copy_n (iter, lineCharactersToCopy, lineA.begin ());
                *i = '\0';
                std::advance (iter, lineCharactersToCopy + int (newLine)); // Skip the '\n' if necessary

                totalCharactersCopied += lineCharactersToCopy + int (newLine);
                detail::print (d, line);
        }

        d.cursor ().x () += widthV;
        return Visibility::visible;
}

template <Dimension widthV, Dimension heightV, typename Buffer> auto text (Buffer &&b)
{
        return Text<widthV, heightV, std::unwrap_ref_decay_t<Buffer>>{std::forward<Buffer> (b)};
}

/****************************************************************************/
/* Label                                                                    */
/****************************************************************************/

/**
 *
 */
template <typename String>
        requires c::string<std::remove_reference_t<String>>
class Label {
public:
        static constexpr Dimension height = 1;

        constexpr explicit Label (String const &l) : label_{l} {}

        template <typename /* Wrapper */> Visibility operator() (auto &disp, Context const & /* ctx */) const
        {
                detail::print (disp, label_);
                disp.cursor () += {Coordinate (label_.size ()), 0};
                return Visibility::visible;
        }

        String const &label () { return label_; }
        String &label () const { return label_; }

private:
        String label_;
};

template <typename String> auto label (String &&str) { return Label<std::unwrap_ref_decay_t<String>> (std::forward<String> (str)); }

/****************************************************************************/
/* Button                                                                   */
/****************************************************************************/

/**
 *
 */
template <typename String, typename Callback>
        requires c::string<std::remove_reference_t<String>>
class Button {
public:
        static constexpr style::Focus focus = style::Focus::enabled;
        static constexpr Dimension height = 1;

        constexpr explicit Button (String const &lbl, Callback clb) : label_{lbl}, callback{std::move (clb)} {}

        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;

        template <typename> void input (auto & /* d */, Context const & /* ctx */, Key key)
        {
                if (key == Key::select) {
                        callback ();
                }
        }

        String const &label () { return label_; }
        String &label () const { return label_; }

private:
        String label_;
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename String, typename Callback>
        requires c::string<std::remove_reference_t<String>>
template <typename Wrapper>
Visibility Button<String, Callback>::operator() (auto &disp, Context const &ctx) const
{
        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                disp.textStyle (style::Text::highlighted);
        }

        detail::print (disp, label_);
        disp.cursor () += {Coordinate (label_.size ()), 0};

        if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                disp.textStyle (style::Text::regular);
        }

        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename String, typename Callback>
auto button (Callback &&clb, String &&str)
        requires std::invocable<Callback> && c::string<std::remove_reference_t<String>>
{
        return Button<std::unwrap_ref_decay_t<String>, std::decay_t<Callback>> (std::forward<String> (str), std::forward<Callback> (clb));
}

/****************************************************************************/
/* Combo                                                                    */
/****************************************************************************/

/**
 * Single combo option.
 */
template <std::regular ValueT, c::string String> struct Option {
public:
        using Value = ValueT;

        Option (Value val, String const &lbl) : value_{std::move (val)}, label_{lbl} {}

        Value &value () { return value_; }
        Value const &value () const { return value_; }

        String &label () { return label_; }
        String const &label () const { return label_; }

private:
        Value value_;
        String label_;
};

template <std::regular Value, c::string String> auto option (Value &&val, String &&label)
{
        return Option<std::remove_reference_t<Value>, std::unwrap_ref_decay_t<String>> (std::forward<Value> (val), std::forward<String> (label));
}

/**
 * A container for options.
 */
template <std::regular ValueT, size_t Num, c::string String> struct Options {
public:
        using OptionType = Option<ValueT, String>;
        using Value = ValueT;
        using ContainerType = std::array<OptionType, Num>;
        using SelectionIndex = typename ContainerType::size_type; // std::array::at accepts this

        template <typename... J> constexpr Options (Option<J, String> &&...elms) : elms{std::forward<Option<J, String>> (elms)...} {}

        SelectionIndex toIndex (Value const &cid) const;

        auto &getOptionByValue (Value const &cid) { return getOptionByIndex (toIndex (cid)); }
        auto const &getOptionByValue (Value const &cid) const { return getOptionByIndex (toIndex (cid)); }

        auto &getOptionByIndex (SelectionIndex idx) { return elms.at (idx); }
        auto const &getOptionByIndex (SelectionIndex idx) const { return elms.at (idx); }

        size_t size () const { return elms.size (); }

private:
        ContainerType elms;
};

template <typename String, typename... J> Options (Option<J, String> &&...elms) -> Options<First_t<J...>, sizeof...(J), String>;

/*--------------------------------------------------------------------------*/

template <std::regular I, size_t Num, c::string String>
typename Options<I, Num, String>::SelectionIndex Options<I, Num, String>::toIndex (Value const &cid) const
{
        size_t ret{};

        // This is brute-force. Can be improved.
        for (auto const &opt : elms) {
                if (opt.value () == cid) {
                        return ret;
                }

                ++ret;
        }

        return 0;
}

/**
 *
 */
template <typename Callback, typename ValueContainerT, typename LocalStyle, typename OptionCollection>
        requires std::invocable<Callback, typename OptionCollection::Value>
class Combo {
public:
        static constexpr style::Focus focus = style::get<style::combo, LocalStyle, style::getter::Focus> (style::Focus::enabled);
        static constexpr style::Editable editable = style::get<style::combo, LocalStyle, style::getter::Editable> (style::Editable::yes);
        static constexpr Dimension height = 1;
        using Option = typename OptionCollection::OptionType;
        using Value = typename OptionCollection::Value;
        using ValueContainer = ValueContainerT;
        using SelectionIndex = typename OptionCollection::SelectionIndex;

        constexpr Combo (Callback clb, ValueContainer cid, OptionCollection const &opts)
            : options{opts}, valueContainer (std::move (cid)), callback{std::move (clb)}
        {
        }

        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;
        template <typename Wrapper> void input (auto &disp, Context const &ctx, Key key);

        ValueContainer &value () { return valueContainer; }
        ValueContainer const &value () const { return valueContainer; }

        SelectionIndex index () const { return index_; };

private:
        Value &toValue () { return static_cast<Value &> (valueContainer); }
        Value const &toValue () const { return static_cast<Value const &> (valueContainer); }

        OptionCollection options;
        ValueContainer valueContainer; // Can be int, can be std::ref (int)
        mutable SelectionIndex index_{};
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ValueContainer, typename LocalStyle, typename OptionCollection>
        requires std::invocable<Callback, typename OptionCollection::Value>
template <typename Wrapper>
Visibility Combo<Callback, ValueContainer, LocalStyle, OptionCollection>::operator() (auto &disp, Context const &ctx) const
{
        if constexpr (focus == style::Focus::enabled) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.textStyle (style::Text::highlighted);
                }
        }

        index_ = options.toIndex (value ());
        auto &label = options.getOptionByIndex (index_).label ();
        detail::print (disp, label);

        if constexpr (focus == style::Focus::enabled) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.textStyle (style::Text::regular);
                }
        }

        disp.cursor () += {Coordinate (label.size ()), 0};
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ValueContainer, typename LocalStyle, typename OptionCollection>
        requires std::invocable<Callback, typename OptionCollection::Value>
template <typename Wrapper>
void Combo<Callback, ValueContainer, LocalStyle, OptionCollection>::input (auto & /* disp */, Context const &ctx, Key key)
{
        if constexpr (focus == style::Focus::enabled && editable == style::Editable::yes) {
                if (ctx.currentFocus == Wrapper::getFocusIndex () && key == Key::select) {
                        ++index_;
                        index_ %= options.size ();
                        toValue () = options.getOptionByIndex (index_).value ();
                        callback (toValue ());
                }
        }
}

/*--------------------------------------------------------------------------*/

template <typename CallbackT, typename... Opts>
        requires std::invocable<CallbackT, typename First_t<Opts...>::Value>
auto combo (CallbackT &&clb, Opts &&...opts)
{
        using Value = typename First_t<Opts...>::Value;
        using Callback = std::remove_reference_t<CallbackT>;
        using OptionCollection = decltype (Options (std::forward<Opts> (opts)...));
        return Combo<Callback, Value, void, OptionCollection> (std::forward<Callback> (clb), Value{}, Options (std::forward<Opts> (opts)...));
}

template <typename CallbackT, typename ValueContainerT, typename... Opts>
        requires std::invocable<CallbackT, typename First_t<Opts...>::Value> &&   //
        std::convertible_to<ValueContainerT, typename First_t<Opts...>::Value> && //
        (!std::invocable<ValueContainerT, typename First_t<Opts...>::Value>)      //

auto combo (CallbackT &&clb, ValueContainerT &&value, Opts &&...opts)
{
        using ValueContainer = std::remove_reference_t<ValueContainerT>;
        using Callback = std::remove_reference_t<CallbackT>;
        using OptionCollection = decltype (Options (std::forward<Opts> (opts)...));

        return Combo<Callback, ValueContainer, void, OptionCollection> (std::forward<CallbackT> (clb), std::forward<ValueContainerT> (value),
                                                                        Options (std::forward<Opts> (opts)...));
}

template <typename LocalStyle = style::Empty, typename ValueContainerT, typename... Opts>
        requires std::convertible_to<ValueContainerT, typename First_t<Opts...>::Value>
        && (!std::invocable<ValueContainerT, typename First_t<Opts...>::Value>)
auto combo (ValueContainerT &&cid, Opts &&...opts)
{
        using Value = typename First_t<Opts...>::Value;
        using Callback = EmptyUnaryInvocable<Value>;
        using ValueContainer = std::remove_reference_t<ValueContainerT>;
        using OptionCollection = decltype (Options (std::forward<Opts> (opts)...));

        return Combo<Callback, ValueContainer, LocalStyle, OptionCollection> ({}, std::forward<ValueContainerT> (cid),
                                                                              Options (std::forward<Opts> (opts)...));
}

/****************************************************************************/
/* Number                                                                   */
/****************************************************************************/

namespace detail {
        template <typename T> struct IntStrLen {};

        template <std::unsigned_integral T> struct IntStrLen<T> {
                /*
                 * +1 because digits10 holds value of digits multiplied by log10⁡radix\small \log_{10}{radix} and rounded down (2 for 8 bit
                 * types)
                 * +1 for storing '\0'
                 */
                static constexpr auto value = std::numeric_limits<T>::digits10 + 2;
        };

        template <std::signed_integral T> struct IntStrLen<T> {
                /*
                 * +1 because digits10 holds value of digits multiplied by log10⁡radix\small \log_{10}{radix} and rounded down (2 for 8 bit
                 * types)
                 * +1 for posisble '-' character.
                 * +1 for storing '\0'
                 */
                static constexpr auto value = std::numeric_limits<T>::digits10 + 3;
        };
} // namespace detail

namespace c {
        // Callback requirements
        template <typename Callback, auto min>
        concept numberCallback = std::invocable<Callback, decltype (min)>;

        // Simply check for operations used in the Number body
        template <typename ValueT, auto min>
        concept numberValueContainer = requires (ValueT t) {
                                               static_cast<decltype (min)> (t);
                                               t = min;
                                       };

        // All of the same integral type
        template <auto min, auto max, auto inc>
        concept numberMinMaxInc = std::integral<decltype (min)> && std::integral<decltype (max)> && std::integral<decltype (inc)>
                && std::same_as<decltype (min), decltype (max)> && std::same_as<decltype (max), decltype (inc)>;

        // Helper to clean the code out;
        template <typename Callback, typename ValueT, auto min, auto max, auto inc>
        concept number = numberValueContainer<ValueT, min> && numberMinMaxInc<min, max, inc> && numberCallback<Callback, min>;
} // namespace c

/**
 * Displays and edits an integer number.
 */
template <typename Callback, typename ValueT, auto min, auto max, auto inc, typename LocalStyle>
        requires c::number<Callback, ValueT, min, max, inc>
class Number {
public:
        static constexpr style::Focus focus = style::get<style::number, LocalStyle, style::getter::Focus> (style::Focus::enabled);
        static constexpr Dimension height = 1; // Width is undetermined. Wrap in a hbox to constrain it.
        using Value = decltype (min);          // std::integral or a reference like int or int &

        constexpr explicit Number (Callback clb, ValueT const &cid) : valueContainer (cid), callback{std::move (clb)} {}

        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;
        template <typename Wrapper> void input (auto &disp, Context const &ctx, Key key);

        // Value &value () { return valueContainer; }
        // Value const &value () const { return valueContainer; }

private:
        ValueT valueContainer; // Can be int, can be int &. Or any other std::integral and its reference
        using Buffer = std::array<char, detail::IntStrLen<Value>::value>;
        mutable Buffer buffer{}; // Big enough to store all the digits + '\0'
        Callback callback;
};

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ValueT, auto min, auto max, auto inc, typename LocalStyle>
        requires c::number<Callback, ValueT, min, max, inc>
template <typename Wrapper>
Visibility Number<Callback, ValueT, min, max, inc, LocalStyle>::operator() (auto &disp, Context const &ctx) const
{

        if constexpr (focus == style::Focus::enabled) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.textStyle (style::Text::highlighted);
                }
        }

        typename Buffer::size_type digits = detail::itoa (static_cast<Value> (valueContainer), buffer);
        detail::print (disp, std::string_view{buffer.cbegin (), digits});

        if constexpr (focus == style::Focus::enabled) {
                if (ctx.currentFocus == Wrapper::getFocusIndex ()) {
                        disp.textStyle (style::Text::regular);
                }
        }

        disp.cursor () += {Coordinate (digits), 0};
        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

template <typename Callback, typename ValueT, auto min, auto max, auto inc, typename LocalStyle>
        requires c::number<Callback, ValueT, min, max, inc>
template <typename Wrapper>
void Number<Callback, ValueT, min, max, inc, LocalStyle>::input (auto & /* disp */, Context const &ctx, Key key)
{
        constexpr auto editable = style::get<style::number, LocalStyle, style::getter::Editable> (style::Editable::yes);

        if constexpr (focus == style::Focus::enabled && editable == style::Editable::yes) {
                if (ctx.currentFocus == Wrapper::getFocusIndex () && key == Key::select) {
                        auto tmp = static_cast<Value> (valueContainer);
                        tmp += inc;

                        if (tmp > max) {
                                tmp = min;
                        }

                        valueContainer = tmp;
                        callback (tmp);
                }
        }
}

/*--------------------------------------------------------------------------*/

constexpr int DEFAULT_NUMBER_MAX = 9;

template <auto min = 0, auto max = DEFAULT_NUMBER_MAX, auto inc = 1, typename LocalStyle = style::Empty, typename ValueT>
        requires c::numberMinMaxInc<min, max, inc> && c::numberValueContainer<std::unwrap_ref_decay_t<ValueT>, min>
auto number (ValueT &&val)
{
        // using Value = std::remove_reference_t<std::unwrap_ref_decay_t<ValueT>>;
        using Callback = EmptyUnaryInvocable<decltype (max)>;
        return Number<Callback, std::unwrap_ref_decay_t<ValueT>, min, max, inc, LocalStyle>{{}, std::forward<ValueT> (val)};
}

// /*--------------------------------------------------------------------------*/

template <auto min = 0, auto max = DEFAULT_NUMBER_MAX, auto inc = 1, typename ValueT, typename Callback, typename LocalStyle = style::Empty>
        requires c::numberMinMaxInc<min, max, inc> && c::numberValueContainer<std::unwrap_ref_decay_t<ValueT>, min>
        && c::numberCallback<Callback, min>
auto number (Callback &&clb)
{
        return Number<std::remove_reference_t<Callback>, std::unwrap_ref_decay_t<ValueT>, min, max, inc, LocalStyle>{
                std::forward<Callback> (clb), min};
}

/*--------------------------------------------------------------------------*/

template <auto min = 0, auto max = DEFAULT_NUMBER_MAX, auto inc = 1, typename LocalStyle = style::Empty, typename ValueT, typename Callback>
        requires c::numberMinMaxInc<min, max, inc> && c::numberValueContainer<std::unwrap_ref_decay_t<ValueT>, min>
        && c::numberCallback<Callback, min>
auto number (Callback &&clb, ValueT &&val)
{
        return Number<std::remove_reference_t<Callback>, std::unwrap_ref_decay_t<ValueT>, min, max, inc, LocalStyle>{
                std::forward<Callback> (clb), std::forward<ValueT> (val)};
}

/****************************************************************************/
/* Layouts                                                                  */
/****************************************************************************/

namespace detail {

        template <typename T> constexpr uint16_t getFocusableWidgetCount ()
        {
                // This introspection is for widget API simplification.
                if constexpr (requires {
                                      {
                                              T::focusableWidgetCount
                                              } -> std::convertible_to<Focus>;
                              }) {
                        // focusableWidgetCount field is used in container widgets like Layout and Group.
                        return T::focusableWidgetCount;
                }
                else if constexpr (requires {
                                           {
                                                   T::focus
                                                   } -> std::convertible_to<style::Focus>;
                                   }) {
                        // This field is optional and is used in regular widgets. Its absence is treated as it were declared false.
                        return uint16_t (T::focus);
                }
                else {
                        return 0;
                }
        }

        template <typename T> constexpr Dimension getHeight ()
        {
                // Again this introspection is for sake of simplifying the API.
                // User defined widgets ought to define the Dimension height field,
                // while augment:: wrappers implement getHeight methods.
                if constexpr (requires { T::getHeight (); }) {
                        return T::getHeight ();
                }
                else {
                        return T::height;
                }
        };

        template <typename T> constexpr Dimension getWidth ()
        {
                if constexpr (requires { T::getWidth (); }) {
                        return T::getWidth ();
                }
                else if constexpr (requires { T::width; }) {
                        return T::width;
                }
                else {
                        return 0; // default
                }
        };

        template <typename T> struct FocusableWidgetCountField {
                static constexpr auto value = getFocusableWidgetCount<std::remove_reference_t<std::unwrap_ref_decay_t<T>>> ();
        };

        template <typename T> struct WidgetHeightField {
                static constexpr auto value = getHeight<std::remove_reference_t<std::unwrap_ref_decay_t<T>>> ();
        };

        template <typename T> struct WidgetWidthField {
                static constexpr auto value = getWidth<std::remove_reference_t<std::unwrap_ref_decay_t<T>>> ();
        };

        /*--------------------------------------------------------------------------*/

        template <typename Tuple, template <typename T> typename Field, int elemIdx = static_cast<int> (std::tuple_size_v<Tuple> - 1)>
        struct Sum {
                static constexpr auto value = Field<std::tuple_element_t<elemIdx, Tuple>>::value + Sum<Tuple, Field, elemIdx - 1>::value;
        };

        template <typename Tuple, template <typename T> typename Field> struct Sum<Tuple, Field, 0> {
                static constexpr auto value = Field<std::tuple_element_t<0, Tuple>>::value;
        };

        // Partial specialization for empty children list
        template <typename Tuple, template <typename T> typename Field> struct Sum<Tuple, Field, -1> {
                static constexpr int value = 0;
        };

        template <typename Tuple, template <typename T> typename Field, int elemIdx = static_cast<int> (std::tuple_size_v<Tuple> - 1)>
        struct Max {
                static constexpr auto value
                        = std::max (Field<std::tuple_element_t<elemIdx, Tuple>>::value, Max<Tuple, Field, elemIdx - 1>::value);
        };

        template <typename Tuple, template <typename T> typename Field> struct Max<Tuple, Field, 0> {
                static constexpr auto value = Field<std::tuple_element_t<0, Tuple>>::value;
        };

        // Partial specialization for empty children list
        template <typename Tuple, template <typename T> typename Field> struct Max<Tuple, Field, -1> {
                static constexpr int value = 0;
        };

        /*--------------------------------------------------------------------------*/
        /* Decorators for vertical and horizontal layouts.                          */
        /*--------------------------------------------------------------------------*/

        template <typename WidgetsTuple> struct VBoxDecoration {
                static constexpr Dimension width = detail::Max<WidgetsTuple, detail::WidgetWidthField>::value;
                static constexpr Dimension height = detail::Sum<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (auto &display, Context const &ctx, Point const &layoutOrigin)
                {
                        display.cursor ().y () += 1;
                        display.cursor ().x () = ctx.origin.x () + layoutOrigin.x ();
                }
                static constexpr Dimension getWidthIncrement (Dimension /* d */) { return 0; }
                static constexpr Dimension getHeightIncrement (Dimension disp) { return disp; }
        };

        template <typename WidgetsTuple> struct HBoxDecoration {
                static constexpr Dimension width = detail::Sum<WidgetsTuple, detail::WidgetWidthField>::value;
                static constexpr Dimension height = detail::Max<WidgetsTuple, detail::WidgetHeightField>::value;

                static void after (auto &display, Context const &ctx, Point const &layoutOrigin)
                {
                        display.cursor ().y () = std::max (ctx.origin.y () + layoutOrigin.y () - ctx.currentScroll, 0);
                }
                static constexpr Dimension getWidthIncrement (Dimension disp) { return disp; }
                static constexpr Dimension getHeightIncrement (Dimension /* d */) { return 0; }
        };

        struct NoDecoration {
                static constexpr Dimension height = 0;
                static constexpr void after (auto &display, Context const &ctx, Point const &layoutOrigin) {}
                static constexpr Dimension getWidthIncrement (Dimension /* d */) { return 0; }
                static constexpr Dimension getHeightIncrement (Dimension /* d */) { return 0; }
        };

} // namespace detail

/****************************************************************************/

/**
 * Radio group.
 */
template <typename Callback, typename ValueContainerT, typename WidgetTuple> class Group {
public:
        using ValueContainer = ValueContainerT;

        Group (Callback const &clb, ValueContainer value, WidgetTuple radios)
            : widgets_{std::move (radios)}, value_ (std::move (value)), callback_{clb}
        {
        }

        static constexpr Focus focusableWidgetCount = detail::Sum<WidgetTuple, detail::FocusableWidgetCountField>::value;

        using Children = WidgetTuple;

        Children &widgets () { return widgets_; }
        Children const &widgets () const { return widgets_; }

        ValueContainer &value () { return value_; }
        ValueContainer const &value () const { return value_; }

        Callback &callback () { return callback_; }
        Callback const &callback () const { return callback_; }

private:
        WidgetTuple widgets_;
        ValueContainer value_; // T, ref_wrap <T> etc
        Callback callback_;
};

/*--------------------------------------------------------------------------*/

template <typename T> struct is_group : public std::bool_constant<false> {};

template <typename Callback, typename ValueContainerT, typename WidgetTuple>
struct is_group<Group<Callback, ValueContainerT, WidgetTuple>> : public std::bool_constant<true> {};

namespace c {
        template <typename T>
        concept group = is_group<T>::value;
} // namespace c

/*--------------------------------------------------------------------------*/

template <typename CallbackT, typename... W>
        requires std::invocable<CallbackT, typename First_t<W...>::Value>
        && std::conjunction_v<is_groupable<W>...> // TODO requires that all the radio
                                                  // values have the same type
auto group (CallbackT &&clb, W &&...widgets)
{
        using Callback = std::remove_reference_t<CallbackT>;
        using RadioCollection = decltype (std::tuple{std::forward<W> (widgets)...});
        using ValueContainer = typename First_t<W...>::Value;

        return Group<Callback, ValueContainer, RadioCollection>{std::forward<CallbackT> (clb), {}, std::tuple{std::forward<W> (widgets)...}};
}

template <typename ValueContainerT, typename... W>
        requires (!std::invocable<ValueContainerT, typename First_t<W...>::Value>) && //
        std::convertible_to<ValueContainerT, typename First_t<W...>::Value> &&        //
        std::conjunction_v<is_groupable<W>...>                                        //

auto group (ValueContainerT &&val, W &&...widgets)
{
        using ValueContainer = std::remove_reference_t<ValueContainerT>;
        using RadioCollection = decltype (std::tuple{std::forward<W> (widgets)...});
        using Value = typename First_t<W...>::Value;

        return Group<EmptyUnaryInvocable<Value>, ValueContainer, RadioCollection>{
                {}, std::forward<ValueContainerT> (val), std::tuple{std::forward<W> (widgets)...}};
}

template <typename CallbackT, typename ValueContainerT, typename... W>
        requires std::invocable<CallbackT, typename First_t<W...>::Value> &&   //
        (!std::invocable<ValueContainerT, typename First_t<W...>::Value>) &&   //
        std::convertible_to<ValueContainerT, typename First_t<W...>::Value> && //
        std::conjunction_v<is_groupable<W>...>                                 //

auto group (CallbackT &&clb, ValueContainerT &&val, W &&...widgets)
{
        using Callback = std::remove_reference_t<CallbackT>;
        using Value = std::remove_reference_t<ValueContainerT>;
        using RadioCollection = decltype (std::tuple{std::forward<W> (widgets)...});

        return Group<Callback, Value, RadioCollection>{std::forward<CallbackT> (clb), std::forward<ValueContainerT> (val),
                                                       std::tuple{std::forward<W> (widgets)...}};
}

/****************************************************************************/

namespace detail {
        template <bool frame> struct FrameHelper {
                static constexpr Coordinate offset = 0;
                static constexpr Dimension cut = 0;
        };

        template <> struct FrameHelper<true> {
                static constexpr Coordinate offset = 1;
                static constexpr Dimension cut = 2;
        };
} // namespace detail

/**
 * A window
 */
template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, typename LocalStyle, typename ChildT> class Window {
public:
        using Child = std::remove_reference_t<std::unwrap_ref_decay_t<ChildT>>;
        explicit Window (ChildT wgt) : child_{std::move (wgt)} {} // ChildT can be a widget (like Label) or reference_wrapper <Label>

        template <typename Wrapper> Visibility operator() (auto &disp, Context & /* ctx */) const;

        ChildT &child () { return child_; }
        ChildT const &child () const { return child_; }

        static constexpr Coordinate x = ox;
        static constexpr Coordinate y = oy;
        static constexpr Dimension width = widthV;   // Dimensions in characters
        static constexpr Dimension height = heightV; // Dimensions in characters
        static constexpr Focus focusableWidgetCount = Child::focusableWidgetCount;

        static constexpr bool frame = style::get<style::window, LocalStyle, style::getter::Frame> (false);
        using FrameHelper = detail::FrameHelper<frame>;

private:
        ChildT child_;
};

/*--------------------------------------------------------------------------*/

template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, typename LocalStyle, typename ChildT>
template <typename Wrapper>
Visibility Window<ox, oy, widthV, heightV, LocalStyle, ChildT>::operator() (auto &disp, Context & /* ctx */) const
{
        using namespace std::string_view_literals;
        disp.cursor () = {ox, oy};

        if constexpr (frame) {
                constexpr auto vline = style::get<style::window, LocalStyle, style::getter::VLine> (":"sv);
                constexpr auto hline = style::get<style::window, LocalStyle, style::getter::HLine> ("-"sv);

                detail::print (disp, style::get<style::window, LocalStyle, style::getter::NWCorner> ("+"sv));
                disp.cursor () += {1, 0};
                detail::line (disp, width - FrameHelper::cut, hline);
                detail::print (disp, style::get<style::window, LocalStyle, style::getter::NECorner> ("+"sv));

                for (int i = 0; i < height - FrameHelper::cut; ++i) {
                        disp.cursor ().x () = ox;
                        disp.cursor () += {0, 1};
                        detail::print (disp, vline);
                        disp.cursor () += {1, 0};
                        detail::line (disp, width - FrameHelper::cut, " "sv);
                        detail::print (disp, vline);
                }

                disp.cursor ().x () = ox;
                disp.cursor () += {0, 1};
                detail::print (disp, style::get<style::window, LocalStyle, style::getter::SWCorner> ("+"sv));
                disp.cursor () += {1, 0};
                detail::line (disp, width - FrameHelper::cut, hline);
                detail::print (disp, style::get<style::window, LocalStyle, style::getter::SECorner> ("+"sv));

                disp.cursor () = {ox + FrameHelper::offset, oy + FrameHelper::offset};
        }

        return Visibility::visible;
}

template <typename T> struct is_window : public std::bool_constant<false> {};

template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, typename LocalStyle, typename ChildT>
struct is_window<Window<ox, oy, widthV, heightV, LocalStyle, ChildT>> : public std::bool_constant<true> {};

namespace c {
        template <typename T>
        concept window = is_window<T>::value;
}

namespace detail {
        template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, typename LocalStyle> auto windowRaw (auto &&wgt)
        {
                return Window<ox, oy, widthV, heightV, LocalStyle, std::decay_t<decltype (wgt)>> (std::forward<decltype (wgt)> (wgt));
        }
} // namespace detail

/****************************************************************************/

// Forward declaration for is_layout
template <template <typename Wtu> typename Decor, typename WidgetsTuple, Dimension widthV = 0> class Layout;

template <typename Wtu> class DefaultDecor {};

/// Type trait for discovering Layouts
template <typename T, template <typename Wtu> typename Decor = DefaultDecor, typename WidgetsTuple = Empty, Dimension widthV = 0>
struct is_layout : public std::bool_constant<false> {};

template <template <typename Wtu> typename Decor, typename WidgetsTuple, Dimension widthV>
struct is_layout<Layout<Decor, WidgetsTuple, widthV>> : public std::bool_constant<true> {};

namespace c {
        // template <typename T>
        // concept layout = is_layout<T>::value;

        template <typename L>
        concept layout = requires {
                                 // TODO is widget etc, the rest of stuff required.
                                 typename L::DecoratorType;
                         };
} // namespace c

namespace detail {

        namespace augment {

                /**
                 * Additional information for all the widget contained in the Layout.
                 */
                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate xV, Coordinate yV>
                        requires c::widget<std::remove_reference_t<T>> // T can be for example Label or Label &. Only these 2 options.
                class Widget {
                public:
                        using Wrapped = std::remove_reference_t<T>;

                        constexpr Widget (T const &wgt) : widget{wgt} {}

                        static constexpr Dimension getWidth ();                             /// Width in characters.
                        static constexpr Dimension getHeight () { return Wrapped::height; } /// Height in characters.
                        static constexpr Coordinate getX () { return xV; }                  /// Position relative to the top left corner.
                        static constexpr Coordinate getY () { return yV; }                  /// Position relative to the top left corner.

                        /// Consecutive number (starting from 0) assigned for every focusable widget.
                        static constexpr Focus getFocusIndex () { return focusIndexV; }

                        Visibility operator() (auto &disp, Context const *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return widget.template operator()<Widget> (disp, *ctx);
                        }

                        template <typename ValueT> Visibility operator() (auto &disp, Context const *ctx, ValueT const &value) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return widget.template operator()<Widget> (disp, *ctx, value);
                        }

                        void input (auto &disp, Context const &ctx, Key key)
                        {
                                if (ctx.currentFocus == getFocusIndex ()) {
                                        if constexpr (requires { widget.template input<Widget> (disp, ctx, key); }) {
                                                widget.template input<Widget> (disp, ctx, key);
                                        }
                                }
                        }

                        template <typename Callback, typename ValueT>
                        void input (auto &disp, Context const &ctx, Key key, Callback &clb, ValueT &value)
                        {
                                if (ctx.currentFocus == getFocusIndex ()) {
                                        if constexpr (requires { widget.template input<Widget> (disp, ctx, key, clb, value); }) {
                                                widget.template input<Widget> (disp, ctx, key, clb, value);
                                        }
                                }
                        }

                        void scrollToFocus (Context *ctx) const;

                private:
                        template <typename X> friend void log (X const &, int);
                        T widget; // Wrapped widget. 2 options X or X&
                };

                template <typename T> struct is_widget_wrapper : public std::bool_constant<false> {};

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate xV, Coordinate yV>
                class is_widget_wrapper<Widget<T, focusIndexV, radioIndexV, xV, yV>> : public std::bool_constant<true> {};

                template <typename T>
                concept widget_wrapper = is_widget_wrapper<T>::value;

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate xV, Coordinate yV>
                        requires c::widget<std::remove_reference_t<T>>
                constexpr Dimension Widget<T, focusIndexV, radioIndexV, xV, yV>::getWidth ()
                {
                        if constexpr (requires {
                                              {
                                                      Wrapped::width
                                                      } -> std::convertible_to<Dimension>;
                                      }) {
                                return Wrapped::width;
                        }
                        else {
                                return 0;
                        }
                }

                template <typename T, Focus focusIndexV, Selection radioIndexV, Coordinate xV, Coordinate yV>
                        requires c::widget<std::remove_reference_t<T>>
                void Widget<T, focusIndexV, radioIndexV, xV, yV>::scrollToFocus (Context *ctx) const
                {
                        if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                if (ctx->currentFocus == getFocusIndex ()) {
                                        if (getY () < ctx->currentScroll) {
                                                ctx->currentScroll = getY ();
                                        }
                                        else {
                                                ctx->currentScroll = getY () - ctx->dimensions.height + 1;
                                        }
                                }
                        }
                }

                /**
                 * Base for widgets in the augment:: namespace that can contain other widgets.
                 */
                template <typename ConcreteClass, typename Decor> struct ContainerWidget {
                protected:
                        Visibility operator() (auto &disp, Context *ctx) const // TODO move method body out
                        {
                                auto tmpCursor = disp.cursor ();

                                // Some container widgets have their own display method (operator ()). For instance og::Window
                                if constexpr (requires {
                                                      static_cast<ConcreteClass const *> (this)->widget.template operator()<ConcreteClass> (
                                                              disp, *ctx);
                                              }) {
                                        static_cast<ConcreteClass const *> (this)->widget.template operator()<ConcreteClass> (disp, *ctx);
                                }

                                // This iterates over the children
                                auto lbd = [&disp, &ctx, concrete = static_cast<ConcreteClass const *> (this)] (auto &itself, auto const &child,
                                                                                                                auto const &...children) {
                                        Visibility status{};

                                        if constexpr (requires { concrete->widget.value (); }) {
                                                status = child (disp, ctx, concrete->widget.value ());
                                        }
                                        else {
                                                status = child (disp, ctx);
                                        }

                                        if (status == Visibility::visible) {
                                                constexpr bool lastWidgetInLayout = (sizeof...(children) == 0);

                                                if (!lastWidgetInLayout) {
                                                        Decor::after (disp, *ctx, Point (ConcreteClass::getX (), ConcreteClass::getY ()));
                                                }
                                        }

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                auto &concreteChildren = static_cast<ConcreteClass const *> (this)->children;

                                std::apply (
                                        [&lbd] (auto const &...children) {
                                                // In case of empty children list for example when vbox() was called (no args)
                                                if constexpr (sizeof...(children) > 0) {
                                                        lbd (lbd, children...);
                                                }
                                        },
                                        concreteChildren);

                                /**
                                 * Move the cursor to the bottom right corner, buut only if width of the container widget
                                 * is defined. This is kind of a hack which works when somebody either defines the layout
                                 * width explicitly using template argument (example : vbox <4>(xxx)), or when Layout's
                                 * contents have the width defined which is usually not the case. Most widgets display dynamic
                                 * contents like std::string and thus cannot figure out the width at compile time.
                                 */
                                if (ConcreteClass::getWidth ()) {
                                        disp.cursor () = tmpCursor;
                                        disp.cursor () += {ConcreteClass::getWidth (), std::max (ConcreteClass::getHeight () - 1, 0)};
                                }

                                return Visibility::visible;
                        }

                        void input (auto &disp, Context &ctx, Key key)
                        {
                                auto lbd = [&disp, &ctx, key, concrete = static_cast<ConcreteClass *> (this)] (auto &itself, auto &child,
                                                                                                               auto &...children) {
                                        if constexpr (requires {
                                                              concrete->widget.callback ();
                                                              concrete->widget.value ();
                                                      }) {

                                                child.input (disp, ctx, key, concrete->widget.callback (), concrete->widget.value ());
                                        }
                                        else {
                                                child.input (disp, ctx, key);
                                        }

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                std::apply (
                                        [&lbd] (auto &...children) {
                                                if constexpr (sizeof...(children) > 0) {
                                                        lbd (lbd, children...);
                                                }
                                        },
                                        static_cast<ConcreteClass *> (this)->children);
                        }

                        void scrollToFocus (Context *ctx) const
                        {
                                // ctx = changeContext (ctx);

                                auto lbd = [&ctx] (auto &itself, auto const &child, auto const &...children) {
                                        child.scrollToFocus (ctx);

                                        if constexpr (sizeof...(children) > 0) {
                                                itself (itself, children...);
                                        }
                                };

                                std::apply (
                                        [&lbd] (auto const &...children) {
                                                if constexpr (sizeof...(children) > 0) {
                                                        lbd (lbd, children...);
                                                }
                                        },
                                        static_cast<ConcreteClass const *> (this)->children);
                        }
                };

                /**
                 * Layout
                 */
                template <typename T, typename WidgetTuple, Coordinate xV, Coordinate yV>
                        requires c::layout<std::remove_reference_t<T>>
                class Layout : public ContainerWidget<Layout<T, WidgetTuple, xV, yV>, typename std::remove_reference_t<T>::DecoratorType> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr Layout (T const &t, WidgetTuple c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getWidth ()
                        {
                                return std::max (Wrapped::template Decorator<WidgetTuple>::width, Wrapped::width);
                        }
                        static constexpr Dimension getHeight () { return Wrapped::template Decorator<WidgetTuple>::height; }
                        static constexpr Coordinate getX () { return xV; }
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d, Context *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return BaseClass::operator() (d, ctx);
                        }

                private:
                        using BaseClass = ContainerWidget<Layout<T, WidgetTuple, xV, yV>, typename std::remove_reference_t<T>::DecoratorType>;
                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        WidgetTuple children;
                };

                template <typename T> struct is_layout_wrapper : public std::bool_constant<false> {};

                template <typename T, typename WidgetTuple, Coordinate xV, Coordinate yV>
                class is_layout_wrapper<Layout<T, WidgetTuple, xV, yV>> : public std::bool_constant<true> {};

                template <typename T>
                concept layout_wrapper = is_layout_wrapper<T>::value;

                /**
                 * Interface for dynamic polypomphism (see docs).
                 */
                struct IWindow {
                        virtual ~IWindow () = default;

                        virtual Visibility operator() (IDisplay &disp) const = 0;
                        virtual void input (IDisplay &disp, Key key) = 0;
                        virtual void incrementFocus (IDisplay &disp) const = 0;
                        virtual void decrementFocus (IDisplay &disp) const = 0;
                };

                /**
                 * Window
                 */
                template <typename T, typename Child> class Window : public IWindow, public ContainerWidget<Window<T, Child>, NoDecoration> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr explicit Window (T const &t, Child c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getWidth () { return Wrapped::width; }
                        static constexpr Dimension getHeight () { return Wrapped::height; }
                        static constexpr Coordinate getX () { return Wrapped::x; }
                        static constexpr Coordinate getY () { return Wrapped::y; }

                        Visibility operator() (IDisplay &disp) const override { return BaseClass::operator() (disp, &context); }

                        void input (IDisplay &disp, Key key) override { BaseClass::input (disp, context, key); }

                        void incrementFocus (IDisplay & /* disp */) const override
                        {
                                if (context.currentFocus < Wrapped::focusableWidgetCount - 1) {
                                        ++context.currentFocus;
                                        scrollToFocus (&context);
                                }
                                else { // TODO this branch should be conditionally enabled based on this CSS style config
                                        context.currentFocus = 0;
                                }
                        }

                        void decrementFocus (IDisplay & /* disp */) const override
                        {
                                if (context.currentFocus > 0) {
                                        --context.currentFocus;
                                        scrollToFocus (&context);
                                }
                                else { // TODO this branch should be conditionally enabled based on this CSS style config
                                        context.currentFocus = Wrapped::focusableWidgetCount - 1;
                                }
                        }

                private:
                        mutable Context context{/* nullptr, */ {Wrapped::x + FrameHelper::offset, Wrapped::y + FrameHelper::offset},
                                                {Wrapped::width - FrameHelper::cut, Wrapped::height - FrameHelper::cut}};

                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        std::tuple<Child> children;
                        friend ContainerWidget<Window, NoDecoration>;
                        using FrameHelper = typename Wrapped::FrameHelper;

                        using BaseClass = ContainerWidget<Window<T, Child>, NoDecoration>;
                        using BaseClass::scrollToFocus, BaseClass::input, BaseClass::operator();
                };

                template <typename T> struct is_window_wrapper : public std::bool_constant<false> {};

                template <typename T, typename Child> class is_window_wrapper<Window<T, Child>> : public std::bool_constant<true> {};

                // template <typename T>
                // concept window_wrapper = is_window_wrapper<T>::value;

                template <typename T>
                concept window_wrapper = requires (T type, EmptyDisplay &disp, Key key) {
                                                 type.incrementFocus (disp);
                                                 type.decrementFocus (disp);
                                                 type.input (disp, key);

                                                 {
                                                         type.operator() (disp)
                                                         } -> std::same_as<Visibility>;
                                         };

                /****************************************************************************/

                /**
                 * Group
                 */
                template <typename T, typename WidgetTuple, Coordinate xV, Coordinate yV, Dimension widthV, Dimension heightV, typename Decor>
                class Group : public ContainerWidget<Group<T, WidgetTuple, xV, yV, widthV, heightV, Decor>, Decor> {
                public:
                        using Wrapped = std::remove_reference_t<T>;
                        constexpr Group (T const &t, WidgetTuple c) : widget{t}, children{std::move (c)} {}

                        static constexpr Dimension getWidth () { return widthV; }
                        static constexpr Dimension getHeight () { return heightV; }
                        static constexpr Coordinate getX () { return xV; }
                        static constexpr Coordinate getY () { return yV; }

                        Visibility operator() (auto &d, Context *ctx) const
                        {
                                if (!detail::heightsOverlap (getY (), getHeight (), ctx->currentScroll, ctx->dimensions.height)) {
                                        return Visibility::outside;
                                }

                                return BaseClass::operator() (d, ctx);
                        }

                private:
                        using BaseClass = ContainerWidget<Group<T, WidgetTuple, xV, yV, widthV, heightV, Decor>, Decor>;
                        template <typename X> friend void log (X const &, int);
                        template <typename CC, typename D> friend class ContainerWidget;

                        T widget;
                        WidgetTuple children;
                };

                template <typename T> struct is_group_wrapper : public std::bool_constant<false> {};

                template <typename T, typename WidgetTuple, Coordinate xV, Coordinate yV, Dimension widthV, Dimension heightV, typename Decor>
                class is_group_wrapper<Group<T, WidgetTuple, xV, yV, widthV, heightV, Decor>> : public std::bool_constant<true> {};

                template <typename T>
                concept group_wrapper = is_group_wrapper<T>::value;

                template <typename GrandParent, typename Parent> constexpr Dimension getHeightIncrement (Dimension d)
                {
                        // Parent : og::Layout or og::Group
                        if constexpr (is_layout<Parent>::value) {
                                return Parent::DecoratorType::getHeightIncrement (d);
                        }
                        else {
                                return GrandParent::DecoratorType::getHeightIncrement (d);
                        }
                }

                template <typename GrandParent, typename Parent> constexpr Dimension getWidthIncrement (Dimension d)
                {
                        // Parent : og::Layout or og::Group
                        if constexpr (is_layout<Parent>::value) {
                                return Parent::DecoratorType::getWidthIncrement (d);
                        }
                        else {
                                return GrandParent::DecoratorType::getWidthIncrement (d);
                        }
                }
        } // namespace augment

        template <Focus f, Coordinate x, Coordinate y, typename GrandParent, typename Parent, typename ChildrenTuple>
        constexpr auto transform (ChildrenTuple &tuple);

        // Wrapper for ordinary widgets
        // TODO can I replace x and y with 1 parameter of type Point?
        template <typename T, typename Parent, Focus f, Selection r, Coordinate x, Coordinate y> struct Wrap {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Widget<std::unwrap_ref_decay_t<W>, f, r, x, y>{std::forward<W> (t)};
                }
        };

        // Wrapper for layouts
        template <c::layout T, typename Parent, Focus f, Selection r, Coordinate x, Coordinate y>
                requires c::layout<Parent> || c::window<Parent> || std::same_as<Parent, void>
        struct Wrap<T, Parent, f, r, x, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Layout<std::unwrap_ref_decay_t<W>,
                                               decltype (transform<f, x, y, Parent, T> (static_cast<T> (t).widgets ())), x, y>{
                                std::forward<W> (t), transform<f, x, y, Parent, T> (static_cast<T> (t).widgets ())};
                }
        };

        // Wrapper for groups
        template <c::group T, c::layout Parent, Focus f, Selection r, Coordinate x, Coordinate y> struct Wrap<T, Parent, f, r, x, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Group<std::unwrap_ref_decay_t<W>,
                                              decltype (transform<f, x, y, Parent, T> (static_cast<T> (t).widgets ())), x, y,
                                              Parent::template Decorator<typename T::Children>::width,
                                              Parent::template Decorator<typename T::Children>::height, typename Parent::DecoratorType>{
                                std::forward<W> (t), transform<f, x, y, Parent, T> (static_cast<T> (t).widgets ())};
                }
        };

        template <typename Parent = void, Focus f = 0, Selection r = 0, Coordinate x = 0, Coordinate y = 0> auto wrap (auto &&t);

        // Partial specialization for Windows
        template <c::window T, typename Parent, Focus f, Selection r, Coordinate x, Coordinate y>
                requires std::same_as<Parent, void> // Means that windows are always top level
        struct Wrap<T, Parent, f, r, x, y> {

                template <typename W> static auto wrap (W &&t)
                {
                        return augment::Window<std::unwrap_ref_decay_t<W>, decltype (og::detail::wrap<T, f, r, x, y> (t.child ()))> (
                                std::forward<W> (t), og::detail::wrap<T, f, r, x, y> (t.child ()));
                }
        };

        /// Default values for template arguments are in the forward declaration above
        template <typename Parent, Focus f, Selection r, Coordinate x, Coordinate y> auto wrap (auto &&t)
        {
                // RawType is for selecting the Wrapper partial specialization.
                using RawType = std::remove_reference_t<std::unwrap_ref_decay_t<decltype (t)>>;
                return Wrap<RawType, Parent, f, r, x, y>::wrap (std::forward<decltype (t)> (t));
        }

        template <Focus f, Selection r, Coordinate x, Coordinate y, typename GrandParent, typename Parent, typename Tuple, typename T,
                  typename... Ts>
        constexpr auto transformImpl (Tuple &&prev, T &t, Ts &...ts)
        {
                auto a = std::make_tuple (og::detail::wrap<Parent, f, r, x, y> (t));
                using Wrapped = std::tuple_element_t<0, decltype (a)>;

                if constexpr (sizeof...(Ts) > 0) { // Parent(Layout)::Decor::filter -> hbox return 0 : vbox return height
                        constexpr auto childWidthIncrement = augment::getWidthIncrement<GrandParent, Parent> (Wrapped::getWidth ());
                        constexpr auto childHeightIncrement = augment::getHeightIncrement<GrandParent, Parent> (Wrapped::getHeight ());

                        using WidgetType = std::remove_reference_t<std::unwrap_ref_decay_t<T>>;
                        return transformImpl<f + getFocusableWidgetCount<WidgetType> (), r + 1, x + childWidthIncrement,
                                             y + childHeightIncrement, GrandParent, Parent> (std::tuple_cat (prev, a), ts...);
                }
                else {
                        return std::tuple_cat (prev, a);
                }
        }

        template <Focus f, Coordinate x, Coordinate y, typename GrandParent, typename Parent, typename ChildrenTuple>
        constexpr auto transform (ChildrenTuple &tuple) // TODO use concept instead of "Children" prefix.
        {
                if constexpr (std::tuple_size<ChildrenTuple>::value == 0) {
                        return std::tuple{};
                }
                else {
                        return std::apply (
                                [] (auto &...element) {
                                        std::tuple dummy{};
                                        return transformImpl<f, 0, x, y, GrandParent, Parent> (dummy, element...);
                                },
                                tuple);
                }
        }

} // namespace detail

template <bool clear = true, bool refresh = true> Visibility draw (auto &display, detail::augment::window_wrapper auto const &...window)
{
        if constexpr (clear) {
                display.clear ();
        }

        Visibility ret = (window (display), ...);

        if constexpr (refresh) {
                display.refresh ();
        }

        return ret;
}

void input (auto &display, detail::augment::window_wrapper auto &window, Key key)
{
        switch (key) {
        case Key::incrementFocus:
                window.incrementFocus (display);
                break;

        case Key::decrementFocus:
                window.decrementFocus (display);
                break;

        default:
                window.input (display, key);
                break;
        }
}

/****************************************************************************/

/**
 * Common interface for easy polymorphism.
 * TODO constrain KeyT with some concept, like std::integral but allowing enums.
 */
template <typename KeyT> class ISuite {
public:
        KeyT &current () { return current_; }
        KeyT const &current () const { return current_; }

private:
        KeyT current_{};
};

/**
 *
 */
template <typename KeyT, typename WinTuple>
// requires detail::augment::window_wrapper<std::remove_reference_t<WinTuple>> // TODO move to the factory method.
class Element {
public:
        using WinType = WinTuple;
        Element (KeyT key, WinTuple win) : key_{key}, win_{std::move (win)} {}

        KeyT key () const { return key_; };
        WinTuple const &win () const { return win_; };
        // WinTuple &win () { return win_; };

        auto &last () { return std::get<size_ - 1> (win ()); }
        auto const &last () const { return std::get<size_ - 1> (win ()); }

private:
        KeyT key_{};
        WinTuple win_{};
        static constexpr auto size_ = std::tuple_size_v<WinType>;
};

/**
 * Facotry method
 */
template <typename KeyT, typename... Win> auto element (KeyT key, Win &&...win)
{
        return Element{key, std::make_tuple (std::forward<Win> (win)...)};
}

/**
 * Suite of windows. Allows you to switch currently displayed window by setting a single
 * integer. Thanks to common interface class ISuite, you can easilly pass pointers to it
 * to your handlers.
 */
template <typename KeyT, typename WindowElementTuple> class WindowSuite : public detail::augment::IWindow, public ISuite<KeyT> {
public:
        using ISuite<KeyT>::current;

        explicit WindowSuite (WindowElementTuple wins) : windows{std::move (wins)} {}

        /// Returns the status of the last one
        Visibility operator() (IDisplay &display) const override
        {
                Visibility ret{};

                applyForOne ([&display, &ret] (auto const &elem) {
                        ret = std::apply ([&display] (auto const &...win) { return og::draw<false, false> (display, win...); }, elem.win ());
                });

                return ret;
        }

        void input (IDisplay &display, Key key) override
        {
                applyForOne ([&display, key]<typename Elm> (Elm const &elem) {
                        constexpr auto i = std::tuple_size_v<typename Elm::WinType>;
                        std::get<i - 1> (elem.win ()).input (display, key);
                });
        }

        void incrementFocus (IDisplay &display) const override
        {
                applyForOne ([&display]<typename Elm> (Elm &elem) { elem.last ().incrementFocus (display); });
        }

        void decrementFocus (IDisplay &display) const override
        {
                applyForOne ([&display]<typename Elm> (Elm &elem) { elem.last ().decrementFocus (display); });
        }

private:
        template <typename Callback> void applyForOne (Callback const &clb) const;

        WindowElementTuple windows;
};

/*--------------------------------------------------------------------------*/

template <typename KeyT, typename WindowElementTuple>
template <typename Callback>
void WindowSuite<KeyT, WindowElementTuple>::applyForOne (Callback const &clb) const
{
        auto condition = [&clb, currentKey = current ()] (auto const &elem) -> bool {
                if (elem.key () == currentKey) {
                        clb (elem);
                        return true;
                }

                return false;
        };

        std::apply ([&condition] (auto const &...elms) { (condition (elms) || ...); }, windows);
}

/**
 * Facotry method
 */
template <typename KeyT, typename... Win> auto suite (Element<KeyT, Win> &&...el)
{
        using Tuple = decltype (std::tuple{std::forward<Element<KeyT, Win>> (el)...});

        return WindowSuite<KeyT, Tuple>{std::tuple{std::forward<Element<KeyT, Win>> (el)...}};
}

/****************************************************************************/

/**
 * Container for other widgets.
 */
template <template <typename Wtu> typename Decor, typename WidgetsTuple, Dimension widthV> class Layout {
public:
        static constexpr Focus focusableWidgetCount = detail::Sum<WidgetsTuple, detail::FocusableWidgetCountField>::value;
        template <typename Wtu> using Decorator = Decor<Wtu>;
        using DecoratorType = Decor<WidgetsTuple>;
        static constexpr Dimension width = widthV;

        explicit Layout (WidgetsTuple wgts) : widgets_{std::move (wgts)} {}

        WidgetsTuple &widgets () { return widgets_; }
        WidgetsTuple const &widgets () const { return widgets_; }

private:
        WidgetsTuple widgets_;
};

/*--------------------------------------------------------------------------*/

template <Dimension width = 0, typename... W> auto vbox (W &&...widgets)
{
        using WidgetsTuple = decltype (std::tuple{std::forward<W> (widgets)...});
        auto vbox = Layout<detail::VBoxDecoration, WidgetsTuple, width>{WidgetsTuple{std::forward<W> (widgets)...}};
        return vbox;
}

template <Dimension width = 0, typename... W> auto hbox (W &&...widgets)
{
        using WidgetsTuple = decltype (std::tuple{std::forward<W> (widgets)...});
        auto hbox = Layout<detail::HBoxDecoration, WidgetsTuple, width>{WidgetsTuple{std::forward<W> (widgets)...}};
        return hbox;
}

/****************************************************************************/

/// window factory function has special meaning that is also wraps.
template <Coordinate ox, Coordinate oy, Dimension widthV, Dimension heightV, typename LocalStyle = style::Empty, typename W = void>
auto window (W &&c)
{
        return detail::wrap (detail::windowRaw<ox, oy, widthV, heightV, LocalStyle> (std::forward<W> (c)));
}

} // namespace og
