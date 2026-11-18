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

// Associative array index types that declare a type or contain a real (IEEE 1800-2023 Sec 7.8, 7.8.5).

typedef struct { real r; } Rt;
typedef struct { int a; } It;
class C; endclass
module top;
  int a1[struct {int a;}];
  int a2[real];
  int a3[shortreal];
  int a4[Rt];
  int a5[int];
  int a6[string];
  int a7[C];
  int a8[*];
  int a9[It];
  int a10[enum {X, Y}];
endmodule
