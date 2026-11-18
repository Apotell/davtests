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

// local and protected class members used from where they are not visible (IEEE 1800-2023 Sec 8.18).

class base;
  local int secret;
  protected int prot;
  int pub;
  local function void lf(); endfunction
  protected function void pf(); endfunction
  function void own(base other);
    other.secret = 1;
    other.prot = 2;
    lf();
  endfunction
  class inner;
    function void f(base b);
      b.secret = 3;
    endfunction
  endclass
endclass
class derived extends base;
  function void g(base b, derived d);
    d.prot = 4;
    b.pub = 5;
    pf();
    b.secret = 6;
  endfunction
endclass
class other;
  function void h(base b);
    b.prot = 7;
    b.lf();
  endfunction
endclass
module top;
  base b = new;
  initial begin
    b.secret = 8;
    b.pub = 9;
    b.pf();
  end
endmodule
