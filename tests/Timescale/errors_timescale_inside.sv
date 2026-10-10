// IEEE 1800-2023 Sec 22.7: "It shall be illegal for the `timescale directive to be
// specified within a design element." Kept in its own file: the parser rejects the
// directive inside the module, which would hide any test that followed it.

module e_timescale_inside;
`timescale 1ns / 1ps
endmodule
