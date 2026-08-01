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

// Tests for tests/NetsAndVariables/illegal_construct/Illegal_construct_package.sv
//
// Illegal construct: a continuous assignment at package scope. Per the LRM,
// a package may contain processes only inside a nested checker declaration,
// so it has no process context of its own to drive a net; an `assign`
// statement directly inside a package is illegal.
//
// Checked:
//   - the compiler reports at least one syntax/error diagnostic for this
//     file (the illegal construct is rejected, not silently accepted)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class IllegalConstructPackageTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "IllegalConstructPackage.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalConstructPackageTest, ContinuousAssignmentAtPackageScopeIsRejected) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "a continuous assignment ('assign pkg_wire = pkg_logic;') directly inside a package is illegal: "
         "a package may only contain processes inside a nested checker declaration, so it has no process "
         "context to drive a net at its own scope.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
