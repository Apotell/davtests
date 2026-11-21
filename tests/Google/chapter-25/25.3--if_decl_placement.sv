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
:name: if_decl_placement
:description: where an interface can be declared and nested
:tags: 25.3
*/

// Checks in this file:
//   CHK1: interface declared after the module that instantiates it
//   CHK2: interface declared inside a module
//   CHK3: interface declared inside another interface
//   CHK4: interface instance inside another interface, accessed hierarchically

module top;
  // CHK1
  c1_if u1();

  // CHK2
  interface c2_local_if;
    logic a;
  endinterface
  c2_local_if u2();

  c3_outer_if u3();

  c4_outer_if u4();
  assign u4.u_inner.x = 1'b1;
endmodule

interface c1_if;
  logic a;
endinterface

// CHK3
interface c3_outer_if;
  interface c3_inner_if;
    logic a;
  endinterface
  c3_inner_if u_inner();
endinterface

// CHK4
interface c4_inner_if;
  logic x;
endinterface

interface c4_outer_if;
  c4_inner_if u_inner();
  logic y;
  assign y = u_inner.x;
endinterface
