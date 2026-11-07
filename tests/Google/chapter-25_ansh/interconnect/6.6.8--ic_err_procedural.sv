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
:name: ic_err_procedural
:description: interconnect nets used in procedural contexts
:should_fail_because: an interconnect net shall not be used in any procedural context
:tags: 6.6.8
*/

// Errors in this file:
//   ERR1: $display of an interconnect net in an initial block (LRM)
//   ERR2: interconnect in an always event control @(w)
//   ERR3: interconnect as an if condition in a procedural block
//   ERR4: interconnect as an edge event @(posedge w)
//   ERR5: interconnect passed as a function argument in a procedural block

module leaf(inout wire p);
endmodule

module top;
  interconnect w1, w2;
  interconnect w3, w4;
  interconnect w5;
  logic r;
  function automatic logic id(logic v);
    return v;
  endfunction
  // ERR1
  initial $display(w1);
  // ERR2
  always @(w2) r = 1'b0;
  // ERR3
  initial if (w3) r = 1'b1;
  // ERR4
  always @(posedge w4) r = 1'b0;
  // ERR5
  initial r = id(w5);
  leaf l1(w1);
  leaf l2(w2);
  leaf l3(w3);
  leaf l4(w4);
  leaf l5(w5);
endmodule
