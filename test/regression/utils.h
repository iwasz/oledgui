/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once

template <typename T> class Cfg {
public:
        explicit Cfg (T &ref) : val{ref} {}

        operator T () const { return val; }

        Cfg &operator= (T const &input)
        {
                val = input;
                return *this;
        }

private:
        T &val{};
};