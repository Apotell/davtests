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

// Illegal construct: a `rand` qualifier on a variable declared directly
// inside a module. Per the LRM, `rand`/`randc` random-variable qualifiers
// are only legal on class properties (and, since IEEE 1800-2017, on
// variables declared inside a checker); a module has no randomize() context,
// so declaring a rand variable at module scope is illegal.

module illegal_construct_module_rand_test();
  rand logic [3:0] mod_rand; // ILLEGAL: rand qualifier at module scope
endmodule
