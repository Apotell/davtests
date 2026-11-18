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

// Virtual interfaces used as ports, interface items or union members (IEEE 1800-2023 Sec 25.9).

interface SBus; logic req; endinterface
interface Wrap;
  virtual SBus inner;
endinterface
module sub (input virtual SBus v);
endmodule
module top;
  typedef union { virtual SBus v; int i; } u_t;
  typedef struct { virtual SBus v; int i; } ok_t;
  virtual SBus ok;
  class C; virtual SBus ok2; endclass
endmodule
