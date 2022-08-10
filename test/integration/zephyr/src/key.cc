// /****************************************************************************
//  *                                                                          *
//  *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
//  *  ~~~~~~~~                                                                *
//  *  License : see COPYING file for details.                                 *
//  *  ~~~~~~~~~                                                               *
//  ****************************************************************************/

// #include "key.h"
// #include <logging/log.h>

// LOG_MODULE_REGISTER (key);

// namespace zephyr {

// template <typename Callback> Key<Callback>::Key (gpio_dt_spec const &devTr, og::Key val, Callback clb) : deviceTree{devTr}, value_{val}
// {

//         if (!device_is_ready (deviceTree.port)) {
//                 LOG_ERR ("Key not ready");
//                 return;
//         }

//         k_timer_init (&debounceTimer, onDebounceElapsed, NULL);

//         int ret = gpio_pin_configure_dt (&deviceTree, GPIO_INPUT | deviceTree.dt_flags);

//         if (ret != 0) {
//                 LOG_ERR ("Error %d: failed to configure %s pin %d\n", ret, deviceTree.port->name, deviceTree.pin);
//                 return;
//         }

//         ret = gpio_pin_interrupt_configure_dt (&deviceTree, GPIO_INT_EDGE_BOTH);
//         if (ret != 0) {
//                 LOG_ERR ("Error %d: failed to configure interrupt on %s pin %d\n", ret, deviceTree.port->name, deviceTree.pin);
//                 return;
//         }

//         gpio_init_callback (&data.buttonCb, onEdge, BIT (deviceTree.pin));
//         gpio_add_callback (deviceTree.port, &data.buttonCb);
// }

// /****************************************************************************/

// template <typename Callback> Key<Callback>::~Key () { gpio_remove_callback (deviceTree.port, &data.buttonCb); }

// /****************************************************************************/

// template <typename Callback> void Key<Callback>::onEdge (const struct device *dev, struct gpio_callback *cb, uint32_t pins)
// {
//         Key::Envelope *data = CONTAINER_OF (cb, Key::Envelope, buttonCb);

//         if (data && data->that) {
//                 data->that->lastPressedStatus = data->that->isPressed ();
//                 k_timer_user_data_set (&data->that->debounceTimer, data->that);
//                 k_timer_start (&data->that->debounceTimer, K_MSEC (32), K_NO_WAIT);
//         }

//         // LOG_INF ("onPressed");
// }

// /****************************************************************************/

// template <typename Callback> void Key<Callback>::onDebounceElapsed (struct k_timer *timer_id)
// {
//         auto *key = reinterpret_cast<Key *> (k_timer_user_data_get (timer_id));

//         if (key->isPressed () != key->lastPressedStatus) {
//                 return;
//         }

//         // LOG_INF ("id: %d, state: %d", int (key->value ()), int (key->lastPressedStatus));

//         if (key->state == State::idle && key->lastPressedStatus) {
//                 key->state = State::down;
//         }
//         else if (key->state == State::down && !key->lastPressedStatus) {
//                 key->state = State::idle;
//                 LOG_INF ("EVENT id: %d", int (key->value ()));
//         }
// }

// } // namespace zephyr

// /****************************************************************************/

// // og::Key getKey (int ch)
// // {
// //         // TODO characters must be customizable (compile time)
// //         switch (ch) {
// //         case KEY_DOWN:
// //                 return Key::incrementFocus;

// //         case KEY_UP:
// //                 return Key::decrementFocus;

// //         case int (' '):
// //                 return Key::select;

// //         default:
// //                 return Key::unknown;
// //         }
// // }

// // enum class Input : uint8_t { down, up, enter, esc };

// // uint8_t getInputs ()
// // {
// //         uint8_t event = (gpio_pin_get (buttonDown, BUTTON_DOWN_PIN) << int (Input::down)) | (gpio_pin_get (buttonUp, BUTTON_UP_PIN) << int
// //         (Input::up)) | gpio_pin_get (buttonEnter, BUTTON_ENTER_PIN) gpio_pin_get (buttonEnter, BUTTON_ESCAPE_PIN)
// // }

// og::Key getKey () { return og::Key::unknown; }
