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

// 23.2.2.1 Example 2: "Ports connected to internal nets of a different
// name". "Nets {c,d} receive the first port bits." "Name 'f' is declared
// inside the module." "Name 'e' is defined outside the module."
module complex_ports({c, d}, .e(f));
  input c, d;
  input f;
endmodule
