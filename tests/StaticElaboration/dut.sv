// StaticElaboration -- demonstrates the parameter-specialization ("static
// elaboration") gap this test file documents. See memory note
// project_static_elaboration_plan (hlc_01) for the full approved design.
//
// IEEE 1800-2023 Sec 6.20 (Parameter and constant declarations) / Sec 23.3
// (Module instantiation): two instances/uses of the same definition supplying
// different #(...) values must be bound as distinct, independently
// specialized definitions -- a reference to a parameter anywhere in the
// definition's own body (not just its own parameter port list) must resolve
// against the value supplied at THAT use, never the definition's own
// default. This file exercises all four parameterizable definition kinds
// (module, interface, program, class), plus the harder scoped-lookup cases:
// a class nested in a package, a class nested in another class, the SAME
// nested-class name reused under two different outer scopes (package or
// class), and a parameterized module instantiated from within a
// non-top-level module (nesting depth independence).

// ---- Module ----------------------------------------------------------------

module param_mod #(
    parameter int W = 8
);
  // Referenced by a plain variable declaration, not just the parameter's own
  // port list -- exercises the "a declaration anywhere in the body must see
  // an overridden parameter" requirement that ruled out a delta/overlay
  // specialization strategy in favor of a full clone (see
  // project_static_elaboration_plan).
  logic [W-1:0] internal_reg;
endmodule

// A parameterized module instantiated from within a non-top-level module --
// confirms specialization is independent of how deep in the hierarchy the
// instantiating module sits.
module mid_level;
  // Same override value as dut's own inst_wide (W=16) -- must dedupe to the
  // SAME specialization, even though it's requested from a different
  // enclosing scope.
  param_mod #(.W(16)) inst_nested_same_as_wide ();

  // A third, distinct override value -- its own, separate specialization.
  param_mod #(.W(24)) inst_nested_distinct ();
endmodule

// ---- Interface ---------------------------------------------------------------

interface param_if #(
    parameter int W = 4
);
  logic [W-1:0] data;
endinterface

// ---- Program -------------------------------------------------------------------

program param_prog #(
    parameter int W = 2
);
  int unused_var;
endprogram

// ---- Class -- plain (no package/nesting) --------------------------------------

class param_cls #(parameter int W = 3);
  int payload;
endclass

// ---- Class nested inside a package --------------------------------------------

package pkg_a;
  class PkgWidget #(parameter int W = 5);
    int value;
  endclass
endpackage

// A second package with a class of the SAME name as pkg_a's -- confirms
// scoped lookup disambiguates by package, not by a flat/global class name.
package pkg_b;
  class PkgWidget #(parameter int W = 50);
    int value;
  endclass
endpackage

// ---- Class nested inside another class ----------------------------------------

class OuterCls;
  class InnerCls #(parameter int W = 6);
    int value;
  endclass
endclass

// A second outer class with a nested class of the SAME name as OuterCls's --
// confirms scoped lookup disambiguates by outer class, not by a flat/global
// class name.
class OtherOuterCls;
  class InnerCls #(parameter int W = 60);
    int value;
  endclass
endclass

// ---- Deeply nested class chain (package -> class -> class -> class) -----------
//
// IEEE 1800-2023 places no cap on scope-resolution nesting depth --
// PkgA::ClassA::ClassB::ClassC is exactly as legal as PkgA::ClassA, each `::`
// descending one level into the previous segment's own nested class list. Each
// of the four segments below (pkg_deep, Level1, Level2, Level3) gets its own
// override at the use site, to confirm every segment of the chain -- not just
// the final target -- is independently specialized.

package pkg_deep;
  class Level1 #(parameter int W = 1);
    class Level2 #(parameter int W = 2);
      class Level3 #(parameter int W = 3);
        int payload;
      endclass
    endclass
  endclass
endpackage

// ---- Top ----------------------------------------------------------------------

module dut;
  // Module: default vs. overridden.
  param_mod inst_default ();
  param_mod #(.W(16)) inst_wide ();

  // Interface: default vs. overridden.
  param_if if_default ();
  param_if #(.W(8)) if_wide ();

  // Program: default vs. overridden.
  param_prog prog_default ();
  param_prog #(.W(4)) prog_wide ();

  // Class: default vs. overridden (positional override).
  param_cls cls_default_handle;
  param_cls #(9) cls_wide_handle;

  // Class nested in a package -- same nested class name, two different
  // packages.
  pkg_a::PkgWidget #(10) pkg_a_handle;
  pkg_b::PkgWidget #(20) pkg_b_handle;

  // Class nested in another class -- same nested class name, two different
  // outer classes.
  OuterCls::InnerCls #(12) outer_inner_handle;
  OtherOuterCls::InnerCls #(24) other_outer_inner_handle;

  // Deep scope chain, four segments (pkg_deep, Level1, Level2, Level3), each
  // individually overridden -- confirms Phase3 walks and specializes every
  // level of an arbitrarily long chain, not just a single Scope::Target pair.
  pkg_deep::Level1#(11)::Level2#(22)::Level3#(33) deep_handle;

  // Module within module -- a parameterized module instantiated from a
  // non-top-level module.
  mid_level mid_inst ();
endmodule
