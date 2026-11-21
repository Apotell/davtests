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
:name: if_modport_expr
:description: modport expressions name ports bound to a part-select, a constant variable and a literal
:tags: 25.5.4
*/

interface I;
  logic [7:0] r;
  const int x = 1;
  bit R;
  modport A (output .P(r[3:0]), input .Q(x), R);
  modport B (output .P(r[7:4]), input .Q(2), R);
endinterface

module M (interface i);
  initial i.P = i.Q;
endmodule

module top;
  I i1 ();
  M u1 (i1.A);
  M u2 (i1.B);
  initial #1 $display("%b", i1.r); // displays 00100001
endmodule
