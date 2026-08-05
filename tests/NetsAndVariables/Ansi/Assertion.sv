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

// Demonstrates a legal local variable declared inside a property (IEEE 1800
// clause 16.10): local variables in sequences/properties may use ordinary
// variable data types (here, int) to sample and carry a value across a
// clocking step. See illegal_construct/Illegal_construct_assertion_net.sv
// for why a net type cannot be used the same way.

module nets_and_variables_assertion(input logic clk, input logic a, input logic b);
  property p_sampled_value;
    int local_val; // property-local variable declaration
    @(posedge clk) (a, local_val = b) |=> (b == local_val);
  endproperty

  assert property (p_sampled_value);
endmodule
