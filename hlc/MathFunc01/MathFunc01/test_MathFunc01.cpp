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

// Tests for tests/MathFunc01/dut.sv (tags: MathFunc01)
//
//   module tb();
//     initial begin
//       real res = 0.0;
//       res = $asin(2.0);
//       res = $acos(1.0);
//       res = $atan(1.0);
//       res = $atan2(1.0, 1.0);
//       res = $hypot(3.0, 4.0);
//       res = $pow(2.0, 8.0);
//       res = $sinh(10.0);
//       res = $cosh(1.0);
//       res = $tanh(1.0);
//       res = $sin(1.570796);
//       res = $asinh(1.0);
//       res = $cos(1.570796);
//       res = $acosh(1.0);
//       res = $tan(0.785398);
//       res = $atanh(0.5);
//     end
//   endmodule
//
// What is checked:
//   - module "tb" has exactly one initial procedure whose body is a
//     begin-end block (Sec 9.2.1, 9.3.1)
//   - Sec 6.12: the block declares the variable "res" of type "real"
//   - Sec 6.21: "real res = 0.0;" is a static variable (declared in a
//     procedural block of a static initial procedure) with an initializer
//     but without an explicit "static"/"automatic" keyword -- "an explicit
//     static keyword shall be required when an initialization value is
//     specified as part of a static variable's declaration". The
//     corresponding diagnostic COMP_MISSING_LIFETIME_KEYWORD is expected for
//     "res".
//   - Sec 20.8.2 / Table 20-4: the 15 statements are blocking assignments
//     (Sec 10.4.1) whose RHS is a system function call (SysFuncCall, not
//     SysTaskCall: each is used as an expression), in source order:
//     $asin $acos $atan $atan2 $hypot $pow $sinh $cosh $tanh $sin $asinh
//     $cos $acosh $tan $atanh
//   - every LHS is a RefObj "res" bound to the declared Variable "res"
//   - Table 20-4 arities: $atan2(y,x), $hypot(x,y), $pow(x,y) take two
//     arguments, all others one; every argument is a real literal
//     (vpiRealConst, Sec 5.7.2), e.g. $hypot(3.0, 4.0) in source order
//   - Table 20-4: "shall accept real value arguments and return a real
//     result type" -> each call's function type is vpiRealFunc
//   - Sec 20.8.2: all 15 names are standard system functions, so no
//     COMP_UNDEFINED_SYSTEM_FUNCTION is reported for any of them
//
// What is NOT checked and why:
//   - The numeric results (e.g. $asin(2.0) is outside the domain of asin
//     and yields NaN per the C library): runtime evaluation, not part of
//     the compiled object model.
//   - The exact node representing the "= 0.0" initializer: its placement
//     (on the Variable or as a separate statement) is a modeling choice;
//     the lifetime diagnostic above covers what the standard requires.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string>
#include <vector>

namespace hlc {

class MathFunc01Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MathFunc01.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTb() { return hldb::findByDefName<hldb::Module>("tb", m_design->getAllModules()); }

  static const hldb::Initial *getInitial() {
    const hldb::Module *const tb = getTb();
    if (tb == nullptr || tb->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *tb->getProcesses()) {
      if (const hldb::Initial *const init = any_cast<hldb::Initial>(p)) return init;
    }
    return nullptr;
  }

