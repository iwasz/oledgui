/****************************************************************************
 *                                                                          *
 *  Author : lukasz.iwaszkiewicz@gmail.com                                  *
 *  ~~~~~~~~                                                                *
 *  License : see COPYING file for details.                                 *
 *  ~~~~~~~~~                                                               *
 ****************************************************************************/

#include "inputApi.h"
#include "regression.h"

namespace {
using namespace og;
using namespace std::string_view_literals;

enum class Windows { menu, textReferences, compositeTextWidget };

ISuite<Windows> *mySuite{};

/*--------------------------------------------------------------------------*/

auto menu = window<0, 0, 18, 7> (vbox (button ("[back]"sv, mainMenu), //
        button ("Text widget"sv, [] {mySuite->current () = Windows::textReferences; }), //
        button ("Composite textWidget"sv, [] { mySuite->current () = Windows::compositeTextWidget; })/*,
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

template <Dimension widthV, Dimension heightV, typename Buffer>
requires c::text_buffer<std::remove_reference_t<Buffer>>
class TextBox : public Focusable {
public:
        static constexpr Dimension height = widthV;
        static constexpr Dimension width = heightV;

        explicit TextBox (Buffer const &buf) : buf_{buf} {}

        auto &widgets () { return textBoxBody.widgets (); }
        auto const &widgets () const { return textBoxBody.widgets (); }

private:
        Buffer buf_; // T or T&
        LineOffset startLine{};

        using Text = decltype (text<width - 1, height> (std::ref (buf_)));
        Text textWidget = text<width - 1, height> (std::ref (buf_));

        /*--------------------------------------------------------------------------*/

        auto upCallback ()
        {
                return [this] { startLine = textWidget.setStartLine (--startLine); };
        }

        struct Callback {
                Callback (LineOffset &startLine, Text &textWidget) : startLine{startLine}, textWidget{textWidget} {}

                void operator() () { startLine = textWidget.setStartLine (--startLine); }

                LineOffset &startLine;
                Text &textWidget;
        };

        Callback callback{startLine, textWidget};

        using ButtonUp = decltype (button ("▲"sv, callback));
        ButtonUp buttonUp = button ("▲"sv, callback);

        /*--------------------------------------------------------------------------*/

        using TextBoxBody = decltype (hbox (std::ref (txt), vbox<1> (std::ref (buttonUp), vspace<height - 2>, std::ref (buttonUp))));
        TextBoxBody textBoxBody = hbox (std::ref (txt), vbox<1> (std::ref (buttonUp), vspace<height - 2>, std::ref (buttonUp)));

public:
        static constexpr auto focusableWidgetCount = TextBoxBody::focusableWidgetCount;

        template <typename Wtu> using Decorator = typename TextBoxBody::template Decorator<Wtu>;
        using DecoratorType = typename TextBoxBody::DecoratorType;
};

// using UpButton = std::invoke_result_t<decltype (&TextBox<0, 0, std::string_view>::up), TextBox<0, 0, std::string_view> *>;
// UpButton upButton{up ()};

template <Dimension widthV, Dimension heightV, typename Buffer> auto textBox (Buffer &&buff)
{
        return TextBox<widthV, heightV, std::unwrap_ref_decay_t<Buffer>>{std::forward<Buffer> (buff)};
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