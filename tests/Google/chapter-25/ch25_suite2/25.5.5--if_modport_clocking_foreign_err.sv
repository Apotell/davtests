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

/*
:name: if_modport_clocking_foreign_err
:description: a modport lists a clocking block declared by a different interface
:should_fail_because: all clocking blocks used in a modport declaration shall be declared by the same interface as the modport itself
:tags: 25.5.5
*/

interface other_if (input logic clk);
  clocking cb @(posedge clk);
  endclocking
endinterface

interface a_if (input logic clk);
  modport STB (clocking cb);
endinterface
