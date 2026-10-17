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

// Validates the HLDB model built for tests/ParamArrayUnsizedPattern/dut.sv:
//
//   module test #(
//     parameter int BobaCount = 2,
//     parameter int unsigned NumBobaDrinkersDefault[BobaCount] = '{default: '1},
//     parameter int unsigned NumBobaDrinkers[BobaCount] = '{'1, '1},
//     parameter int unsigned NumBobaDrinkersMulti[BobaCount] = '{BobaCount{'1}}
//   )();
//   endmodule
//
// The point of the fixture is unpacked-array parameters whose size is
// another parameter, each given its value with a different array assignment
// pattern (IEEE 1800-2023 10.9.1) built from the unbased unsized literal '1
// (5.7.1): a 'default:' key, two positional items, and a replication
// ("Each replication shall represent an entire single dimension", 10.9.1).
// The regression this file exists to catch is HLC mis-modeling one of the
// three pattern forms.
//
// What is checked, and why:
//   Module test (23.2)
//     - '()' declares one null port (37.14 detail 10, "module M();")
//     - exactly 4 parameters, all declared in the parameter port list, so
//       none is local (6.20.1)
//     - BobaCount is a signed int with the default Constant "2"
//     - NumBobaDrinkersDefault, NumBobaDrinkers and NumBobaDrinkersMulti each
//       have an unpacked dimension (7.4.2) and the element type int unsigned
//       (an IntTypespec that is not signed, 6.11.3)
//     - that dimension, '[BobaCount]', is "the same as [0:size-1]" (7.4.2):
//       left bound Constant "0", right bound Operation vpiSubOp over RefObj
//       'BobaCount', bound to that Parameter, and Constant "1"
//   The three defaults, unreduced
//     - NumBobaDrinkersDefault: Operation vpiAssignmentPatternOp with exactly
//       1 item, the 'default:' key whose value is the Constant "'1"
//     - NumBobaDrinkers: Operation vpiAssignmentPatternOp with exactly 2
//       items, each the Constant "'1"
//     - NumBobaDrinkersMulti: a replication pattern, Operation
//       vpiMultiAssignmentPatternOp, whose count is RefObj 'BobaCount'
//       bound to that Parameter and whose value is the Constant "'1"
//   Elaboration (23.3.1)
//     - test appears in no instantiation, so on an elaborated design it is
//       the only top-level instance
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: on an elaborated design each array parameter
// holds two elements of 32'hFFFFFFFF. How HLC records the value of an
// unpacked-array parameter is not asserted (see below); only the instance
// tree is checked under getElaborated().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models 'module test ... ();' with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port. TestHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// KNOWN COMPILER BUG (replication in an assignment pattern lost), not a
// defect in this test: HLC models '{BobaCount{'1}} as a plain
// vpiAssignmentPatternOp with the two positional items BobaCount and '1,
// which is the different pattern '{BobaCount, '1} (the parameterized module
// name it builds even reads ".NumBobaDrinkersMulti('{BobaCount, '1})"). The
// replication of 10.9.1 is gone. ReplicationPatternRepeatsOnesBobaCountTimes
// is expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// KNOWN COMPILER BUG (unpacked [size] dimension malformed), not a defect in
// this test: HLC records each '[BobaCount]' dimension as a range whose left
// bound is a vpiSubOp Operation with the single operand BobaCount and which
// has no right bound, so neither bound of [0:BobaCount-1] can be read from
// it. ArrayDimensionsAreZeroToBobaCountMinusOne is expected to fail until
// HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The reduced values of the array parameters. How HLC represents the
//     value of an unpacked array (an Operation of Constants, or another
//     form) is a model convention.
//   - Whether HLC records an unpacked parameter dimension on the
//     parameter's typespec or on the parameter's own ranges is a model
//     convention; either is accepted. The numeric element count of
//     '[BobaCount]' depends on elaboration; the unreduced bounds are
//     asserted instead.
//   - How HLC labels the 'default:' key, and whether it wraps the
//     replication in an outer pattern, are model conventions; the key's
//     value and the replication's count and value are asserted.
//   - The name HLC gives a parameterized module definition: the module is
//     looked up by its definition name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

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
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/tagged_pattern.h>

#include <string_view>

namespace hlc {

class ParamArrayUnsizedPatternTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamArrayUnsizedPattern.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTest() { return hldb::findByDefName<hldb::Module>("test", m_design->getAllModules()); }

  static const hldb::Parameter *getParam(std::string_view name) {
    const hldb::Module *const m = getTest();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, m->getParameters());
  }

  static const hldb::Any *getDefault(std::string_view name) {
    const hldb::Module *const m = getTest();
    if (m == nullptr || m->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *m->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa->getRhs();
    }
    return nullptr;
  }

  // Verifies 'expr' is the unbased unsized literal '1.
  static void ExpectAllOnes(const hldb::Any *expr, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(expr);
    ASSERT_NE(c, nullptr) << what << " should be a Constant";
    EXPECT_EQ(c->getDecompile(), "'1") << "5.7.1: " << what << " is '1, which sets all bits to 1";
  }
};

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamArrayUnsizedPatternTest, TestHasOneNullPort) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr) << "module 'test' not found";
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
}

TEST_F(ParamArrayUnsizedPatternTest, FourOverridableParameters) {
  const hldb::Module *const m = getTest();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 4u);
  for (std::string_view name : {"BobaCount", "NumBobaDrinkersDefault", "NumBobaDrinkers", "NumBobaDrinkersMulti"}) {
    const hldb::Parameter *const p = getParam(name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: '" << name << "' is in the parameter port list, so it is overridable";
  }
}

