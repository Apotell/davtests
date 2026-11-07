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
:name: mp_err_undeclared_names
:description: modports that list names not declared by the same interface
:should_fail_because: all names used in a modport shall be declared by the same interface as the modport
:tags: 25.5
*/

// Errors in this file:
//   ERR1: modport lists a signal that does not exist
//   ERR2: nested interface modport lists signals of the enclosing interface (LRM illegal_i)
//   ERR3: interface with modports but no signals at all (LRM illegal_i)
//   ERR4: modport refers to a clocking block that is not declared
//   ERR5: modport imports a task that is not declared

// ERR1
interface if_err1;
  logic a;
  modport mp (input a, ghost);
endinterface

// ERR2
interface if_err2;
  wire x, y;
  interface illegal_i;
    wire a, b, c, d;
    modport initiator (input a, b, x, output c, d, y);
  endinterface : illegal_i
endinterface : if_err2

// ERR3
interface if_err3;
  modport initiator (input a, b, output c, d);
  modport target (output a, b, input c, d);
endinterface

// ERR4
interface if_err4(input logic clk);
  logic a;
  modport mp (clocking no_cb);
endinterface

// ERR5
interface if_err5;
  logic a;
  modport mp (input a, import no_task);
endinterface

module top;
  logic clk;
  if_err1 u1();
  if_err2 u2();
  if_err3 u3();
  if_err4 u4(clk);
  if_err5 u5();
endmodule
