## SetIcon ##

This is a command line utility to set the icon for a file in os x. I use it to set icons for graphical programs that I usually run from the command line. This way, you still get a nice icon when you alt-tab etc, even without having started the program from a bundle.

### Installing ###

Running the ./INSTALL script will build the source and copy the resulting `seticon` executable to its parent directory, which for me is `~/bin`. If you want something different, change the script.

### Bugs ###

* Only works on OS X.
* Not much validation of arguments.

### License ###

Copyright © 2016 Elliot Pennington
Distributed under the Eclipse Public License either version 1.0 or (at your option) any later version.
