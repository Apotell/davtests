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

// Malformed `pragma protect envelopes (IEEE 1800-2023 Sec 22.11, 34.3).

module top;
`pragma protect begin
  wire a;
`pragma protect end
`pragma protect end
`pragma protect end_protected
`pragma protect author = "x"
`pragma protect begin
  wire b;
endmodule
