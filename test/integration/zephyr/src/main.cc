/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "cfb_font_oldschool.h"
#include "kernel.h"
#include "key.h"
#include <cstddef>
#include <device.h>
#include <devicetree.h>
#include <display/cfb.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <oledgui/zephyrCfb.h>
#include <optional>
#include <pm/pm.h>
#include <string_view>
#include <sys/_stdint.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <type_traits>
#include <zephyr.h>
#include <zephyr/types.h>
#include <zephyr/zephyr.h>

LOG_MODULE_REGISTER (main);

const struct device *display;

void init ()
{
        uint16_t rows;
        uint8_t ppt;
        uint8_t font_width;
        uint8_t font_height;

        display = device_get_binding ("SSD1306");

        if (display == NULL) {
                LOG_ERR ("Device not found\n");
                return;
        }

        if (display_set_pixel_format (display, PIXEL_FORMAT_MONO10) != 0) {
                LOG_ERR ("Failed to set required pixel format\n");
                return;
        }

        if (cfb_framebuffer_init (display)) {
                LOG_ERR ("Framebuffer initialization failed!\n");
                return;
        }

        cfb_framebuffer_clear (display, true);
        display_blanking_off (display);

        rows = cfb_get_display_parameter (display, CFB_DISPLAY_ROWS);
        ppt = cfb_get_display_parameter (display, CFB_DISPLAY_PPT);

        // for (int idx = 0; idx < 42; idx++) {
        //         if (cfb_get_font_size (dev, idx, &font_width, &font_height)) {
        //                 break;
        //         }

        //         // if (font_height == ppt) {
        //         cfb_framebuffer_set_font (dev, idx);
        //         printf ("font width %d, font height %d\n", font_width, font_height);
        //         // }
        // }

        // printf ("x_res %d, y_res %d, ppt %d, rows %d, cols %d\n", cfb_get_display_parameter (dev, CFB_DISPLAY_WIDTH),
        //         cfb_get_display_parameter (dev, CFB_DISPLAY_HEIGH), ppt, rows, cfb_get_display_parameter (dev, CFB_DISPLAY_COLS));

        cfb_framebuffer_set_font (display, 0);
        cfb_framebuffer_invert (display);
}

int main ()
{
        int err;

        init ();

        // TODO IRQ collision
        // zephyr::Key escKey (GPIO_DT_SPEC_GET (DT_PATH (ui_buttons, button_escape), gpios));
        zephyr::Keyboard keyboard;
        zephyr::Key upKey (GPIO_DT_SPEC_GET (DT_PATH (ui_buttons, button_up), gpios), og::Key::decrementFocus);
        zephyr::Key downKey (GPIO_DT_SPEC_GET (DT_PATH (ui_buttons, button_down), gpios), og::Key::incrementFocus);
        zephyr::Key enterKey (GPIO_DT_SPEC_GET (DT_PATH (ui_buttons, button_enter), gpios), og::Key::select);

        /*--------------------------------------------------------------------------*/

        using namespace og;
        using namespace std::string_view_literals;

        og::zephyr::cfb::Display<18, 7> d1 (display);

        bool showDialog{};

        std::string_view buff{R"(The
class
template
basic_string_view describes an object that can refer to a constant contiguous sequence of
char-like
objects
with
the first element of the sequence at position zero.)"};
        // std::string buff{"aaa"};

        auto txt = text<17, 7> (std::ref (buff));
        LineOffset startLine{};
        auto up = button ("^"sv, [&txt, &startLine] { startLine = txt.setStartLine (--startLine); });
        auto dwn = button ("v"sv, [&txt, &startLine] { startLine = txt.setStartLine (++startLine); });
        auto txtComp = hbox (std::ref (txt), vbox<1> (std::ref (up), vspace<5>, std::ref (dwn)));

        auto chk = check (" 1 "sv);

        auto grp = group ([] (auto o) {}, radio (0, " R "sv), radio (1, " G "sv), radio (1, " B "sv), radio (1, " A "sv), radio (1, " C "sv),
                          radio (1, " M "sv), radio (1, " Y "sv), radio (1, " K "sv));

        auto vv = vbox (txtComp, //
                        hbox (hbox<1> (label ("^"sv)), label ("^"sv), label ("V"sv)), vbox ());
        //                 hbox (std::ref (chk), check (" 2 "sv)), //
        //                 std::ref (grp)                          //
        // );

        auto x = window<0, 0, 18, 7> (std::ref (txtComp));
        // log (x);

        /*--------------------------------------------------------------------------*/

        auto v = vbox (label ("  PIN:"sv), label (" 123456"sv),
                       hbox (button ("[OK]"sv, [&showDialog] { showDialog = false; }), button ("[Cl]"sv, [] {})), check (" 15 "sv));

        auto dialog = window<4, 1, 10, 5, true> (std::ref (v));

        // log (dialog);

        while (true) {

                if (showDialog) {
                        draw (d1, x, dialog);
                        input (d1, dialog, getKey ()); // Blocking call.
                }
                else {
                        draw (d1, x);
                        input (d1, x, getKey ());
                }

                k_sleep (K_SECONDS (3));
        }
}