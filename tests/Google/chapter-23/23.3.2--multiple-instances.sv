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

// 23.3.2: "One or more module instances (identical copies of a module) can
// be specified in a single module instantiation statement." The LRM's ffnand
// example, with the stray ';' after ff2 replaced by ',' and the second .q()
// connection of ff2 removed (23.3.2.2 forbids connecting a port twice).
module ffnand (output q, qbar, input clear, preset);
endmodule

module top;
  wire out1, out2, out3, in1, in2;
  ffnand ff1 (.q(), .qbar(out1), .clear(in1), .preset(in2)),
         ff2 (.q(), .qbar(out2), .clear(in2), .preset(in1)),
         ff3 (.q(out3), .qbar(), .clear(in1), .preset(in2));
endmodule
