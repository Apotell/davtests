// Nested design elements (IEEE 1800-2023 Sec 3.14.2.3 rule a): a nested module or
// interface with no timeunit of its own inherits from the enclosing module or
// interface. No `timescale and no compilation-unit timeunit in this file.
// Expected (unit / precision, 0 = default):

module n_outer;                         // -8 / -12
  timeunit 10ns / 1ps;

  module n_inner_none;                  // -8 / -12    (both inherited)
  endmodule

  module n_inner_prec;                  // -8 / -13    (unit inherited)
    timeprecision 100fs;
  endmodule

  module n_inner_unit;                  // -6 / -12    (precision inherited)
    timeunit 1us;
  endmodule

  module n_mid;                         // -8 / -12
    module n_deep;                      // -8 / -12    (two levels down)
    endmodule
  endmodule

  module n_mid_own;                     // -7 / -8
    timeunit 100ns / 10ns;
    module n_deep_own;                  // -7 / -8     (nearest enclosing wins)
    endmodule
  endmodule

  interface n_inner_if;                 // -8 / -12    (interface nested in a module)
  endinterface
endmodule

interface n_outer_if;                   // -7 / -8
  timeunit 100ns / 10ns;

  interface n_nested_if;                // -7 / -8     (interface nested in an interface)
  endinterface
endinterface

module n_outer_none;                    // 0 / 0       (rule d)
  module n_inner_of_none;               // 0 / 0       (inherits the enclosing default)
  endmodule
endmodule
