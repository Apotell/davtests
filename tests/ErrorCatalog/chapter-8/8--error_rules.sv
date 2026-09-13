/*
:name: chapter8_error_rules
:description: IEEE 1800-2023 Clause 8 error scenarios
:tags: 8.10 8.11 8.23 8.24
*/

// Every scenario below is derived from one row of the SV error catalog
// (docs/error_catalog.xml). The "catalog row N" comment is the link back to
// that row; the matching gtest is named RowN_...
//
// All four scenarios parse cleanly (each is a COMP-category violation, not a
// syntax error), so one compilation observes all of them at once; no
// sibling _invN.sv fixtures are needed for this chapter.

// catalog row 201 | 8.10 | COMP
// A static method has no access to non-static members. Access to non-static
// class properties or methods, or to the special this handle, within the
// body of a static method is illegal.
class r201_C;
  int nonstat;
  static function void f();
    nonstat = 1;       // ILLEGAL: non-static property in a static method
    this.nonstat = 2;  // ILLEGAL: this inside a static method
  endfunction
endclass

// catalog row 203 | 8.11 | COMP
// The this keyword shall only be used as type(this) or within non-static
// class methods, constraints, inlined constraint methods, or covergroups
// embedded within classes; otherwise an error shall be issued.
module r203_m;
  int x;
  initial x = this.x;  // ILLEGAL: this used outside a class context
endmodule

// catalog row 236 | 8.23 | COMP
// A nested class shall not have implicit access to non-static properties
// and methods of the containing class; an unqualified reference to a
// non-static outer class member from a nested class is illegal (there is no
// implicit this handle to the outer class).
class r236_Outer;
  int outerProp;
  class r236_Inner;
    function void innerMethod();
      outerProp = 0;  // ILLEGAL: unqualified access to non-static outer member
    endfunction
  endclass
endclass

// catalog row 239 | 8.24 | COMP
// An out-of-block declaration shall be declared in the same scope as the
// class declaration and shall follow the class declaration.
function void r239_C::f();  // ILLEGAL: precedes the class declaration
endfunction
class r239_C;
  extern function void f();
endclass
