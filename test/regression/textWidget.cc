/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "inputApi.h"
#include "regression.h"
#include <iostream>

namespace {
using namespace og;
using namespace std::string_view_literals;

enum class Windows { menu, textReferences, compositeTextWidget };

ISuite<Windows> *mySuite{};

/*--------------------------------------------------------------------------*/

auto menu = window<0, 0, 18, 7> (vbox (button ("[back]"sv, mainMenu), //
        button ("Text widget"sv, [] {mySuite->current () = Windows::textReferences; }), //
        button ("Composite"sv, [] { mySuite->current () = Windows::compositeTextWidget; })/*,
                                       button ("Combo API"sv, [] { mySuite->current () = Windows::dataReferencesCombo; }),
                                       button ("Combo enum"sv, [] { mySuite->current () = Windows::dataReferencesComboEnum; }),
                                       button ("Radio API"sv, [] { mySuite->current () = Windows::dataReferencesRadio; })*/) );

auto backButton = button ("[back]"sv, [] { mySuite->current () = Windows::menu; });

/*--------------------------------------------------------------------------*/

std::string buff{R"(The
class
template
basic_string_view describes an object that can refer to a constant contiguous sequence of
char-like
objects
with
the first element of the sequence at position zero.)"};
// std::string buff{"aaa"};

auto txt = text<17, 6> (std::ref (buff));
LineOffset startLine{};
auto up = button ("▲"sv, [/* &txt, &startLine */] { startLine = txt.setStartLine (--startLine); });
auto dwn = button ("▼"sv, [/* &txt, &startLine */] { startLine = txt.setStartLine (++startLine); });
auto txtComp = hbox (std::ref (txt), vbox<1> (std::ref (up), vspace<4>, std::ref (dwn)));

auto textReferences = window<0, 0, 18, 7> (vbox (std::ref (backButton), std::ref (txtComp)));

/*--------------------------------------------------------------------------*/

// auto newIdea ()
// {
//         LineOffset startLine{};
//         auto txtComp = hbox (text<17, 6> (std::ref (buff)),
//                              vbox<1> (button ("▲"sv, [/* &txt, &startLine */] { startLine = txt.setStartLine (--startLine); }), //
//                                       vspace<4>,                                                                                //
//                                       button ("▼"sv, [/* &txt, &startLine */] { startLine = txt.setStartLine (++startLine); })));

//         auto &text = txtComp.getChild (0);
//         auto &buttonUp = txtComp.getChild (1).getChild (0);
//         auto &callback = buttonUp.callback ();
//         callback.setText (text);
//         callback.setStartLine (text);

//         // Then it copieas references are broken
//         return txtComp;
// }

/*--------------------------------------------------------------------------*/

namespace {
        struct Y {
                void runY () {}
        };

        struct X {
                void runX () {}
                Y *y{};
        };

        template <typename YT> struct X1 {
                YT *y;
        };

        struct Z {

                Y y{};
                X1<Y> x{&y};
        };

        Z z{};
} // namespace

auto lll = [y = Y{}] () mutable { return [x = X{&y}] () mutable { x.runX (); }; };

auto lll2 = [txt = text<17, 6> (std::ref (buff)), startLine = LineOffset{}] () mutable {
        return [up = button ("▲"sv, [&txt, &startLine] () { startLine = txt.setStartLine (--startLine); })] () mutable {

        };
};

template </* typename BufferT, */ typename TextWidgetT, typename ButtonUpT, typename ButtonDownT>
// requires c::text_buffer<std::remove_reference_t<BufferT>>
class TextBox1 : public Focusable {
public:
        // using Buffer = std::remove_reference_t<BufferT>;
        using TextWidget = std::remove_reference_t<TextWidgetT>;
        using ButtonUp = std::remove_reference_t<ButtonUpT>;
        using ButtonDown = std::remove_reference_t<ButtonDownT>;

        static constexpr Dimension height = TextWidget::width;
        static constexpr Dimension width = TextWidget::height + 1; // 1 is for the scrollbar

        TextBox1 (/* Buffer const &buf,  */ TextWidget const &twgt, ButtonUp const &bup, ButtonDown const &bdn)
            : /* buf_{buf}, */ textWidget{twgt}, buttonUp{bup}, buttonDown{bdn}
        {
        }

        // template <typename Wrapper> Visibility operator() (IDisplay &disp, Context const &ctx) const { return txtWidget (disp, ctx); }
        // template <typename Wrapper> void input (IDisplay &disp, Context const &ctx, Key key) { txtWidget.input (disp, ctx, key); }

