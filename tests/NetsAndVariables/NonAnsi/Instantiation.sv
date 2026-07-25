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

// The module, program, interface and class below are minimal, self-contained
// redeclarations of the ones in Ports.sv, Program.sv, Interface.sv and
// Class.sv so this file (Instantiation.sv) can be compiled and tested
// independently, since top_nonansi instantiates all of them.

`default_nettype wire

module nets_and_variables_nonansi(a, b, y);
  input a;
  input b;
  output y;
  assign y = a & b;
endmodule

program nets_and_variables_program;
  input wire a;
  input wire b;
  output logic y;
  always_comb y = a ^ b;
endprogram

interface nets_and_variables_if_nonansi;
  logic if_logic;
  wire if_wire;
  modport mp(input if_logic, output if_wire);
endinterface

class nets_and_variables_class_nonansi;
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
  nets_and_variables_class_nonansi cls0;

  initial begin
    cls0 = new();
    cls0.run();
  end
endmodule

`default_nettype none
