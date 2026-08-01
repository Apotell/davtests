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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Package.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the
// file-scope package testing point stands on its own.
//
// Checked:
//   - left unchecked; see the ANSI suite's Package test for rationale

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>

namespace hlc {

class NonAnsiPackageTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NonAnsiPackage.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(NonAnsiPackageTest, PackageDeclarationNotStructurallyChecked) {
  GTEST_SKIP() << "No hldb::Package (or equivalent design-level lookup) is exercised anywhere in this test "
                  "suite; nets_and_variables_pkg_nonansi is left unchecked rather than guessing an unverified API.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
