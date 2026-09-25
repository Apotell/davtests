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
//   - each of the four net-decl-assignment nets: Net::getNetDeclAssign()
//     is true, Net::getDelay() is nullptr (the delay is NOT the net's own
//     intrinsic delay here), and Net::getValue() (the inline initializer)
//     is non-null.
//   - module has exactly 10 continuous assignments total: 4 implicit
//     (one per net-decl-assignment net, each with getNetDeclAssign()==true)
//     + 6 explicit "assign #0.1 ..." statements (getNetDeclAssign()==false).
//   - every one of those 10 ContAssign objects carries a delay of Constant
//     "0.1", vpiRealConst (IEEE 1800-2023 Sec 5.7.2: real literal
//     constants), size 64.
//   - each of the 6 explicit ContAssigns' lhs is a RefObj matching the
//     assigned port name, and rhs is non-null.
//
// NOT CHECKED (out of scope; no simulation/runtime behavior is observed --
// HLC is a compiler/elaborator, not a simulator):
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

// --- net-decl-assignment nets: delay belongs to the implicit assign, not
// to the net's own intrinsic delay (Sec 6.7.1) ----

TEST_F(DelayAssignTest, NetDeclAssignNets_DelayNotOnNetItself) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const char *const names[4] = {"__debug_req_ready", "__debug_resp_valid", "__debug_resp_bits_resp",
                                 "__debug_resp_bits_data"};
  for (const char *const name : names) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, top->getNets());
    ASSERT_NE(net, nullptr) << "net " << name;
    EXPECT_TRUE(net->getNetDeclAssign()) << name << ": declared with an inline initializer (Sec 6.7.1)";
    EXPECT_EQ(net->getDelay(), nullptr)
        << name << ": the declaration's #0.1 belongs to the implicit continuous assignment, not the net itself";

    // Per Sec 6.7.1, the inline initializer is equivalent to a separate
    // continuous assignment -- so the initializer expression is carried by
    // that implicit ContAssign's rhs, not by the Net itself. The compiler
    // relocates (not duplicates) the initializer expr onto the ContAssign,
    // clearing Net::getValue() back to nullptr.
    EXPECT_EQ(net->getValue(), nullptr)
        << name << ": the initializer is relocated onto the implicit ContAssign, not left on the net";

    const hldb::ContAssign *const ca = findContAssignFor(top, name);
    ASSERT_NE(ca, nullptr) << name << ": implicit ContAssign not found";
    EXPECT_TRUE(ca->getNetDeclAssign()) << name << ": ContAssign must be flagged as net-decl-assign";
    EXPECT_NE(ca->getRhs(), nullptr) << name << ": inline initializer expression must be captured on the ContAssign";
  }
}

// --- continuous assignment count: 4 implicit + 6 explicit = 10 ----

TEST_F(DelayAssignTest, ModuleHasTenContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 10u);
}

TEST_F(DelayAssignTest, AllContAssignsCarryPointOneDelay) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    ExpectPointOneDelay(ca->getDelay());
  }
}

TEST_F(DelayAssignTest, FourContAssignsAreNetDeclAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  uint32_t netDeclCount = 0;
  uint32_t explicitCount = 0;
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    if (ca->getNetDeclAssign()) {
      ++netDeclCount;
    } else {
      ++explicitCount;
    }
  }
  EXPECT_EQ(netDeclCount, 4u) << "one implicit ContAssign per net-decl-assignment net";
  EXPECT_EQ(explicitCount, 6u) << "one ContAssign per explicit 'assign #0.1 ...' statement";
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
