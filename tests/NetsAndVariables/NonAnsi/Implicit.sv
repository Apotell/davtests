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

module nets_and_variables_nonansi(a, b, y);
  input a;
  input b;
  output y;

  // Implicit net declaration by continuous assignment
  // (depends on default_nettype wire)
  assign implicit_net_nonansi = a & b;
endmodule

`default_nettype none
