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

// Illegal construct: a net-type local variable declared inside a property.
// Per IEEE 1800 clause 16.10, local variables in sequences/properties must
// be variable data types; a net type (e.g. wire) is illegal there since a
// property has no driver context for a net.

module illegal_construct_assertion_net_test(input logic clk, input logic a, input logic b);
  property p_bad_local_net;
    wire local_val; // ILLEGAL: net type as a property-local variable
    @(posedge clk) (a, local_val = b) |=> (b == local_val);
  endproperty

  assert property (p_bad_local_net);
endmodule
