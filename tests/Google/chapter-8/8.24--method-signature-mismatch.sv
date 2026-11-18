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

// Out-of-block methods and virtual overrides whose arguments do not match (IEEE 1800-2023 Sec 8.20, 8.24).

class base;
  extern function int f(int a, int b);
  extern task t(input int x);
  extern function void ok(int a, int b = 2);
  virtual function void v1(int a, output int b); endfunction
  virtual function void v2(int a); endfunction
  pure virtual function void v3(int a);
  function void nv(int a); endfunction
endclass
function int base::f(int a, int c); return a; endfunction
task base::t(output int x); endtask
function void base::ok(int a, int b); endfunction
class derived extends base;
  virtual function void v1(int a, output int b); endfunction
  virtual function void v2(int a, int extra); endfunction
  function void v3(int z); endfunction
  function void nv(int a, int b); endfunction
endclass
