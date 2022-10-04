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
template <Dimension widthV, Dimension heightV> class NcursesDisplay : public AbstractDisplay<NcursesDisplay<widthV, heightV>, widthV, heightV> {
public:
        using Base = AbstractDisplay<NcursesDisplay<widthV, heightV>, widthV, heightV>;
        using Base::cursor;
        using Base::width, Base::height;

        NcursesDisplay ();
        NcursesDisplay (NcursesDisplay const &) = default;
        NcursesDisplay &operator= (NcursesDisplay const &) = default;
        NcursesDisplay (NcursesDisplay &&) noexcept = default;
        NcursesDisplay &operator= (NcursesDisplay &&) noexcept = default;
        ~NcursesDisplay () noexcept override
        {
                clrtoeol ();
                refresh ();
                endwin ();
        }

        void print (std::span<const char> const &str) override
        {
                // This is to ensure mvwprintw does not print the whole string.
                std::string tmp{str.begin (), str.end ()};
                mvwprintw (win, cursor ().y (), cursor ().x (), tmp.data ());
        }

        void clear () override
        {
                wclear (win);
                cursor ().x () = 0;
                cursor ().y () = 0;
        }

        void style (Style clr) override
        {
                if (clr == Style::highlighted) {
                        wattron (win, COLOR_PAIR (2));
                }
                else {
                        wattron (win, COLOR_PAIR (1));
                }
        }

        void refresh () override
        {
                ::refresh ();
                wrefresh (win);
        }

private:
        WINDOW *win{};
};

/****************************************************************************/

template <Dimension widthV, Dimension heightV> NcursesDisplay<widthV, heightV>::NcursesDisplay ()
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
        win = newwin (height (), width (), 0, 0);
        wbkgd (win, COLOR_PAIR (1));
        refresh ();
}

/****************************************************************************/

template <Dimension widthV, Dimension heightV> auto ncurses () { return NcursesDisplay<widthV, heightV>{}; }

/****************************************************************************/

inline og::Key getKey (int chr)
{
        // TODO characters must be customizable (compile time)
        switch (chr) {
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

/****************************************************************************/

inline og::Key getKey () { return getKey (getch ()); }

} // namespace og
