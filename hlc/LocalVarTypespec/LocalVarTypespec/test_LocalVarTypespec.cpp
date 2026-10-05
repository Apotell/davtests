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

// Tests for tests/LocalVarTypespec/dut.sv:
//
//   module shift();
//   function logic [1-1:0] fshr_u;
//     reg signed [2-1:0] signed_result;
//     reg unsigned [3-1:0] unsigned_result;
//     reg [4-1:0] nosign_result;
//   endfunction
//   endmodule // shift
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 13.4 "Functions": 'function logic [1-1:0] fshr_u;'
//   declares function 'fshr_u' whose return type is an explicit
//   'logic [1-1:0]' (a packed dimension whose left bound is the constant
//   expression '1-1', a binary subtraction per Sec 11.4.3, and right
//   bound 0). The block item declarations inside it are local variables
//   of the function (Sec 13.4, function_body_declaration's
//   tf_item_declaration / block_item_declaration).
// IEEE 1800-2023 Sec 13.4.2: "Functions defined within a module ...
//   default to being static, with all declared items being statically
//   allocated" -- 'fshr_u' and its three local variables are static, not
//   automatic.
// IEEE 1800-2023 Sec 6.11.2: "logic and reg denote the same type" -- each
//   local is a 4-state vector modeled as LogicTypespec.
// IEEE 1800-2023 Sec 6.11.3: "reg, and logic default to unsigned ... The
//   signedness can be explicitly defined" -- 'reg signed' is signed,
//   'reg unsigned' is unsigned, plain 'reg' is unsigned.
// IEEE 1800-2023 Sec 7.4.1: each '[N-1:0]' is one packed dimension whose
//   left bound is the subtraction 'N-1' and right bound is 0.
// IEEE 1800-2023 Sec 6.8: the locals are variables (no net-type keyword).
//
// Model note: the HLDB object model does not fix whether the locals of a
// function with no explicit begin/end hang directly off the Function scope
// or off an implicit body Begin; findLocal() accepts either location. The
// standard only requires that they are declared within the function.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LocalVarTypespecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LocalVarTypespec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getShift() { return hldb::findByName<hldb::Module>("shift", m_design->getAllModules()); }

  static const hldb::Function *getFunc() {
    const hldb::Module *const m = getShift();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("fshr_u", m->getTaskFuncs());
  }

  // Returns the local variable 'name' declared in fshr_u, whether it is
  // attached to the Function scope itself or to its implicit body Begin.
  static const hldb::Variable *findLocal(std::string_view name) {
    const hldb::Function *const f = getFunc();
    if (f == nullptr) return nullptr;
    if (const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, f->getVariables())) return v;
    if (const hldb::Begin *const b = f->getStmt<hldb::Begin>()) {
      return hldb::findByName<hldb::Variable>(name, b->getVariables());
    }
    return nullptr;
  }

  static size_t countLocals() {
    const hldb::Function *const f = getFunc();
    if (f == nullptr) return 0;
    size_t n = (f->getVariables() == nullptr) ? 0 : f->getVariables()->size();
    if (const hldb::Begin *const b = f->getStmt<hldb::Begin>()) {
      if (b->getVariables() != nullptr) n += b->getVariables()->size();
    }
    return n;
  }

  // Checks that range 'r' is '[<n>-1:0]' written as Operation(n - 1) : 0.
  static void expectNMinus1To0(const hldb::Range *r, std::string_view n) {
    ASSERT_NE(r, nullptr);
    const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
    ASSERT_NE(left, nullptr) << "left bound '" << n << "-1' should be an Operation";
    EXPECT_EQ(left->getOpType(), vpiSubOp);
    ASSERT_NE(left->getOperands(), nullptr);
    ASSERT_EQ(left->getOperands()->size(), 2u);
    const hldb::Constant *const a = any_cast<hldb::Constant>(left->getOperands()->at(0));
    const hldb::Constant *const b = any_cast<hldb::Constant>(left->getOperands()->at(1));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->getDecompile(), n);
    EXPECT_EQ(b->getDecompile(), std::string_view("1"));
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getDecompile(), std::string_view("0"));
  }

  static void expectLocal(std::string_view name, bool isSigned, std::string_view n) {
    const hldb::Variable *const v = findLocal(name);
    ASSERT_NE(v, nullptr) << "local variable '" << name << "' not found in 'fshr_u'";
    EXPECT_FALSE(v->getAutomatic()) << "'" << name << "' is statically allocated (Sec 13.4.2)";
    ASSERT_NE(v->getTypespec(), nullptr);
    ASSERT_NE(v->getTypespec()->getActual(), nullptr);
    ASSERT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec)
        << "'reg' denotes the same type as 'logic' (Sec 6.11.2)";
    const hldb::LogicTypespec *const lts = v->getTypespec()->getActual<hldb::LogicTypespec>();
    EXPECT_EQ(lts->getSigned(), isSigned) << "signedness of '" << name << "' (Sec 6.11.3)";
    ASSERT_NE(lts->getRanges(), nullptr);
    ASSERT_EQ(lts->getRanges()->size(), 1u);
    expectNMinus1To0(lts->getRanges()->at(0), n);
  }
};

