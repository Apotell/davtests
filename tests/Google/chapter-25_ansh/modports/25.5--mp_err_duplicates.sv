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
:name: mp_err_duplicates
:description: duplicate names inside and between modports
:should_fail_because: a modport name or modport port identifier shall not be defined twice
:tags: 25.5
*/

// Errors in this file:
//   ERR1: two modports with the same name in one interface
//   ERR2: same signal listed twice in one modport
//   ERR3: same signal listed under two different directions in one modport
//   ERR4: same port expression name .P defined twice in one modport
//   ERR5: port expression name equal to a simple port name in the same modport

// ERR1
interface if_err1;
  logic a, b;
  modport mp (input a);
  modport mp (output b);
endinterface

// ERR2
interface if_err2;
  logic a;
  modport mp (input a, a);
endinterface

// ERR3
interface if_err3;
  logic a;
  modport mp (input a, output a);
endinterface

// ERR4
interface if_err4;
  logic [7:0] r;
  modport mp (output .P(r[3:0]), .P(r[7:4]));
endinterface

// ERR5
interface if_err5;
  logic [7:0] r;
  logic R;
  modport mp (input R, output .R(r[0]));
endinterface

module top;
  if_err1 u1();
  if_err2 u2();
  if_err3 u3();
  if_err4 u4();
  if_err5 u5();
endmodule
