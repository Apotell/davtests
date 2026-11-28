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

// 23.2.4, Syntax 23-5: non_port_module_item includes generate_region, and
// module_common_item includes loop_generate_construct and
// conditional_generate_construct.
module m;
  generate
    for (genvar i = 0; i < 2; i++) begin : g_loop
      wire w;
    end
  endgenerate
  if (1) begin : g_if
    wire x;
  end
  case (1)
    1: begin : g_case
      wire y;
    end
  endcase
endmodule
