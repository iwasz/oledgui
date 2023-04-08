/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include <oledgui/extra.h>
#include <oledgui/oledgui.h>

namespace og::style {

template <> struct Style<window> {
        static constexpr std::string_view nwcorner{"┌"};
        static constexpr std::string_view necorner{"┐"};
        static constexpr std::string_view swcorner{"└"};
        static constexpr std::string_view secorner{"┘"};
        static constexpr std::string_view vline{"│"};
        static constexpr std::string_view hline{"─"};
};

template <> struct Style<radio> {
        static constexpr std::string_view checked{"◉"};
        static constexpr std::string_view unchecked{"○"};
};

template <> struct Style<check> {
        static constexpr std::string_view checked{"☑"};
        static constexpr std::string_view unchecked{"☐"};
};

template <> struct Style<line> {
        static constexpr std::string_view hline{"─"};
};

template <> struct Style<progress> {
        static constexpr std::string_view left0{"_"};
        static constexpr std::string_view left1{"|"};
        static constexpr std::string_view left2{"["};
        static constexpr std::string_view mid0{"_"};
        static constexpr std::string_view mid1{"▌"};
        static constexpr std::string_view mid2{"█"};
        static constexpr std::string_view right0{"_"};
        static constexpr std::string_view right1{"|"};
        static constexpr std::string_view right2{"]"};
        static constexpr bool distinctEnds = true;
        static constexpr auto charsPerSegment = style::CharsPerSegment::chars3;
};

} // namespace og::style

struct ShowFrame {
        static constexpr bool frame = true;
};
