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

class nets_and_variables_class_nonansi;
  // Class members are variables only; net declarations are illegal.
  logic cls_logic;
  reg cls_reg;
  bit [3:0] cls_bits;

  task run();
    cls_logic = 1'b1;
    cls_reg = cls_logic;
    cls_bits = 4'hA;
  endtask
endclass
