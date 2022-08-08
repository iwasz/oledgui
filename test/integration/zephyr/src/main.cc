/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "cfb_font_oldschool.h"
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

namespace zephyr {

class Key;

class Keyboard {
public:
        Keyboard () { k_timer_init (&debounceTimer, onDebounceElapsed, NULL); }

        // void onPressed (auto const);
        static void onDebounceElapsed (struct k_timer *timer_id);

        friend class Key;

private:
        k_timer debounceTimer{};
};

/****************************************************************************/

/* template <auto Code>  */ class Key {
public:
        explicit Key (gpio_dt_spec devTr, Keyboard *keyb, og::Key val); // TODO val -> template
        ~Key ();

        bool isPressed () const { return gpio_pin_get_dt (&deviceTree); }
        og::Key value () const { return value_; };

private:
        static void onPressed (const struct device *dev, struct gpio_callback *cb, uint32_t pins);

        gpio_dt_spec deviceTree;

        struct Envelope {
                gpio_callback buttonCb{};
                Key *that{};
        };

        Envelope data{{}, this};
        Keyboard *keyboard{};
        og::Key value_{};
        bool bouncing{};

        static_assert (std::is_standard_layout_v<Key::Envelope>);
};

Key::Key (gpio_dt_spec devTr, Keyboard *keyb, og::Key val) : deviceTree (std::move (devTr)), keyboard{keyb}, value_{val}
{

        if (!device_is_ready (deviceTree.port)) {
                LOG_ERR ("Key not ready");
                return;
        }

        int ret = gpio_pin_configure_dt (&deviceTree, GPIO_INPUT | deviceTree.dt_flags);

        if (ret != 0) {
                LOG_ERR ("Error %d: failed to configure %s pin %d\n", ret, deviceTree.port->name, deviceTree.pin);
                return;
        }

        ret = gpio_pin_interrupt_configure_dt (&deviceTree, GPIO_INT_EDGE_TO_ACTIVE);
        if (ret != 0) {
                LOG_ERR ("Error %d: failed to configure interrupt on %s pin %d\n", ret, deviceTree.port->name, deviceTree.pin);
                return;
        }

        gpio_init_callback (&data.buttonCb, onPressed, BIT (deviceTree.pin));
        gpio_add_callback (deviceTree.port, &data.buttonCb);
}

Key::~Key () { gpio_remove_callback (deviceTree.port, &data.buttonCb); }

void Key::onPressed (const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
        Key::Envelope *data = CONTAINER_OF (cb, Key::Envelope, buttonCb);

        if (data && data->that && data->that->keyboard) {
                data->that->bouncing = true;
                k_timer_user_data_set (&data->that->keyboard->debounceTimer, data->that);
                k_timer_start (&data->that->keyboard->debounceTimer, K_MSEC (32), K_NO_WAIT);
        }
}

/****************************************************************************/

void Keyboard::onDebounceElapsed (struct k_timer *timer_id)
{
        auto *key = reinterpret_cast<Key *> (k_timer_user_data_get (timer_id));

        if (key->isPressed ()) {
                LOG_INF ("P %d", int (key->value ()));
        }
}

} // namespace zephyr

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

// og::Key getKey (int ch)
// {
//         // TODO characters must be customizable (compile time)
//         switch (ch) {
//         case KEY_DOWN:
//                 return Key::incrementFocus;

//         case KEY_UP:
//                 return Key::decrementFocus;

//         case int (' '):
//                 return Key::select;

//         default:
//                 return Key::unknown;
//         }
// }

// enum class Input : uint8_t { down, up, enter, esc };

// uint8_t getInputs ()
// {
//         uint8_t event = (gpio_pin_get (buttonDown, BUTTON_DOWN_PIN) << int (Input::down)) | (gpio_pin_get (buttonUp, BUTTON_UP_PIN) << int
//         (Input::up)) | gpio_pin_get (buttonEnter, BUTTON_ENTER_PIN) gpio_pin_get (buttonEnter, BUTTON_ESCAPE_PIN)
// }

og::Key getKey () { return og::Key::unknown; }

int main ()
{
        int err;

        init ();

        // TODO IRQ collision
        // zephyr::Key escKey (GPIO_DT_SPEC_GET (DT_PATH (ui_buttons, button_escape), gpios));
        zephyr::Keyboard keyboard;
        zephyr::Key upKey (GPIO_DT_SPEC_GET (DT_PATH (ui_buttons, button_up), gpios), &keyboard, og::Key::decrementFocus);
        zephyr::Key downKey (GPIO_DT_SPEC_GET (DT_PATH (ui_buttons, button_down), gpios), &keyboard, og::Key::incrementFocus);
        zephyr::Key enterKey (GPIO_DT_SPEC_GET (DT_PATH (ui_buttons, button_enter), gpios), &keyboard, og::Key::select);

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