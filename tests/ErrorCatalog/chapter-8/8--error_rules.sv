/*
:name: chapter8_error_rules
:description: IEEE 1800-2023 Clause 8 (Classes) error scenarios
:tags: 8.7 8.10 8.11 8.15 8.17 8.21 8.24
*/

// Every scenario below is derived from one row of the SV error catalog
// (sv_error_catalog_Latest.xlsx, sheet "Error Catalog"). The "catalog row N"
// comment is the link back to that row; the matching gtest is named RowN_...
//
// One file per chapter: all Clause 8 scenarios are compiled together, so no
// test here may assert an error count -- only findError() on a specific type
// plus the object it names and/or its line:column.

// catalog row 198 | 8.7 | COMP
// The class constructor new is defined as a function with no return type and,
// like any other function, it shall be nonblocking; it may not contain
// blocking timing controls.
class r198_c;
  function new();
    #10;
  endfunction
endclass

// catalog row 201 | 8.10 | COMP
// A static method has no access to non-static members. Access to non-static
// class properties or methods, or to the special this handle, within the body
// of a static method is illegal.
class r201_c;
  int nonstat;
  static function void f();
    nonstat = 1;
    this.nonstat = 2;
  endfunction
endclass

// catalog row 203 | 8.11 | COMP
// The this keyword shall only be used as type(this) or within non-static class
// methods, constraints, inlined constraint methods, or covergroups embedded
// within classes; otherwise an error shall be issued.
module r203_m;
  int x;
  initial x = this.x;
endmodule

// catalog row 207 | 8.15 | COMP
// A super.new call shall be the first statement executed in the constructor;
// placing it after other statements is illegal.
class r207_a;
  function new(int a);
  endfunction
endclass
class r207_b extends r207_a;
  int x;
  function new();
    x = 1;
    super.new(5);
  endfunction
endclass

// catalog row 215 | 8.17 | COMP
// If the superclass constructor arguments are specified in the extends
// specifier, the subclass constructor shall not contain a super.new() call.
class r215_packet;
  function new(int a);
  endfunction
endclass
class r215_ether extends r215_packet(5);
  function new();
    super.new(5);
  endfunction
endclass

// catalog row 230 | 8.21 | COMP
// An object of an abstract (virtual) class shall not be constructed directly;
// its constructor may only be called indirectly through the chaining of
// constructor calls originating in an extended non-abstract object.
virtual class r230_base;
endclass
module r230_m;
  r230_base p;
  initial p = new;
endmodule

// catalog row 239 | 8.24 | COMP
// An out-of-block declaration shall be declared in the same scope as the class
// declaration and shall follow the class declaration.
//
// Kept last in this file: the out-of-block body names a class that does not
// exist yet at that point, so anything the parser loses while recovering is
// lost from the end of the file rather than from the middle.
function void r239_c::f();
endfunction
class r239_c;
  extern function void f();
endclass
