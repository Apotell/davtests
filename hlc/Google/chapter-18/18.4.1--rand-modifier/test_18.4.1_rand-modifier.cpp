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

// Source under test: tests/Google/chapter-18/18.4.1--rand-modifier.sv
//
//   class a;
//       rand int b;
//   endclass
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide). Every point below was
// confirmed by actually compiling and running this test's own
// SetUpTestSuite(), not by reading the existing 18.4.1--rand-modifier.log:
//   - Sec 18.4/18.4.1 ("The rand and randc modifiers"): a class property
//     declared with the "rand" modifier must report vpiRandType ==
//     vpiRand (IEEE VPI constant, Table "vpiRandType" values: vpiNotRand=1,
//     vpiRand=2, vpiRandC=3), modeled here as hldb::Variable::getRandType().
//   - Sec 6.11 (int is a 2-state signed 32-bit integer type): "b"'s
//     declared type resolves to an IntTypespec that is signed.
//   - The class property is modeled as a plain hldb::Variable inside the
//     ClassDefn's own Scope (ClassDefn::getVariables(), inherited from
//     Scope) -- confirmed by actually running this test (class "a" is
//     found via Design::getAllClasses(), and "b" inside its
//     getVariables() with getRandType() == 2, i.e. vpiRand).
//   - The whole file is legal SV; zero compiler diagnostics are expected.
//
// Not checked:
//   - Any runtime/randomize() behavior -- this tool is a compiler/
//     elaborator, not a simulator; there is no way to observe an actual
//     randomized value here.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/class_defn.h>
#include <hldb/variable.h>
#include <hldb/int_typespec.h>
#include <hldb/vpi_user.h>
#include <hldb/sv_vpi_user.h>

namespace hlc {
class RandModifierTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.4.1--rand-modifier.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(RandModifierTest, ClassAndPropertyExist) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr);
}

TEST_F(RandModifierTest, PropertyBIsMarkedRand) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr);

  EXPECT_EQ(b->getRandType(), vpiRand) << "Sec 18.4.1: 'rand int b;' must report vpiRandType == vpiRand";
}

TEST_F(RandModifierTest, PropertyBIsSignedIntTypespec) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr);

  const hldb::IntTypespec *const ts = hldb::getTypespec<hldb::IntTypespec>(b);
  ASSERT_NE(ts, nullptr) << "'int' should resolve to an IntTypespec";
  EXPECT_TRUE(ts->getSigned()) << "Sec 6.11: 'int' is signed";
}

TEST_F(RandModifierTest, CompilerReportsZeroErrors) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
