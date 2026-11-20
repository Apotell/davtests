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
:name: mp_expressions_advanced
:description: modport expressions using arrays, structs, concatenation and empty expressions
:tags: 25.5.4
*/

// Checks in this file:
//   CHK1: unpacked array element as port expression .E0(arr[0])
//   CHK2: struct member as port expression .ADDR(req.addr)
//   CHK3: concatenation as input port expression .CAT({hi, lo})
//   CHK4: port expression with no expression .NC()
//   CHK5: bit select as output port expression .B0(v[0])

interface bus_if;
  typedef struct packed { logic [7:0] addr; logic wr; } req_t;
  logic [3:0] arr [0:1];
  req_t req;
  logic [3:0] hi, lo;
  logic [7:0] v;

  modport mp (
    input  .E0(arr[0]),      // CHK1
    input  .ADDR(req.addr),  // CHK2
    input  .CAT({hi, lo}),   // CHK3
    input  .NC(),            // CHK4
    output .B0(v[0])         // CHK5
  );
endinterface

module user(bus_if.mp m);
  logic [3:0] e;
  logic [7:0] a, c;
  assign e = m.E0;
  assign a = m.ADDR;
  assign c = m.CAT;
  assign m.B0 = 1'b1;
endmodule

module top;
  bus_if u_if();
  user u_user(.m(u_if));
endmodule
