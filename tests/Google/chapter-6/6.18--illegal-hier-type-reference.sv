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

// Illegal hierarchical references to a type (IEEE 1800-2023 Sec 6.18, 25.10).

interface I;
  typedef logic [3:0] T;
endinterface
module M;
  typedef int U;
endmodule
module top(I p);
  I i();
  M m();
  typedef p.T ok_t;
  typedef i.T bad1_t;
  typedef m.U bad2_t;
endmodule
