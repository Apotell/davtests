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

`default_nettype wire

// Example summary:
// - input without type is an implicit net port (wire by default)
// - output without type is an implicit net port (wire by default)

module nets_and_variables_nonansi(a, b, y);
  // Port: implicit net port (wire)
  input a;
  // Port: implicit net port (wire)
  input b;
  // Port: implicit net port (wire)
  output y;
endmodule

`default_nettype none
