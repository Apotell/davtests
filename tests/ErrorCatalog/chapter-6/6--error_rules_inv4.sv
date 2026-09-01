/*
:name: chapter6_error_rules_inv4
:description: IEEE 1800-2023 6.21 -- statement precedes its variable's declaration
:tags: 6.21
:should_fail_because: a variable declaration must precede any statement referencing it
*/

// catalog row 25 | 6.21 | COMP
// A variable declaration shall precede any simple (non-hierarchical)
// reference to that variable, and variable declarations shall precede any
// statements within a procedural block.
//
// Kept in its own fixture: HLC's grammar itself cannot parse a plain
// statement followed by a variable declaration inside the same procedural
// block ("no viable alternative at input 'int x'") -- once it has committed
// to the statement production it can no longer back into a declaration, so
// this is a PARSE-level rejection rather than a semantic one. See Row25.
module r25_m;
  initial begin
    r25_x = 1;   // illegal - statement precedes the declaration below
    int r25_x;
  end
endmodule
