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

// Tests for 6.5--variable_redeclare.sv (tags: 6.5)
//   module top();
//     reg v;
//     wire v;
//   endmodule
//
// Surelog merges reg+wire into a single Net; wire wins for vpiNetType,
// reg declaration maps to a LogicTypespec.
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'v' (vpiNetType=vpiWire, RefTypespec→LogicTypespec)
//   - work@top has no processes
//   - work@top has no continuous assignments
//
// Not checked:
//   - Surelog error/warning reporting for variable redeclaration (SV spec: should fail)
//   - order-dependence (reg first vs wire first)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class VariableRedeclare : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.5--variable_redeclare.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(VariableRedeclare, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net — reg v and wire v merge into a single Net named 'v'
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclare, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getNets()->size(), 1u)
      << "reg v and wire v should collapse to exactly one net";
}

TEST_F(VariableRedeclare, NetNameIsV) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr) << "net 'v' not found in module";
}

// ---------------------------------------------------------------------------
// wire wins — vpiNetType should be vpiWire (1)
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclare, NetTypeIsWire) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getNetType(), vpiWire)
      << "expected vpiNetType wire (1) — wire declaration wins over reg";
}

// ---------------------------------------------------------------------------
// Typespec — reg maps to LogicTypespec referenced via RefTypespec
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclare, NetHasRefTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec(), nullptr) << "net 'v' has no typespec";
}

TEST_F(VariableRedeclare, NetTypespecIsLogic) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);

  const uhdm::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'v' has no RefTypespec";

  const uhdm::LogicTypespec *const lts = rts->getActual<uhdm::LogicTypespec>();
  EXPECT_NE(lts, nullptr)
      << "RefTypespec actual is not a LogicTypespec (expected from reg declaration)";
}

// ---------------------------------------------------------------------------
// No continuous assignments — the module only has declarations, no assign
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclare, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "unexpected continuous assignments in redeclaration-only module";
}

TEST_F(VariableRedeclare, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
