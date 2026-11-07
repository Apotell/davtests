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
:name: if_err_declarations
:description: illegal interface declarations and interface contents
:should_fail_because: interface names must be unique and labelled correctly, items unique, and modules cannot be instantiated in interfaces
:tags: 25.3
*/

// Errors in this file:
//   ERR1: endinterface label does not match the interface name
//   ERR2: same interface name declared twice
//   ERR3: interface and module share the same name
//   ERR4: same signal declared twice inside one interface
//   ERR5: module instantiated inside an interface

// ERR1
interface e1_if;
  logic a;
endinterface : other_if

// ERR2
interface e2_if;
  logic a;
endinterface

interface e2_if;
  logic b;
endinterface

// ERR3
interface e3_name;
  logic a;
endinterface

module e3_name;
endmodule

// ERR4
interface e4_if;
  logic a;
  logic a;
endinterface

// ERR5
module e5_leaf;
endmodule

interface e5_if;
  e5_leaf u_leaf();
endinterface

module top;
  e1_if u1();
  e2_if u2();
  e4_if u4();
  e5_if u5();
endmodule
