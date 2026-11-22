// A `timescale written after a design element (IEEE 1800-2023 Sec 3.14.2.1, Sec 22.7).
// No compilation-unit timeunit in this file.
// Expected (unit / precision, 0 = default):

module t_before_timescale;              // 0 / 0       (the `timescale below does not reach it, rule d)
endmodule

`timescale 1ns / 1ps

module t_after_timescale;               // -9 / -12    (rule b)
endmodule
