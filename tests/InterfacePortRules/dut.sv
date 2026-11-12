// Interface, modport, port and interconnect rules (IEEE 1800-2023 Ch. 23, 25, 6.6.8, 28.3.6).
// Each illegal construct is marked with the clause it breaks.

// 23.2.2.1: "It shall be illegal to specify signed for a port declared as an interconnect port."
module ic_signed (input interconnect signed [3:0] sp);
endmodule

// 23.3.3.2: "It shall be illegal to connect a port variable to an interconnect port or
// interconnect net."
module ic_leaf (input interconnect ip);
endmodule
module ic_var_leaf (input var logic vp);
endmodule
module ic_top;
  interconnect icn;
  logic lv;
  ic_leaf u_var (lv);          // variable to an interconnect port
  ic_var_leaf u_icn (icn);     // interconnect net to a variable port
endmodule

// 28.3.6: an interconnect expression to an instance array has one bit per instance.
module ic_bit (input interconnect b);
endmodule
module ic_array_top;
  interconnect [2:0] bus3;
  ic_bit arr[3:0] (bus3);      // 3 bits for 4 instances
endmodule

// 23.3.3.4: "An interface port cannot be left unconnected." / "connected to an interface
// instance of the identical type".
interface bus_if;
  logic d;
  modport mp (input d);
  modport sp (output d);
endinterface
interface other_if;
  logic d;
endinterface
module needs_bus (bus_if b);
endmodule
module needs_mp (bus_if.mp b);
endmodule
module conn_top;
  bus_if bi ();
  other_if oi ();
  needs_bus u_open ();         // b left unconnected
  needs_bus u_wrong (.b(oi));  // wrong interface type
  needs_mp u_mp (.b(bi.sp));   // modport names differ (25.5)
  needs_bus u_unknown (.b(bi), .zz(bi)); // 23.3.2.2: no port zz
endmodule

// 23.2.2.1: every port_identifier of the list of ports shall be declared in the body.
module undeclared (a, b);
  input a;
endmodule

// 23.3.2: an interface instance needs parentheses.
module no_parens;
  bus_if bn;
endmodule

// 25.5.4: "The port expression shall resolve to a legal expression for the type of module port".
interface expr_if;
  logic v;
  modport bad (output .Q(2));
endinterface

// 25.7: prototypes, imports and exports.
interface tf_if;
  function automatic int f(int x); return x; endfunction
  extern forkjoin task t_ext (input int a);
  modport mismatch (import function int f(int y));
  modport by_name_ext (import t_ext);
  modport undefined_import (import nosuch);
  modport exp (export task w (input int a));
  function automatic int g(int x = 1); return x; endfunction
  modport by_name (import g);
endinterface
module tf_user (tf_if.by_name p);
  int r;
  initial r = p.g();           // 25.7: a default argument needs a full prototype
endmodule
module no_export (tf_if.exp p);
endmodule
module has_export (tf_if.exp p);
  task w (input int a); endtask
endmodule
module export_top;
  tf_if t ();
  no_export u_none (.p(t));    // 25.7: the exported task w is not defined
  has_export u_one (.p(t));
  has_export u_two (.p(t));    // 25.7.4: a second exporter of w
  tf_user u_user (.p(t));
endmodule

// 25.5: a modport named in a module header shall exist; 23.8: members shall exist.
module bad_modport (bus_if.nosuch b);
endmodule
module member_user (bus_if b);
  logic x;
  assign x = b.nomember;
endmodule

// 13.5: arguments bind to formals.
interface args_if;
  function automatic int one(int a); return a; endfunction
endinterface
module args_user (args_if p);
  int r;
  initial r = p.one(1, 2);
endmodule

// 25.6: "A ref port cannot be used as a terminal in a specify block."
module ref_specify (input logic i, ref logic r);
  specify
    (i => r) = 1;
  endspecify
endmodule

// 23.2.2.4: only an input port may have a default value.
module bad_inout_default (inout wire io = 1'b0);
endmodule

// 3.13: instance and modport names share their scope's name space.
module dup_instances;
  bus_if same ();
  bus_if same ();
endmodule
interface dup_modports;
  logic d;
  modport m (input d);
  modport m (output d);
endinterface

// 23.2.2.1: a name used in a port declaration shall not be declared again in another port
// declaration -- here an interface port declaration of a port that already has a direction.
module redeclared_as_interface (s);
  output s;
  bus_if s;
endmodule

// 23.2.2.3: an output port with an omitted or implicit data type is a net, so it cannot be
// initialized; b inherits a's net kind.
module bad_output_default (output o = 1'b0);
endmodule
module bad_inherited_default (output wire a, b = 1'b0);
endmodule

// 23.2.2.1: one "signed" written for two interconnect ports.
module ic_signed_pair (input interconnect signed [3:0] p1, p2);
endmodule
