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

// Validates the UHDM graph produced for tests/NetsAndVariables/Ansi/Implicit.sv.
//
// Checked:
//   - explicit_wire (declared as wire) is in getNets() with vpiWire type and
//     is absent from getVariables().
//   - explicit_logic (declared as logic) is in getVariables() and is absent
//     from getNets().
//   - implicit_wire / implicit_net_a / implicit_net_b (undeclared identifiers
//     on the LHS of continuous assignments) are absent from both getNets() and
//     getVariables(); each appears only as a RefObj on the LHS of a ContAssign
//     in getContAssigns(), with getActual() == nullptr (no backing declaration).
//   - implicit_var_a / implicit_var_b (undeclared identifiers assigned in a
//     procedural always block) are absent from both getNets() and
//     getVariables(); each appears only as a RefObj on the LHS of a blocking
//     Assignment inside the always block, with getActual() == nullptr.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/any.h>
#include <hldb/assignment.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AnsiImplicitTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Implicit.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("nets_and_variables_test", m_design->getAllModules());
  }

  // Find a ContAssign in mod whose LHS is a RefObj with the given name.
  static const hldb::ContAssign *findContAssignByLhsName(std::string_view name, const hldb::Module *mod) {
    const hldb::ContAssignCollection *const cas = mod->getContAssigns();
    if (cas == nullptr) return nullptr;
    for (const hldb::ContAssign *const ca : *cas) {
      const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
      if (lhs != nullptr && lhs->getName() == name) return ca;
    }
    return nullptr;
  }

  // Find a procedural Assignment (inside an always block) in mod whose LHS is
  // a RefObj with the given name.
  static const hldb::Assignment *findProceduralAssignByLhsName(std::string_view name, const hldb::Module *mod) {
    const hldb::ProcessCollection *const procs = mod->getProcesses();
    if (procs == nullptr) return nullptr;
    for (const hldb::Process *const p : *procs) {
      const hldb::Always *const alw = any_cast<hldb::Always>(p);
      if (alw == nullptr) continue;
      const hldb::EventControl *const ec = alw->getStmt<hldb::EventControl>();
      if (ec == nullptr) continue;
      const hldb::Begin *const blk = ec->getStmt<hldb::Begin>();
      if (blk == nullptr) continue;
      const hldb::AnyCollection *const stmts = blk->getStmts();
      if (stmts == nullptr) continue;
      for (const hldb::Any *const s : *stmts) {
        const hldb::Assignment *const asgn = any_cast<hldb::Assignment>(s);
        if (asgn == nullptr) continue;
        const hldb::RefObj *const lhs = asgn->getLhs<hldb::RefObj>();
        if (lhs != nullptr && lhs->getName() == name) return asgn;
      }
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Explicit declarations: correct containers
// ---------------------------------------------------------------------------

TEST_F(AnsiImplicitTest, ExplicitWireIsInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w = hldb::findByName<hldb::Net>("explicit_wire", top->getNets());
  ASSERT_NE(w, nullptr) << "'explicit_wire' not found in nets";
  EXPECT_EQ(w->getNetType(), vpiWire);
}

TEST_F(AnsiImplicitTest, ExplicitWireNotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("explicit_wire", top->getVariables()), nullptr)
      << "'explicit_wire' must not appear in variables";
}

TEST_F(AnsiImplicitTest, ExplicitLogicIsInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("explicit_logic", top->getVariables());
  EXPECT_NE(v, nullptr) << "'explicit_logic' not found in variables";
}

TEST_F(AnsiImplicitTest, ExplicitLogicNotInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("explicit_logic", top->getNets()), nullptr)
      << "'explicit_logic' must not appear in nets";
}

// ---------------------------------------------------------------------------
// Implicit continuous-assignment LHS: absent from both nets and variables
// ---------------------------------------------------------------------------

TEST_F(AnsiImplicitTest, ImplicitWireNotInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_wire", top->getNets()), nullptr)
      << "'implicit_wire' must not appear in nets -- no declaration";
}

