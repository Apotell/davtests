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
:name: ic_err_syntax_three_delays
:description: interconnect declared with three delay values (LRM: interconnect #(1,2,3) w5)
:should_fail_because: an interconnect net shall specify at most one delay value
:tags: 6.6.8
*/

// Errors in this file:
//   ERR1: interconnect #(1,2,3) w5 - more than one delay
// Syntax error: kept alone in its own file so it does not hide other checks.

module top;
  interconnect #(1,2,3) w5;
endmodule
