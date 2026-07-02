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

// Tests for integral.sv (tags: 7.8.4 7.8)
//   module top ();
//     int arr [ integer ];
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'arr' (associative ArrayTypespec, idx=IntegerTypespec, elem=IntTypespec)
//   - index type is IntegerTypespec (4-state `integer` keyword) — NOT IntTypespec (2-state `int`)
//   - work@top has no processes
//   - work@top has no continuous assignments
//
// Not checked:
//   - runtime behavior of integer-keyed associative arrays

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class Integral : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "integral.hlc"});

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

TEST_F(Integral, ModuleExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(Integral, ModuleHasOneNet) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Integral, NetNameIsArr) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "arr");
}

TEST_F(Integral, NetHasAssociativeArrayTypespec) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Integral, AssocArrayKeyTypeIsInteger) {
  // `integer` keyword maps to IntegerTypespec (4-state 32-bit),
  // distinct from IntTypespec (2-state `int` keyword)
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()
          ->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::IntegerTypespec>(), nullptr);
}

TEST_F(Integral, AssocArrayKeyTypeIsNotInt) {
  // Confirm it is NOT IntTypespec — `integer` and `int` are different types
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()
          ->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_EQ(at->getIndexTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Integral, AssocArrayValueTypeIsInt) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()
          ->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Integral, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(Integral, NoContAssigns) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc
