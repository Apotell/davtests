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

// 23.2.4, Syntax 23-5: non_port_module_item includes
// specparam_declaration and specify_block.
module m(input a, output b);
  specparam tRise = 5;
  specify
    specparam tFall = 6;
    (a => b) = (tRise, tFall);
  endspecify
  assign b = a;
endmodule
