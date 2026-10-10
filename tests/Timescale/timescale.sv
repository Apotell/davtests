// `timescale (IEEE 1800-2023 Sec 3.14.2.1, Sec 22.7) and its place in the Sec 3.14.2.3
// precedence (rule b). No compilation-unit timeunit in this file.
// Expected (unit / precision, 0 = default):

`timescale 1ns / 10ps

module t_a;                             // -9 / -11    (Sec 3.14.2.1 example, module A)
endmodule

module t_b;                             // -9 / -11    (module B, same directive)
endmodule

module t_own;                           // -10 / -15   (own declaration beats `timescale)
  timeunit 100ps / 1fs;
endmodule

module t_prec_only;                     // -9 / -12    (unit from `timescale)
  timeprecision 1ps;
endmodule

module t_unit_only;                     // -8 / -11    (precision from `timescale)
  timeunit 10ns;
endmodule

module t_outer;                         // -6 / -9
  timeunit 1us / 1ns;
  module t_inner;                       // -6 / -9     (rule a beats rule b)
  endmodule
endmodule

module t_outer_none;                    // -9 / -11
  module t_inner_of_none;               // -9 / -11    (rule a, enclosing got it from b)
  endmodule
endmodule

interface t_if;                         // -9 / -11
endinterface

program t_prog;                         // -9 / -11
endprogram

package t_pkg;                          // -9 / -11
endpackage

`timescale 1ps / 1ps

module t_c;                             // -12 / -12   (module C: the second directive replaces the first)
endmodule

`timescale 10 us / 100 ns

module t_spaced;                        // -5 / -7     (Sec 22.7 example, spaces around the units)
endmodule

`resetall

module t_after_resetall;                // 0 / 0       (Sec 22.7: reset by `resetall -> default)
endmodule

`timescale 10 ns / 1 ns

module t_test;                          // -8 / -9     (Sec 22.7 example, module test)
endmodule
