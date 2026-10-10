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

module e_step;                          // Sec 3.14.3: step cannot set the time unit
  timeunit 1step;
endmodule

`timescale 1ns / 1us

module e_after_coarse_timescale;        // Sec 22.7: precision longer than the unit
endmodule

`timescale 3ns / 1ps

module e_after_bad_magnitude;           // Sec 22.7: valid integers are 1, 10 and 100
endmodule
