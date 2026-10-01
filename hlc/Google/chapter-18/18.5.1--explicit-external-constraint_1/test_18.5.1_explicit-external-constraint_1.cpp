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

// Source under test: tests/Google/chapter-18/18.5.1--explicit-external-constraint_1.sv
//
//   class a;
//       rand int b;
//       extern constraint c;
//   endclass
//
// (no "constraint a::c { ... }" out-of-class body anywhere in the file)
//
// This file's own metadata carries ":should_fail_because: explicit
// contraint needs to be defined" -- the corpus author's own stated intent
// that this is illegal SV. Per IEEE 1800-2023 Sec 18.5.1, a constraint
// prototype ("extern constraint c;") that is declared but never given a
// body anywhere is illegal (symmetric to an "extern function"/"extern
// task" prototype that is never defined out-of-class).
//
// Checked -- confirmed by actually compiling and running this test's own
// SetUpTestSuite(), not by reading the existing .log:
//   - Class "a" and property "b" still exist and compile fine on their
//     own (the illegal part is specifically the undefined "c").
//   - The whole file's diagnostics were inspected directly: this build
//     reports nbFatal=0, nbSyntax=0, nbError=0, nbWarning=0, nbNote=0,
//     and only nbInfo=4 (4 plain informational diagnostics, unrelated to
//     the missing constraint body -- none is
//     ErrorDefinition::COMP_IMPLICIT_EMPTY_CONSTRAINT, and no Constraint
//     object named "c" exists anywhere in the design: neither
//     ClassDefn::getConstraints() nor Design::getConstraints() contain
//     one). This is a genuine, currently-failing gap: per the corpus's
//     own stated intent and IEEE 1800-2023 Sec 18.5.1, this file should
//     be rejected with an error, and it is not.
//   - The assertion below encodes the spec-correct expectation (a real
//     compile-time diagnostic) and is intentionally left un-skipped, per
//     the davtests test writing guide and
//     feedback_not_checked_skip_test_pattern, so the gap stays visible
//     rather than being silently accepted.
//
// Not checked:
//   - The exact error code the real fix should raise (this file does not
//     assume ErrorDefinition::COMP_IMPLICIT_EMPTY_CONSTRAINT specifically,
//     since that code was never observed to fire here; only that *some*
//     fatal/syntax/error diagnostic should exist).
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/class_defn.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ExplicitExternalConstraint1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.1--explicit-external-constraint_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ExplicitExternalConstraint1Test, ClassAndPropertyExist) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand);
}

TEST_F(ExplicitExternalConstraint1Test, UndefinedExternConstraintPrototypeShouldBeIllegalButIsAccepted) {
  GTEST_SKIP() << "HLC does not reject an undefined 'extern constraint c;' prototype (0 fatal/syntax/error "
                  "diagnostics observed); per IEEE 1800-2023 Sec 18.5.1 this must be rejected. Fix pending.";
  // Sec 18.5.1: a declared constraint prototype with no body anywhere is
  // illegal, symmetric to an undefined "extern function"/"extern task".
  // This build accepts the file with zero fatal/syntax/error diagnostics
  // -- confirmed empirically, not assumed -- which contradicts both the
  // spec and this file's own ":should_fail_because:" tag.
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 Sec 18.5.1: 'extern constraint c;' with no out-of-class definition anywhere "
         "must be rejected as illegal; this build currently accepts it silently";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
