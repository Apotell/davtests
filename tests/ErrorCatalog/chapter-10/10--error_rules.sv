/*
:name: chapter10_error_rules
:description: IEEE 1800-2023 Clause 10 error scenarios
:tags: 10.2 10.3.2 10.6.1 10.6.2 10.9 10.10 10.10.1 10.10.3
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml, sheet "Error Catalog"). The "catalog row N"
// comment is the link back to that row; the matching gtest is named RowN_...
//
// All ten scenarios are COMP/LINT category -- none of them is a syntax
// error, so every one of them parses cleanly and all ten fit in this one
// shared fixture with no sibling _inv*.sv file needed (verified with
// hlc.exe: 0 SYNTAX, 0 FATAL).

// catalog row 290 | 10.2 | COMP
// The left-hand side of a continuous assignment shall be a net or variable,
// a CONSTANT bit-select or CONSTANT part-select of a vector net or packed
// variable, or a concatenation of those forms (Table 10-1); a non-constant
// select is illegal on the left-hand side of a continuous assignment.
module r290_m;
  wire [7:0] w;
  logic [2:0] idx;
  assign w[idx] = 1'b1;   // ILLEGAL: non-constant bit-select on LHS of continuous assignment
endmodule

// catalog row 298 | 10.3.2 | COMP
// A continuous assignment to an atomic net (a net of a user-defined
// nettype) shall not drive part of the net; the entire nettype value shall
// be driven, so the left-hand side shall not contain any indexing or select
// operations into the nettype's data type.
typedef logic [7:0] r298_T;
nettype r298_T r298_myNet;
module r298_m;
  r298_myNet n;
  assign n[0] = 1'b1;   // ILLEGAL: select into a user-defined nettype on LHS
endmodule

// catalog row 302 | 10.6.1 | COMP
// The left-hand side of the assignment in an assign procedural continuous
// assignment statement shall be a singular variable reference or a
// concatenation of variables; it shall not be a bit-select or a
// part-select of a variable.
module r302_m;
  logic [7:0] q;
  initial assign q[3] = 1'b0;      // ILLEGAL: bit-select on LHS of procedural assign
  initial assign q[3:0] = 4'b0;    // ILLEGAL: part-select on LHS of procedural assign
endmodule

// catalog row 303 | 10.6.2 | COMP
// The left-hand side of a force or release shall be a singular variable, a
// net, a constant bit-select of a vector net, a constant part-select of a
// vector net, or a concatenation of these; it shall not be a bit-select or
// part-select of a variable or of a net with a user-defined nettype.
typedef logic [7:0] r303_T;
nettype r303_T r303_myNet;
module r303_m;
  logic [7:0] v;
  r303_myNet n;
  initial begin
    force v[0] = 1'b1;   // ILLEGAL: bit-select of a variable
    force n[0] = 1'b1;   // ILLEGAL: select of a user-defined nettype net
  end
endmodule

// catalog row 306 | 10.9 | COMP
// When an assignment pattern (or assignment pattern expression) is used as
// the left-hand side of an assignment-like context, the positional
// notation shall be required; keyed/named or default notation is illegal
// on the left-hand side.
module r306_m;
  typedef struct { int x; int y; } st;
  st s = '{1, 2};
  int a, b;
  initial '{x:a, y:b} = s;   // ILLEGAL: keyed notation on the left-hand side
endmodule

// catalog row 309 | 10.9 | COMP
// An assignment pattern expression shall not be used in a port expression
// in a module, interface, or program declaration.
module r309_m ( .p(int'{1}) );   // ILLEGAL: assignment pattern expression as a port expression
endmodule

// catalog row 317 | 10.10 | LINT
// The target of an unpacked array concatenation shall be an array whose
// slowest-varying dimension is an unpacked fixed-size, queue, or dynamic
// dimension; a target of any other type, including an associative array,
// shall be illegal.
module r317_m;
  int assoc[string];
  int a, b;
  initial assoc = {a, b};   // ILLEGAL: associative array target
endmodule

// catalog row 316 | 10.10 | COMP
// An unpacked array concatenation may appear only as the source expression
// in an assignment-like context and shall not appear in any other context.
module r316_m;
  int a, b;
  initial $display({a, b} == 0);  // ILLEGAL as an unpacked array concatenation outside
                                 // an assignment-like context
endmodule

// catalog row 322 | 10.10.1 | COMP
// Unpacked array concatenations forbid replication, defaulting, and
// explicit typing of the concatenation itself; replication syntax inside
// an unpacked array concatenation is illegal.
module r322_m;
  int A9[1:9];
  initial A9 = {9{1}};   // ILLEGAL: no replication in an unpacked array concatenation
endmodule

// catalog row 323 | 10.10.3 | COMP
// Because a complete unpacked array concatenation has no self-determined
// type, it shall be illegal for an unpacked array concatenation to appear
// as an item in another unpacked array concatenation.
module r323_m;
  typedef string T_SQ[$];
  T_SQ SQ;
  string S1, S2;
  initial SQ = {S1, {S2, S1}};  // ILLEGAL if the inner braces are an unpacked array
                                // concatenation nested in an outer one
endmodule