        // private:
        // BufferT buf_; // T or T&
        LineOffset startLine{};
        TextWidgetT textWidget;
        ButtonUpT buttonUp;
        ButtonDownT buttonDown;

        // /// NOTE : I would be delighted to learn how to minimize code duplication here:
        // using Text = decltype (text<width - 1, height> (std::ref (buf_)));
        // Text textWidget = text<width - 1, height> (std::ref (buf_));

        // // auto textWidget () { return text<width - 1, height> (std::ref (buf_)); }
        // // using Text = std::result_of_t<(TextBox::textWidget)(TextBox)>;
        // // Text txt{};

        // auto up ()
        // {
        //         using DownButton = decltype (button ("▲"sv, [this] { startLine = txt.setStartLine (--startLine); }));
        // }

        // using UpButton = std::invoke_result_t<decltype (&TextBox1::up), void>;
        // UpButton upButton{up ()};

        // auto down ()
        // {
        //         return button ("▼"sv, [this] { startLine = txt.setStartLine (++startLine); });
        // }

        // using DownButton = decltype (down ());
        // DownButton downButton{};

        // auto txtComp () { return hbox (std::ref (txt), vbox<1> (std::ref (upButton), vspace<height - 2>, std::ref (downButton))); }

        // using TxtComp = decltype (txtComp ());
        // TxtComp txtWidget{};
};

// template <Dimension widthV, Dimension heightV, typename Buffer>
// requires c::text_buffer<std::remove_reference_t<Buffer>>
// class TextBox : public Focusable {
// public:
//         static constexpr Dimension height = heightV;
//         static constexpr Dimension width = widthV;

//         explicit TextBox (Buffer const &buf) : buf_{buf} {}
//         TextBox (TextBox const &bbb) : buf_{bbb.buf_} {}

//         auto &widgets () { return textBoxBody.widgets (); }
//         auto const &widgets () const { return textBoxBody.widgets (); }

// private:
//         Buffer buf_; // T or T&
//         LineOffset startLine{};

//         using Text = decltype (text<width - 1, height> (std::ref (buf_)));
//         Text textWidget = text<width - 1, height> (std::ref (buf_));

//         /*--------------------------------------------------------------------------*/

//         struct UpCallback {
//                 UpCallback (LineOffset &startLine, Text &textWidget) : startLine{startLine}, textWidget{textWidget} {}

//                 void operator() () { startLine = textWidget.setStartLine (--startLine); }

//                 LineOffset &startLine;
//                 Text &textWidget;
//         };

//         UpCallback upCallback{startLine, textWidget};

//         using ButtonUp = decltype (button ("▲"sv, upCallback));
//         ButtonUp buttonUp = button ("▲"sv, upCallback);

//         /*--------------------------------------------------------------------------*/

//         struct DnCallback {
//                 DnCallback (LineOffset &startLine, Text &textWidget) : startLine{startLine}, textWidget{textWidget} {}

//                 void operator() () { startLine = textWidget.setStartLine (++startLine); }

//                 LineOffset &startLine;
//                 Text &textWidget;
//         };

//         DnCallback dnCallback{startLine, textWidget};

//         using ButtonDn = decltype (button ("▲"sv, dnCallback));
//         ButtonDn buttonDn = button ("▲"sv, dnCallback);

//         /*--------------------------------------------------------------------------*/

//         using TextBoxBody = decltype (hbox (std::ref (txt), vbox<1> (std::ref (buttonUp), vspace<height - 2>, std::ref (buttonDn))));
//         TextBoxBody textBoxBody = hbox (std::ref (txt), vbox<1> (std::ref (buttonUp), vspace<height - 2>, std::ref (buttonDn)));

// public:
//         static constexpr auto focusableWidgetCount = TextBoxBody::focusableWidgetCount;

//         template <typename Wtu> using Decorator = typename TextBoxBody::template Decorator<Wtu>;
//         using DecoratorType = typename TextBoxBody::DecoratorType;
// };

/*--------------------------------------------------------------------------*/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

// TODO total mess
template <Dimension widthV, Dimension heightV, typename Buffer>
// requires c::text_buffer<std::remove_reference_t<Buffer>>
class TextBox : public Focusable {
public:
        static constexpr Dimension height = heightV;
        static constexpr Dimension width = widthV;

        explicit TextBox (Buffer buf)
            : buf_{std::move (buf)},
              startLine{},
              textWidget{text<width - 1, height> (std::ref (buf_))},
              textBoxBody{hbox (std::ref (textWidget),
                                vbox<1> (button ("▲"sv, UpCallback{startLine, textWidget}), vspace<height - 2>,
                                         button ("▲"sv, DnCallback{startLine, textWidget})))}
        {
        }

