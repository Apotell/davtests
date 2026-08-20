// Copyright 2020 Apotell
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License is distributed
// on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for
// the specific language governing permissions and limitations under the License.

// Regression coverage for IEEE 1800-2023 Sec 5.12 attribute_instance attachment.
//
// This file exercises every grammar production (per grammar/SV3_1aParser.g4) that carries its
// own attribute_instance* prefix, one representative attribute per construct. The goal is NOT to
// validate every detail of attribute semantics (name/value/type) -- that is covered elsewhere
// (AttributesNets, DataAttrib, 5.12-attributes-variable). The goal is narrower: confirm that
// whichever leaf model-builder is responsible for each construct actually calls
// appendPendingAttributes() (or the equivalent per-item sibling-pairing mechanism for the
// deliberately-excluded repeated-list forms) at all -- i.e. the attribute is not silently
// dropped. Each attribute name below is unique and encodes the construct it targets so the
// companion test file can look it up unambiguously.
//
// Deliberately NOT covered: attribute_instance appearing inline between individual operators
// inside ordinary (non-constant) ternary/matches/inside forms that are syntactically
// indistinguishable from the covered binary-operator forms, and a handful of rarely-used
// modport/checker-port-connection sub-alternatives (modport_hierarchical_ports_declaration,
// modport_tf_ports_declaration, named/ordered_checker_port_connection) -- these share the same
// underlying mechanism as a covered sibling alternative and would not exercise new code paths.
//
// NOT COVERED (grammar does not support it): streaming_concatenation
// (`{ >> {...} }`) carries no attribute_instance anywhere in this parser's actual
// streaming_concatenation/stream_operator/stream_concatenation/stream_expression rules --
// confirmed by reading grammar/SV3_1aParser.g4 directly. An earlier draft of this file assumed
// otherwise by misreading a commented-out (dead) alternate grammar block further up the same
// file; that was a mistake, not a real construct.

package Attributes2_pkg;
  // package_declaration: attribute_instance* PACKAGE identifier ...
  (* a_package_item *) parameter int A_PKG_PARAM = 1;  // package_item
endpackage

// interface_class_declaration's own items: interface_class_item
interface class Attributes2_iface_class;
  (* a_interface_class_item *) pure virtual function void do_it();
endclass

// class_declaration's own items: class_item (class_property alternative)
class Attributes2_class;
  (* a_class_item *) int class_field;
endclass

// port_declaration (non-ansi form) + named_port_connection + ordered_port_connection.
module bar(clk, rst);
  (* a_port_declaration *) input wire clk;
  input wire rst;
endmodule

module foo(clk, rst);
  input wire clk, rst;

  bar bar_named ((* a_named_port_connection *) .clk(clk), .rst(rst));
  bar bar_ordered ((* a_ordered_port_connection *) clk, rst);
endmodule

// udp_nonansi_declaration + udp_output_declaration/udp_input_declaration/udp_reg_declaration
(* a_udp_nonansi_declaration *) primitive Attributes2_udp_nonansi (o, i0, i1);
  (* a_udp_output_declaration *) output o;
  (* a_udp_input_declaration *) input i0, i1;
  (* a_udp_reg_declaration *) reg o;

  table
    0 0 : 0;
    0 1 : 0;
    1 0 : 0;
    1 1 : 1;
  endtable
endprimitive

// udp_ansi_declaration (attribute + declaration_port_list combined form)
(* a_udp_ansi_declaration *) primitive Attributes2_udp_ansi (output o, input i0, input i1);
  table
    0 0 : 0;
    0 1 : 0;
    1 0 : 0;
    1 1 : 1;
  endtable
endprimitive

// interface_nonansi_header + interface_or_generate_item + non_port_interface_item +
// modport_ports_declaration + clocking_item
(* a_interface_nonansi_header *) interface Attributes2_iface_nonansi (clk, d);
  input clk, d;

  (* a_interface_or_generate_item *) wire nonansi_marker;

  modport mp (
    input clk,
    (* a_modport_ports_declaration *) output d
  );

  clocking cb @(posedge clk);
    (* a_clocking_item *) property p_cb_check;
      1;
    endproperty
  endclocking
endinterface

// interface_ansi_header + non_port_interface_item (program_declaration alternative, via a
// nested program)
(* a_interface_ansi_header *) interface Attributes2_iface_ansi (input clk2, input d2);
  program Attributes2_nested_program (input clk3);
  endprogram
endinterface

// program_nonansi_header + non_port_program_item
(* a_program_nonansi_header *) program Attributes2_program_nonansi (clk4, d4);
  input clk4, d4;
  (* a_non_port_program_item *) wire program_marker;
endprogram

// program_ansi_header
(* a_program_ansi_header *) program Attributes2_program_ansi (input clk5, input d5);
endprogram

// checker_port_item (checker_declaration/checker_or_generate_item themselves have no attribute
// prefix of their own -- see file header note)
checker Attributes2_checker (input logic c1, (* a_checker_port_item *) input logic c2);
endchecker

