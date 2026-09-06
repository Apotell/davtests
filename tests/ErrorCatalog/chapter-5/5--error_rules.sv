/*
:name: chapter5_error_rules
:description: IEEE 1800-2023 Clause 5 error scenarios that parse cleanly
:tags: 5.10 5.13
*/

// Every scenario below is derived from one row of the SV error catalog
// (filtered set). The "catalog row N" comment is the link back to that row;
// the matching gtest is named RowN_...

// catalog row 5 | 5.10 | COMP
// A structure or array literal (assignment pattern) shall have a type,
// indicated either explicitly with a type prefix or implicitly by an
// assignment-like context; an untyped assignment pattern used where no such
// context exists is illegal.
module r5_m;
  initial $display('{0, 0.0});   // ILLEGAL: no type prefix and no assignment-like context (5.10)
endmodule

// catalog row 6 | 5.10 | COMP
// Nested braces in a structure literal shall reflect the structure; the
// C-like flattened alternative is not allowed.
typedef struct {int a; shortreal b;} r6_ab;
module r6_m;
  r6_ab r6_abarr[1:0] = '{1, 1.0, 2, 2.0};   // ILLEGAL: flattened C-style literal, braces must reflect structure (5.10)
endmodule

// catalog row 7 | 5.13 | COMP
// Empty parentheses on a subroutine call with no arguments are optional in
// general (5.13), but 13.5.5 carves out one specific exception: "It shall be
// illegal to omit the parentheses in a directly recursive nonvoid class
// function method call that is not hierarchically qualified." A bare
// reference to the function's own name is always its implicit return-value
// variable (13.4.1), never a call -- so recursion needs explicit "()" even
// with no arguments to pass.
//
// NOTE: an earlier version of this fixture used 'sequence.triggered' without
// parentheses (assert property (... s.triggered)), reasoning from 5.13's
// general "implicit variable" wording. That was a misattribution: 16.7.1 and
// every worked example in Clause 16 write .triggered WITHOUT parentheses as
// the normal, legal form (e.g. "e1.triggered", "wait (e4.triggered)") -- the
// 13.4.1 implicit-variable concept 5.13 cross-references is specifically a
// function's own return-value variable, not an SVA sequence method. Replaced
// with the actual 13.5.5 construct below.
class r7_c;
  function int r7_f(int n);
    if (n <= 1) r7_f = 1;
    else r7_f = n * r7_f;   // ILLEGAL: recursive call omits (), must be r7_f(n - 1) (5.13, 13.5.5)
  endfunction
endclass
