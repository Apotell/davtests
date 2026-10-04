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

// Tests for dut.sv (DynArrayKind)
//   module top;
//      int dynamic1 [];
//      int dynamic2 [][1:0];
//      int dynamic3 [1:0][];
//      int dynamic4 [1:0][][2:0];
//      int assoc [int];
//      int assoc_string [string];
//      int queue [$];
//   endmodule
//
// What to check and why (IEEE 1800-2023 7.5 "Dynamic arrays", 7.8
// "Associative arrays" and 7.10 "Queues", checked before any test code
// was written -- no .log file consulted to decide expected shape; only
// used, per the test-writing guide's narrow exception, to confirm
// which hldb classes are populated for mixed dynamic/static unpacked
// dimensions, since no build/include/hldb checkout is available in
// this repo snapshot; API shapes below match Google/chapter-7's
// existing dynamic/associative/queue/multidimensional array tests,
// which were read first):
//   7.5: "int dynamic1 [];" has a single unsized dimension "[]" -- no
//   bound expression at all, unlike a queue's "[$]".
//   7.4.5/7.5 (nested unpacked dims, "leftmost is outermost, closest to
//   the name varies most rapidly"): "int dynamic2 [][1:0];" is an
//   outer dynamic (unsized) dimension whose element is itself a static
//   [1:0] array; "int dynamic3 [1:0][];" is the reverse -- outer
//   static [1:0], inner dynamic; "int dynamic4 [1:0][][2:0];" nests
//   three dimensions: outer static [1:0], middle dynamic [], inner
//   static [2:0].
//   7.8: "int assoc [int];" is an associative array keyed by (2-state,
//   signed) `int`; "int assoc_string [string];" is keyed by `string`.
//   7.10: "int queue [$];" is an unbounded queue -- ArrayTypespec
//   vpiArrayType=queue(4) whose range has a single unbounded "$" left
//   bound Constant and no right bound.
//   Per IEEE 1800-2023 Sec 6.7/6.8, none of these declare a net-type
//   keyword, so all seven are variable_declarations, not net
//   declarations.
//
// What is checked:
//   - design has module top with exactly 7 variables, none duplicated
//     as Nets
//   - dynamic1: ArrayTypespec dynamic(2), no range, no index typespec,
//     elemTypespec -> IntTypespec (signed)
//   - dynamic2: outer ArrayTypespec dynamic(2) with no range, whose
//     elemTypespec is an inner ArrayTypespec static(1) with range
//     [1:0], whose own elemTypespec is IntTypespec
//   - dynamic3: outer ArrayTypespec static(1) range [1:0], whose
//     elemTypespec is an inner ArrayTypespec dynamic(2) with no range,
//     whose own elemTypespec is IntTypespec
//   - dynamic4: outer static(1) range [1:0] -> middle dynamic(2) no
//     range -> inner static(1) range [2:0] -> IntTypespec
//   - assoc: ArrayTypespec associative(3), index typespec ->
//     IntTypespec (signed), elemTypespec -> IntTypespec
//   - assoc_string: ArrayTypespec associative(3), index typespec ->
//     StringTypespec, elemTypespec -> IntTypespec
//   - queue: ArrayTypespec queue(4), not packed, range with left bound
//     Constant "$" (vpiConstType=unbounded) and no right bound,
//     elemTypespec -> IntTypespec (signed)
//   - none of the 7 variables have an initial value
//   - top has no processes and no continuous assignments
//   - compiler reports zero errors (this file is fully legal per 7.5/
//     7.8/7.10)
//
// What is NOT checked and why:
//   - none -- dut.sv is declarations-only with no runtime behavior to
//     defer to simulation

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DynArrayKindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DynArrayKind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ArrayTypespec *getArrayTypespec(const std::string &varName) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getVariables() == nullptr) return nullptr;
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(varName, top->getVariables());
    if (v == nullptr || v->getTypespec() == nullptr) return nullptr;
    return v->getTypespec()->getActual<hldb::ArrayTypespec>();
  }
};

// --- module ----

TEST_F(DynArrayKindTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(DynArrayKindTest, ModuleHasSevenVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 7u);
}

TEST_F(DynArrayKindTest, NoneOfTheVariablesAreDuplicatedAsNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (top->getNets() == nullptr) return;
  static const char *const names[] = {"dynamic1", "dynamic2", "dynamic3", "dynamic4", "assoc", "assoc_string",
                                       "queue"};
  for (const char *const name : names) {
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, top->getNets()), nullptr)
        << name << " has no net-type keyword and must not also appear as a Net";
  }
}

TEST_F(DynArrayKindTest, NoneOfTheVariablesHaveAnInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  for (const hldb::Variable *const v : *top->getVariables()) {
    EXPECT_EQ(v->getValue(), nullptr) << v->getName() << " has no initializer";
  }
}

// --- dynamic1: int dynamic1 []; ----

TEST_F(DynArrayKindTest, Dynamic1IsDynamicArrayOfInt) {
  const hldb::ArrayTypespec *const at = getArrayTypespec("dynamic1");
  ASSERT_NE(at, nullptr) << "'dynamic1' should resolve to an ArrayTypespec";
  EXPECT_EQ(at->getArrayType(), vpiDynamicArray);
  EXPECT_EQ(at->getRange(), nullptr) << "'[]' is an unsized dimension with no bound expression";
  EXPECT_EQ(at->getIndexTypespec(), nullptr) << "only associative arrays carry an index typespec";
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_TRUE(elem->getSigned());
}

