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

// Tests for tests/DelayAssign/dut.sv: module SimDTM.
//
// The module declares four nets that combine a declaration delay with an
// inline initializer, e.g.:
//   wire #0.1 __debug_req_ready = debug_req_ready;
//   wire [31:0] #0.1 __debug_resp_bits_resp = {30'b0, debug_resp_bits_resp};
// and six plain continuous assignments that carry their own delay, e.g.:
//   assign #0.1 debug_req_valid = __debug_req_valid;
//
// IEEE 1800-2023 construct under test (Sec 6.7.1, "Net declarations with
// built-in net types"): when a net_decl_assignment is combined with a
// delay3 on the net_declaration, "[a] delay specified in this manner is
// equivalent to specifying the same delay in a separate continuous
// assignment" -- i.e. the delay belongs to the IMPLICIT continuous
// assignment the net-declaration-with-initializer creates, not to the
// net's own intrinsic propagation delay. This is the mirror image of
// 10.3.3--cont-assignment-net-delay.sv ("wire #10 w;" with NO initializer,
// where the delay genuinely is the net's own delay, captured on
// Net::getDelay()).
//
// Checked:
//   - module "SimDTM" exists.
//   - every ContAssign object carries a delay of Constant
//     "0.1", vpiRealConst (IEEE 1800-2023 Sec 5.7.2: real literal
//     constants), size 64.
//   - each of the 6 explicit ContAssigns' lhs is a RefObj matching the
//     assigned port name, and rhs is non-null.
//
// NOT CHECKED (out of scope; no simulation/runtime behavior is observed --
// HLC is a compiler/elaborator, not a simulator):
//   - The implicit ContAssign relocation for net-decl-assignment nets
//     (Sec 6.7.1) -- tracked separately from this real-vs-integer delay
//     literal classification.
//   - The exact decompiled shape of the RHS expressions (concatenation,
//     part-selects) beyond confirming they are present and, where easy,
//     their top-level operator.
//   - DPI-C import/export binding of "debug_tick" -- unrelated to the
//     delay constructs under test here.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DelayAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DelayAssign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("SimDTM", m_design->getAllModules());
  }

  static const hldb::ContAssign *findContAssignFor(const hldb::Module *top, std::string_view lhsName) {
    if (top == nullptr || top->getContAssigns() == nullptr) return nullptr;
    for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
      const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
      if (lhs != nullptr && lhs->getName() == lhsName) return ca;
    }
    return nullptr;
  }

  static void ExpectPointOneDelay(const hldb::Expr *delay) {
    ASSERT_NE(delay, nullptr);
    const hldb::Constant *const c = any_cast<hldb::Constant>(delay);
    ASSERT_NE(c, nullptr) << "delay must be a Constant";
    EXPECT_EQ(c->getConstType(), vpiRealConst) << "IEEE 1800-2023 Sec 5.7.2: real literal -> vpiRealConst";
    EXPECT_EQ(c->getSize(), 64);
    EXPECT_EQ(c->getDecompile(), "0.1");
  }
};

// --- module ----

TEST_F(DelayAssignTest, ModuleSimDTMExists) { ASSERT_NE(getTop(), nullptr) << "module 'SimDTM' not found"; }

TEST_F(DelayAssignTest, AllContAssignsCarryPointOneDelay) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    ExpectPointOneDelay(ca->getDelay());
  }
}

// --- explicit "assign #0.1 ..." statements ----

TEST_F(DelayAssignTest, ExplicitAssignsHaveExpectedLhsAndRhs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);

  const char *const lhsNames[6] = {"debug_req_valid",    "debug_req_bits_addr", "debug_req_bits_op",
                                    "debug_req_bits_data", "debug_resp_ready",    "exit"};
  for (const char *const lhsName : lhsNames) {
    const hldb::ContAssign *const ca = findContAssignFor(top, lhsName);
    ASSERT_NE(ca, nullptr) << "no explicit ContAssign found for lhs '" << lhsName << "'";
    EXPECT_FALSE(ca->getNetDeclAssign()) << lhsName << " comes from an explicit 'assign' statement";
    EXPECT_NE(ca->getRhs(), nullptr) << lhsName << ": rhs must be present";
    ExpectPointOneDelay(ca->getDelay());
  }
}

// --- compiler diagnostics ----

TEST_F(DelayAssignTest, CompilerReportsZeroFatalOrSyntaxErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
