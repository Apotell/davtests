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

// Tests for wildcard.sv (tags: 7.8.1)
//   module top ();
//     int arr[*];
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'arr' (assoc ArrayTypespec, idx=wildcard, elem=IntTypespec)
//   - wildcard index [*]: IndexTypespec RefTypespec resolves to no concrete type (nullptr)
//   - net has no initial value (plain declaration, no initializer)
//   - work@top has no processes
//   - work@top has no continuous assignments
//
// Not checked:
//   - runtime behavior of wildcard-indexed associative arrays
//   - positive confirmation of no actual at all (no getActual<Any>() in UHDM API)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/any.h>
#include <uhdm/array_typespec.h>
#include <uhdm/design.h>
#include <uhdm/int_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_typespec.h>

namespace SURELOG {

class Wildcard : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "wildcard.hlc"});

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

// --- module ---------------------------------------------------------------

TEST_F(Wildcard, ModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- net "arr" : int[*] ---------------------------------------------------

TEST_F(Wildcard, ModuleHasOneNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Wildcard, NetNameIsArr) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "arr");
}

TEST_F(Wildcard, NetHasAssociativeArrayTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::ArrayTypespec *const at = rt->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Wildcard, AssocArrayKeyIsWildcard) {
  // int arr[*] — the wildcard index [*] is stored as a RefTypespec with no
  // resolved actual type (getActual() returns nullptr for any concrete type).
  // This distinguishes it from string.sv where getActual<StringTypespec>() != nullptr.
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  // No concrete type resolved for the wildcard — getActual<IntTypespec> is null
  EXPECT_EQ(at->getIndexTypespec()->getActual<uhdm::IntTypespec>(), nullptr);
}

TEST_F(Wildcard, AssocArrayValueTypeIsInt) {
  // element type is IntTypespec (from `int arr[*]`)
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<uhdm::IntTypespec>(), nullptr);
}

TEST_F(Wildcard, NetHasNoInitialValue) {
  // int arr[*] — plain declaration with no initializer
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getValue<uhdm::Any>(), nullptr);
}

TEST_F(Wildcard, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

TEST_F(Wildcard, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG
