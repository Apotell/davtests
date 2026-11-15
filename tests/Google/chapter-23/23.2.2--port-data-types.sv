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

// 23.2.2: "A port can be a declaration of an interface, an event, or a
// variable or net of any allowed data type, including an array, a
// structure, or a union." Adapted from the mh1 example in 23.2.2.
typedef struct {
  bit isfloat;
  union { int i; shortreal f; } n;
} tagged_st;

module mh1(input var int in1,
           input var shortreal in2,
           output tagged_st out);
endmodule
