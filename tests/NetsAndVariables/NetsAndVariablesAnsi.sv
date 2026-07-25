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

module nets_and_variables_test(
  input logic clk,
  input logic a,
  input logic b,
  output logic y
);

  // Net declarations
  wire w0; // net declaration
  tri t0; // net declaration
  tri0 t0z; // net declaration
  tri1 t1z; // net declaration
  wand wand_net; // net declaration
  wor wor_net; // net declaration
  triand triand_net; // net declaration
  trior trior_net; // net declaration
  supply0 supply0_net; // net declaration
  supply1 supply1_net; // net declaration
  uwire uwire_net; // net declaration
  wire [7:0] w_bus; // net declaration
  tri [3:0] tri_bus; // net declaration

  // Variable declarations
  logic var_logic; // variable declaration
  reg var_reg; // variable declaration

  // Implicit declarations
  // In SystemVerilog, undeclared identifiers used in assignments may be implicitly declared
  // as nets or variables depending on context, and these examples are intended to exercise that.
  wire implicit_wire; // implicit net declaration via wire keyword
  logic implicit_logic; // explicit variable declaration, not implicit
  assign implicit_wire = a | b; // continuous assignment to an implicit net-like object

  // Implicit net examples from use before declaration
  assign implicit_net_a = a & b; // implicit net declaration by continuous assignment
  assign implicit_net_b = implicit_net_a | a; // implicit net declaration by continuous assignment

  // Implicit variable examples from procedural assignment
  always_comb begin
    implicit_var_a = a ^ b; // implicit variable declaration by procedural assignment
    implicit_var_b = implicit_var_a; // implicit variable declaration by procedural assignment
  end

  // Implicit variable examples from nonblocking assignment
  always_ff @(posedge clk) begin
    implicit_var_c <= implicit_var_a; // implicit variable declaration by nonblocking assignment
  end

  // Explicitly declared variables that are also used in assignments
  bit var_bit; // variable declaration
  int var_int; // variable declaration
  integer var_integer; // variable declaration
  shortint var_shortint; // variable declaration
  longint var_longint; // variable declaration
  byte var_byte; // variable declaration
  time var_time; // variable declaration
  real var_real; // variable declaration
  realtime var_realtime; // variable declaration
  string var_string; // variable declaration
  chandle var_chandle; // variable declaration
  event var_event; // variable declaration
  enum logic [1:0] {IDLE, BUSY} var_enum; // variable declaration
  logic [3:0] var_vector; // variable declaration
  reg [7:0] var_reg_vector; // variable declaration

  assign w0 = a & b; // continuous assignment to net
  assign w_bus[0] = a; // continuous assignment to net
  assign uwire_net = a ^ b; // continuous assignment to net

  always_comb begin
    var_logic = a ^ b; // procedural assignment to variable
    var_bit = a ? 1'b1 : 1'b0; // procedural assignment to variable
  end

  always_ff @(posedge clk) begin
    var_int <= var_int + 1; // nonblocking assignment to variable
    var_reg_vector <= {var_reg_vector[2:0], a}; // nonblocking assignment to variable
  end

  always_latch begin
    if (a) begin
      var_integer = var_int; // procedural assignment to variable
    end
  end

  initial begin
    var_real = 3.14159; // procedural assignment to variable
    var_string = "sv"; // procedural assignment to variable
  end

  always @(posedge clk) begin
    y <= var_logic; // procedural assignment to variable
  end

  // Function/task examples for ANSI style module scope.
  // In SystemVerilog, a task/function local scope is a procedural scope.
  // It may contain variable declarations such as logic/bit/int, but it cannot
  // contain net declarations such as wire/tri/supply0. Net declaration inside
  // task/function is illegal.
  function automatic logic fn_xor(input logic x, input logic z);
    logic local_var; // legal local variable inside function
    local_var = x ^ z;
    return local_var;
  endfunction

  task t_drive_output(input logic d);
    logic local_var; // legal local variable inside task
    local_var = d;
    y = local_var;
  endtask

endmodule

program nets_and_variables_program(
  input logic clk,
  input logic a,
  input logic b,
  output logic y
);
  // Program scope can contain both nets and variables.
  wire prog_net; // legal net declaration in program
  logic prog_var; // legal variable declaration in program

  // A task/function under program is still a procedural scope.
  // Local declarations inside task/function are variables only.
  function automatic logic fn_prog_xor(input logic x, input logic z);
    logic local_var; // legal local variable inside function
    local_var = x ^ z;
    return local_var;
  endfunction

  task t_prog_drive();
    logic local_var; // legal local variable inside task
    local_var = fn_prog_xor(a, b);
    y = local_var;
  endtask

  assign prog_net = a & b;

  always_comb begin
    prog_var = a ^ b;
    y = prog_var;
  end
endprogram

package nets_and_variables_pkg;
  // Package-scoped variable and net-like declarations
  logic pkg_logic; // package variable declaration
  reg pkg_reg; // package variable declaration
  wire pkg_wire; // package net declaration
  assign pkg_wire = pkg_logic; // package continuous assignment
endpackage

interface nets_and_variables_if;
  // Interface-scoped declarations
  logic if_logic; // interface variable declaration
  wire if_wire; // interface net declaration
  modport mp(input if_logic, output if_wire);
endinterface

checker nets_and_variables_checker(input logic clk, input logic a);
  // Checker-scoped declarations
  logic ck_logic; // checker variable declaration
  wire ck_wire; // checker net declaration
  logic [1:0] ck_vec; // checker variable declaration
  always_ff @(posedge clk) begin
    ck_logic <= a; // checker procedural assignment
    ck_vec <= {ck_vec[0], a}; // checker procedural assignment
  end
endchecker

class nets_and_variables_class;
  // Class-scoped declarations
  // Nets such as wire/tri are not standard class members; use variables instead.
  logic cls_logic; // class variable declaration
  reg cls_reg; // class variable declaration
  bit [3:0] cls_bits; // class variable declaration
  task run();
    cls_logic = 1'b1; // class procedural assignment
    cls_reg = cls_logic; // class procedural assignment
    cls_bits = 4'hA; // class procedural assignment
  endtask
endclass

module nets_and_variables_second();
  // Module instance and local declarations
  nets_and_variables_if if0();
  nets_and_variables_class cls0;
  initial begin
    cls0 = new();
    cls0.run();
  end
endmodule
