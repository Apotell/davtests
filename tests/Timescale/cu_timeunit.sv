// Compilation-unit timeunit / timeprecision (IEEE 1800-2023 Sec 3.14.2.3 rule c).
// "The time unit of the compilation-unit scope can only be set by a timeunit
// declaration, not a `timescale directive." This file is its own compilation unit
// (-fileunit), so the declarations below are its compilation-unit scope.
// Expected (unit / precision):

timeunit 1us;                           // SourceFile: -6 / -9
timeprecision 1ns;

module c_none;                          // -6 / -9     (rule c)
endmodule

module c_unit_only;                     // -8 / -9     (precision from rule c)
  timeunit 10ns;
endmodule

module c_prec_only;                     // -6 / -12    (unit from rule c)
  timeprecision 1ps;
endmodule

module c_outer;                         // -7 / -8
  timeunit 100ns / 10ns;
  module c_inner;                       // -7 / -8     (rule a beats rule c)
  endmodule
endmodule

interface c_if;                         // -6 / -9
endinterface

program c_prog;                         // -6 / -9
endprogram

package c_pkg;                          // -6 / -9
endpackage

`timescale 1ps / 1fs

module c_after_timescale;               // -12 / -15   (rule b beats rule c)
endmodule

`resetall

module c_after_resetall;                // -6 / -9     (`resetall resets directives only, rule c again)
endmodule
