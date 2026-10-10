// Illegal timeunit / timeprecision / `timescale usage. Each design element breaks
// exactly one rule. No compilation-unit timeunit in this file.

module e_coarse_decl;                   // Sec 3.14: precision may not be longer than the unit
  timeunit 1ns / 1us;
endmodule

module e_repeat_mismatch;               // Sec 3.14.2.2: a repeat shall match the previous declaration
  timeunit 1ns;
  logic x;
  timeunit 10ns;
endmodule

module e_not_first;                     // Sec 3.14.2.2: the declarations shall precede any other item
  logic x;
  timeunit 1ns;
endmodule

module e_step;                          // Annex A time_unit has no step; Sec 3.14.3: step cannot set the time unit
  timeunit 1step;
endmodule

module e_bad_magnitude;                 // Sec 3.14: the magnitude is 1, 10 or 100
  timeunit 3ns;
endmodule

module e_fixed_point;                   // Sec 3.14: the magnitude is 1, 10 or 100
  timeunit 1.5ns;
endmodule

module e_space_unit;                    // Annex A footnote 49: no white_space inside a time_literal
  timeunit 10 ns;
endmodule

module e_space_precision;               // Annex A footnote 49: no white_space inside a time_literal
  timeprecision 1 ps;
endmodule

module e_parent;                        // 1ns / default
  timeunit 1ns;
  module e_child_coarse;                // Sec 3.14: precision 1us is longer than the inherited 1ns unit
    timeprecision 1us;
  endmodule
endmodule

`timescale 1ns / 1us

module e_after_coarse_timescale;        // Sec 22.7: precision longer than the unit
endmodule

`timescale 3ns / 1ps

module e_after_bad_magnitude;           // Sec 22.7: valid integers are 1, 10 and 100
endmodule
