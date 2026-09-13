/*
:name: chapter12_error_rules
:description: IEEE 1800-2023 Clause 12 error scenarios
:tags: 12.7.3
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...

// catalog row 385 | 12.7.3 | COMP
// It shall be an error to include a function call as an implicit variable
// declaration in the foreach-loop array identifier.
module r385_m;
  function int get_arr();
  endfunction
  initial
    foreach (get_arr()[i]) ; // ILLEGAL: function call as the foreach array identifier
endmodule
