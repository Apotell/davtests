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

class nets_and_variables_class;
  // Class-scoped declarations
  // Nets such as wire/tri are not standard class members; use variables instead.
  logic cls_logic; // class variable declaration
  reg cls_reg; // class variable declaration
  bit [3:0] cls_bits; // class variable declaration
  rand bit [3:0] cls_rand; // class rand variable declaration
  randc bit [1:0] cls_randc; // class randc variable declaration
  task run();
    cls_logic = 1'b1; // class procedural assignment
    cls_reg = cls_logic; // class procedural assignment
    cls_bits = 4'hA; // class procedural assignment
  endtask
endclass
