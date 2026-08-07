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

// Illegal construct: a `randc` qualifier on a variable declared directly
// inside a module. Same reasoning as the `rand` case: `rand`/`randc` are
// only legal on class properties (and, since IEEE 1800-2017, on variables
// declared inside a checker), not on a variable declared directly inside a
// module.

module illegal_construct_module_randc_test();
  randc logic [1:0] mod_randc; // ILLEGAL: randc qualifier at module scope
endmodule
