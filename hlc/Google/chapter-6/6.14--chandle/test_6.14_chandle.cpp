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

// Validates the UHDM graph for a module with a chandle-typed variable:
//   module top();
//     chandle a;
//   endmodule
//
// Checked:
//   - design has module work@top with 1 net ('a')
//   - net 'a' has a RefTypespec node (typespec is present)
//   - net 'a' RefTypespec vpiActual is null — Surelog does not resolve chandle
//     to a ChandleTypespec in the global type pool
//   - net 'a' has no initial value
//   - no ContAssigns and no processes in work@top
//
// Not checked:
//   - no function call exists in this design; return value concept does not apply
//   - ChandleTypespec resolution — unresolved in this Surelog version

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Chandle : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.14--chandle.hlc"});

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

TEST_F(Chandle, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(Chandle, OneNetExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Chandle, ANetExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr)
      << "net 'a' not found";
}

// ---------------------------------------------------------------------------
// Typespec — RefTypespec node present but vpiActual is unresolved
// ---------------------------------------------------------------------------
TEST_F(Chandle, ANetHasTypespec) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec(), nullptr)
      << "net 'a' should have a RefTypespec node";
}

TEST_F(Chandle, ANetTypespecActualIsNull) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getActual(), nullptr)
      << "chandle net typespec vpiActual is unresolved in this Surelog version";
}

// ---------------------------------------------------------------------------
// No initial value, no continuous assignments, no processes
// ---------------------------------------------------------------------------
TEST_F(Chandle, ANetHasNoInitialValue) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr)
      << "chandle variable 'a' should have no initial value";
}

TEST_F(Chandle, NoContAssigns) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(Chandle, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
