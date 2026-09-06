/*
:name: chapter18_error_rules
:description: IEEE 1800-2023 Clause 18 (Constrained random value generation) error scenarios
:tags: 18.4 18.17.1
*/

// catalog row 608 | 18.4 | COMP
// The size and index values of an associative array are not randomizable;
// constraining an associative array's size (or index) is illegal.
class r608_c;
  rand int aa [string];
  constraint c { aa.size == 4; }
endclass

// catalog row 663 | 18.17.1 | COMP
// An rs_weight_specification shall evaluate to an integral non-negative value.
module r663_m;
  initial begin
    randsequence( first )
      first : add := -3
            | dec := 1 ;
      add : { ; } ;
      dec : { ; } ;
    endsequence
  end
endmodule
