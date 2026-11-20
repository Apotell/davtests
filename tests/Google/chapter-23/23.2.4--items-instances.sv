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

// 23.2.4, Syntax 23-5: module_or_generate_item includes gate_instantiation,
// udp_instantiation and module_instantiation; module_common_item includes
// interface_instantiation and program_instantiation.
primitive udp_buf(output o, input i);
  table
    0 : 0;
    1 : 1;
  endtable
endprimitive

interface ifc;
endinterface

program prg;
endprogram

module child;
endmodule

module m;
  wire a, b, c, o1, o2;
  and g1(o1, a, b);
  udp_buf u1(o2, c);
  child c1();
  ifc i1();
  prg p1();
endmodule
