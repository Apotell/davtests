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

// Illegal member types of structures and unions (IEEE 1800-2023 Sec 7.2, 7.2.1, 7.3, 7.3.1, 7.3.2).

typedef struct packed { logic [3:0] n; } pk_t;
typedef struct { int a; } up_t;
typedef real real_t;
module top;
  typedef struct { void v; int i; } s1_t;
  typedef union { void v; int i; } u1_t;
  typedef union tagged { void v; int i; } ok1_t;
  typedef struct packed { real r; bit [7:0] b; } s2_t;
  typedef struct packed { up_t u; pk_t p; int i; } s3_t;
  typedef struct packed { real_t r; } s4_t;
  typedef union packed { shortreal r; bit [31:0] b; } u2_t;
  typedef union tagged packed { real r; bit [9:0] b; } u3_t;
  typedef union { int q[$]; int i; } u4_t;
  typedef union { chandle h; int i; } u5_t;
  typedef union { string s; int i; } u6_t;
  typedef union tagged { int q[$]; chandle h; } ok2_t;
  typedef struct { real r; int q[$]; string s; } ok3_t;
  typedef struct packed { pk_t p; logic [2:0] x; byte b; enum logic {A, B} e; } ok4_t;
  typedef struct packed { pk_t [1:0] pa; pk_t [1:0][2:0] pb; } ok5_t;
endmodule
