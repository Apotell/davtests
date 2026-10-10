// Compilation-unit timeunit/timeprecision, a `timescale, then `resetall.
//   IEEE 1800-2023 Sec 3.14.2.3 precedence, Sec 22.7 `timescale, Sec 22.3 `resetall
timeunit 10us;
timeprecision 1us;

module cu_before_timescale(); endmodule         // rule c: 10us / 1us

`timescale 1ns/1ps

module after_timescale(); endmodule             // rule b beats c: 1ns / 1ps

module own_timeunit();
  timeunit 100ps;                               // own unit, precision from `timescale
endmodule                                       // 100ps / 1ps

`resetall

module cu_after_resetall(); endmodule           // `timescale gone, back to rule c: 10us / 1us
