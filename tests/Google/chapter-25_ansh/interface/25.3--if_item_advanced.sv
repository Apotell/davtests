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
:name: if_item_advanced
:description: generate, assertions, clocking blocks and localparams inside an interface
:tags: 25.3
*/

// Checks in this file:
//   CHK1: generate for loop and generate if inside the interface
//   CHK2: property and labeled assert property
//   CHK3: clocking block with input and output
//   CHK4: localparam derived from a parameter with $clog2

// CHK1
interface c1_if #(parameter int N = 4);
  logic [N-1:0] v;
  for (genvar i = 0; i < N; i++) begin : g_bit
    logic b;
    assign b = v[i];
  end
  if (N > 2) begin : g_wide
    logic wide;
  end
endinterface

// CHK2
interface c2_if(input logic clk);
  logic req, gnt;
  property p_req_gnt;
    @(posedge clk) req |-> ##[1:3] gnt;
  endproperty
  a_req_gnt : assert property (p_req_gnt);
endinterface

// CHK3
interface c3_if(input logic clk);
  logic req, gnt;
  clocking cb @(posedge clk);
    input gnt;
    output req;
  endclocking
endinterface

// CHK4
interface c4_if #(parameter int DEPTH = 16);
  localparam int AW = $clog2(DEPTH);
  logic [AW-1:0] ptr;
endinterface

module top;
  logic clk;
  c1_if u1();
  c2_if u2(clk);
  c3_if u3(clk);
  c4_if u4();
endmodule
