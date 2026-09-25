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

// Tests for tests/HighLow/dut.sv:
//
//   module GOOD();
//   endmodule
//
//   module top ();
//     parameter [2:1] a = 10;
//     parameter  high = $high(a);
//     parameter  low = $low(a);
//     parameter  left = $left(a);
//     parameter  right = $right(a);
//
//     assign ccc = $high(a);
//
//     if (high == 2) begin
//       GOOD good1();
//     end
//     if (low == 1) begin
//       GOOD good2();
//     end
//     if (left == 2) begin
//       GOOD good3();
//     end
//     if (right == 1) begin
//       GOOD good4();
//     end
//   endmodule
//
// HighLow.hlc compiles at "-d db -d ast" (no "-d inst"), so this file
// exercises the unelaborated parse-time shape: $high/$low/$left/$right are
// not evaluated at this phase (that is elaboration's job), and the four
// generate-ifs stay as GenIf nodes with unresolved conditions rather than
// being collapsed to their taken/untaken branches.
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 7.4.5 "Packed arrays": for 'parameter [2:1] a = 10;',
// the packed dimension's left (msb) bound is 2 and right (lsb) bound is 1
// (left > right, i.e. "big endian"/normal ordering, so no bit reversal
// applies).
// IEEE 1800-2023 Sec 20.6 "Bit vector system functions": for a vector with
// left bound 2 and right bound 1 (left > right), $left == 2, $right == 1,
// $high == greater of the two == 2, $low == lesser of the two == 1 -- each
// is a compile-time function per Sec 20.6, so 'high'/'low'/'left'/'right'
// are themselves parameters whose default value is a SysFuncCall over a
// RefObj to 'a'.
// IEEE 1800-2023 Sec 27.5 "Conditional generate constructs": each
// 'if (cond) begin GOOD goodN(); end' with no 'else' is its own GenIf.
// IEEE 1800-2023 Sec 11.4.5 "Equality operators": '==' is vpiEqOp.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

#include <vector>

namespace hlc {

class HighLowTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HighLow.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Parameter *findParam(const hldb::Module *m, std::string_view name) {
    if (m == nullptr || m->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *m->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  static const hldb::ParamAssign *findParamAssign(const hldb::Module *m, std::string_view name) {
    return (m == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(m));
  }

  // Returns the SysFuncCall default value of parameter 'name' -- e.g.
  // 'parameter high = $high(a);' -> SysFuncCall("$high", [RefObj("a")]).
  static const hldb::SysFuncCall *findParamSysFuncCall(const hldb::Module *m, std::string_view name) {
    const hldb::ParamAssign *const pa = findParamAssign(m, name);
    return (pa == nullptr) ? nullptr : pa->getRhs<hldb::SysFuncCall>();
  }

  // Returns all GenIf statements directly in m's getGenStmts(), in source
  // order.
  static std::vector<const hldb::GenIf *> findAllGenIf(const hldb::Module *m) {
    std::vector<const hldb::GenIf *> result;
    if (m == nullptr || m->getGenStmts() == nullptr) return result;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenIf *const gi = any_cast<hldb::GenIf>(stmt)) result.emplace_back(gi);
    }
    return result;
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(HighLowTest, ModulesExist) {
  EXPECT_NE(getTop(), nullptr) << "module 'top' not found";
  EXPECT_NE(hldb::findByDefName<hldb::Module>("GOOD", m_design->getAllModules()), nullptr)
      << "module 'GOOD' not found";
}

// ---------------------------------------------------------------------------
// 'parameter [2:1] a = 10;' -- Sec 7.4.5: left (msb) == 2, right (lsb) == 1.
// ---------------------------------------------------------------------------

TEST_F(HighLowTest, ParamAHasPackedRangeTwoToOne) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Parameter *const a = findParam(top, "a");
  ASSERT_NE(a, nullptr) << "'parameter [2:1] a' not found";
  ASSERT_NE(a->getRanges(), nullptr) << "'[2:1]' must produce a packed range";
  ASSERT_EQ(a->getRanges()->size(), 1u);

  const hldb::Range *const range = a->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const leftExpr = range->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const rightExpr = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(leftExpr, nullptr) << "range left (msb) bound must be a Constant";
  ASSERT_NE(rightExpr, nullptr) << "range right (lsb) bound must be a Constant";
  EXPECT_EQ(std::string(leftExpr->getDecompile()), "2") << "Sec 7.4.5: left (msb) bound of [2:1] is 2";
  EXPECT_EQ(std::string(rightExpr->getDecompile()), "1") << "Sec 7.4.5: right (lsb) bound of [2:1] is 1";
}

TEST_F(HighLowTest, ParamAHasDefaultValueTen) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ParamAssign *const pa = findParamAssign(top, "a");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'a' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'a = 10': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "10");
}

// ---------------------------------------------------------------------------
// 'parameter high = $high(a);' / 'low = $low(a);' / 'left = $left(a);' /
// 'right = $right(a);' -- each a Sec 20.6 bit-vector system function called
// on 'a', unevaluated at this (non-elaborated) compile phase.
// ---------------------------------------------------------------------------

