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

// Events first raised by sr/iodecl (Sec 10.11, 23.6, 25.3, 25.5.5, 23.3.3.2, 25.5, 11.2).
module ipr_alias; wire a; tri1 b; alias a = b; endmodule
interface ipr_ii; logic s; endinterface
module ipr_idx; ipr_ii arr[0:3](); wire w; assign w = arr[4].s; endmodule
module ipr_leaf; endmodule
interface ipr_hasmod; ipr_leaf u(); endinterface
interface ipr_ck; modport m(clocking nocb); endinterface
module ipr_io(inout wire p); endmodule
module ipr_ioc; logic v; ipr_io u(.p(v)); endmodule
interface ipr_ai; logic d; modport m(input d); endinterface
module ipr_aw(ipr_ai.m p); initial p.d = 1; endmodule
interface ipr_ie; logic s; endinterface
module ipr_iec; ipr_ie x(); wire w; assign w = x; endmodule
// Non-ANSI port declarations (Sec 23.2.2.1).
interface ipr_if; endinterface
module ipr_dupimp(a, a); input a; endmodule  // legal: 23.2.2.1 Ex. 6
module ipr_dupif(p); ipr_if p; ipr_if p; endmodule
module ipr_dupin(q); input q; input q; endmodule
module ipr_undecl(.p(a), b[1:0], {c, d}); input c; wire a; wire [3:0] b; endmodule
// A generic interface port takes only an interface instance (Sec 23.3.3.4).
module ipr_gen(interface p); endmodule
module ipr_gentop; logic o; ipr_gen u(.p(o)); endmodule
// Default values on inherited ANSI ports (Sec 23.2.2.3).
module ipr_d1(output var integer x, [3:0] y = 0); endmodule
module ipr_d2(output var logic x, .b(s), c = 1); logic s; endmodule
// Parameterized interface instances and virtual interfaces (Sec 25.9, 25.10).
interface ipr_pif #(parameter int W = 8); logic s, h; modport mp(input s); modport other(output s); endinterface
interface ipr_qif; logic s; endinterface
module ipr_virt;
  ipr_pif #(16) wide();
  ipr_pif narrow();
  ipr_qif q();
  virtual ipr_pif #(8) v8;
  virtual ipr_pif.mp vmp;
  virtual ipr_pif.other voth;
  virtual ipr_pif vplain;
  logic y;
  initial begin
    v8 = wide;
    v8 = q;
    vmp = voth;
    vplain = vmp;
    vmp = narrow;
    y = vmp.h;
  end
endmodule
// Interface and program ports have io decls (Sec 37.13).
program ipr_prog(ipr_pif.mp p); initial $display(p.s); endprogram
// Implicit .name and .* connections (Sec 23.3.2.3, 23.3.2.4).
module ipr_c(input logic a, input logic b = 0, input logic d); endmodule
module ipr_late; ipr_c u(.a, .b(), .d(a)); logic a; endmodule
module ipr_star; logic a; ipr_c u(.*); endmodule
// Through a generic interface port (Sec 25.3.3): a signal in an event control is a reference, a
// name used as a statement is a subroutine call (Sec 13.5.5).
module ipr_generic(interface a); always @(a.start) a.run; endmodule
// Sec 10.11: aliased nets shall be of the same net type; an interconnect is not merged into a wire.
module ipr_icalias; interconnect i; wire w; alias i = w; endmodule
// Sec 25.9: a virtual interface may be compared with an interface instance of the same type.
interface ipr_vi; logic s; endinterface
module ipr_veq; ipr_vi inst(); virtual ipr_vi vif; wire eq; assign eq = (vif == inst); endmodule
// Sec 23.6: every index of a multi-dimensional instance array shall be a legal index value.
interface ipr_ba; logic x; endinterface
module ipr_arr2;
  ipr_ba u[3][4]();
  initial begin
    u[1][2].x = 0;
    u[1][9].x = 0;
  end
endmodule

// ".*" is ".name" for every port not connected explicitly (Sec 23.3.2.4), except that a port with a
// default value, and no name of its own in the instantiating scope, uses the default.
module ipr_wc_def(input logic a, input logic b, input logic c = 1'b1, output logic y); endmodule
module ipr_wc; logic a, y; ipr_wc_def u(.*, .b(a)); endmodule
