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

// Illegal construct: a continuous assignment at package scope. Per the LRM,
// a package may contain processes only inside a nested checker declaration;
// it cannot itself contain module/interface/program/checker instances. A
// continuous assignment is a driver that needs a process context to live
// in, and a package has none at its own scope.

package illegal_construct_package_test;
  assign pkg_wire = pkg_logic; // ILLEGAL: continuous assignment at package scope
endpackage
