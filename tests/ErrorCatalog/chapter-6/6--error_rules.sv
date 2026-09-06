/*
:name: chapter6_error_rules
:description: IEEE 1800-2023 Clause 6 error scenarios that parse cleanly
:tags: 6.5 6.6.7 6.6.8 6.7.1 6.12 6.14 6.18 6.19 6.20.5 6.20.6 6.20.7 6.21 6.24.1 6.24.3
*/

// Every scenario below is derived from one row of the SV error catalog
// (filtered set). The "catalog row N" comment is the link back to that row;
// the matching gtest is named RowN_...
//
// This file holds only the scenarios that parse without a syntax error, so a
// single compilation observes all of them at once. Rows 13, 18 and 25 raise
// syntax errors and live in the sibling _inv*.sv fixtures instead -- see
// those files for why each is kept separate.

// catalog row 8 | 6.5 | COMP
// A continuous assignment is implied when a variable is connected to an
// input port declaration; this makes any assignment to a variable declared
// as an input port illegal.
module r8_m(input var logic r8_d);
  initial r8_d = 1'b1; // illegal - assignment to input port variable
endmodule

// catalog row 9 | 6.6.7 | COMP
// The data type of a user-defined nettype is restricted to a 4-state
// integral type, a 2-state integral type with 2-state members, a
// real/shortreal type, or a fixed-size unpacked array/struct/union
// recursively composed of these; any other data type (e.g. a string, class,
// dynamic array, or queue) is illegal.
nettype string r9_badnet; // illegal - string is not a valid nettype data type

// catalog row 10 | 6.6.7 | COMP
// A user-defined resolution function for a nettype with data type T shall be
// a function whose return type is T and whose single input argument is a
// dynamic array of elements of type T.
typedef struct { real f; } r10_t;
function automatic int r10_bad (input r10_t d[]); return 0; endfunction
nettype r10_t r10_wt with r10_bad; // illegal - return type is not T

// catalog row 11 | 6.6.8 | COMP
// An interconnect net or port shall not be used in any procedural context,
// nor in any continuous or procedural continuous assignment.
module r11_m;
  interconnect r11_w1;
  assign r11_w1 = 1;            // illegal - continuous assignment
  initial $display(r11_w1);     // illegal - procedural context
endmodule

// catalog row 12 | 6.7.1 | COMP
// A valid data type for a net with a built-in net type shall be a 4-state
// integral type or a fixed-size unpacked array/structure/union recursively
// composed of valid net data types; 2-state and real data types are not
// valid net data types.
module r12_m;
  wire bit r12_b;   // illegal - 2-state data type for a net
  wire real r12_r;  // illegal - real data type for a built-in net type
endmodule

// catalog row 13 | 6.9.2 | COMP
// If a net is declared with the vectored keyword, an implementation may
// (optionally) disallow bit-selects, part-selects, and strength
// specifications on that net.
//
// COMP_ILLEGAL_VECTORED_SELECT (WARNING, not ERROR): 6.9.2 says an
// implementation MAY disallow the select, not that it shall -- this is an
// optional restriction, not a violation, so a tool that permits it (as HLC
// currently does) is equally standard-conformant. The code exists only so
// the option has somewhere to report through if HLC ever chooses to enforce
// it; its %s names the select expression plus its enclosing design element.
//
// Kept in its own fixture: the catalog's own source used "logic vectored
// [15:0] a" (a variable, not a net), which HLC's grammar rejects outright --
// 'vectored' is a net_declaration-only keyword (IEEE 1800-2023 Annex A.2.1.3,
// net_declaration ::= ... [ vectored | scalared ] ...; it does not appear in
// the data_declaration production a 'logic' variable uses), so that spelling
// never reaches this row's rule at all. Rewritten below as a proper net
// declaration ("wire vectored") so the bit-select is the only thing under
// test.
module r13_m;
  wire vectored [15:0] r13_a;
  assign r13_a[1] = 1; // optional - an implementation MAY disallow this bit-select of a vectored net (6.9.2)
endmodule

// catalog row 14 | 6.12 | COMP
// Edge event controls (posedge, negedge, edge) shall not be applied to real
// variables.
// Reference of existing case is already in
// tests/Google/chapter-6/6.12--real_edge.sv.
module r14_m;
  real r14_r;
  always @(posedge r14_r) ; // illegal - edge event control on a real variable
endmodule

// catalog row 15 | 6.12 | COMP
// Real index expressions of bit-selects or part-selects of vectors are
// prohibited.
// Reference of existing case is already in
// tests/Google/chapter-6/6.12--real_bit_select_idx.sv.
module r15_m;
  logic [7:0] r15_v;
  real r15_idx = 1.0;
  logic r15_b;
  initial r15_b = r15_v[r15_idx]; // illegal - real index expression in a bit-select
endmodule

// catalog row 16 | 6.14 | COMP
// Ports shall not have the chandle data type.
module r16_m(input chandle r16_h); // illegal - chandle used as a port type
endmodule

// catalog row 17 | 6.14 | COMP
// Only equality/inequality and case equality/inequality against another
// chandle or null are valid operators on chandle variables; any other
// operator or expression use is illegal.
module r17_m;
  chandle r17_h1, r17_h2;
  int r17_i;
  initial r17_i = (r17_h1 < r17_h2); // illegal - relational operator on chandles
endmodule

