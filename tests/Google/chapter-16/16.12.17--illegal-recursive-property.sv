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

// IEEE 1800-2023 Sec 16.12.17: the four restrictions on recursive properties.
module m(input logic clk, b, s1, s2, phase1_prop, phase2_prop);
  property prop_always(p);
    p and (1'b1 |=> prop_always(p));
  endproperty

  property check_phase1;
    s1 |-> (phase1_prop and (1'b1 |=> check_phase2));
  endproperty
  property check_phase2;
    s2 |-> (phase2_prop and (1'b1 |=> check_phase1));
  endproperty

  property illegal_recursion_2(p);
    p and (1'b1 |=> not illegal_recursion_2(p));
  endproperty

  property illegal_recursion_3(p);
    disable iff (b)
    p and (1'b1 |=> illegal_recursion_3(p));
  endproperty

  property legal_3(p);
    disable iff (b) prop_always(p);
  endproperty

  property illegal_recursion_4(p);
    p and (1'b1 |-> illegal_recursion_4(p));
  endproperty

  property strong_eventually(p);
    s_eventually prop_always(p);
  endproperty

  property negated_dependency(p);
    not legal_3(p);
  endproperty

  property fibonacci1 (local input int a, b, n, int fib_sig);
    (n > 0) |-> ((fib_sig == a) and (1'b1 |=> fibonacci1(b, a + b, n - 1, fib_sig)));
  endproperty

  property fibonacci2 (int a, b, n, fib_sig);
    (n > 0) |-> ((fib_sig == a) and (1'b1 |=> fibonacci2(b, a + b, n - 1, fib_sig)));
  endproperty

  A1: assert property (@(posedge clk) prop_always(b));
  A2: assert property (@(posedge clk) not prop_always(b));
endmodule
