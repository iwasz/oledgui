/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include "oledgui.h"

namespace og {

/****************************************************************************/
/* Progress bar                                                             */
/****************************************************************************/

namespace style {
        constexpr Tag progress = 13;
        enum class CharsPerSegment { chars2, chars3 };

        namespace getter {
                // clang-format off
                template <typename T> struct Left0 {};
                template <typename T> requires requires { T::left0; } struct Left0<T> { static constexpr auto value = T::left0; };
                
                template <typename T> struct Left1 {};
                template <typename T> requires requires { T::left1; } struct Left1<T> { static constexpr auto value = T::left1; };
                
                template <typename T> struct Left2 {};
                template <typename T> requires requires { T::left2; } struct Left2<T> { static constexpr auto value = T::left2; };

                template <typename T> struct Mid0 {};
                template <typename T> requires requires { T::mid0; } struct Mid0<T> { static constexpr auto value = T::mid0; };
                
                template <typename T> struct Mid1 {};
                template <typename T> requires requires { T::mid1; } struct Mid1<T> { static constexpr auto value = T::mid1; };
                
                template <typename T> struct Mid2 {};
                template <typename T> requires requires { T::mid2; } struct Mid2<T> { static constexpr auto value = T::mid2; };

                template <typename T> struct Right0 {};
                template <typename T> requires requires { T::right0; } struct Right0<T> { static constexpr auto value = T::right0; };
                
                template <typename T> struct Right1 {};
                template <typename T> requires requires { T::right1; } struct Right1<T> { static constexpr auto value = T::right1; };
                
                template <typename T> struct Right2 {};
                template <typename T> requires requires { T::right2; } struct Right2<T> { static constexpr auto value = T::right2; };

                template <typename T> struct DistinctEnds {};
                template <typename T> requires requires { T::distinctEnds; } struct DistinctEnds<T> { static constexpr auto value = T::distinctEnds; };

                template <typename T> struct CharsPerSegment {};
                template <typename T> requires requires { T::charsPerSegment; } struct CharsPerSegment<T> { static constexpr auto value = T::charsPerSegment; };
                // clang-format on

                template <typename LocalStyle, bool distinctEnds, style::CharsPerSegment charsPerSegment> struct SegmentHelper {};

                template <typename LocalStyle> struct SegmentHelper<LocalStyle, false, style::CharsPerSegment::chars2> {
                        static constexpr std::array midCharacters = {
                                style::get<progress, LocalStyle, Mid0> (std::string_view{" "}),
                                style::get<progress, LocalStyle, Mid1> (std::string_view{":"}),
                        };
                };

                template <typename LocalStyle> struct SegmentHelper<LocalStyle, false, style::CharsPerSegment::chars3> {
                        static constexpr std::array midCharacters = {
                                style::get<progress, LocalStyle, Mid0> (std::string_view{" "}),
                                style::get<progress, LocalStyle, Mid1> (std::string_view{"."}),
                                style::get<progress, LocalStyle, Mid2> (std::string_view{":"}),
                        };
                };

                template <typename LocalStyle> struct SegmentHelper<LocalStyle, true, style::CharsPerSegment::chars2> {
                        static constexpr std::array leftCharacters = {
                                style::get<progress, LocalStyle, Left0> (std::string_view{"|"}),
                                style::get<progress, LocalStyle, Left1> (std::string_view{"["}),
                        };

                        static constexpr std::array midCharacters = {
                                style::get<progress, LocalStyle, Mid0> (std::string_view{" "}),
                                style::get<progress, LocalStyle, Mid1> (std::string_view{":"}),
                        };

                        static constexpr std::array rightCharacters = {
                                style::get<progress, LocalStyle, Right0> (std::string_view{"|"}),
                                style::get<progress, LocalStyle, Right1> (std::string_view{"]"}),
                        };
                };