TEST_F(ParamArrayUnsizedPatternTest, BobaCountIsSignedIntTwo) {
  const hldb::Parameter *const p = getParam("BobaCount");
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "'BobaCount' is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11: 'int' is signed";
  const hldb::Constant *const def = any_cast<hldb::Constant>(getDefault("BobaCount"));
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->getDecompile(), "2");
}

TEST_F(ParamArrayUnsizedPatternTest, ArrayParametersAreUnpackedArraysOfUnsignedInt) {
  for (std::string_view name : {"NumBobaDrinkersDefault", "NumBobaDrinkers", "NumBobaDrinkersMulti"}) {
    const hldb::Parameter *const p = getParam(name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    ASSERT_NE(p->getTypespec(), nullptr) << name;
    const hldb::RefTypespec *elem = p->getTypespec();
    const bool hasRanges = (p->getRanges() != nullptr) && !p->getRanges()->empty();
    if (const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>()) {
      EXPECT_FALSE(at->getPacked()) << "7.4.2: '[BobaCount]' after " << name << " is an unpacked dimension";
      elem = at->getElemTypespec();
    } else {
      EXPECT_TRUE(hasRanges) << "7.4.2: '" << name << "' declares an unpacked dimension";
    }
    ASSERT_NE(elem, nullptr) << name;
    const hldb::IntTypespec *const it = elem->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << "the elements of '" << name << "' are 'int unsigned'";
    EXPECT_FALSE(it->getSigned()) << "6.11.3: 'unsigned' overrides int's default signedness";
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (unpacked
// [size] dimension malformed) in the file header.
TEST_F(ParamArrayUnsizedPatternTest, ArrayDimensionsAreZeroToBobaCountMinusOne) {
  for (std::string_view name : {"NumBobaDrinkersDefault", "NumBobaDrinkers", "NumBobaDrinkersMulti"}) {
    const hldb::Parameter *const p = getParam(name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    const hldb::Range *range = nullptr;
    if (p->getTypespec() != nullptr) {
      if (const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>()) {
        range = at->getRange();
      }
    }
    if ((range == nullptr) && (p->getRanges() != nullptr) && (p->getRanges()->size() == 1u)) {
      range = p->getRanges()->at(0);
    }
    ASSERT_NE(range, nullptr) << "7.4.2: '" << name << "' declares one unpacked dimension";
    const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << "7.4.2: '[BobaCount]' is the same as '[0:BobaCount-1]', so the left bound is 0";
    EXPECT_EQ(left->getDecompile(), "0") << name;
    const hldb::Operation *const right = range->getRightExpr<hldb::Operation>();
    ASSERT_NE(right, nullptr) << "7.4.2: the right bound of '" << name << "' is 'BobaCount-1'";
    EXPECT_EQ(right->getOpType(), vpiSubOp) << name;
    ASSERT_NE(right->getOperands(), nullptr);
    ASSERT_EQ(right->getOperands()->size(), 2u) << "'BobaCount-1' has two operands";
    const hldb::RefObj *const count = any_cast<hldb::RefObj>(right->getOperands()->at(0));
    ASSERT_NE(count, nullptr);
    EXPECT_EQ(count->getName(), "BobaCount");
    EXPECT_EQ(count->getActual(), getParam("BobaCount"));
    const hldb::Constant *const one = any_cast<hldb::Constant>(right->getOperands()->at(1));
    ASSERT_NE(one, nullptr);
    EXPECT_EQ(one->getDecompile(), "1");
  }
}

TEST_F(ParamArrayUnsizedPatternTest, DefaultKeyPatternSetsEveryElementToOnes) {
  const hldb::Operation *const pattern = any_cast<hldb::Operation>(getDefault("NumBobaDrinkersDefault"));
  ASSERT_NE(pattern, nullptr) << "'{default: '1} is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an array assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 1u) << "the single item 'default: '1'";
  const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(0));
  ASSERT_NE(item, nullptr) << "a 'key: value' item";
  ExpectAllOnes(item->getPattern(), "the default value");
}

TEST_F(ParamArrayUnsizedPatternTest, PositionalPatternListsTwoOnes) {
  const hldb::Operation *const pattern = any_cast<hldb::Operation>(getDefault("NumBobaDrinkers"));
  ASSERT_NE(pattern, nullptr) << "'{'1, '1} is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an array assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u) << "two positional items";
  ExpectAllOnes(pattern->getOperands()->at(0), "item 0");
  ExpectAllOnes(pattern->getOperands()->at(1), "item 1");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (replication
// in an assignment pattern lost) in the file header.
TEST_F(ParamArrayUnsizedPatternTest, ReplicationPatternRepeatsOnesBobaCountTimes) {
  const hldb::Operation *op = any_cast<hldb::Operation>(getDefault("NumBobaDrinkersMulti"));
  ASSERT_NE(op, nullptr) << "'{BobaCount{'1}} is an Operation";
  if ((op->getOpType() == vpiAssignmentPatternOp) && (op->getOperands() != nullptr) &&
      (op->getOperands()->size() == 1u)) {
    op = any_cast<hldb::Operation>(op->getOperands()->at(0));
    ASSERT_NE(op, nullptr);
  }
  EXPECT_EQ(op->getOpType(), vpiMultiAssignmentPatternOp) << "10.9.1: a replication inside an assignment pattern";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u) << "the count, then the replicated value";
  const hldb::RefObj *const count = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(count, nullptr) << "the count is 'BobaCount'";
  EXPECT_EQ(count->getName(), "BobaCount");
  EXPECT_EQ(count->getActual(), getParam("BobaCount"));
  ExpectAllOnes(op->getOperands()->at(1), "the replicated value");
}

TEST_F(ParamArrayUnsizedPatternTest, TestIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'test' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "test");
  }
}

TEST_F(ParamArrayUnsizedPatternTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
