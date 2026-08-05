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

// Tests for tests/NetsAndVariables/illegal_construct/Illegal_construct_assertion_net.sv
//
// Illegal construct: a net-type (wire) local variable declared inside a
// property. Per IEEE 1800 clause 16.10, local variables in sequences/
// properties must be variable data types; a net type is illegal there since
// a property has no driver context for a net.
//
// Checked:
//   - the compiler reports at least one syntax/error diagnostic for this
//     file (the illegal construct is rejected, not silently accepted)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class AssertionNetTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AssertionNet.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AssertionNetTest, NetLocalVariableInPropertyIsRejected) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "a net-type (wire) local variable declared inside a property is illegal: local variables in "
         "sequences/properties must be variable data types, and a property has no driver context for a net.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
