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

// IEEE 1800-2023 Sec 8.24 and 18.5.1: an out-of-block declaration shall be in the same scope as its class
task C2::early(); endtask

class C2;
  extern task early();
endclass

class C1;
  rand int x;
  extern function int late();
  extern task t();
  extern function new();
  extern constraint c_late;
  extern function int wrong();
  extern constraint c_wrong;
endclass

function int C1::late(); return 1; endfunction
task C1::t(); endtask
function C1::new(); endfunction
constraint C1::c_late { x < 10; }

package other;
  function int C1::wrong(); return 3; endfunction
  constraint C1::c_wrong { x > 0; }
endpackage

package pk;
  class C3;
    extern function int f();
  endclass
  function int C3::f(); return 4; endfunction
endpackage
