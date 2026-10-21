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

// Validates the HLDB model built for tests/ParamOverload1/dut.sv:
//
//   module dut(output int x);
//      parameter int P [5] = '{1, 2, 3, 4, 5};
//      assign x = P[2];
//   endmodule
//
//   module top(output int o);
//      dut #(.P('{1000, 2000})) u_dut1(.x(o));
//      dut u_dut2(.x(o));
//      dut #(.P(3000)) u_dut3(.x(o));
//   endmodule // top
//
// The point of the fixture is an unpacked array parameter (6.20.2: "A
// parameter can also be declared as an aggregate type, such as an unpacked
// array") overridden by values that do not fit it. The regression this file
// exists to catch is HLC silently accepting those overrides. The fixture is
// illegal in three independent ways:
//   - u_dut1 overrides P, an array of 5 elements, with a 2-element pattern.
//     10.9.1: "The expressions shall match element for element".
//   - u_dut3 overrides P with the integer 3000. 6.20.2: "In an assignment
//     to, or override of, a parameter with an explicit type declaration, the
//     type of the right-hand expression shall be assignment compatible with
//     the declared type", and 7.6: "A packed array cannot be directly assigned
//     to an unpacked array without an explicit cast".
//   - o is a variable (23.2.2.3: an output declared with an explicit data
//     type defaults to a variable) connected to the output port of three
//     instances. 23.3.3.2: "A continuous assignment shall be implied when a
//     variable is connected to the output port of an instance", and 6.5:
//     "it shall be an error to have multiple continuous assignments ...
//     writing to any term in the expansion of the longest static prefix of a
//     variable".
//
// What is checked, and why:
//   Module dut
//     - exactly 1 port, x, an output whose kind is variable and whose type
//       is int (23.2.2.3)
//     - exactly 1 parameter, P, overridable (dut has no parameter port list,
//       6.20.1), an unpacked array of int with one dimension [5], which
//       7.4.2 defines as "the same as [0:size-1]", so [0:4]
//     - P defaults to a positional pattern of the 5 literals 1 to 5 (10.9.1)
//     - one continuous assignment of the element select P[2] to x, with P
//       and x bound to dut's declarations
//   Module top
//     - exactly 1 port, o, an output variable of type int (23.2.2.3)
//     - exactly 3 instances of dut, each connecting x to o
//     - u_dut1 overrides P with a 2-element pattern of 1000 and 2000;
//       u_dut2 writes no override; u_dut3 overrides P with the literal 3000
//   Diagnostics
//     - after elaboration, the three violations above are reported as at
//       least 3 errors
//     - there is no fatal and no syntax diagnostic
//
// Reduction and elaboration: the override checks are elaboration-time
// checks, so the error count is asserted only when the design is elaborated.
// No elaborated value is checked: the design is illegal, so the standard
// gives none.
//
// KNOWN COMPILER BUG (unpacked [size] dimension malformed), not a defect in
// this test: 7.4.2 says "[size] shall mean the same as [0:size-1]", so P's
// dimension is [0:4]. HLC records '[5]' as a range with no left bound whose
// right bound is a subtraction with the single operand 5, so neither bound
// can be read from it. PIsArrayOfIntIndexedZeroToFour is expected to fail
// until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Which errors HLC emits and in what order is a tool convention; only
//     the lower bound of one error per violation is asserted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class ParamOverload1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamOverload1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getP() {
    const hldb::Module *const dut = getModule("dut");
    if (dut == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("P", dut->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::RefInstance *getInstance(std::string_view name) {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(name, top->getRefInstances());
  }

  // The overrides the unelaborated instance writes.
  static const std::vector<hldb::ParamAssign *> *getOverrides(const hldb::RefInstance *inst) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr) return nullptr;
    return mt->getParamAssigns();
  }

  // The variable the port lowConn of 'port' is bound to.
  static const hldb::Variable *getPortVariable(const hldb::Port *port) {
    const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
    if (low == nullptr) return nullptr;
    return low->getActual<hldb::Variable>();
  }

  // Verifies module 'name' has exactly 1 port, 'port', an output int
  // variable.
  static void ExpectOutputIntVariablePort(std::string_view name, std::string_view port) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr);
    ASSERT_EQ(m->getPorts()->size(), 1u) << name << " declares one port";
    const hldb::Port *const p = m->getPorts()->at(0);
    EXPECT_EQ(p->getName(), port);
    EXPECT_EQ(p->getDirection(), vpiOutput) << "'output int " << port << "'";
    const hldb::Variable *const v = getPortVariable(p);
    ASSERT_NE(v, nullptr) << "23.2.2.3: an output with an explicit data type defaults to a variable";
    ASSERT_NE(v->getTypespec(), nullptr);
    EXPECT_NE(v->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'" << port << "' is an int";
  }

  // Verifies 'any' is a positional assignment pattern of the literals
  // 'values'.
  static void ExpectPositionalPattern(const hldb::Any *any, const std::vector<std::string_view> &values) {
    const hldb::Operation *const pattern = any_cast<hldb::Operation>(any);
    ASSERT_NE(pattern, nullptr) << "the value is an assignment pattern";
    EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.1";
    ASSERT_NE(pattern->getOperands(), nullptr);
    ASSERT_EQ(pattern->getOperands()->size(), values.size());
    for (size_t i = 0; i < values.size(); ++i) {
      const hldb::Constant *const c = any_cast<hldb::Constant>(pattern->getOperands()->at(i));
      ASSERT_NE(c, nullptr) << "item " << i << " is a literal";
      EXPECT_EQ(c->getDecompile(), values[i]) << "item " << i;
    }
  }
};

