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
:name: mp_err_export
:description: errors with subroutines exported through modports
:should_fail_because: exported subroutines must be defined by the connected module and match the modport prototype
:tags: 25.7.3
*/

// Errors in this file:
//   ERR1: module connected to a modport that exports Read but never defines it
//   ERR2: exported prototype does not match the module definition (argument type)
//   ERR3: exported prototype does not match the module definition (argument count)
//   ERR4: two modules export the same task without extern forkjoin
//   ERR5: modport exports a function but the module defines a task with that name

// ERR1
interface if1;
  modport target (export Read);
  modport initiator (import task Read());
endinterface

module m1(interface a);
endmodule

// ERR2
interface if2;
  modport target (export task Put(input int v));
endinterface

module m2(interface a);
  task a.Put(input logic [7:0] v);
  endtask
endmodule

// ERR3
interface if3;
  modport target (export task Put(input int v));
endinterface

module m3(interface a);
  task a.Put(input int v, input int w);
  endtask
endmodule

// ERR4
interface if4;
  modport target (export Ping);
  modport initiator (import task Ping());
endinterface

module m4(interface a);
  task a.Ping();
  endtask
endmodule

// ERR5
interface if5;
  modport target (export function int Get());
endinterface

module m5(interface a);
  task a.Get();
  endtask
endmodule

module top;
  if1 i1();
  if2 i2();
  if3 i3();
  if4 i4();
  if5 i5();
  m1 u1(i1.target);
  m2 u2(i2.target);
  m3 u3(i3.target);
  m4 u4a(i4.target);
  m4 u4b(i4.target);
  m5 u5(i5.target);
endmodule