  static const hldb::Begin *getBlock() {
    const hldb::Initial *const init = getInitial();
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  static const hldb::Variable *getRes() {
    const hldb::Begin *const blk = getBlock();
    if (blk == nullptr || blk->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("res", blk->getVariables());
  }

  // Blocking assignments in the block whose RHS is a system function call,
  // in source order.
  static std::vector<const hldb::Assignment *> getCallAssignments() {
    std::vector<const hldb::Assignment *> result;
    const hldb::Begin *const blk = getBlock();
    if (blk == nullptr || blk->getStmts() == nullptr) return result;
    for (const hldb::Any *const s : *blk->getStmts()) {
      const hldb::Assignment *const a = any_cast<hldb::Assignment>(s);
      if (a != nullptr && a->getRhs() != nullptr && a->getRhs()->getAnyType() == hldb::AnyType::SysFuncCall) {
        result.emplace_back(a);
      }
    }
    return result;
  }

  static const hldb::SysFuncCall *findCall(std::string_view name) {
    for (const hldb::Assignment *const a : getCallAssignments()) {
      const hldb::SysFuncCall *const call = a->getRhs<hldb::SysFuncCall>();
      if (call != nullptr && call->getName() == name) return call;
    }
    return nullptr;
  }

  static const std::vector<std::string> &expectedNames() {
    static const std::vector<std::string> kNames = {"$asin",  "$acos", "$atan",  "$atan2", "$hypot",
                                                    "$pow",   "$sinh", "$cosh",  "$tanh",  "$sin",
                                                    "$asinh", "$cos",  "$acosh", "$tan",   "$atanh"};
    return kNames;
  }
};

// ---------------------------------------------------------------------------
// Sec 9.2.1: initial begin ... end
// ---------------------------------------------------------------------------

TEST_F(MathFunc01Test, TbHasInitialWithBeginBlock) {
  const hldb::Module *const tb = getTb();
  ASSERT_NE(tb, nullptr) << "module 'tb' not found";
  ASSERT_NE(tb->getProcesses(), nullptr);
  EXPECT_EQ(tb->getProcesses()->size(), 1u) << "exactly one procedure";
  const hldb::Initial *const init = getInitial();
  ASSERT_NE(init, nullptr) << "initial procedure not found";
  ASSERT_NE(init->getStmt(), nullptr);
  EXPECT_EQ(init->getStmt()->getAnyType(), hldb::AnyType::Begin) << "body is 'begin ... end'";
}

// ---------------------------------------------------------------------------
// Sec 6.12: real res
// ---------------------------------------------------------------------------

TEST_F(MathFunc01Test, BlockDeclaresRealRes) {
  const hldb::Variable *const res = getRes();
  ASSERT_NE(res, nullptr) << "'real res' not declared in the initial block";
  ASSERT_NE(res->getTypespec(), nullptr);
  ASSERT_NE(res->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(res->getTypespec()->getActual()->getAnyType(), hldb::AnyType::RealTypespec)
      << "Sec 6.12: 'res' is declared 'real'";
}

// ---------------------------------------------------------------------------
// Sec 6.21: static variable with initializer requires explicit lifetime
// ---------------------------------------------------------------------------

TEST_F(MathFunc01Test, InitializedStaticVariableRequiresLifetimeKeyword) {
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_LIFETIME_KEYWORD, "res"), nullptr)
      << "Sec 6.21: 'real res = 0.0;' in an initial block needs an explicit 'static' or 'automatic'";
}

// ---------------------------------------------------------------------------
// Sec 20.8.2: the 15 calls, in source order
// ---------------------------------------------------------------------------

TEST_F(MathFunc01Test, FifteenRealMathCallsInSourceOrder) {
  const std::vector<const hldb::Assignment *> assigns = getCallAssignments();
  const std::vector<std::string> &names = expectedNames();
  ASSERT_EQ(assigns.size(), names.size()) << "15 'res = $func(...)' statements";
  for (size_t i = 0; i < names.size(); ++i) {
    const hldb::SysFuncCall *const call = assigns[i]->getRhs<hldb::SysFuncCall>();
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getName(), names[i]) << "statement #" << i;
    EXPECT_TRUE(assigns[i]->getBlocking()) << "Sec 10.4.1: '=' is a blocking assignment (" << names[i] << ")";
  }
}

TEST_F(MathFunc01Test, EveryLhsBindsToRes) {
  const hldb::Variable *const res = getRes();
  ASSERT_NE(res, nullptr);
  const std::vector<const hldb::Assignment *> assigns = getCallAssignments();
  ASSERT_FALSE(assigns.empty());
  for (const hldb::Assignment *const a : assigns) {
    const hldb::RefObj *const lhs = a->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "res");
    EXPECT_EQ(lhs->getActual(), res) << "LHS 'res' must bind to the block's Variable 'res'";
  }
}

// ---------------------------------------------------------------------------
// Table 20-4: arities and real arguments
// ---------------------------------------------------------------------------

TEST_F(MathFunc01Test, ArgumentCountsMatchTable20_4) {
  for (const std::string &name : expectedNames()) {
    const hldb::SysFuncCall *const call = findCall(name);
    ASSERT_NE(call, nullptr) << name << " not found";
    ASSERT_NE(call->getArguments(), nullptr) << name << " has arguments";
    const size_t expected = (name == "$atan2" || name == "$hypot" || name == "$pow") ? 2u : 1u;
    EXPECT_EQ(call->getArguments()->size(), expected) << name;
  }
}

TEST_F(MathFunc01Test, ArgumentsAreRealLiterals) {
  for (const std::string &name : expectedNames()) {
    const hldb::SysFuncCall *const call = findCall(name);
    ASSERT_NE(call, nullptr) << name << " not found";
    ASSERT_NE(call->getArguments(), nullptr);
    for (const hldb::Any *const arg : *call->getArguments()) {
      const hldb::Constant *const c = any_cast<hldb::Constant>(arg);
      ASSERT_NE(c, nullptr) << name << ": argument must be a Constant";
      EXPECT_EQ(c->getConstType(), vpiRealConst) << name << ": Sec 5.7.2 real literal";
    }
  }
}

TEST_F(MathFunc01Test, HypotArgumentsInSourceOrder) {
  const hldb::SysFuncCall *const call = findCall("$hypot");
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  const hldb::Constant *const x = any_cast<hldb::Constant>(call->getArguments()->at(0));
  const hldb::Constant *const y = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(x, nullptr);
  ASSERT_NE(y, nullptr);
  EXPECT_EQ(x->getDecompile(), "3.0");
  EXPECT_EQ(y->getDecompile(), "4.0");
}

TEST_F(MathFunc01Test, CallsReturnReal) {
  for (const std::string &name : expectedNames()) {
    const hldb::SysFuncCall *const call = findCall(name);
    ASSERT_NE(call, nullptr) << name << " not found";
    EXPECT_EQ(call->getFuncType(), vpiRealFunc) << name << ": Table 20-4 functions return a real result";
  }
}

TEST_F(MathFunc01Test, NoUndefinedSystemFunctionReported) {
  for (const std::string &name : expectedNames()) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_SYSTEM_FUNCTION, name), nullptr)
        << name << " is a standard system function (Sec 20.8.2)";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
