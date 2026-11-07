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
:name: mp_all_directions
:description: every modport port direction used and accessed from a module
:tags: 25.5
*/

// Checks in this file:
//   CHK1: input modport signal read in the module
//   CHK2: output modport signal written with a continuous assign
//   CHK3: inout modport signal on a wire, driven conditionally
//   CHK4: ref modport signal written procedurally
//   CHK5: all four directions mixed in one modport

interface bus_if;
  logic       in_sig;
  logic       out_sig;
  wire        io_sig;
  logic [7:0] ref_sig;

  // CHK5
  modport mp (input in_sig, output out_sig, inout io_sig, ref ref_sig);
endinterface

module user(bus_if.mp b);
  // CHK1 + CHK2
  assign b.out_sig = b.in_sig;
  // CHK3
  assign b.io_sig = b.in_sig ? 1'b1 : 1'bz;
  // CHK4
  initial b.ref_sig = 8'hA5;
endmodule

module top;
  bus_if u_if();
  user u_user(.b(u_if));
endmodule
