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

// Validates the UHDM graph for a module using string.getc():
//   module top();
//     string a = "Test";
//     byte b = a.getc(2);
//   endmodule
//
// Checked:
//   - design has module work@top with 2 nets (a: string, b: byte)
//   - net 'a' typespec resolves to StringTypespec; initial value is "Test" (vpiStringConst)
//   - net 'b' typespec resolves to ByteTypespec
//   - net 'b' has a non-null initial value (vpiValue is set)
//   - net 'b' initial value is a HierPath named "a.getc(2)"
//   - HierPath element[0] is RefObj "a" with vpiActual resolving to Net 'a'
//   - HierPath element[1] is FuncCall "getc" with 1 argument (Constant "2")
//
// Not checked:
//   - b does NOT get a pre-evaluated constant value — Surelog stores the
//     unevaluated HierPath expression only; compile-time evaluation not performed
//   - const type of argument "2" (vpiUIntConst — unsized integer literals are
//     stored as unsigned by Surelog, as established in the itoa test)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/byte_typespec.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/func_call.h>
#include <uhdm/hier_path.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/string_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class StringGetc : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.16.3--string_getc.hlc"});

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

TEST_F(StringGetc, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — string 'a' and byte 'b'
// ---------------------------------------------------------------------------
TEST_F(StringGetc, TwoNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(StringGetc, ANetTypespecIsString) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::StringTypespec>(), nullptr);
}

TEST_F(StringGetc, ANetInitialValueIsTest) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiStringConst);
  EXPECT_EQ(init->getDecompile(), "\"Test\"");
}

TEST_F(StringGetc, BNetTypespecIsByte) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::RefTypespec *const rts = b->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::ByteTypespec>(), nullptr)
      << "net 'b' typespec should resolve to ByteTypespec";
}

// ---------------------------------------------------------------------------
// HierPath — b's initial value is the method call a.getc(2)
// ---------------------------------------------------------------------------
TEST_F(StringGetc, BNetHasValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getValue(), nullptr)
      << "net 'b' should have a vpiValue set from byte b = a.getc(2)";
}

TEST_F(StringGetc, BNetValueIsNotPreEvaluatedConstant) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<uhdm::Constant>(), nullptr)
      << "Surelog does not pre-evaluate a.getc(2) to a constant; b holds only the HierPath expression";
}

TEST_F(StringGetc, BNetValueIsHierPath) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::HierPath *const hp = b->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr) << "net 'b' initial value is not a HierPath";
  EXPECT_EQ(hp->getName(), "a.getc(2)");
}

TEST_F(StringGetc, HierPathReceiverIsA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::HierPath *const hp = b->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 1u);

  const uhdm::RefObj *const receiver =
      any_cast<uhdm::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "a");
  EXPECT_NE(receiver->getActual<uhdm::Net>(), nullptr);
}

TEST_F(StringGetc, HierPathMethodIsGetc) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::HierPath *const hp = b->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_GE(hp->getPathElems()->size(), 2u);

  const uhdm::FuncCall *const call =
      any_cast<uhdm::FuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "getc");
}

TEST_F(StringGetc, GetcArgumentIsTwo) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const b = uhdm::findByName<uhdm::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const uhdm::HierPath *const hp = b->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  const uhdm::FuncCall *const call =
      any_cast<uhdm::FuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);

  const uhdm::Constant *const arg =
      any_cast<uhdm::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "getc argument is not a Constant";
  EXPECT_EQ(arg->getDecompile(), "2");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
