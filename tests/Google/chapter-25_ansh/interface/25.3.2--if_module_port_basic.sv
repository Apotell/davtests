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
:name: if_module_port_basic
:description: module ports of a named interface type
:tags: 25.3.2
*/

// Checks in this file:
//   CHK1: interface port connected by name
//   CHK2: interface port connected by position
//   CHK3: interface port connected with .name (LRM test_bus example)
//   CHK4: interface port connected with .*
//   CHK5: module with two ports of the same interface type

interface test_bus;
  logic req, gnt, x;
endinterface : test_bus

module mem(test_bus a);
  assign a.gnt = a.req;
endmodule

module sub(test_bus iface);
endmodule

module bridge(test_bus src, test_bus dst);
  assign dst.x = src.x;
endmodule

module top;
  test_bus sb(), sb2(), iface(), s1(), s2();
  // CHK1
  mem u1(.a(sb));
  // CHK2
  mem u2(sb2);
  // CHK3
  sub u3(.iface);
  // CHK4
  sub u4(.*);
  // CHK5
  bridge u5(.src(s1), .dst(s2));
endmodule
