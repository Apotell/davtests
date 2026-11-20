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

// 23.2.4, Syntax 23-5: module_or_generate_item_declaration covers
// package_or_generate_item_declaration (nets, variables, parameters, types,
// tasks and functions) and genvar_declaration.
module m;
  wire w;
  logic v;
  parameter int P = 1;
  localparam int L = 2;
  typedef logic [3:0] nibble_t;
  genvar g;
  function automatic int f(int x);
    return x;
  endfunction
  task automatic t;
  endtask
endmodule