// --- dynamic2: int dynamic2 [][1:0]; ----

TEST_F(DynArrayKindTest, Dynamic2OuterIsDynamicWithNoRange) {
  const hldb::ArrayTypespec *const outer = getArrayTypespec("dynamic2");
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getArrayType(), vpiDynamicArray);
  EXPECT_EQ(outer->getRange(), nullptr);
}

TEST_F(DynArrayKindTest, Dynamic2InnerIsStaticRangeOneToZero) {
  const hldb::ArrayTypespec *const outer = getArrayTypespec("dynamic2");
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  const hldb::ArrayTypespec *const inner = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(inner, nullptr) << "the inner [1:0] dimension of 'dynamic2' should be a static ArrayTypespec";
  EXPECT_EQ(inner->getArrayType(), vpiStaticArray);
  ASSERT_NE(inner->getRange(), nullptr);
  EXPECT_EQ(inner->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(inner->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  ASSERT_NE(inner->getElemTypespec(), nullptr);
  EXPECT_NE(inner->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- dynamic3: int dynamic3 [1:0][]; ----

TEST_F(DynArrayKindTest, Dynamic3OuterIsStaticRangeOneToZero) {
  const hldb::ArrayTypespec *const outer = getArrayTypespec("dynamic3");
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getArrayType(), vpiStaticArray);
  ASSERT_NE(outer->getRange(), nullptr);
  EXPECT_EQ(outer->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(outer->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(DynArrayKindTest, Dynamic3InnerIsDynamicWithNoRange) {
  const hldb::ArrayTypespec *const outer = getArrayTypespec("dynamic3");
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  const hldb::ArrayTypespec *const inner = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(inner, nullptr) << "the inner [] dimension of 'dynamic3' should be a dynamic ArrayTypespec";
  EXPECT_EQ(inner->getArrayType(), vpiDynamicArray);
  EXPECT_EQ(inner->getRange(), nullptr);
  ASSERT_NE(inner->getElemTypespec(), nullptr);
  EXPECT_NE(inner->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- dynamic4: int dynamic4 [1:0][][2:0]; ----

TEST_F(DynArrayKindTest, Dynamic4OuterIsStaticRangeOneToZero) {
  const hldb::ArrayTypespec *const outer = getArrayTypespec("dynamic4");
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getArrayType(), vpiStaticArray);
  ASSERT_NE(outer->getRange(), nullptr);
  EXPECT_EQ(outer->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
  EXPECT_EQ(outer->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(DynArrayKindTest, Dynamic4MiddleIsDynamicWithNoRange) {
  const hldb::ArrayTypespec *const outer = getArrayTypespec("dynamic4");
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  const hldb::ArrayTypespec *const middle = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(middle, nullptr) << "the middle [] dimension of 'dynamic4' should be a dynamic ArrayTypespec";
  EXPECT_EQ(middle->getArrayType(), vpiDynamicArray);
  EXPECT_EQ(middle->getRange(), nullptr);
}

TEST_F(DynArrayKindTest, Dynamic4InnerIsStaticRangeTwoToZero) {
  const hldb::ArrayTypespec *const outer = getArrayTypespec("dynamic4");
  ASSERT_NE(outer, nullptr);
  const hldb::ArrayTypespec *const middle = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(middle, nullptr);
  ASSERT_NE(middle->getElemTypespec(), nullptr);
  const hldb::ArrayTypespec *const inner = middle->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(inner, nullptr) << "the inner [2:0] dimension of 'dynamic4' should be a static ArrayTypespec";
  EXPECT_EQ(inner->getArrayType(), vpiStaticArray);
  ASSERT_NE(inner->getRange(), nullptr);
  EXPECT_EQ(inner->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "2");
  EXPECT_EQ(inner->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  ASSERT_NE(inner->getElemTypespec(), nullptr);
  EXPECT_NE(inner->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- assoc: int assoc [int]; ----

TEST_F(DynArrayKindTest, AssocIsAssociativeKeyedByInt) {
  const hldb::ArrayTypespec *const at = getArrayTypespec("assoc");
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiAssocArray);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  const hldb::IntTypespec *const idx = at->getIndexTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(idx, nullptr) << "'[int]' should resolve the index typespec to IntTypespec";
  EXPECT_TRUE(idx->getSigned());
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- assoc_string: int assoc_string [string]; ----

TEST_F(DynArrayKindTest, AssocStringIsAssociativeKeyedByString) {
  const hldb::ArrayTypespec *const at = getArrayTypespec("assoc_string");
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiAssocArray);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "'[string]' should resolve the index typespec to StringTypespec";
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- queue: int queue [$]; ----

TEST_F(DynArrayKindTest, QueueIsQueueArrayType) {
  const hldb::ArrayTypespec *const at = getArrayTypespec("queue");
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(DynArrayKindTest, QueueRangeLeftIsUnboundedDollarWithNoRightExpr) {
  const hldb::ArrayTypespec *const at = getArrayTypespec("queue");
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr) << "queue ArrayTypespec has no range (expected unsized '$' bound)";
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
  EXPECT_EQ(at->getRange()->getRightExpr(), nullptr) << "an unsized dimension has a single '$' bound, no right bound";
}

TEST_F(DynArrayKindTest, QueueElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getArrayTypespec("queue");
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_TRUE(elem->getSigned());
}

// --- module structural completeness ----

TEST_F(DynArrayKindTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(DynArrayKindTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(DynArrayKindTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