// ---------------------------------------------------------------------------
// module dut
// ---------------------------------------------------------------------------

TEST_F(ParamOverload1Test, DutPortXIsOutputIntVariable) { ExpectOutputIntVariablePort("dut", "x"); }

TEST_F(ParamOverload1Test, PIsOverridableArrayOfInt) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParameters(), nullptr);
  EXPECT_EQ(dut->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getP();
  ASSERT_NE(p, nullptr) << "parameter 'P' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: dut has no parameter port list, so 'P' is overridable";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "6.20.2: 'P' is an unpacked array";
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "the elements are int";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (unpacked
// [size] dimension malformed) in the file header.
TEST_F(ParamOverload1Test, PIsArrayOfIntIndexedZeroToFour) {
  const hldb::Parameter *const p = getP();
  ASSERT_NE(p, nullptr);
  const hldb::Range *range = nullptr;
  if (p->getTypespec() != nullptr) {
    if (const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>()) {
      range = at->getRange();
    }
  }
  ASSERT_NE(range, nullptr) << "7.4.2: 'P' declares one unpacked dimension";
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr) << "7.4.2: '[5]' is the same as '[0:5-1]', so the left bound is 0";
  EXPECT_EQ(left->getDecompile(), "0");
  const hldb::Any *const right = range->getRightExpr();
  ASSERT_NE(right, nullptr) << "7.4.2: the right bound of '[5]' is 5-1";
  if (const hldb::Constant *const c = any_cast<hldb::Constant>(right)) {
    EXPECT_EQ(c->getDecompile(), "4") << "7.4.2: the right bound is 4";
  } else {
    const hldb::Operation *const sub = any_cast<hldb::Operation>(right);
    ASSERT_NE(sub, nullptr) << "7.4.2: the right bound is 4 or the expression 5-1";
    EXPECT_EQ(sub->getOpType(), vpiSubOp);
    ASSERT_NE(sub->getOperands(), nullptr);
    ASSERT_EQ(sub->getOperands()->size(), 2u) << "'5-1' has two operands";
    const hldb::Constant *const five = any_cast<hldb::Constant>(sub->getOperands()->at(0));
    ASSERT_NE(five, nullptr);
    EXPECT_EQ(five->getDecompile(), "5");
    const hldb::Constant *const one = any_cast<hldb::Constant>(sub->getOperands()->at(1));
    ASSERT_NE(one, nullptr);
    EXPECT_EQ(one->getDecompile(), "1");
  }
}

TEST_F(ParamOverload1Test, PDefaultsToOneThroughFive) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("dut"), "P");
  ASSERT_NE(pa, nullptr);
  ExpectPositionalPattern(pa->getRhs(), {"1", "2", "3", "4", "5"});
}

