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
:name: mp_pass_through_hierarchy
:description: modport views passed down through several levels of hierarchy
:tags: 25.5
*/

// Checks in this file:
//   CHK1: top passes full interface, middle passes to leaf with modport header
//   CHK2: middle module has modport header, passes same view to leaf
//   CHK3: middle generic interface port forwarded to a named-modport leaf
//   CHK4: leaf reads input and writes output through the forwarded modport
//   CHK5: two different modports of one instance passed down different paths

interface bus_if;
  logic x, y;
  modport target (input x, output y);
  modport initiator (output x, input y);
endinterface

// CHK4
module leaf_t(bus_if.target b);
  assign b.y = b.x;
endmodule

module leaf_i(bus_if.initiator b);
  assign b.x = 1'b1;
endmodule

// CHK1
module mid_full(bus_if b);
  leaf_t u_leaf(.b(b));
endmodule

// CHK2
module mid_mp(bus_if.initiator b);
  leaf_i u_leaf(.b(b));
endmodule

// CHK3
module mid_generic(interface b);
  leaf_t u_leaf(.b(b));
endmodule

module top;
  bus_if b1(), b2();
  mid_full u1(.b(b1));
  // CHK5
  mid_mp u2(.b(b1));
  mid_generic u3(.b(b2));
endmodule
