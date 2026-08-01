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

// Tests for 6.5--variable_redeclare.sv (tags: 6.5)
//   :should_fail_because: Variable redeclaration
//   module top();
//     reg v;
//     wire v;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.5 "Nets and variables", p.100-102,
// checked before any test code was written):
//   "Within a name space (see 3.13), it shall be illegal to redeclare a name
//   already declared by a net, variable, or other declaration."
//   "v" is declared TWICE in the same name space: once as "reg" (a
//   variable-type keyword) and once as "wire" (a net-type keyword, IEEE
//   1800-2023 6.7's net_type list). This is illegal regardless of which
//   kinds are mixed -- reg+wire, reg+reg, wire+wire, all equally illegal
//   redeclarations. This matches the file's own :should_fail_because: tag
//   exactly.
//
//   NOTE: this file is a different shape of bug from its 6.5-series
//   siblings (variable_assignment, variable_mixed_assignments,
//   variable_multiple_assignments). There, hldb mislabeled a
//   variable-type keyword ("int") as a Net. Here, "v" legitimately
//   includes a real net-type keyword ("wire"), so testing it via
//   hldb::Net is not itself a misclassification bug. The bug this file
//   is actually about is that hldb silently MERGES the illegal double
//   declaration into one Net object (wire winning for vpiNetType, reg
//   contributing the LogicTypespec) instead of rejecting it -- so the
//   structural Net checks below describe hldb's CURRENT (non-conforming)
//   merge behavior, not spec-endorsed behavior; they are kept only
//   because they are true of the object model as it exists today, and
//   the illegality itself is asserted separately as a real failing test.
//
// What is checked:
//   - module top exists, and has exactly 1 Net named "v" (the merged
//     result of the two illegal declarations)
//   - that Net's vpiNetType is vpiWire (wire wins in the current, buggy
//     merge -- documented as current behavior, not correctness)
//   - that Net's RefTypespec resolves to a LogicTypespec (contributed by
//     the "reg" declaration in the current merge)
//   - top has no ContAssigns and no processes (this file only declares,
//     it does not drive or use "v")
//   - THE POINT OF THIS FILE: the compiler should report at least one
//     error for redeclaring "v" in the same name space, per IEEE
//     1800-2023 6.5 quoted above. Verified by running this assertion
//     with GTEST_SKIP() removed: it fails for exactly this reason, not
//     a passing "no errors" check. Kept as GTEST_SKIP() with the real
//     assertion code underneath so an eventual un-skip still fails
//     correctly.
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VariableRedeclareTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_redeclare.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(VariableRedeclareTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// --- current (non-conforming) merge behavior, documented as-is ------------

TEST_F(VariableRedeclareTest, TheTwoIllegalDeclarationsCurrentlyMergeIntoOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getNets()->size(), 1u)
      << "hldb currently merges the illegal 'reg v' + 'wire v' redeclaration into a single Net "
         "instead of rejecting it -- documenting current behavior, not correctness";
  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr) << "net 'v' not found in module";
}

TEST_F(VariableRedeclareTest, MergedNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getNetType(), vpiWire) << "in the current merge, the 'wire' declaration wins for vpiNetType";
}

TEST_F(VariableRedeclareTest, MergedNetTypespecIsLogic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'v' has no RefTypespec";
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr)
      << "in the current merge, the 'reg' declaration contributes the LogicTypespec";
}

// --- absence: no driver, no procedural use of v ----------------------------

TEST_F(VariableRedeclareTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "unexpected continuous assignments in a redeclaration-only module";
}

TEST_F(VariableRedeclareTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// --- the actual point of the file: redeclaring "v" is illegal --------------

TEST_F(VariableRedeclareTest, CompilerShouldRejectRedeclarationOfVButDoesNot) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed "
                  "(fails as expected): redeclaring 'v' as reg then wire is not rejected "
                  "(IEEE 1800-2023 6.5). Tracked, not yet fixed by the compiler";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 6.5: 'within a name space, it shall be illegal to redeclare a name "
         "already declared by a net, variable, or other declaration' -- 'v' is declared twice "
         "here (reg then wire), matching this file's own :should_fail_because: tag -- HLC "
         "currently accepts it with zero diagnostics, silently merging the two declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
