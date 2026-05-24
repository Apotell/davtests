// InstDisambig: single SV file validating that grammar predicates correctly
// dispatch all instantiation variants. Syntactically correct; not required
// to be semantically meaningful.
//
// Predicate coverage:
//   isInterfaceElem  -> interface_instantiation  (plain + parameterized)
//   isModuleElem     -> module_instantiation     (plain + parameterized)
//   isProgramElem    -> program_instantiation
//   isCheckerElem    -> checker_instantiation    (module body + concurrent_assertion_item)
//   isPrimitiveElem  -> udp_instantiation
//   isUnsupportedElem-> unsupported_instantiation (black-box, not defined here)
//   (keyword-driven) -> gate_instantiation       (and/or/not)
//   (keyword-driven) -> gate_instantiation       (tran switch primitive)

// --------------------------------------------------------------------------
// Interface definitions
// --------------------------------------------------------------------------

`define INTERFACE_BEGIN(name) interface name
`define INTERFACE_END endinterface

interface simple_if;
  logic clk;
  logic data;
`INTERFACE_END

`INTERFACE_BEGIN(bus_if) #(parameter int W = 8);
  logic [W-1:0] data;
  logic         valid;
  logic         ready;
  modport master(output data, valid, input  ready);
  modport slave (input  data, valid, output ready);
`INTERFACE_END

// --------------------------------------------------------------------------
// Macro-defined module: both 'module' and 'endmodule' are inside a macro so
// the preprocessor callbacks never fire.  The parser-side retroactive insert
// must register it so isModuleElem() returns true at instantiation time.
// --------------------------------------------------------------------------
`define DEF_MODULE(N) module N(); endmodule
`DEF_MODULE(macro_mod)

// --------------------------------------------------------------------------
// Module definitions
// --------------------------------------------------------------------------
module passthru (
  input  logic in_clk,
  input  logic in_data,
  output logic out_data
);
  assign out_data = in_data;
endmodule

module adder #(parameter int W = 8) (
  input  logic [W-1:0] a,
  input  logic [W-1:0] b,
  output logic [W-1:0] sum
);
  assign sum = a + b;
endmodule

// --------------------------------------------------------------------------
// Program definition
// --------------------------------------------------------------------------
program my_prog (input logic clk);
  initial begin
    @(posedge clk);
  end
endprogram

// --------------------------------------------------------------------------
// Checker definition
// --------------------------------------------------------------------------
checker my_checker (property p, sequence s);
  assert property (p);
  cover property (s);
endchecker

// --------------------------------------------------------------------------
// UDP definition (combinational AND)
// --------------------------------------------------------------------------
primitive my_udp (output out, input a, b);
  table
    0 0 : 0;
    0 1 : 0;
    1 0 : 0;
    1 1 : 1;
  endtable
endprimitive

// --------------------------------------------------------------------------
// Top module: all instantiation types in one module body
// --------------------------------------------------------------------------
module top;
  logic clk, a, b, out;
  logic [7:0] data_a, data_b, data_sum;

  // 1. Interface instantiation — isInterfaceElem("simple_if")
  simple_if u_simple_if();

  // 2. Interface instantiation, parameterized — isInterfaceElem("bus_if")
  bus_if #(.W(16)) u_bus();

  // 3. Module instantiation — isModuleElem("passthru")
  passthru u_pt(.in_clk(clk), .in_data(u_simple_if.data), .out_data(out));

  // 4. Module instantiation, parameterized — isModuleElem("adder")
  adder #(.W(8)) u_add(.a(data_a), .b(data_b), .sum(data_sum));

  // 5. Program instantiation — isProgramElem("my_prog")
  my_prog u_prog(.clk(clk));

  // 6. Checker instantiation — isCheckerElem("my_checker")
  my_checker u_chk(
    @(posedge clk) a |=> b,
    @(posedge clk) a ##1 b
  );

  // 7. UDP instantiation — isPrimitiveElem("my_udp")
  my_udp u_udp(out, a, b);

  // 8. Unsupported / black-box — isUnsupportedElem("black_box")
  //    Type not defined in this compilation unit.
  black_box u_bb(.clk(clk), .q(out));

  // 9. Macro-defined module — isModuleElem("macro_mod") via retroactive insert
  macro_mod u_macro_mod();

  // 10. Gate primitives — keyword-driven, no predicate needed
  and  g_and(out, a, b);
  or   g_or(out, a, b);
  not  g_not(out, a);

  // 11. Switch primitive — keyword-driven
  tran g_tran(a, b);

endmodule

// --------------------------------------------------------------------------
// Generate-block: exercises the module_or_generate_item path
// (interface + module instantiation inside generate-for)
// --------------------------------------------------------------------------
module gen_top #(parameter int N = 2);
  genvar i;
  generate
    for (i = 0; i < N; i++) begin : gen_lane
      simple_if u_if();
      passthru  u_pt(.in_clk(1'b0), .in_data(u_if.data), .out_data());
    end
  endgenerate
endmodule
