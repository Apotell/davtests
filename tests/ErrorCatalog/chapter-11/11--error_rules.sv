/*
:name: chapter11_error_rules
:description: IEEE 1800-2023 Clause 11 (Operators and expressions) error scenarios
:tags: 11.3.6 11.4.12 11.4.12.1 11.4.12.2 11.4.14 11.9 11.12
*/

// catalog row 333 | 11.3.6 | COMP
// It shall be illegal to include an assignment operator in an event
// expression, in an expression within a procedural continuous assignment,
// or in an expression that is not within a procedural statement.
module r333_m;
  int a, b;
  wire w;
  always @((a = b)) ;         // ILLEGAL: assignment operator in an event expression
  assign w = (a += 1);        // ILLEGAL: not within a procedural statement
  initial assign a = (b = 2); // ILLEGAL: inside a procedural continuous assignment
endmodule

// catalog row 338 | 11.4.12 | COMP
// A bit-select or part-select applied to a concatenation shall not be legal
// as a net_lvalue or variable_lvalue, i.e. it shall not appear on the
// left-hand side of an assignment.
module r338_m;
  byte a, b;
  initial {a, b}[3:0] = 4'h5; // ILLEGAL: select of a concatenation used as an lvalue
endmodule

// catalog row 340 | 11.4.12.1 | COMP
// An expression containing a replication shall not appear on the left-hand
// side of an assignment.
module r340_m;
  logic a;
  initial {4{a}} = 4'b1010; // ILLEGAL: replication as an assignment target
endmodule

// catalog row 341 | 11.4.12.1 | LINT
// An expression containing a replication shall not be connected to an
// output or inout port.
module r341_sub(output logic [3:0] o);
endmodule
module r341_m;
  logic a;
  r341_sub u1({4{a}}); // ILLEGAL: replication connected to an output port
endmodule

// catalog row 343 | 11.4.12.2 | COMP
// String concatenation is allowed only as an expression; it shall not
// appear on the left-hand side of an assignment.
module r343_m;
  string a, b;
  initial {a, b} = "hello"; // ILLEGAL: string concatenation as an assignment target
endmodule

// catalog row 346 | 11.4.14 | COMP
// It shall be an error to use a streaming_concatenation as an operand in an
// expression without first casting it to a bit-stream type.
module r346_m;
  int a, b;
  int r;
  initial r = {>>{a}} + b; // ILLEGAL: streaming concatenation used directly as an operand
endmodule

// catalog row 362 | 11.9 | COMP
// The type of a tagged union expression shall be known from its context
// (assignment target, cast, or enclosing expression); a context-free tagged
// union expression is illegal.
typedef union tagged { void Invalid; int Valid; } r362_vint;
module r362_m;
  initial $display(tagged Valid (1));
endmodule

// catalog row 363 | 11.9 | COMP
// The only member names allowed after the tagged keyword are the member names
// of the tagged union type of the expression.
typedef union tagged { void Invalid; int Valid; } r363_vint;
module r363_m;
  r363_vint v;
  initial v = tagged Bogus (1);
endmodule

// catalog row 368 | 11.12 | COMP
// If a formal argument of a let is typed, the type shall be event or one of
// the types allowed in 16.6; other formal types are illegal.
module r368_m;
  let f(chandle x) = 1;
endmodule

// catalog row 369 | 11.12 | COMP
// If a let formal argument is of type event, the actual argument shall be
// an event_expression and every reference to that formal shall appear
// where an event_expression may be written.
module r369_m;
  logic clk, a;
  int x;
  let r369_e(event ev) = x + 1; // ILLEGAL: event formal referenced where an event_expression may not be written
  initial x = r369_e(posedge clk);
endmodule

// catalog row 373 | 11.12 | COMP
// Recursive let instantiations are not permitted.
module r373_m;
  logic a;
  let r(x) = r(x) || x;
endmodule

// catalog row 375 | 11.12 | COMP
// A let may be declared only in a module, interface, program, checker,
// clocking block, package, compilation-unit scope, generate block,
// sequential/parallel block, or subroutine; a let declaration elsewhere (e.g.
// inside a class body) is illegal.
//
// Kept last in this file: a let is not a legal class_item, so the parser may
// not recover cleanly from it.
class r375_c;
  let f(x) = x + 1;
endclass
