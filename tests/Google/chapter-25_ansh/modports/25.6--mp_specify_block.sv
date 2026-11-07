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
:name: mp_specify_block
:description: interface signals restricted by a modport used in a specify block
:tags: 25.6
*/

// Checks in this file:
//   CHK1: modport flop(input c, d, output q)
//   CHK2: always_ff writing the modport output
//   CHK3: edge-sensitive path delay using modport terminals
//   CHK4: $setup timing check on modport input terminals
//   CHK5: simple parallel path between modport input and output

interface itf;
  logic c, q, d;
  // CHK1
  modport flop (input c, d, output q);
endinterface

module dtype(itf.flop ch);
  // CHK2
  always_ff @(posedge ch.c) ch.q <= ch.d;
  specify
    // CHK3
    (posedge ch.c => (ch.q +: ch.d)) = (5, 6);
    // CHK4
    $setup(ch.d, posedge ch.c, 1);
    // CHK5
    (ch.d => ch.q) = 2;
  endspecify
endmodule

module top;
  itf u_itf();
  dtype u_d(u_itf);
endmodule
