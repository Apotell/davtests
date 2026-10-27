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

// ---------------------------------------------------------------------------
// SV source under test: tests/Google/chapter-18/18.5.14--soft-constraints_2.sv
// (IEEE 1800-2017 18.5.14 == IEEE 1800-2023 18.5.13 / 18.5.13.1 "Soft
// constraints" / "Soft constraint priorities", which is the text used here)
//
//   /*
//   :should_fail_because: Soft constraints can only be specified on random
//                         variables; they may not be specified for randc
//                         variables.
//   :type: simulation
//   */
//   class a;
//       randc int b;
//
//       constraint c {
//           soft b > 4;
//           soft b < 12; }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is ILLEGAL. IEEE 1800-2023 Sec 18.5.13.1: "Soft constraints
//      can only be specified on random variables; they may not be specified
//      for randc variables." b is 'randc' and both items of c are soft
//      constraints on b, so the compiler shall report an error.
//      - coarse check: at least one fatal/syntax/error diagnostic exists.
//      - specific check: an "illegal constraint" diagnostic
//        (COMP_ILLEGAL_CONSTRAINT) is reported for constraint 'c'. This is
//        the same diagnostic HLC already uses for the sibling randc rule of
//        18.5.9 (randc in solve...before), where it names the constraint.
//   2. The model still shows WHY it is illegal:
//      - b is 'randc int' -> vpiRandType == vpiRandC (not vpiRand), int.
//      - c holds two soft items in order: [0] soft "b > 4" (vpiGtOp),
//        [1] soft "b < 12" (vpiLtOp), each with vpiSoft == 1 and b bound to
//        the randc variable a::b. The 'soft' flag must NOT be silently
//        dropped to make the file look legal.
//   3. Constraint c: parent 'a', non-static (vpiAutomatic == 1), in-body.
//
// CHECKED (TEST_F below):
//   - items 1 to 3.
//
// NOT CHECKED (and why):
//   - Exact error wording/location: not defined by the standard; if HLC
//     reports this rule with a different event type, the specific
//     check is the one to review (the coarse check still applies).
//   - Run-time behavior of randc cycling: simulation only.
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace {
hldb::AnyType typespecKind(const hldb::Variable *var) {
  const hldb::RefTypespec *const ref = var->getTypespec();
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

// Checks that 'item' is the relation "<var> <opType> <literal>" with the given soft flag.
void expectRelationOnLiteral(const hldb::Any *item, int32_t opType, const hldb::Variable *var, std::string_view literal,
                             bool soft) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(item);
  ASSERT_NE(op, nullptr) << "constraint item should be an Operation";
  EXPECT_EQ(op->getOpType(), opType);
  EXPECT_EQ(op->getSoft(), soft) << "soft flag (37.38 vpiSoft)";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(ref, nullptr) << "left operand should reference '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "'" << ref->getName() << "' binds to the variable";
  const hldb::Constant *const value = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(value, nullptr) << "right operand should be the literal " << literal;
  EXPECT_EQ(value->getDecompile(), literal);
}
}  // namespace

namespace hlc {
class SoftConstraints2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.14--soft-constraints_2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClassA() const { return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses()); }
};

// Check 1 (coarse): soft constraints on a randc variable must produce a diagnostic.
TEST_F(SoftConstraints2Test, CompilerRejectsSoftConstraintOnRandcVariable) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_GT(stats.m_fatals + stats.m_syntaxes + stats.m_errors, 0)
      << "'soft b > 4' with b declared randc is illegal: IEEE 1800-2023 Sec 18.5.13.1 says soft constraints "
         "may not be specified for randc variables";
}

// Check 1 (specific): the diagnostic is an illegal-constraint error on constraint c.
TEST_F(SoftConstraints2Test, IllegalConstraintErrorOnConstraintC) {
  EXPECT_NE(findEvent(EventType::CompIllegalConstraint, "c"), nullptr)
      << "expected an illegal-constraint error for constraint 'c' (soft on randc b, IEEE 1800-2023 Sec 18.5.13.1)";
}

// Check 2: b is 'randc int'.
TEST_F(SoftConstraints2Test, BIsRandcInt) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr) << "variable 'b' not found";
  EXPECT_EQ(b->getRandType(), vpiRandC) << "b is declared 'randc' (not 'rand')";
  EXPECT_EQ(typespecKind(b), hldb::AnyType::IntTypespec);
}

// Check 2: c keeps both soft items on the randc variable b.
TEST_F(SoftConstraints2Test, CHasTwoSoftItemsOnRandcB) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  ASSERT_NE(c, nullptr) << "constraint 'c' not found";
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 2u);
  expectRelationOnLiteral(c->getConstraintItems()->at(0), vpiGtOp, b, "4", true);
  expectRelationOnLiteral(c->getConstraintItems()->at(1), vpiLtOp, b, "12", true);
}

// Check 3: constraint c properties.
TEST_F(SoftConstraints2Test, ConstraintCProperties) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 1u);
  const hldb::Constraint *const c = cls->getConstraints()->at(0);
  EXPECT_EQ(c->getName(), "c");
  EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(cls));
  EXPECT_TRUE(c->getAutomatic()) << "not static, so vpiAutomatic is 1 (37.34 detail 1)";
  EXPECT_EQ(c->getAccessType(), 0) << "in-body, not extern";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
