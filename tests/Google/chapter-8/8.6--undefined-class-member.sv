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

// Methods and properties used through a class handle that the class does not declare (IEEE 1800-2023 Sec 8.4, 8.6).

class B;
  int bv;
  function void bf(); endfunction
endclass
class C extends B;
  int a;
  extern function void e();
  function void f(); endfunction
  function void h();
    this.g();
    this.f();
    this.bf();
    this.e();
    void'(this.randomize());
  endfunction
endclass
function void C::e(); endfunction
module top;
  C c;
  int x;
  initial begin
    c = new;
    c.g();
    c.bf();
    c.srandom(1);
    x = c.a + c.bv;
    x = c.nope;
    c.nope2 = 1;
  end
endmodule
