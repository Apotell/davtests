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

// Tests for string.sv (tags: 7.8.2 7.8)
//   module top ();
//     int arr[string];
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'arr' (assoc ArrayTypespec, idx=StringTypespec, elem=IntTypespec)
//   - net has no initial value (plain declaration, no initializer)
//   - work@top has no processes
//   - work@top has no continuous assignments
//
// Also checked:
//   - HLC reports no compile errors for a string-keyed associative array
//     declaration (structural proxy for "this construct is legal"; full
//     runtime behavior requires simulation, out of scope for this frontend)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/array_typespec.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>

namespace hlc {

class AssocString : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "string.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ---------------------------------------------------------------

TEST_F(AssocString, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- net "arr" : int[string] ----------------------------------------------

TEST_F(AssocString, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(AssocString, NetNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "arr");
}

TEST_F(AssocString, NetHasAssociativeArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(AssocString, AssocArrayKeyTypeIsString) {
  // `string` keyword maps to StringTypespec (variable-length string)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(AssocString, AssocArrayValueTypeIsInt) {
  // element type is IntTypespec (from `int arr[string]`)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(AssocString, NetHasNoInitialValue) {
  // int arr[string] — plain declaration with no initializer
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getValue<hldb::Any>(), nullptr);
}

TEST_F(AssocString, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(AssocString, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(AssocString, CompilerHasNoErrors) {
  // int arr[string] is legal SystemVerilog; HLC must accept it without diagnostics.
  const hlc::ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "string-keyed associative array declaration must not produce compile errors";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