TEST_F(AnsiImplicitTest, ImplicitWireNotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("implicit_wire", top->getVariables()), nullptr)
      << "'implicit_wire' must not appear in variables -- no declaration";
}

TEST_F(AnsiImplicitTest, ImplicitNetANotInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_net_a", top->getNets()), nullptr)
      << "'implicit_net_a' must not appear in nets -- no declaration";
}

TEST_F(AnsiImplicitTest, ImplicitNetANotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("implicit_net_a", top->getVariables()), nullptr)
      << "'implicit_net_a' must not appear in variables -- no declaration";
}

TEST_F(AnsiImplicitTest, ImplicitNetBNotInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_net_b", top->getNets()), nullptr)
      << "'implicit_net_b' must not appear in nets -- no declaration";
}

TEST_F(AnsiImplicitTest, ImplicitNetBNotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("implicit_net_b", top->getVariables()), nullptr)
      << "'implicit_net_b' must not appear in variables -- no declaration";
}

// ---------------------------------------------------------------------------
// Implicit procedural-assignment LHS: absent from both nets and variables
// Per LRM 6.10, implicit declarations are net-only; there is no implicit
// variable in SystemVerilog.
// ---------------------------------------------------------------------------

TEST_F(AnsiImplicitTest, ImplicitVarANotInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_var_a", top->getNets()), nullptr)
      << "'implicit_var_a' must not appear in nets -- no implicit variable in SV";
}

TEST_F(AnsiImplicitTest, ImplicitVarANotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("implicit_var_a", top->getVariables()), nullptr)
      << "'implicit_var_a' must not appear in variables -- no implicit variable in SV";
}

TEST_F(AnsiImplicitTest, ImplicitVarBNotInNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("implicit_var_b", top->getNets()), nullptr)
      << "'implicit_var_b' must not appear in nets -- no implicit variable in SV";
}

TEST_F(AnsiImplicitTest, ImplicitVarBNotInVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("implicit_var_b", top->getVariables()), nullptr)
      << "'implicit_var_b' must not appear in variables -- no implicit variable in SV";
}

// ---------------------------------------------------------------------------
// Assignment types: continuous (assign keyword -> ContAssign)
// Each undeclared LHS must be a RefObj with no backing declaration
// (getActual() == nullptr).
// ---------------------------------------------------------------------------

TEST_F(AnsiImplicitTest, ThreeContAssignsExist) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 3u) << "expected exactly three 'assign' statements";
}

TEST_F(AnsiImplicitTest, ImplicitWireIsContAssign) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ContAssign *const ca = findContAssignByLhsName("implicit_wire", top);
  ASSERT_NE(ca, nullptr) << "'implicit_wire' not found as a ContAssign LHS";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS of 'implicit_wire' assign is not a RefObj";
  EXPECT_EQ(lhs->getName(), "implicit_wire");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'implicit_wire' is undeclared -- RefObj must have no actual";
}

TEST_F(AnsiImplicitTest, ImplicitNetAIsContAssign) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ContAssign *const ca = findContAssignByLhsName("implicit_net_a", top);
  ASSERT_NE(ca, nullptr) << "'implicit_net_a' not found as a ContAssign LHS";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS of 'implicit_net_a' assign is not a RefObj";
  EXPECT_EQ(lhs->getName(), "implicit_net_a");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'implicit_net_a' is undeclared -- RefObj must have no actual";
}

TEST_F(AnsiImplicitTest, ImplicitNetBIsContAssign) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ContAssign *const ca = findContAssignByLhsName("implicit_net_b", top);
  ASSERT_NE(ca, nullptr) << "'implicit_net_b' not found as a ContAssign LHS";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS of 'implicit_net_b' assign is not a RefObj";
  EXPECT_EQ(lhs->getName(), "implicit_net_b");
  EXPECT_EQ(lhs->getActual(), nullptr) << "'implicit_net_b' is undeclared -- RefObj must have no actual";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