TEST_F(HighLowTest, HighParamDefaultIsSysFuncCallOfAWithNameDollarHigh) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::SysFuncCall *const call = findParamSysFuncCall(top, "high");
  ASSERT_NE(call, nullptr) << "'high = $high(a)': default RHS must be a SysFuncCall";
  EXPECT_EQ(call->getName(), std::string_view{"$high"});
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "'$high(a)' argument must be a RefObj naming 'a'";
  EXPECT_EQ(arg->getName(), std::string_view{"a"});
}

TEST_F(HighLowTest, LowParamDefaultIsSysFuncCallOfAWithNameDollarLow) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::SysFuncCall *const call = findParamSysFuncCall(top, "low");
  ASSERT_NE(call, nullptr) << "'low = $low(a)': default RHS must be a SysFuncCall";
  EXPECT_EQ(call->getName(), std::string_view{"$low"});
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view{"a"});
}

TEST_F(HighLowTest, LeftParamDefaultIsSysFuncCallOfAWithNameDollarLeft) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::SysFuncCall *const call = findParamSysFuncCall(top, "left");
  ASSERT_NE(call, nullptr) << "'left = $left(a)': default RHS must be a SysFuncCall";
  EXPECT_EQ(call->getName(), std::string_view{"$left"});
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view{"a"});
}

TEST_F(HighLowTest, RightParamDefaultIsSysFuncCallOfAWithNameDollarRight) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::SysFuncCall *const call = findParamSysFuncCall(top, "right");
  ASSERT_NE(call, nullptr) << "'right = $right(a)': default RHS must be a SysFuncCall";
  EXPECT_EQ(call->getName(), std::string_view{"$right"});
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), std::string_view{"a"});
}

// ---------------------------------------------------------------------------
// Four independent 'if (cond == N) begin GOODk(); end' generate-ifs --
// Sec 27.5.
// ---------------------------------------------------------------------------

TEST_F(HighLowTest, TopHasExactlyFourGenIfStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr) << "'top' has no generate statements";
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(top);
  ASSERT_EQ(genIfs.size(), 4u) << "'top' has four separate 'if' generate constructs";
}

// Verifies genIfs[index]'s condition is '<paramName> == <expectedRhs>'
// (Sec 11.4.5: vpiEqOp) and its body is 'begin GOODk(); end' instantiating
// module 'GOOD' as 'instName'.
static void expectGuardedGoodInstance(const std::vector<const hldb::GenIf *> &genIfs, size_t index,
                                       std::string_view paramName, std::string_view expectedRhs,
                                       std::string_view instName) {
  ASSERT_LT(index, genIfs.size());
  const hldb::GenIf *const gi = genIfs[index];
  ASSERT_NE(gi, nullptr);

  const hldb::Operation *const cond = gi->getCondition<hldb::Operation>();
  ASSERT_NE(gi->getCondition(), nullptr) << instName;
  ASSERT_NE(cond, nullptr) << instName << ": condition must be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiEqOp) << instName << ": '==' must produce vpiEqOp";
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(lhs, nullptr) << instName << ": left operand must be a RefObj";
  EXPECT_EQ(lhs->getName(), paramName);

  const hldb::Constant *const rhs = any_cast<hldb::Constant>((*cond->getOperands())[1]);
  ASSERT_NE(rhs, nullptr) << instName << ": right operand must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), expectedRhs);

  const hldb::Begin *const body = gi->getStmt<hldb::Begin>();
  ASSERT_NE(gi->getStmt(), nullptr) << instName;
  ASSERT_NE(body, nullptr) << instName << ": 'begin ... end' body must be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  const hldb::RefInstance *inst = nullptr;
  for (const hldb::Any *const s : *body->getStmts()) {
    if (const hldb::RefInstance *const ri = any_cast<hldb::RefInstance>(s)) {
      if (ri->getName() == instName) {
        inst = ri;
        break;
      }
    }
  }
  ASSERT_NE(inst, nullptr) << "instance '" << instName << "' not found in generate-if body";
}

TEST_F(HighLowTest, FirstGenIfGuardsGood1OnHighEqualsTwo) {
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(getTop());
  expectGuardedGoodInstance(genIfs, 0u, "high", "2", "good1");
}

TEST_F(HighLowTest, SecondGenIfGuardsGood2OnLowEqualsOne) {
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(getTop());
  expectGuardedGoodInstance(genIfs, 1u, "low", "1", "good2");
}

TEST_F(HighLowTest, ThirdGenIfGuardsGood3OnLeftEqualsTwo) {
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(getTop());
  expectGuardedGoodInstance(genIfs, 2u, "left", "2", "good3");
}

TEST_F(HighLowTest, FourthGenIfGuardsGood4OnRightEqualsOne) {
  const std::vector<const hldb::GenIf *> genIfs = findAllGenIf(getTop());
  expectGuardedGoodInstance(genIfs, 3u, "right", "1", "good4");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
