// No compilation-unit timeunit: after `resetall the default (tool-specific) applies.
//   IEEE 1800-2023 Sec 22.7
`timescale 1ns/1ps

module timescale_before_resetall(); endmodule   // 1ns / 1ps

`resetall

module default_after_resetall(); endmodule      // tool default: unset