// ---------------------------------------------------------------------------
// Existence -- Sec 13.4
// ---------------------------------------------------------------------------

TEST_F(LocalVarTypespecTest, ModuleShiftExists) { EXPECT_NE(getShift(), nullptr) << "module 'shift' not found"; }

TEST_F(LocalVarTypespecTest, FunctionFshrUExists) {
  EXPECT_NE(getFunc(), nullptr) << "function 'fshr_u' not found in module 'shift'";
}

TEST_F(LocalVarTypespecTest, FunctionIsStatic) {
  const hldb::Function *const f = getFunc();
  ASSERT_NE(f, nullptr);
  EXPECT_FALSE(f->getAutomatic()) << "module-level functions default to static (Sec 13.4.2)";
}

TEST_F(LocalVarTypespecTest, ReturnTypeIsUnsignedLogicOneMinusOneToZero) {
  const hldb::Function *const f = getFunc();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getReturn(), nullptr) << "explicit return type 'logic [1-1:0]' missing";
  ASSERT_NE(f->getReturn()->getActual(), nullptr);
  ASSERT_EQ(f->getReturn()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
  const hldb::LogicTypespec *const lts = f->getReturn()->getActual<hldb::LogicTypespec>();
  EXPECT_FALSE(lts->getSigned());
  ASSERT_NE(lts->getRanges(), nullptr);
  ASSERT_EQ(lts->getRanges()->size(), 1u);
  expectNMinus1To0(lts->getRanges()->at(0), "1");
}

// ---------------------------------------------------------------------------
// Local variables -- Sec 6.11.2, 6.11.3, 7.4.1, 13.4.2
// ---------------------------------------------------------------------------

TEST_F(LocalVarTypespecTest, FunctionDeclaresExactlyThreeLocals) { EXPECT_EQ(countLocals(), 3u); }

TEST_F(LocalVarTypespecTest, SignedResultIsSignedLogicTwoMinusOneToZero) { expectLocal("signed_result", true, "2"); }

TEST_F(LocalVarTypespecTest, UnsignedResultIsUnsignedLogicThreeMinusOneToZero) {
  expectLocal("unsigned_result", false, "3");
}

TEST_F(LocalVarTypespecTest, NosignResultIsUnsignedLogicFourMinusOneToZero) {
  expectLocal("nosign_result", false, "4");
}

TEST_F(LocalVarTypespecTest, LocalsAreNotModuleVariables) {
  const hldb::Module *const m = getShift();
  ASSERT_NE(m, nullptr);
  for (const std::string_view name : {"signed_result", "unsigned_result", "nosign_result"}) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, m->getVariables()), nullptr)
        << "'" << name << "' is local to 'fshr_u', not a module item";
  }
}

TEST_F(LocalVarTypespecTest, LocalsHaveNoInitializer) {
  for (const std::string_view name : {"signed_result", "unsigned_result", "nosign_result"}) {
    const hldb::Variable *const v = findLocal(name);
    ASSERT_NE(v, nullptr) << name;
    EXPECT_EQ(v->getValue(), nullptr) << name;
  }
}

// ---------------------------------------------------------------------------
// Diagnostics -- the source is legal
// ---------------------------------------------------------------------------

TEST_F(LocalVarTypespecTest, CompilerReportsZeroErrors) {
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
