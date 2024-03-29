/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#pragma once
#include "oledgui.h"
#include <type_traits>
#include <zephyr/display/cfb.h>
#include <zephyr/sys/printk.h>

// #define debugMacro printk

namespace og::zephyr::cfb {

/**
 * Zephyr backend
 * TODO make character width and height customizable (template param)
 * TODO optimize this "old-school" font because now there are 2 pixel vertical spaces between the glyphs.
 */
template <Dimension widthV, Dimension heightV> class Display : public AbstractDisplay<Display<widthV, heightV>, widthV, heightV> {
public:
        using Base = og::AbstractDisplay<Display<widthV, heightV>, widthV, heightV>;
        using Base::cursor;
        using Base::width, Base::height;

        Display (device const *disp);

        void print (std::span<const char> const &str) override
        {
                // We do the copy to have null-terminated string suitable for C API.
                std::array<char, widthV + 1> tmp;
                auto n = std::min<size_t> (widthV, str.size ());
                std::copy_n (str.begin (), n, tmp.begin ());
                tmp.at (n) = '\0';

                // TODO why data instead of c_str! Obviously this code expects null-trminated string!
                // if (cfb_print (display, const_cast<char *> (static_cast<const char *> (str.data ())), cursor ().x () * 7, cursor ().y () * 8)
                if (cfb_print (display, tmp.data (), cursor ().x () * 7, cursor ().y () * 8) != 0) {
                        printk ("cfb_print is unable to print\r\n");
                }

                if (style_ == style::Text::highlighted) {
                        if (int err = cfb_invert_area (display, cursor ().x () * 7, cursor ().y () * 8, str.size () * 7, 8); err != 0) {
                                printk ("Could not invert (err %d)\n", err);
                        }
                }
        }

        void clear () override
        {
                // This clears the whole screen. Not desirable, multiple layers do not work.
                if (int err = cfb_framebuffer_clear (display, false); err) {
                        printk ("Could not clear framebuffer (err %d)\n", err);
                }

                cursor ().x () = 0;
                cursor ().y () = 0;
                // printk ("clear\r\n");
        }

        void textStyle (style::Text stl) override
        {
                style_ = stl;
                // printk ("style %d,%d\r\n", cursor ().x () * 7, cursor ().y () * 8);
        }

        void refresh () override
        {
                if (int err = cfb_framebuffer_finalize (display); err) {
                        printk ("Could not finalize framebuffer (err %d)\n", err);
                }
                // printk ("refresh\r\n");
        }

private:
        device const *display{};
        style::Text style_{};
};

/****************************************************************************/

template <Dimension widthV, Dimension heightV> Display<widthV, heightV>::Display (device const *disp) : display{disp}
{
        if (!device_is_ready (display)) {
                printk ("Display device not ready\n");
                return;
        }
}

/****************************************************************************/

template <Dimension widthV, Dimension heightV> auto zephyrCfb () { return Display<widthV, heightV>{}; }

} // namespace og::zephyr::cfb
