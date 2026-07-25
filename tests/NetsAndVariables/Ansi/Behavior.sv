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

  wire w0;
  wire uwire_net;
  wire [7:0] w_bus;

  wire implicit_wire;
  logic var_logic;
  bit var_bit;
  int var_int;
  integer var_integer;
  reg [7:0] var_reg_vector;
  real var_real;
  string var_string;

  assign implicit_wire = a | b;
  assign implicit_net_a = a & b; // implicit net declaration by continuous assignment
  assign implicit_net_b = implicit_net_a | a; // implicit net declaration by continuous assignment
  assign w0 = a & b;
  assign w_bus[0] = a;
  assign uwire_net = a ^ b;

  // Implicit variable examples from procedural assignment
  always_comb begin
    implicit_var_a = a ^ b; // implicit variable declaration by procedural assignment
    implicit_var_b = implicit_var_a; // implicit variable declaration by procedural assignment
  end

  // Implicit variable examples from nonblocking assignment
  always @(posedge clk) begin
    implicit_var_c <= implicit_var_a; // implicit variable declaration by nonblocking assignment
  end

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

endmodule
