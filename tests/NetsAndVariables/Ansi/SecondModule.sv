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

// The interface and class below are duplicated from Interface.sv and
// Class.sv so this file can be compiled and tested independently, since
// nets_and_variables_second instantiates both.

interface nets_and_variables_if;
  logic if_logic;
  wire if_wire;
  modport mp(input if_logic, output if_wire);
endinterface

class nets_and_variables_class;
  logic cls_logic;
  reg cls_reg;
  bit [3:0] cls_bits;
  task run();
    cls_logic = 1'b1;
    cls_reg = cls_logic;
    cls_bits = 4'hA;
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
