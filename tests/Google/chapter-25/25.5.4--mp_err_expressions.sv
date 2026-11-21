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
:name: mp_err_expressions
:description: illegal modport expressions
:should_fail_because: a modport expression must be a legal expression for its port direction
:tags: 25.5.4
*/

// Errors in this file:
//   ERR1: constant literal port expression declared as output
//   ERR2: const variable port expression declared as inout
//   ERR3: port expression that references an undeclared name
//   ERR4: concatenation port expression used as ref
//   ERR5: writing to an input port expression from the module

// ERR1
interface if_err1;
  modport mp (output .Q(2));
endinterface

// ERR2
interface if_err2;
  const int x = 1;
  modport mp (inout .Q(x));
endinterface

// ERR3
interface if_err3;
  logic [7:0] r;
  modport mp (input .P(missing[3:0]));
endinterface

// ERR4
interface if_err4;
  logic [3:0] hi, lo;
  modport mp (ref .C({hi, lo}));
endinterface

interface if_ok;
  logic [7:0] r;
  modport mp (input .P(r[3:0]));
endinterface

// ERR5
module m5(if_ok.mp p);
  assign p.P = 4'h0;
endmodule

module top;
  if_err1 u1();
  if_err2 u2();
  if_err3 u3();
  if_err4 u4();
  if_ok   uok();
  m5 u5(uok);
endmodule
