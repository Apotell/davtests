/*
:name: chapter11_error_rules
:description: IEEE 1800-2023 Clause 11 (Operators and expressions) error scenarios
:tags: 11.9 11.12
*/

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
  let f(real x) = x > 0.0;
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