TEST_F(ParamOverload1Test, XIsAssignedElementTwoOfP) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);
  ASSERT_EQ(dut->getContAssigns()->size(), 1u) << "dut writes one continuous assignment";
  const hldb::ContAssign *const ca = dut->getContAssigns()->at(0);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the LHS is the name 'x'";
  EXPECT_EQ(lhs->getName(), "x");
  ASSERT_NE(dut->getPorts(), nullptr);
  ASSERT_EQ(dut->getPorts()->size(), 1u);
  EXPECT_EQ(lhs->getActual(), getPortVariable(dut->getPorts()->at(0))) << "'x' is dut's output variable";
  const hldb::BitSelect *const sel = ca->getRhs<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr) << "11.5.1: 'P[2]' selects one element";
  const hldb::RefObj *const base = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "P");
  ASSERT_NE(getP(), nullptr);
  EXPECT_EQ(base->getActual(), getP()) << "'P' is dut's own parameter";
  const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "2");
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamOverload1Test, TopPortOIsOutputIntVariable) { ExpectOutputIntVariablePort("top", "o"); }

TEST_F(ParamOverload1Test, TopInstantiatesDutThreeTimesOnO) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 3u);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u);
  const hldb::Variable *const o = getPortVariable(top->getPorts()->at(0));
  ASSERT_NE(o, nullptr);
  for (std::string_view name : {"u_dut1", "u_dut2", "u_dut3"}) {
    SCOPED_TRACE(name);
    const hldb::RefInstance *const inst = getInstance(name);
    ASSERT_NE(inst, nullptr) << "instance not found";
    ASSERT_NE(inst->getTypespec(), nullptr);
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    ASSERT_NE(mt, nullptr);
    EXPECT_EQ(mt->getModule(), getModule("dut"));
    ASSERT_NE(inst->getArguments(), nullptr);
    ASSERT_EQ(inst->getArguments()->size(), 1u) << "'.x(o)'";
    const hldb::NamedArgument *const conn = inst->getArguments()->at(0);
    const hldb::RefObj *const formal = conn->getLowConn<hldb::RefObj>();
    ASSERT_NE(formal, nullptr);
    EXPECT_EQ(formal->getName(), "x");
    const hldb::RefObj *const actual = conn->getHighConn<hldb::RefObj>();
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual->getName(), "o");
    EXPECT_EQ(actual->getActual(), o) << "'o' is top's output variable";
  }
}

TEST_F(ParamOverload1Test, UDut1OverridesPWithTwoElementPattern) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("u_dut1"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  const hldb::RefObj *const lhs = overrides->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "P");
  ExpectPositionalPattern(overrides->at(0)->getRhs(), {"1000", "2000"});
}

TEST_F(ParamOverload1Test, UDut2WritesNoOverride) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("u_dut2"));
  EXPECT_TRUE(overrides == nullptr || overrides->empty()) << "'dut u_dut2(.x(o));' writes no '#(...)'";
}

TEST_F(ParamOverload1Test, UDut3OverridesPWithScalar) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getInstance("u_dut3"));
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  const hldb::RefObj *const lhs = overrides->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "P");
  const hldb::Constant *const c = overrides->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "the override value is the literal 3000";
  EXPECT_EQ(c->getDecompile(), "3000");
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamOverload1Test, IllegalOverridesAndDriversAreErrors) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_session->getErrorContainer(), nullptr);
    EXPECT_GE(m_session->getErrorContainer()->getErrorStats().nbError, 3)
        << "10.9.1: u_dut1's pattern has 2 items for 5 elements; 6.20.2: 3000 is not assignment compatible "
           "with int [5]; 6.5, 23.3.3.2: o is driven by three output ports";
  }
}

TEST_F(ParamOverload1Test, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
