/*
:name: chapter3_error_rules_inv3
:description: IEEE 1800-2023 3.14.2.3 -- mixed timescale / no-timescale design elements
:tags: 3.14.2.3
:should_fail_because: some design elements have a time unit and precision while others do not
*/

// catalog row 12 | 3.14.2.3 | LINT
// It shall be an error if some design elements in the design have a time unit
// and precision specified and others do not.
//
// Kept in its own fixture because `timescale / `resetall are file-scoped
// directives: placing them in the shared fixture would silently retime every
// other scenario in that file.
//
// r12_a picks up the `timescale; `resetall clears it again, so r12_b has none.

`timescale 1ns/1ps
module r12_a;
endmodule

`resetall
module r12_b;
endmodule
