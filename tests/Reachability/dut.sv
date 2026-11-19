// Reachability -- which modules and programs are top-level (IEEE 1800-2023 Sec 23.3.1, 24.3),
// and what the design rooted at them uses: what it instantiates (including through bind and
// implicitly instantiated nested modules, Sec 23.4), the classes it uses as types (directly,
// through a typedef, an extends clause, a type parameter value), and the packages and classes
// owning what it calls. Everything else -- an unused class, an unused package, a module only an
// ignored nested module instantiates -- is compiled and bound, but not reachable.

package util_pkg;
  function automatic int helper();
    return 1;
  endfunction
  class PkgUnused;
  endclass
endpackage

package unused_pkg;
  function automatic int nobody();
    return 0;
  endfunction
endpackage

class Used;
endclass

class Unused;
endclass

class ViaTypedefBase;
endclass
typedef ViaTypedefBase via_typedef_t;

class ExtBase;
endclass

class ExtDerived extends ExtBase;
endclass

class TypeArg;
endclass

class Holder #(type T = int);
  T item;
endclass

class Statics;
  static function int get();
    return 0;
  endfunction
endclass

// A class using itself: the walk visits it once.
class SelfRef;
  SelfRef next;
endclass

module leaf;
endmodule

module gen_only;
endmodule

module orphan_leaf;
endmodule

module bound_mod;
endmodule

// Instantiated into every instance of leaf (Sec 23.11): used wherever leaf is.
bind leaf bound_mod u_bound ();

module top;
  leaf u_leaf ();

  // Sec 23.3.1: an instantiation inside a generate block that is not itself instantiated still
  // keeps gen_only from being top-level. Sec 27.3: the block is not instantiated, so gen_only is
  // not part of the design.
  if (0) begin : g_never
    gen_only u_gen ();
  end

  // Sec 23.4: a nested module with no ports that is not explicitly instantiated is implicitly
  // instantiated once.
  module nested_implicit;
  endmodule

  // Sec 23.4: a nested module with ports that is not explicitly instantiated is ignored -- and so
  // is what it instantiates.
  module nested_ignored (
      input logic a
  );
    orphan_leaf u_orphan ();
  endmodule

  Used used_h;
  via_typedef_t typedef_h;
  ExtDerived ext_h;
  Holder #(TypeArg) holder_h;
  SelfRef self_h;
  int x;

  initial begin
    x = util_pkg::helper();
    x = Statics::get();
  end
endmodule

program prog;
endprogram
