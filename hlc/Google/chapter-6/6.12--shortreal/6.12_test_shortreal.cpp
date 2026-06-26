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

// Validates the UHDM graph for a module with a shortreal-typed variable:
//   module top();
//     shortreal a = 0.5;
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'a'
//   - 'a' typespec → ShortRealTypespec (NOT RealTypespec — distinct 32-bit type)
//   - 'a' initial value: Constant vpiRealConst, decompile "0.5"
//     (shortreal stores constant using same vpiRealConst as real)
//   - work@top has no continuous assignments
//   - work@top has no processes
//
// Not checked:
//   - actual 32-bit vs 64-bit precision difference (simulation-only)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/real_typespec.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/short_real_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class Shortreal : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.12--shortreal.hlc"});

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

TEST_F(Shortreal, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(Shortreal, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Shortreal, ANetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(uhdm::findByName<uhdm::Net>("a", top->getNets()), nullptr)
      << "net 'a' not found";
}

// ---------------------------------------------------------------------------
// Typespec — net 'a' must resolve to ShortRealTypespec, NOT RealTypespec
// ---------------------------------------------------------------------------
TEST_F(Shortreal, ANetTypespecIsShortReal) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);

  const uhdm::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'a' has no typespec";
  EXPECT_NE(rts->getActual<uhdm::ShortRealTypespec>(), nullptr)
      << "net 'a' typespec should resolve to ShortRealTypespec (not RealTypespec)";
}

TEST_F(Shortreal, ANetTypespecIsNotPlainReal) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getActual<uhdm::RealTypespec>(), nullptr)
      << "shortreal should NOT resolve to RealTypespec";
}

// ---------------------------------------------------------------------------
// Initial value — recorded as a real constant "0.5"
// ---------------------------------------------------------------------------
TEST_F(Shortreal, ANetInitialValueConstType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr) << "net 'a' has no initial value Constant";
  EXPECT_EQ(init->getConstType(), vpiRealConst);
}

TEST_F(Shortreal, ANetInitialValueIsHalf) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(Shortreal, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(Shortreal, NoProcesses) {
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
