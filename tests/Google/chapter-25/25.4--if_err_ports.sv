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
:name: if_err_ports
:description: illegal interface port declarations and connections
:should_fail_because: interface ports must be declared once, have directions, and only ports can be connected
:tags: 25.4
*/

// Errors in this file:
//   ERR1: same port name declared twice in the header
//   ERR2: ANSI port redeclared as a variable in the body
//   ERR3: non-ANSI port listed in the header but given no direction
//   ERR4: named connection to an internal signal that is not a port

// ERR1
interface e1_if(input logic a, input logic a);
endinterface

// ERR2
interface e2_if(input logic a);
  logic a;
endinterface

// ERR3
interface e3_if(a, b);
  input a;
endinterface

// ERR4
interface e4_if(input logic clk);
  logic internal_sig;
endinterface

module top;
  logic a, b, clk, x;
  e1_if u1(a, a);
  e2_if u2(a);
  e3_if u3(a, b);
  e4_if u4(.clk(clk), .internal_sig(x));
endmodule