        TextBox (TextBox const &bbb) : TextBox (bbb.buf_) {}
        TextBox &operator= (TextBox const &bbb) = delete;

        TextBox (TextBox &&bbb) : TextBox (bbb.buf_) {}
        TextBox &operator= (TextBox &&bbb) = delete;

        ~TextBox () = default;

        auto &widgets () { return textBoxBody.widgets (); }
        auto const &widgets () const { return textBoxBody.widgets (); }

private:
        Buffer buf_; // T or std::ref T
        LineOffset startLine;

        using Text = decltype (text<width - 1, height> (std::ref (buf_)));
        Text textWidget;

        /*--------------------------------------------------------------------------*/

        struct UpCallback {
                UpCallback (LineOffset &startLine, Text &textWidget) : startLine{startLine}, textWidget{textWidget} {}

                void operator() () { startLine = textWidget.setStartLine (--startLine); }

                LineOffset &startLine;
                Text &textWidget;
        };

        /*--------------------------------------------------------------------------*/

        struct DnCallback {
                DnCallback (LineOffset &startLine, Text &textWidget) : startLine{startLine}, textWidget{textWidget} {}

                void operator() () { startLine = textWidget.setStartLine (++startLine); }

                LineOffset &startLine;
                Text &textWidget;
        };

        /*--------------------------------------------------------------------------*/

        using TextBoxBody = decltype (hbox (std::ref (textWidget),
                                            vbox<1> (button ("▲"sv, UpCallback{startLine, textWidget}), vspace<height - 2>,
                                                     button ("▼"sv, DnCallback{startLine, textWidget}))));

        TextBoxBody textBoxBody;

public:
        static constexpr auto focusableWidgetCount = TextBoxBody::focusableWidgetCount;

        template <typename Wtu> using Decorator = typename TextBoxBody::template Decorator<Wtu>;
        using DecoratorType = typename TextBoxBody::DecoratorType;
};

/****************************************************************************/

// template <Dimension widthV, Dimension heightV, typename Buffer>
// requires c::text_buffer<std::remove_reference_t<Buffer>>
// class TextBox : public Focusable {
// public:
//         static constexpr Dimension height = heightV;
//         static constexpr Dimension width = widthV;

//         explicit TextBox (Buffer const &buf) : buf_{buf} {}
//         TextBox (TextBox const &bbb) : buf_{bbb.buf_} {}

//         auto &widgets () { return textBoxBody.widgets (); }
//         auto const &widgets () const { return textBoxBody.widgets (); }

// private:
//         Buffer buf_; // T or T&
//         LineOffset startLine{};

//         using Text = decltype (text<width - 1, height> (std::ref (buf_)));
//         Text textWidget = text<width - 1, height> (std::ref (buf_));

//         /*--------------------------------------------------------------------------*/

//         using TextBoxBody = decltype (hbox (std::ref (textWidget), vbox<1> (button ("▲"sv, [] {}), vspace<height - 2>, button ("▲"sv, []
//         {})))); TextBoxBody textBoxBody = hbox (std::ref (textWidget), vbox<1> (button ("▲"sv, [] {}), vspace<height - 2>, button ("▲"sv, []
//         {})));

// public:
//         static constexpr auto focusableWidgetCount = TextBoxBody::focusableWidgetCount;

//         template <typename Wtu> using Decorator = typename TextBoxBody::template Decorator<Wtu>;
//         using DecoratorType = typename TextBoxBody::DecoratorType;
// };

template <Dimension widthV, Dimension heightV, typename Buffer> auto textBox (Buffer &&buff)
{
        return TextBox<widthV, heightV, std::remove_reference_t<Buffer>>{std::forward<Buffer> (buff)};
}

auto compositeTextWidget = window<0, 0, 18, 7> (vbox (std::ref (backButton), textBox<18, 6> (std::ref (buff))));

/*--------------------------------------------------------------------------*/

auto s = suite<Windows> (element (Windows::menu, std::ref (menu)), //
                         element (Windows::textReferences, std::ref (textReferences)),
                         element (Windows::compositeTextWidget, std::ref (compositeTextWidget))
                         //  element (Windows::dataReferencesCombo, std::ref (dataReferencesCombo)),
                         //  element (Windows::dataReferencesComboEnum, std::ref (dataReferencesComboEnum)),
                         //  element (Windows::dataReferencesRadio, std::ref (dataReferencesRadio))
);

} // namespace

og::detail::augment::IWindow &textWidget ()
{
        mySuite = &s;
        return s;
}