// catalog row 18 | 6.18 | COMP
// Hierarchical references to type identifiers shall not be allowed
// (interface-port-based typedefs are not hierarchical references and are
// permitted).
//
// Kept in its own fixture: HLC's grammar does not accept an instance-prefixed
// name ("u.data_t") in a data_type position at all -- the IEEE 1800-2023
// Annex A.2.2.1 data_type grammar only allows a ps_type_identifier
// ([package_scope] type_identifier), not an arbitrary hierarchical prefix, so
// this is a PARSE-level rejection rather than a semantic one. See Row18.
module r18_sub; typedef int r18_data_t; endmodule
module r18_top;
  r18_sub u();
  u.r18_data_t v; // illegal - hierarchical reference to a type identifier
endmodule

// catalog row 19 | 6.18 | COMP
// It shall be an error if a basic data type was specified by a forward type
// declaration and the actual type definition does not conform to the
// specified basic data type.
module r19_m;
  typedef enum r19_e_t;
  typedef struct { int a; } r19_e_t; // illegal - actual type is not an enum
endmodule

// catalog row 20 | 6.18 | COMP
// Use of the class scope resolution operator on a prefix that is an
// incomplete forward type or an interface-based typedef shall be restricted
// to typedef declarations, the type operator, and type parameter
// assignments; it shall be an error if the prefix does not resolve to a
// class.
//
// NOTE: HLC currently reports COMP_MULTIPLY_DEFINED_TYPEDEF at the "class
// r20_c" declaration below, treating the later full class definition as
// redeclaring the earlier forward "typedef r20_c;" -- a separate, unrelated
// defect from the rule this row tests (forward class typedefs are meant to
// be completed this way, not rejected). See Row20's skip message.
typedef r20_c;
module r20_m;
  r20_c::T x;             // illegal - C is an incomplete forward type used outside a typedef
  typedef r20_c::T r20_ct;   // legal
endmodule
class r20_c; typedef int T; endclass

// catalog row 21 | 6.19 | COMP
// The optional value of an enum named constant is an elaboration-time
// constant expression; hierarchical names and const variables are not
// allowed in it.
module r21_sub; parameter P = 1; endmodule
module r21_m;
  r21_sub u();
  const int r21_cv = 2;
  enum {r21_a = u.P, r21_b = r21_cv} r21_e; // illegal - hierarchical name and const variable
endmodule

// catalog row 22 | 6.20.5 | COMP
// parameter and localparam shall not be assigned a constant expression that
// includes any specify parameters.
module r22_m;
  specparam r22_dhold = 1.0;
  parameter r22_regsize = r22_dhold + 1.0; // illegal - specparam assigned to a parameter
endmodule

// catalog row 23 | 6.20.6 | COMP
// A constant declared with the const keyword acts like a variable that
// cannot be written; assigning to it (or to a const member of a const
// object) after its initialization is illegal.
module r23_m;
  const int r23_c = 5;
  initial r23_c = 6; // illegal - write to a const constant
endmodule

// catalog row 24 | 6.20.7 | COMP
// $ may be assigned only to a value parameter of a simple bit vector type,
// and a parameter to which $ is assigned may be used only where $ may be
// specified as a literal constant; $ parameters are not permitted in queue
// contexts.
module r24_m;
  parameter r24_r2 = $;
  int r24_q[$:r24_r2];    // illegal - $ parameter used in a queue context
  parameter real r24_rp = $; // illegal - $ assigned to a non-simple-bit-vector parameter
endmodule

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

// catalog row 26 | 6.21 | COMP
// Hierarchical references shall not be used to access variables declared in
// unnamed blocks by name.
module r26_sub;
  initial begin
    int r26_v;
  end
endmodule
module r26_top;
  r26_sub u();
  initial u.r26_v = 1; // illegal - hierarchical reference into an unnamed block
endmodule

// catalog row 27 | 6.21 | COMP
// An explicit static or automatic keyword shall be required when an
// initialization value is specified as part of the declaration of a
// variable in a context where the variable could legally be declared
// automatic.
module r27_m;
  initial begin
    int r27_svar2 = 2; // illegal - static/automatic needed to show intent
  end
endmodule

// catalog row 28 | 6.21 | COMP
// Automatic variables and elements of dynamically sized array variables
// shall not be written with nonblocking, continuous, or procedural
// continuous assignments.
module r28_m;
  int r28_dyn[];
  assign r28_dyn[0] = 1; // illegal - continuous assignment to a dynamic array element
  task automatic r28_t(); int r28_a; r28_a <= 1; endtask // illegal - nonblocking write to automatic
endmodule

// catalog row 29 | 6.21 | COMP
// References to automatic variables and to elements or members of dynamic
// variables shall be limited to procedural blocks.
module r29_m;
  int r29_dyn[];
  wire r29_w = r29_dyn[0]; // illegal - dynamic variable element referenced outside a procedural block
endmodule

// catalog row 30 | 6.24.1 | COMP
// The expression inside a cast shall be an integral value when the cast
// changes the size or the signing.
module r30_m;
  real r30_r;
  logic [7:0] r30_v;
  initial r30_v = 8'(r30_r); // illegal - non-integral expression in a size cast
endmodule

// catalog row 31 | 6.24.3 | COMP
// An associative array type or a class shall be illegal as the destination
// type of a bit-stream cast.
module r31_m;
  typedef int r31_aa_t[string];
  int r31_src;
  initial begin
    r31_aa_t r31_d = r31_aa_t'(r31_src); // illegal - associative array as bit-stream cast destination
  end
endmodule

// catalog row 32 | 6.24.3 | COMP
// If the source and destination of a bit-stream cast are both fixed-size
// types of different sizes and either type is unpacked, the cast shall
// generate a compile-time error; if either contains dynamically sized
// types, a size mismatch shall be an error as soon as it can be determined
// (compile time or run time).
module r32_m;
  struct {bit [7:0] a; shortint b;} r32_s;
  int r32_b = int'(r32_s);                     // illegal - 24-bit unpacked source to 32-bit int
endmodule
