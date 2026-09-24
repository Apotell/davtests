/*
:name: chapter23_error_rules
:description: IEEE 1800-2023 Clause 23 error scenarios
:tags: 23.2.2.1 23.3.3.2 23.3.3.4 23.10.1
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...
//
// None of the three scenarios here raise a syntax error (all are COMP/LINT
// semantic checks on otherwise well-formed designs), so all three are
// observed by a single compilation and no sibling fixture is needed.

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

// catalog row 835 | 23.2.2.1 | COMP
// It shall be illegal to specify signed for a port declared as an
// interconnect port.
module r835_m (r835_a);
  inout interconnect signed r835_a; // illegal: signed on an interconnect port
endmodule
