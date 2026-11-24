/*
:name: chapter23_error_rules
:description: IEEE 1800-2023 Clause 23 error scenarios
:tags: 23.2.2 23.3.3.2 23.3.3.4 23.10.1
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...
//
// No scenario here raises a syntax error (all are COMP/LINT semantic checks;
// the last, pinit_m, is not a catalog row), so all of them are observed by a
// single compilation and no sibling fixture is needed.

// catalog row 862 | 23.3.3.2 | LINT
// A ref port shall be connected to an equivalent variable data type and
// cannot be left unconnected.
module r862_sub (ref int r); endmodule
module r862_top;
  r862_sub u (); // illegal: a ref port cannot be left unconnected
endmodule

// catalog row 864 | 23.3.3.4 | LINT
// An interface port shall always be connected to an interface instance or a
// higher level interface port; an interface port cannot be left unconnected.
interface r864_I; logic a; endinterface
module r864_sub (r864_I i); endmodule
module r864_top;
  r864_sub u (); // illegal: interface port left unconnected
endmodule

// catalog row 881 | 23.10.1 | LINT
// Parameters referenced on the right-hand side of a defparam shall be
// declared in the same module as the defparam statement.
module r881_other; parameter q = 4; endmodule
module r881_sub; parameter p = 1; endmodule
module r881_top;
  r881_other o ();
  r881_sub u ();
  defparam u.p = o.q; // illegal: referenced parameter is not declared in the module containing the defparam
endmodule

// 23.2.2, Syntax 23-4 footnote 2 | COMP (not a catalog row)
// It shall be illegal to initialize a port that is not a variable output port.
// An output port declared with a user-defined nettype is a net (23.2.2.3), so
// it cannot be initialized.
nettype real pinit_net;
module pinit_m(output pinit_net pinit_o = 1.0); // illegal: initializer on an output net
endmodule
