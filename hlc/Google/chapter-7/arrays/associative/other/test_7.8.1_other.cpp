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

// Tests for other.sv (tags: 7.8.1)
//   module top ();
//     typedef struct { byte B; int I[*]; } Unpkt;
//     int arr[Unpkt];
//   endmodule
//
// Surelog emits EL0535 ("Illegal implicit net Unpkt") — typedef unresolved
// as associative-array index type; ArrayTypespec falls back to static(1).
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net: 'arr' (ArrayTypespec static=1 — error recovery)
//   - ArrayTypespec elem type is IntTypespec
//   - module has TypedefTypespec "Unpkt" (from typedef struct definition)
//   - work@top has no processes
//   - work@top has no continuous assignments
//
// Not checked:
//   - StructTypespec internals of Unpkt (members B: ByteTypespec, I: ArrayTypespec wildcard)
//   - IndexTypespec is absent (null) in the error-recovery ArrayTypespec

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>

namespace hlc {

class Other : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "other.hlc"});

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

TEST_F(Other, ModuleExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- net arr (error-recovery: static array, not associative) --------------

TEST_F(Other, ModuleHasOneNet) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Other, NetNameIsArr) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "arr");
}

TEST_F(Other, NetHasArrayTypespec) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(Other, ArrayTypespecIsStaticDueToErrorRecovery) {
  // int arr[Unpkt] — Surelog could not resolve Unpkt as an index type (EL0535),
  // so the ArrayTypespec falls back to static(1) instead of associative(3)
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()
          ->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 1);  // static = 1 (error recovery)
}

TEST_F(Other, ArrayTypespecElemTypeIsInt) {
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

TEST_F(Other, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getProcesses(), nullptr);
}

// --- typedef Unpkt -----------------------------------------------------------

TEST_F(Other, ModuleHasTypedefUnpkt) {
  // typedef struct { ... } Unpkt creates a TypedefTypespec named "Unpkt"
  // accessible via module typespecs (not through the net)
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *const td =
      hldb::findByName<hldb::TypedefTypespec>("Unpkt", top->getTypespecs());
  EXPECT_NE(td, nullptr);
}

// --- structural completeness -------------------------------------------------

TEST_F(Other, NoContAssigns) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc
