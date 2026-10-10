// Every legal time value (IEEE 1800-2023 Sec 3.14 Table 3-1, Sec 22.7): magnitude
// 1, 10 or 100 with unit s, ms, us, ns, ps or fs, as a time unit and as a time
// precision. The model stores each as a power of ten of one second (1ns = -9).
// No `timescale and no compilation-unit timeunit in this file.

// Time unit: "timeunit <value> / 1fs" -> unit = <value>, precision = -15.
module u_unit_1s;       // 0 / -15
  timeunit 1s / 1fs;
endmodule
module u_unit_10s;      // 1 / -15
  timeunit 10s / 1fs;
endmodule
module u_unit_100s;     // 2 / -15
  timeunit 100s / 1fs;
endmodule
module u_unit_1ms;      // -3 / -15
  timeunit 1ms / 1fs;
endmodule
module u_unit_10ms;     // -2 / -15
  timeunit 10ms / 1fs;
endmodule
module u_unit_100ms;    // -1 / -15
  timeunit 100ms / 1fs;
endmodule
module u_unit_1us;      // -6 / -15
  timeunit 1us / 1fs;
endmodule
module u_unit_10us;     // -5 / -15
  timeunit 10us / 1fs;
endmodule
module u_unit_100us;    // -4 / -15
  timeunit 100us / 1fs;
endmodule
module u_unit_1ns;      // -9 / -15
  timeunit 1ns / 1fs;
endmodule
module u_unit_10ns;     // -8 / -15
  timeunit 10ns / 1fs;
endmodule
module u_unit_100ns;    // -7 / -15
  timeunit 100ns / 1fs;
endmodule
module u_unit_1ps;      // -12 / -15
  timeunit 1ps / 1fs;
endmodule
module u_unit_10ps;     // -11 / -15
  timeunit 10ps / 1fs;
endmodule
module u_unit_100ps;    // -10 / -15
  timeunit 100ps / 1fs;
endmodule
module u_unit_1fs;      // -15 / -15
  timeunit 1fs / 1fs;
endmodule
module u_unit_10fs;     // -14 / -15
  timeunit 10fs / 1fs;
endmodule
module u_unit_100fs;    // -13 / -15
  timeunit 100fs / 1fs;
endmodule

// Time precision: "timeunit 100s / <value>" -> unit = 2, precision = <value>.
module u_prec_1s;       // 2 / 0
  timeunit 100s / 1s;
endmodule
module u_prec_10s;      // 2 / 1
  timeunit 100s / 10s;
endmodule
module u_prec_100s;     // 2 / 2
  timeunit 100s / 100s;
endmodule
module u_prec_1ms;      // 2 / -3
  timeunit 100s / 1ms;
endmodule
module u_prec_10ms;     // 2 / -2
  timeunit 100s / 10ms;
endmodule
module u_prec_100ms;    // 2 / -1
  timeunit 100s / 100ms;
endmodule
module u_prec_1us;      // 2 / -6
  timeunit 100s / 1us;
endmodule
module u_prec_10us;     // 2 / -5
  timeunit 100s / 10us;
endmodule
module u_prec_100us;    // 2 / -4
  timeunit 100s / 100us;
endmodule
module u_prec_1ns;      // 2 / -9
  timeunit 100s / 1ns;
endmodule
module u_prec_10ns;     // 2 / -8
  timeunit 100s / 10ns;
endmodule
module u_prec_100ns;    // 2 / -7
  timeunit 100s / 100ns;
endmodule
module u_prec_1ps;      // 2 / -12
  timeunit 100s / 1ps;
endmodule
module u_prec_10ps;     // 2 / -11
  timeunit 100s / 10ps;
endmodule
module u_prec_100ps;    // 2 / -10
  timeunit 100s / 100ps;
endmodule
module u_prec_1fs;      // 2 / -15
  timeunit 100s / 1fs;
endmodule
module u_prec_10fs;     // 2 / -14
  timeunit 100s / 10fs;
endmodule
module u_prec_100fs;    // 2 / -13
  timeunit 100s / 100fs;
endmodule
