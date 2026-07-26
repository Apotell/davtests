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
//   - design has module top
//   - module has exactly 1 net: 'a'
//   - 'a' typespec → ShortRealTypespec (NOT RealTypespec — distinct 32-bit type)
//   - 'a' initial value: Constant vpiRealConst, decompile "0.5"
//     (shortreal stores constant using same vpiRealConst as real)
//   - top has no continuous assignments
//   - top has no processes
//   - the 32-bit vs 64-bit precision difference between shortreal and real is
//     not observable statically: the initial value Constant reports vpiSize=64
//     regardless of the shortreal typespec (storage width is a simulation-time
//     property, not present in the UHDM graph)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/short_real_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Shortreal : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--shortreal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Shortreal, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(Shortreal, OneNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Shortreal, ANetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "net 'a' not found";
}

// ---------------------------------------------------------------------------
// Typespec — net 'a' must resolve to ShortRealTypespec, NOT RealTypespec
// ---------------------------------------------------------------------------
TEST_F(Shortreal, ANetTypespecIsShortReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);

  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::ShortRealTypespec>(), nullptr)
      << "net 'a' typespec should resolve to ShortRealTypespec (not RealTypespec)";
}

TEST_F(Shortreal, ANetTypespecIsNotPlainReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getActual<hldb::RealTypespec>(), nullptr) << "shortreal should NOT resolve to RealTypespec";
}

// ---------------------------------------------------------------------------
// Initial value — recorded as a real constant "0.5"
// ---------------------------------------------------------------------------
TEST_F(Shortreal, ANetInitialValueConstType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'a' has no initial value Constant";
  EXPECT_EQ(init->getConstType(), vpiRealConst);
}

TEST_F(Shortreal, ANetInitialValueIsHalf) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(Shortreal, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(Shortreal, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Precision -- shortreal is a distinct 32-bit type vs real's 64-bit, but the
// UHDM graph has no field for storage width; this can only be observed by
// simulating an assignment that overflows shortreal precision.
// ---------------------------------------------------------------------------
TEST_F(Shortreal, StorageWidthNotObservableStatically) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getSize(), 64)
      << "shortreal's 32-bit storage width is not reflected in the Constant's vpiSize (always 64, same as real)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
