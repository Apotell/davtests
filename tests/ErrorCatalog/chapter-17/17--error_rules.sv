/*
:name: chapter17_error_rules
:description: IEEE 1800-2023 Clause 17 (Checkers) error scenarios
:tags: 17.2
*/

// catalog row 573 | 17.2 | COMP
// Modules, interfaces, and programs shall not be instantiated inside checkers.
module r573_sub;
endmodule

checker r573_c;
  r573_sub s1();
endchecker
