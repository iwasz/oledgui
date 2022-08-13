/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include "oledgui.h"
#include <ncurses.h>
#include <type_traits>

namespace og {

/**
 * Ncurses backend.
 */
template <Dimension widthV, Dimension heightV, typename Child = Empty>
class NcursesDisplay : public Display<NcursesDisplay<widthV, heightV, Child>, widthV, heightV, Child> {
public:
        using Base = Display<NcursesDisplay<widthV, heightV, Child>, widthV, heightV, Child>;
        using Base::cursor;
        using Base::width, Base::height;

        explicit NcursesDisplay (Child c = {});
        NcursesDisplay (NcursesDisplay const &) = default;
        NcursesDisplay &operator= (NcursesDisplay const &) = default;
        NcursesDisplay (NcursesDisplay &&) noexcept = default;
        NcursesDisplay &operator= (NcursesDisplay &&) noexcept = default;
        ~NcursesDisplay () noexcept
        {
                clrtoeol ();
                refresh ();
                endwin ();
        }

        template <typename String>
        requires requires (String s)
        {
                {
                        s.data ()
                        } -> std::convertible_to<const char *>;
        }
        void print (String const &str) { mvwprintw (win, cursor ().y (), cursor ().x (), static_cast<const char *> (str.data ())); }

        void clear ()
        {
                wclear (win);
                cursor ().x () = 0;
                cursor ().y () = 0;
        }

        void color (Color c) { wattron (win, COLOR_PAIR (c)); }

        void refresh ()
        {
                ::refresh ();
                wrefresh (win);
        }

private:
        using Base::child;
        WINDOW *win{};
};

template <Dimension widthV, Dimension heightV, typename Child> NcursesDisplay<widthV, heightV, Child>::NcursesDisplay (Child c) : Base (c)
{
        setlocale (LC_ALL, "");
        initscr ();
        curs_set (0);
        noecho ();
        cbreak ();
        use_default_colors ();
        start_color ();
        init_pair (1, COLOR_WHITE, COLOR_BLUE);
        init_pair (2, COLOR_BLUE, COLOR_WHITE);
        keypad (stdscr, true);
        win = newwin (height, width, 0, 0);
        wbkgd (win, COLOR_PAIR (1));
        refresh ();
}

template <Dimension widthV, Dimension heightV> auto ncurses (auto &&child)
{
        return NcursesDisplay<widthV, heightV, std::remove_reference_t<decltype (child)>>{std::forward<decltype (child)> (child)};
}

og::Key getKey (int ch)
{
        // TODO characters must be customizable (compile time)
        switch (ch) {
        case KEY_DOWN:
                return Key::incrementFocus;

        case KEY_UP:
                return Key::decrementFocus;

        case int (' '):
                return Key::select;

        default:
                return Key::unknown;
        }
}

og::Key getKey () { return getKey (getch ()); }

} // namespace og
