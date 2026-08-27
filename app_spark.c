// https://linuxcommandlibrary.com/man/spark
//
// spark [1] [5] [22] [13] [53]
// echo "[1,2,3,4,5]" | spark
// spark < [data.txt]
// seq [100] | sort -R | head -20 | spark
// spark -h


// https://pypi.org/project/sparklines/

// horizontal
// - https://github.com/piotrmurach/tty-sparkline

// plotting:
// - braille lines - https://lyngvaer.no/log/graph-plotting-terminal-braille
// 

// density:
// - https://tplot.readthedocs.io/en/latest/






/*
  spark generates sparkline graphs from a list of
  numbers, rendering them as Unicode block characters
  (▁▂▃▄▅▆▇█) in the terminal. Values are mapped
  proportionally across eight height levels, with
  the minimum value getting the shortest bar and
  the maximum the tallest. Numbers can be provided
  as command-line arguments, piped via stdin, or
  read from a file. Input supports comma-separated,
  space-separated, and newline-separated formats,
  making it easy to integrate with other Unix tools
  through pipes. The output is plain text using
  standard Unicode characters, so it works in any
  terminal that supports Unicode. Sparklines are
  useful for visualizing trends in data at a glance
  without requiring a full graphing tool.
*/
         
/*
$ sparklines -h
usage: sparklines [-h] [-V] [-d] [-m MIN] [-M MAX]
                  [-e STRING] [-n NUMBER]
                  [--zero {up,none}] [-w PERIOD]                            [VALUE ...]
                                                          Sparklines on the command-line, e.g. ▃▁▄▁▄█▂▅ for 3 1 4
1 5 9 2 6. Please add bug reports and suggestions to
https://github.com/deeplook/sparklines/issues.

positional arguments:
  VALUE                 A positive numeric value >= 0,
                        e.g. 0, 3.14, 2e2. Negative
                        numbers are supported. The
                        string values null and None (in
                        any spelling) represent empty
                        slots, but not the value 0!

options:
  -h, --help            show this help message and exit
  -V, --version         Display version number and quit.
  -d, --demo            Show a few usage examples for
                        given (mandatory) input values.
                        All other options are ignored.
  -m, --min MIN         Use this value as the minimum
                        for scaling.
  -M, --max MAX         Use this value as the maximum
                        for scaling.                        -e, --emphasize STRING
                        Emphasize bars by value (e.g.
                        "green:gt:5.0") or by index                               using a Python slice (e.g.
                        "red:[0:3]", "blue:[::2]",                                "yellow:[-1:]"). This option
                        takes one argument value, but                             can be given repeatedly. Works
                        only when optional dependency
                        "termcolor" is met (which is
                        True here). Otherwise has no
                        effect.
  -n, --num-lines NUMBER
                        rows per sparkline: integer,
                        'auto', or up:down (e.g. 4:4).
                        Default: 1.
  --zero {up,none}      0 handling: 'up' = positive
                        baseline (default); 'none' =
                        gap.
  -w, --wrap PERIOD     Wrap the graph to a new line
                        after PERIOD data points. This                            is useful for data with natural
                        periodicity: for example daily                            or weekly.

*/
