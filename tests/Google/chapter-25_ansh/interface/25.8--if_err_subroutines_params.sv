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
:name: if_err_subroutines_params
:description: illegal interface subroutine calls and parameter overrides
:should_fail_because: subroutine calls must match the interface declarations and only existing non-local parameters can be overridden
:tags: 25.8
*/

// Errors in this file:
//   ERR1: call to a function the interface does not declare
//   ERR2: interface function called with too many arguments
//   ERR3: override of a localparam
//   ERR4: named override of a parameter that does not exist

interface tf_if;
  function automatic int twice(int v);
    return 2 * v;
  endfunction
endinterface

interface lp_if;
  localparam int W = 4;
  logic [W-1:0] data;
endinterface

interface p_if #(parameter int W = 8);
  logic [W-1:0] data;
endinterface

module top;
  tf_if u_tf();
  int r;
  // ERR1
  initial u_tf.no_such_func();
  // ERR2
  initial r = u_tf.twice(1, 2);
  // ERR3
  lp_if #(.W(8)) u3();
  // ERR4
  p_if #(.WIDTH(16)) u4();
endmodule
