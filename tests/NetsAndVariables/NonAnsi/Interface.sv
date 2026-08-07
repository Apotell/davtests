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

interface nets_and_variables_if_nonansi;
  // Interface scope variable declaration
  logic if_logic;
  // Interface scope net declaration
  wire if_wire;
  // Implicit net declaration via continuous assignment
  assign if_implicit_net = if_logic;
  modport mp(input if_logic, output if_wire);
endinterface
