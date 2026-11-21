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
:name: if_err_instantiation
:description: illegal interface instantiations
:should_fail_because: interface instance connections must match the declared ports and the interface must exist
:tags: 25.3
*/

// Errors in this file:
//   ERR1: .* used but a port has no matching signal
//   ERR2: more positional connections than ports
//   ERR3: named connection to a port that does not exist
//   ERR4: two instances with the same instance name
//   ERR5: instantiation of an interface that is never declared

interface bus_if(input logic clk, input logic rst);
endinterface

interface one_if(input logic clk);
endinterface

module m1;
  logic clk;
  // ERR1
  bus_if u(.*);
endmodule

module m2;
  logic clk, extra;
  // ERR2
  one_if u(clk, extra);
endmodule

module m3;
  logic clk;
  // ERR3
  one_if u(.clock(clk));
endmodule

module m4;
  logic clk;
  // ERR4
  one_if u(clk);
  one_if u(clk);
endmodule

module m5;
  // ERR5
  missing_if u();
endmodule

module top;
  m1 u1();
  m2 u2();
  m3 u3();
  m4 u4();
  m5 u5();
endmodule
