// Own timeunit / timeprecision declarations (IEEE 1800-2023 Sec 3.14.2.2).
// This file has no `timescale and no compilation-unit timeunit, so whatever a design
// element does not declare itself falls to Sec 3.14.2.3 rule (d), the default.
// Expected (unit / precision, 0 = default):

module d_unit_only;                     // -10 / 0
  timeunit 100ps;
endmodule

module d_unit_slash;                    // -10 / -14   (Sec 3.14.2.2 example E)
  timeunit 100ps / 10fs;
endmodule

module d_unit_then_prec;                // -10 / -14   (Sec 3.14.2.2 example D)
  timeunit 100ps;
  timeprecision 10fs;
endmodule

module d_prec_then_unit;                // -10 / -14   (either order is legal)
  timeprecision 10fs;
  timeunit 100ps;
endmodule

module d_prec_only;                     // 0 / -12
  timeprecision 1ps;
endmodule

module d_unit_equals_prec;              // -9 / -9     (precision may equal the unit)
  timeunit 1ns / 1ns;
endmodule

module d_repeat_match;                  // -9 / -12    (a matching repeat is legal)
  timeunit 1ns;
  timeprecision 1ps;
  logic x;
  timeunit 1ns;
  timeprecision 1ps;
endmodule

module d_none;                          // 0 / 0       (rule d)
endmodule

interface d_if;                         // -6 / -9
  timeunit 1us / 1ns;
endinterface

program d_prog;                         // -5 / -7
  timeunit 10us / 100ns;
endprogram

package d_pkg;                          // -4 / -6
  timeunit 100us / 1us;
endpackage
