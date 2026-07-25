/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

`default_nettype wire

// Example summary:
// - input without type is an implicit net port (wire by default)
// - input wire is an explicit net port
// - output logic is a variable port
// - wire/tri/etc. are nets; logic/reg/bit/int are variables

module nets_and_variables_nonansi(a, b, y);
  // Port: implicit net port (wire)
  input a;
  // Port: implicit net port (wire)
  input b;
  // Port: implicit net port (wire)
  output y;

  // Explicit net declarations
  // Net declaration
  wire w0;
  // Net declaration
  wire [3:0] w_bus;
  // Net declaration
  tri t0;

  // Explicit variable declarations
  // Variable declaration
  logic var_logic;
  // Variable declaration
  reg var_reg;

  // Implicit net declaration by continuous assignment
  // (depends on default_nettype wire)
  assign implicit_net_nonansi = a & b;

  assign w0 = a | b;
  assign w_bus[0] = a;

  always_comb begin
    var_logic = a ^ b;
    var_reg = {var_reg[6:0], a};
  end

  // Function/task local scope comments for non-ANSI style.
  // Inside a function or task, the local declarations are procedural variables.
  // A local net declaration such as wire local_net; is illegal.
  function automatic logic fn_nonansi_xor(input logic x, input logic z);
    logic local_var; // legal local variable in function
    local_var = x ^ z;
    return local_var;
  endfunction

  task t_nonansi_drive(input logic d);
    logic local_var; // legal local variable in task
    local_var = d;
    y = local_var;
  endtask

  assign y = w0;
endmodule

program nets_and_variables_program;
  // Program scope behavior is the same in ANSI and non-ANSI style.
  // The only difference is the module port declaration syntax.
  // No separate program-only example is needed beyond showing that
  // a program may contain both net and variable declarations.
  input wire a;
  input wire b;
  output logic y;

  wire prog_net; // legal net declaration in program scope
  logic prog_var; // legal variable declaration in program scope

  assign prog_net = a & b;

  always_comb begin
    prog_var = a ^ b;
    y = prog_var;
  end
endprogram

package nets_and_variables_pkg_nonansi;
  // Package scope variable declaration
  logic pkg_logic;
  // Package scope variable declaration
  reg pkg_reg;
  // Package scope net declaration
  wire pkg_wire;
  assign pkg_wire = pkg_logic;
endpackage

interface nets_and_variables_if_nonansi;
  // Interface scope variable declaration
  logic if_logic;
  // Interface scope net declaration
  wire if_wire;
  modport mp(input if_logic, output if_wire);
endinterface

checker nets_and_variables_checker_nonansi(clk, a);
  // Port: implicit net port (wire)
  input clk;
  // Port: implicit net port (wire)
  input a;

  // Checker scope variable declaration
  logic ck_logic;
  // Checker scope net declaration
  wire ck_wire;
  // Checker scope variable declaration
  logic [1:0] ck_vec;

  always_ff @(posedge clk) begin
    ck_logic <= a;
    ck_vec <= {ck_vec[0], a};
  end
endchecker

class nets_and_variables_class_nonansi;
  // No difference from the ANSI class example.
  // Class members are variables only; net declarations are illegal.
  // So no separate non-ANSI class example is needed here.
  logic cls_logic;
  reg cls_reg;
  bit [3:0] cls_bits;

  task run();
    cls_logic = 1'b1;
    cls_reg = cls_logic;
    cls_bits = 4'hA;
  endtask
endclass

module top_nonansi();
  // Top-level net declarations
  wire a = 1'b1;
  wire b = 1'b0;
  wire y1;
  wire y2;

  nets_and_variables_nonansi mod_nonansi(a, b, y1);
  nets_and_variables_program prog_inst(a, b, y2);
  nets_and_variables_if_nonansi if0();
  nets_and_variables_checker_nonansi ck0(.clk(a), .a(b));
  nets_and_variables_class_nonansi cls0;

  initial begin
    import nets_and_variables_pkg_nonansi::*;
    cls0 = new();
    cls0.run();
  end
endmodule

`default_nettype none
