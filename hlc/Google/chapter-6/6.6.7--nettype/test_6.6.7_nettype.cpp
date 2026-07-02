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

// Tests for 6.6.7--nettype.sv (tags: 6.6.7)
//   module top();
//     nettype real real_net;
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has 1 TypedefTypespec "real_net" (alias→RealTypespec, no resolution function)
//   - work@top has no nets, no processes, no task/functions
//
// Not checked:
//   - full name / scope path of the TypedefTypespec

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>

namespace hlc {

class Nettype : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.6.7--nettype.hlc"});

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

TEST_F(Nettype, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module has exactly one typespec: TypedefTypespec "real_net"
// ---------------------------------------------------------------------------
TEST_F(Nettype, ModuleHasOneTypespec) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

TEST_F(Nettype, NettypeIsTypedefTypespecNamedRealNet) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *const td =
      any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr) << "nettype declaration creates a TypedefTypespec";
  EXPECT_EQ(td->getName(), "real_net");
}

// ---------------------------------------------------------------------------
// TypedefTypespec alias: RefTypespec → RealTypespec
// ---------------------------------------------------------------------------
TEST_F(Nettype, NettypeAliasIsRealTypespec) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td =
      any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<hldb::RealTypespec>(), nullptr)
      << "nettype real real_net: alias base type is RealTypespec";
}

// ---------------------------------------------------------------------------
// No resolution function — this nettype has no 'with' clause
// ---------------------------------------------------------------------------
TEST_F(Nettype, NettypeHasNoResolutionFunction) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td =
      any_cast<hldb::TypedefTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getResolutionFunc(), nullptr)
      << "nettype without 'with' clause has no resolution function";
}

// ---------------------------------------------------------------------------
// No nets or processes — nettype declaration does not instantiate a net
// ---------------------------------------------------------------------------
TEST_F(Nettype, NoNets) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "nettype declaration does not create a net instance in the module";
}

TEST_F(Nettype, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(Nettype, NoTaskFunctions) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty())
      << "nettype without resolution function has no task/function declarations";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
