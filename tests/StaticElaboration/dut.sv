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

// ---- Typedefs in scope resolution ---------------------------------------------
//
// IEEE 1800-2023 Sec 6.18/26.3 make a package's own typedefs equally reachable via `::` as its
// nested classes. A typedef never carries its own #(...) -- any override lives on (and is
// resolved through) the typedef's own underlying type instead, so pkg_alias_t below overrides
// param_cls's W directly on the aliased type, not on the typedef itself.

package pkg_td;
  typedef param_cls #(40) pkg_alias_t;
endpackage

// A typedef declared OUTSIDE any module/package/class -- file/compilation-unit ("$unit") scope.
// Parented directly under Design itself (Design is NOT a Scope, but still needs its own
// typedefs field for exactly this -- confirmed via Phase2ModelBuilder::leavePA_Type_declaration's
// own getModelOnStack<hldb::Scope, hldb::Design>() call). Referenced by a bare name from deep
// inside dut below -- exercises findTypedefInEnclosingScopes() walking all the way out to Design.
typedef param_cls #(96) unit_scope_alias_t;

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

  // Two more, INDEPENDENT references to the exact same overrides deep_handle above already
  // uses (Level1#(11), and Level1#(11)::Level2#(22)) -- regression coverage for a real
  // duplicate-specialization bug: a scoped chain's own segment RefTypespec (e.g. "Level1"
  // within deep_handle's own chain) is independently, wrongly reachable by
  // resolveUnsupportedTypespecs()'s own flat sweep too (Phase2 parents it directly under the
  // enclosing chain, same as any other RefTypespec), and if that sweep resolves it BEFORE
  // resolveScopedUnsupportedTypespec()'s own per-segment loop gets to it, the segment's own
  // override gets silently lost, falling back to the UNSPECIALIZED base -- corrupting every
  // later segment's own search scope (confirmed via -d db: Level2 ended up specialized TWICE,
  // once correctly nested under Level1's own specialization, once wrongly nested under
  // Level1's own base). level1_alias_handle/level2_alias_handle below must dedupe to the exact
  // same Level1/Level2 specializations deep_handle's own chain already resolved to.
  pkg_deep::Level1#(11) level1_alias_handle;
  pkg_deep::Level1#(11)::Level2#(22) level2_alias_handle;

  // Typedef reached through a package scope (Task 10's scoped fallback --
  // resolveScopedUnsupportedTypespec()'s per-segment typedef lookup).
  pkg_td::pkg_alias_t pkg_alias_handle;

  // A bare, unqualified typedef -- no `::` at all -- declared directly in this
  // same scope. Exercises resolveUnsupportedTypespec()'s own flat-case typedef
  // fallback, which only searches the reference's own immediate enclosing
  // scope (not yet an outward walk through enclosing scopes -- see
  // project_static_elaboration_plan), so this is deliberately declared here
  // rather than at file scope.
  typedef param_cls #(41) bare_alias_t;
  bare_alias_t bare_alias_handle;

  // A typedef aliasing ANOTHER bare, unscoped typedef declared in this same
  // scope -- bare_alias_t itself is just another not-yet-resolved
  // RefTypespec/UnsupportedTypespec entry in the exact same flat sweep, so
  // this exercises resolveUnsupportedTypespecs()'s own fixed-point retry.
  typedef bare_alias_t chained_alias_t;
  chained_alias_t chained_alias_handle;

  // A typedef whose own underlying type is a SPECIALIZED class that itself
  // has a chain of further-parameterized nested classes -- TdOuterAlias
  // aliases TdOuter#(80); TdMid must be looked up (and re-specialized) within
  // THAT specialization's own nested class list, not TdOuter's base one; and
  // TdInner must in turn be looked up within TdMid's OWN specialization, not
  // TdOuter's base TdMid. Resolving TdOuterAlias::TdMid#(81)::TdInner#(82)
  // this way genuinely REQUIRES resolveUnsupportedTypespecs()'s fixed-point
  // retry: the segment-0 typedef fallback needs TdOuterAlias's own aliased
  // class (TdOuter#(80)) already resolved AND specialized, which only
  // happens once that OTHER, independent RefTypespec entry has itself been
  // visited by the same sweep -- a single pass would fail whenever the
  // sweep happens to visit this chain before that one.
  class TdOuter #(parameter int W = 70);
    // Declared directly on TdOuter -- referenced by a BARE name from within TdMid below (a
    // separate, nested scope) to exercise findTypedefInEnclosingScopes()'s own walk-up: a
    // class member function (or, as here, a nested class's own member declaration) can see a
    // typedef declared on an ENCLOSING class scope without re-declaring or qualifying it.
    typedef param_cls #(95) outer_scope_alias_t;
    class TdMid #(parameter int W = 71);
      // Bare reference to outer_scope_alias_t -- declared on TdOuter, NOT on TdMid itself.
      outer_scope_alias_t scope_walk_member;
      class TdInner #(parameter int W = 72);
        int payload;
      endclass
    endclass
  endclass
  typedef TdOuter#(80) TdOuterAlias;
  TdOuterAlias::TdMid#(81)::TdInner#(82) td_chain_handle;

  // Bare reference to unit_scope_alias_t -- declared outside any module/package/class, above --
  // exercises findTypedefInEnclosingScopes() walking all the way out to Design itself.
  unit_scope_alias_t unit_scope_handle;

  // Module within module -- a parameterized module instantiated from a
  // non-top-level module.
  mid_level mid_inst ();
endmodule
