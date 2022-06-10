Modern C++ GUI library for small displays. Developed with microcontrollers in mind. Experimental phase.

![Demo](demo.gif)

* [Character based](https://en.wikipedia.org/wiki/Box-drawing_character), platform independent, [immediate mode](https://en.wikipedia.org/wiki/Immediate_mode_(computer_graphics)). Works as long as a character can be printed in x,y coordinates (for instance in [ncurses](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/index.html), [Zephyr RTOS cfb](https://docs.zephyrproject.org/latest/reference/display/index.html), etc).
* Cursor (x,y).
* Many screens at once. **????** **???????!!!!!!**
* Input handling hooks for easy integration.
* C++20, no dependencies.
* No dynamic allocation (apart of std::string used only for optional debug).

# Hooks
* print at cursor's possition with clipping (clips to the viewport area even if passed coordinates are **negative**).
* switch color (2 colors)
* move cursor

# FAQ
* How to add a margin? No automatic margins, just add a space.

# Tutorial
aaa

# TODO
* [x] vbox (hbox, hbox) works OK, but hbox (vbox, vbox) does not (at least not when the nesting structure is complicated).
* [x] screen wide/tall
* [x] current focus
* [ ] text wrapping widget (without scrolling up - only for showinng bigger chunkgs of text, like logs)
* [ ] scrolling container - as the above, but has a buffer (license, help message).
* [x] checkboxes
* [x] radiobutons (no state)
* [x] radiobutton group (still no state, but somehow manages the radios. Maybe integer?)
* [ ] menu / list (possibly implemented using vbox and radio-s without radio icon (as an option))
  * [ ] Demo - menu 
  * [ ] Navigation between screens - research how to do it the best from the API perspective.
* [x] combo - "action" key changes the value (works for numbers as well)
  * [ ] icon aka indicator aka animation (icon with states) - this can be implemented using the combo (with an option).
* [x] Button (with callback).
* [ ] std::string - like strings??? naah...
* [ ] callbacks and / or references - for all widgets that has input. 
* [x] bug: empty line after nested container 
* [x] A window. Like ncurses window (i.e. area). It has to have its own coordinate system and focusCounter. It should overlay what was previously displayed (easy and concise)
  * [x] Dialog
* [ ] Prepare for compile time optimization
  * [ ] Cmake target which tests the size with a tool. Like size or bloaty. Saves a list with statistics + commit hash. Maybe a commit hook?
    * [ ] Debug, release and -OS targets
  * [ ] Benchmark compilation size.
  * [ ] Benchmark cpu?
* [x] Console backend (with or even without input)
* [ ] Styling (external template class impacting various aspects of the output)
* [ ] AND / OR Widget parameters dictating the looks.
* [x] Implement a test widget which is more than 1 character tall.
  * [x] Test this widget in the vertical and horizontal layouts.
    * [ ] Widgets having height > 1 have to implement their own scrolling. I haven't anticipated that a widget can be half visible.
* [ ] vbox(); (no parameters) results in an error.
* [x] Write concepts for widgtes, layouts, widgteTuples, groups etc and use them instead of raw typename.
* [x] Describe how the code works in a comment on the top.
* [x] Make the functions std::ref aware
* [ ] Separate directory for tests, and static_assert tests.
* [ ] Composite widgets as a possibility (or implement the text widget with scroll buttons).
* [ ] hbox (vbox (), vbox()) was not ever tested. But it won't work anyway, because widgets have ho width. I could add tmpl. param `width` to the container widgets however.


Document:
* When you implemnt a custom widget, bu default it is not focusable. Inherit from og::Focusable to change it.
* Display has its own context, so you don;t have to use a window???
* Some (text() ?)functions behave like the std::make_pair does in a way that they strip out the reference wrappers.
* No window in a window should be possible. Just display two one after another.
* Glossary : 
  * container widget : layout, group & window. 
  * composite widget : widgets returned by special factory methods which was designed to compose few other widgets in a container (for instance a text widget with buttons to scroll the contents up and down.)

Layouts:
* Layer 1 requires height field, but width is optional. If no width is present in a widget, 0 is assumed
* Show (on a diagram) where the cursor ends up after drawing various configurations of layouts and widgets
* All widgets have their heights defined (either as a variable or calculated) and available at compile time. Width at the other hand, is trickier, as widgets don't have to provide it at compile time. For example it would be impossible for a label to know its with based on its run time contents.
  * As a result it is easy (easy as C++ meta-programing goes) to calculate layout's height (you sum all the contained widgets' heights) but not that easy to do the same with width. This is why you can explicitly specify the width of a layout. However it sums the contained children' widths but then gets the max of the sum and explicit value.