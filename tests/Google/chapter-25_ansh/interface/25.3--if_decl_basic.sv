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
:name: if_decl_basic
:description: basic interface declaration forms
:tags: 25.3
*/

// Checks in this file:
//   CHK1: empty interface without port list
//   CHK2: interface with empty port list parentheses
//   CHK3: endinterface with a matching end label
//   CHK4: interface with automatic lifetime and a function
//   CHK5: interface with static lifetime

// CHK1
interface c1_if;
endinterface

// CHK2
interface c2_if();
endinterface

// CHK3
interface c3_if;
  logic a;
endinterface : c3_if

// CHK4
interface automatic c4_if;
  function int get_one();
    int x = 1;
    return x;
  endfunction
endinterface

// CHK5
interface static c5_if;
  logic a;
endinterface

module top;
  c1_if u1();
  c2_if u2();
  c3_if u3();
  c4_if u4();
  c5_if u5();
endmodule
