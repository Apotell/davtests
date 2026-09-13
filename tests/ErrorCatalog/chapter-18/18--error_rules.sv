/*
:name: chapter18_error_rules
:description: IEEE 1800-2023 Clause 18 error scenarios
:tags: 18.4 18.5 18.5.1 18.5.3 18.5.4 18.5.7.1 18.5.9 18.5.11 18.5.13.1 18.7 18.11 18.17.1 18.17.4 18.17.5
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...
//
// Every top-level entity (class/module) below is uniquely named with an
// r<ROW>_ prefix, and every member it declares (constraint/function/rand
// variable/randsequence production, etc.) carries the same prefix, since
// this chapter is dominated by scenarios that all reuse the bare names
// "class C;", "constraint c;" and "rand int x;" in the original per-clause
// dut files -- unique prefixes throughout keep every scenario's own
// diagnostic (and this file's findError() lookups) from being confused
// with another scenario's same-named member.

// catalog row 608 | 18.4 | COMP
// The size and index values of an associative array are not randomizable;
// constraining an associative array's size (or index) is illegal.
class r608_C;
  rand int r608_aa [string];
  constraint r608_c { r608_aa.size == 4; } // ILLEGAL: associative array size is not randomizable
endclass

// catalog row 615 | 18.5 | PARSE
// dist expressions may not appear inside other expressions; a dist may
// only form a complete constraint expression.
class r615_C;
  rand int r615_x;
  constraint r615_c { (r615_x dist {1:=1, 2:=1}) && (r615_x > 0); } // ILLEGAL: dist nested in another expression
endclass

// catalog row 614 | 18.5 | PARSE
// Operators with side effects, such as ++ and --, are not allowed in
// constraint expressions.
class r614_C;
  rand int r614_x;
  int r614_y;
  constraint r614_c { r614_x == r614_y++; } // ILLEGAL: side-effect operator in a constraint
endclass

// catalog row 613 | 18.5 | COMP
// In a constraint_primary, parentheses are allowed only when the
// constraint_primary is an array built-in method such as size().
class r613_C;
  rand int r613_x, r613_y;
  constraint r613_c { solve r613_x() before r613_y; } // ILLEGAL: parentheses on a non-method constraint_primary
endclass

// catalog row 616 | 18.5.1 | COMP
// An external constraint block shall appear in the same scope as the
// corresponding class declaration and shall appear after the class
// declaration in that scope.
constraint r616_C::r616_proto1 { r616_x inside {1,2}; } // ILLEGAL: external constraint block precedes the class declaration
class r616_C;
  rand int r616_x;
  constraint r616_proto1;
endclass

// catalog row 630 | 18.5.3 | PARSE
// The default specification in a distribution shall always use the :/
// operator; it shall be an error if the operator is omitted or if := is
// used.
class r630_C;
  rand int r630_x;
  constraint r630_c { r630_x dist { [1:3] :/ 3, default := 1 }; } // ILLEGAL: default must use :/
endclass

// catalog row 633 | 18.5.3 | COMP
// A dist expression requires that the expression contain at least one rand
// variable.
class r633_C;
  int r633_s; // state variable, not rand
  constraint r633_c { r633_s dist { 1 := 1, 2 := 2 }; } // ILLEGAL: dist expression contains no rand variable
endclass

// catalog row 632 | 18.5.3 | COMP
// A dist operation shall not be applied to randc variables.
class r632_C;
  randc bit [3:0] r632_x;
  constraint r632_c { r632_x dist { 1 := 1, 2 := 2 }; } // ILLEGAL: dist applied to a randc variable
endclass

// catalog row 629 | 18.5.3 | COMP
// A dist_item whose value_range is a range of real values shall use the :/
// operator and shall specify a weight.
class r629_C;
  rand real r629_a;
  constraint r629_c { r629_a dist { [0.5:1.5] := 3 }; } // ILLEGAL: real range must use :/ with a weight
endclass

// catalog row 636 | 18.5.4 | COMP
// No randc variable shall appear in the group of a uniqueness constraint.
class r636_C;
  randc bit [3:0] r636_a;
  rand bit [3:0] r636_b;
  constraint r636_u { unique { r636_a, r636_b }; } // ILLEGAL: randc variable in a unique group
endclass

// catalog row 634 | 18.5.4 | COMP
// The range_list of a uniqueness_constraint shall contain only expressions
// that denote singular variables of integral or real type, or unpacked
// array variables (or slices) whose leaf element type is integral or real.
class r634_C;
  rand int r634_a, r634_b;
  constraint r634_u { unique { r634_a + r634_b, r634_a }; } // ILLEGAL: unique item is an expression, not a variable
endclass

// catalog row 637 | 18.5.7.1 | COMP
// It shall be an error to include a function call as an implicit variable
// declaration in the array identifier of a foreach iterative constraint.
class r637_C;
  rand int r637_A[];
  function int r637_f(); return 0; endfunction
  constraint r637_c { foreach ( r637_A[r637_f()] ) r637_A[0] > 0; } // ILLEGAL: function call as implicit loop variable declaration
endclass

// catalog row 640 | 18.5.9 | COMP
// Only rand random variables are allowed in a solve...before ordering
// constraint; nonrandom (state) variables are not allowed.
class r640_C;
  rand int r640_x;
  int r640_y; // not rand
  constraint r640_o { solve r640_x before r640_y; } // ILLEGAL: y is not a random variable
endclass

// catalog row 641 | 18.5.9 | COMP
// randc variables are not allowed in a solve...before ordering constraint
// (randc variables are always solved first).
class r641_C;
  randc bit [3:0] r641_x;
  rand bit [3:0] r641_y;
  constraint r641_o { solve r641_x before r641_y; } // ILLEGAL: randc variable in an ordering constraint
endclass

// catalog row 645 | 18.5.11 | LINT
// Functions that appear in constraint expressions shall not have output,
// inout, or ref arguments (const ref is allowed).
class r645_C;
  rand int r645_x;
  function int r645_f(output int r645_o); r645_o = 1; return 0; endfunction
  int r645_t;
  constraint r645_c { r645_x == r645_f(r645_t); } // ILLEGAL: function in constraint has an output argument
endclass

// catalog row 647 | 18.5.11 | LINT
// Functions called from constraints cannot modify the constraints, e.g.,
// by calling the rand_mode() or constraint_mode() methods.
class r647_C;
  rand int r647_x;
  constraint r647_d { r647_x > 0; }
  function int r647_f(); this.r647_d.constraint_mode(0); return 1; endfunction // ILLEGAL: modifies constraints
  constraint r647_c { r647_x == r647_f(); }
endclass

// catalog row 646 | 18.5.11 | LINT
// Functions that appear in constraint expressions shall be automatic (or
// preserve no state information) and have no side effects.
class r646_C;
  rand int r646_x;
  function int r646_f(); static int r646_cnt; r646_cnt++; return r646_cnt; endfunction // ILLEGAL: static state / side effect
  constraint r646_c { r646_x == r646_f(); }
endclass

// catalog row 650 | 18.5.13.1 | COMP
// Soft constraints can only be specified on rand random variables; they may
// not be specified for randc variables.
class r650_C;
  randc bit [3:0] r650_x;
  constraint r650_c { soft r650_x == 3; } // ILLEGAL: soft constraint on a randc variable
endclass

// catalog row 652 | 18.7 | COMP
// In a randomize_call that is not a method call on an object of class type
// (a scope randomize), the optional parenthesized identifier_list after
// the keyword with shall be illegal, and the use of null as an argument
// shall be illegal.
module r652_m;
  int r652_a, r652_b;
  initial begin
    void'(std::randomize(r652_a) with (r652_a) { r652_a < r652_b; }); // ILLEGAL: identifier_list after with in a scope randomize
    void'(std::randomize(null));                                     // ILLEGAL: null in a scope randomize
  end
endmodule

// catalog row 659 | 18.11 | COMP
// Arguments to the randomize() method are limited to the names of
// properties of the calling object; expressions are not allowed as
// arguments.
class r659_CA;
  rand byte r659_x, r659_y;
endclass
module r659_m;
  initial begin
    r659_CA r659_a = new;
    void'(r659_a.randomize(r659_x + r659_y)); // ILLEGAL: expression as a randomize() argument
  end
endmodule

// catalog row 663 | 18.17.1 | COMP
// An rs_weight_specification shall evaluate to an integral non-negative
// value.
module r663_m;
  initial begin
    randsequence( r663_first )
      r663_first : r663_add := -3 // ILLEGAL: production weight must be a non-negative integral value
                 | r663_dec := 1 ;
      r663_add : { ; } ;
      r663_dec : { ; } ;
    endsequence
  end
endmodule

// catalog row 665 | 18.17.4 | COMP
// The repeat expression of a repeat production statement shall evaluate to
// a non-negative integral value.
module r665_m;
  initial begin
    randsequence( r665_P )
      r665_P : repeat (-1) r665_PUSH ; // ILLEGAL: repeat count must be a non-negative integral value
      r665_PUSH : { ; } ;
    endsequence
  end
endmodule

// catalog row 666 | 18.17.5 | COMP
// The optional expression following the rand join keywords shall be a real
// number in the range 0.0 to 1.0.
module r666_m;
  initial begin
    randsequence( r666_TOP )
      r666_TOP : rand join (1.5) r666_S1 r666_S2 ; // ILLEGAL: rand join weight outside 0.0..1.0
      r666_S1 : { ; } ;
      r666_S2 : { ; } ;
    endsequence
  end
endmodule