// module_ansi_header (ansi port list; port_declaration_list is the deliberately-excluded
// repeated-list form) + top-level module body constructs.
(* a_module_ansi_header *) module Attributes2_top (
    input wire clk,
    input wire rst,
    input wire [7:0] in_val,
    (* a_ansi_port_declaration *) output reg [7:0] out_val
);

  // parameter_declaration / local_parameter_declaration (bare statement form -- these have no
  // attribute_instance prefix of their own; the attribute belongs to the wrapping
  // module_or_generate_item, exactly like module_or_generate_item's net_declaration case below).
  (* a_parameter_declaration *) parameter int P = 1;
  (* a_local_parameter_declaration *) localparam int LP = 2;

  // module_or_generate_item (net_declaration path)
  (* a_module_or_generate_item *) wire net_marker;

  // non_port_module_item (module_common_item routed through non_port_module_item, exercised
  // here via a net_alias -- same wrapping shape as net_declaration but a distinct item)
  wire alias_src, alias_dst;
  (* a_non_port_module_item *) alias alias_dst = alias_src;

  // bind_directive
  (* a_bind_directive *) bind Attributes2_checker Attributes2_checker bound_checker (.c1(clk), .c2(rst));

  // struct_union_member (typedef struct, one attributed member)
  typedef struct packed {
    (* a_struct_union_member *) logic [3:0] field_a;
    logic [3:0] field_b;
  } packed_struct_t;
  packed_struct_t struct_var;

  // property_port_item / sequence_port_item / let_port_item
  property Attributes2_prop(a, (* a_property_port_item *) b);
    a |-> b;
  endproperty

  sequence Attributes2_seq(a, (* a_sequence_port_item *) b);
    a ##1 b;
  endsequence

  let Attributes2_let(a, (* a_let_port_item *) b) = a & b;

  // covergroup: coverage_spec_or_option + bins_selection_or_option
  covergroup Attributes2_cg @(posedge clk);
    cp_in: coverpoint in_val {
      bins low = {[0:127]};
      bins high = {[128:255]};
    }
    (* a_coverage_spec_or_option *) cp_rst: coverpoint rst;
    x_cross: cross cp_in, cp_rst {
      (* a_bins_selection_or_option *) bins sel = binsof(cp_in.low);
    }
  endgroup
  Attributes2_cg cg_inst = new();

  // block_item_declaration + statement + statement_or_null + function_statement_or_null +
  // tf_port_declaration + tf_port_item + generate_item
  function void Attributes2_func((* a_tf_port_item *) input int fa);
    (* a_block_item_declaration *) int local_var;
    (* a_statement *) local_var = fa;
    if (fa > 0) (* a_statement_or_null *) local_var = local_var + 1;
    else (* a_function_statement_or_null *) local_var = 0;
  endfunction

  task Attributes2_task((* a_tf_port_declaration *) input int ta);
    Attributes2_func(ta);
  endtask

  generate
    if (P > 0) begin : gen_blk
      (* a_generate_item *) extern function void a_generate_item_extern_fn();
    end
  endgenerate

  // Operator-level attribute_instance positions (IEEE 1800-2023 Sec 11.3, informative note 2).
  int op_unary, op_binary, op_ternary, op_incdec;

  initial begin
    op_unary  = -(* a_unary_op *) in_val;
    op_binary = in_val + (* a_binary_op *) P;
    op_ternary = rst ? (* a_ternary_op *) in_val : P;
    op_incdec  = op_incdec (* a_incdec_op *) ++;
  end

  // method_call_body / array_manipulation_call / subroutine_call inline attribute positions.
  // All three grammar productions share the identical "name attribute_instance* (args)?" shape,
  // so the attribute always belongs to the argument list that follows it (per IEEE 1800 Sec
  // 5.12: a prefix to what follows), never to the preceding call name -- confirmed via the AST
  // dump that a bare "base.name(args)" call (even one using a built-in array-method name like
  // sum/product) is consumed entirely by subroutine_call's OWN identifier-chain +
  // attribute_instance* + argument_list, so it never reaches method_call_body/
  // array_manipulation_call at all. Those two are reachable only through the grammar's trailing
  // "(DOT method_call_body)?" clause -- a SECOND dot-call chained after an initial one. Without a
  // following WITH clause, method_call_body's plain-identifier alternative wins (a genuine
  // grammar ambiguity that ANTLR4 resolves in favor of the first-listed alternative); a trailing
  // WITH clause -- which only array_manipulation_call's alternative can consume -- forces the
  // built-in-method alternative instead.
  int dyn_arr[];
  initial begin
    dyn_arr = new[4];

    // subroutine_call: attribute belongs to the argument (Constant 1), not to the call name.
    void'(Attributes2_func (* a_subroutine_call *) (1));

    // method_call_body: chained dot-call, no WITH clause -> plain-identifier alternative.
    // Attribute belongs to the chained call's own argument (Constant 1), not to "sum".
    void'(dyn_arr.sum().sum (* a_method_call_body *) (1));

    // array_manipulation_call: chained dot-call with a trailing WITH clause, forcing the
    // built-in-method alternative. Attribute belongs to the argument (Constant 1), not to "sum".
    void'(dyn_arr.sum().sum (* a_array_manipulation_call *) (1) with (item));
  end

  Attributes2_udp_nonansi u_udp (out_val[0], in_val[0], in_val[1]);

endmodule

// module_nonansi_header (separate module so the ansi/nonansi header forms are both exercised).
(* a_module_nonansi_header *) module Attributes2_top_nonansi (clk, rst, in_val, out_val);
  input clk, rst;
  input [7:0] in_val;
  output reg [7:0] out_val;

  always @(posedge clk) out_val <= rst ? 8'h0 : in_val;
endmodule