                template <typename LocalStyle> struct SegmentHelper<LocalStyle, true, style::CharsPerSegment::chars3> {
                        static constexpr std::array leftCharacters = {
                                style::get<progress, LocalStyle, Left0> (std::string_view{"|"}),
                                style::get<progress, LocalStyle, Left1> (std::string_view{"I"}),
                                style::get<progress, LocalStyle, Left2> (std::string_view{"["}),
                        };

                        static constexpr std::array midCharacters = {
                                style::get<progress, LocalStyle, Mid0> (std::string_view{" "}),
                                style::get<progress, LocalStyle, Mid1> (std::string_view{"."}),
                                style::get<progress, LocalStyle, Mid2> (std::string_view{":"}),
                        };

                        static constexpr std::array rightCharacters = {
                                style::get<progress, LocalStyle, Right0> (std::string_view{"|"}),
                                style::get<progress, LocalStyle, Right1> (std::string_view{"I"}),
                                style::get<progress, LocalStyle, Right2> (std::string_view{"]"}),
                        };
                };

        } // namespace getter
} // namespace style

/**
 * A progress bar (not focusable, not editable).
 * Template (and runtime) params:
 * - Width in characters
 * - Minimum value
 * - Maximum value
 *
 * CSS-style parameters
 * - Number of unique characters for displaying progress among one segment can be 2 or 3
 * - Separate character set for either ends (left and right).
 * - Characters themselves.
 */
template <typename ValueT,                              /// Value of this exact type is stored inside this class. For example int or int &.
          Dimension widthV,                             /// Width in characters.
          typename std::remove_reference_t<ValueT> min, /// Minimum value, the bar is empty when this value is set
          typename std::remove_reference_t<ValueT> max, /// Maximum value, the bar is full when this value is set
          typename LocalStyle>
        requires std::integral<typename std::remove_reference_t<ValueT>>
class Progress {
public:
        static constexpr Dimension height = 1;
        static constexpr Dimension width = widthV;
        using Value = std::remove_reference_t<ValueT>; // std::integral or a reference like int or int &

        constexpr explicit Progress (ValueT const &cid) : valueContainer (cid) {}
        template <typename Wrapper> Visibility operator() (auto &disp, Context const &ctx) const;

        Value &value () { return valueContainer; }
        Value const &value () const { return valueContainer; }

private:
        static constexpr Value span = std::abs (max - min);
        static constexpr Value unit = /* Value (std::round ( */ span / width /* )) */;
        static_assert (unit != 0, "(max - min) / width equals 0 (integer math). Use floating point types if you want fractional increments.");

        static constexpr bool distinctEnds = style::get<style::progress, LocalStyle, style::getter::DistinctEnds> (false);
        static constexpr auto charsPerSegment
                = style::get<style::progress, LocalStyle, style::getter::CharsPerSegment> (style::CharsPerSegment::chars2);

        using MySegmentHelper = style::getter::SegmentHelper<LocalStyle, distinctEnds, charsPerSegment>;

        enum class Segment { segment0, segment1, segment2 };
        static auto &getSegments (int characterNum);

        ValueT valueContainer; // Can be int, can be int &. Or any other std::integral and its reference
};

/*--------------------------------------------------------------------------*/

template <typename ValueT, Dimension widthV, typename std::remove_reference_t<ValueT> min, typename std::remove_reference_t<ValueT> max,
          typename LocalStyle>
        requires std::integral<typename std::remove_reference_t<ValueT>>
auto &Progress<ValueT, widthV, min, max, LocalStyle>::getSegments (int characterNum)
{
        if constexpr (distinctEnds) {
                if (characterNum == 0) {
                        return MySegmentHelper::leftCharacters;
                }

                if (characterNum == width - 1) {
                        return MySegmentHelper::rightCharacters;
                }
        }

        return MySegmentHelper::midCharacters;
}

/*--------------------------------------------------------------------------*/

template <typename ValueT, Dimension widthV, typename std::remove_reference_t<ValueT> min, typename std::remove_reference_t<ValueT> max,
          typename LocalStyle>
        requires std::integral<typename std::remove_reference_t<ValueT>>
template <typename Wrapper>
Visibility Progress<ValueT, widthV, min, max, LocalStyle>::operator() (auto &disp, Context const & /* ctx */) const
{

        Value const barLen = std::min<Value> (valueContainer / unit, width);
        int num{};

        for (; num < barLen; ++num) {
                detail::print (disp, getSegments (num).back ());
                ++disp.cursor ().x ();
        }

        Value const fraction = valueContainer % unit;

        if (fraction > 0 && num < width) {
                size_t character = fraction * (getSegments (num).size () - 1) / unit;
                detail::print (disp, getSegments (num).at (character));
                ++num;
                ++disp.cursor ().x ();
        }

        for (; num < width; ++num) {
                detail::print (disp, getSegments (num).front ());
                ++disp.cursor ().x ();
        }

        return Visibility::visible;
}

/*--------------------------------------------------------------------------*/

// TODO does not compile when ValueT holds std::ref <uint32_t>
template <auto widthV, auto min, auto max, typename LocalStye = void, typename ValueT>
        requires std::integral<typename std::remove_reference_t<std::unwrap_ref_decay_t<ValueT>>>
auto progress (ValueT &&val)
{
        return Progress<std::unwrap_ref_decay_t<ValueT>, widthV, min, max, LocalStye>{std::forward<ValueT> (val)};
}

// template <auto widthV, auto min, auto max, typename ValueT>
// requires std::integral<typename std::remove_reference_t<std::unwrap_ref_decay_t<ValueT>>> Progress (ValueT const &)
// ->Progress<std::unwrap_ref_decay_t<ValueT>, widthV, min, max>;

} // namespace og