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

// Tests for tests/NetType/dut.sv (all declarations except module dut are in
// the compilation-unit scope):
//
//    3 function automatic bit my_function();
//    4 endfunction
//    6 nettype bit net_bit with my_function;
//    9 nettype real my_real;
//   11 module dut ();
//   13     my_real my_real_net;
//   15 endmodule
//   18 typedef some_other_type myalias;
//   22 typedef struct {
//   23   real field1;
//   24   bit field2;
//   25 } T;
//   28 function automatic T Tsum (input T driver[]);
//   29   Tsum.field1 = 0.0;
//   30   foreach (driver[i])
//   31     Tsum.field1 += driver[i].field1;
//   32 endfunction
//   34 nettype T wT;
//   36 nettype T wTsum with Tsum;
//   39 nettype real my_real with my_function2;
//   41 function automatic real my_function2(input real driver []);
//   42 endfunction
//
// What to check and why (IEEE 1800-2023):
//   - Sec 6.6.7 (Syntax 6-1): "nettype data_type nettype_identifier [ with
//     tf_identifier ] ;" declares a user-defined nettype, "similar to a
//     typedef", optionally naming a resolution function. HLDB models this as
//     a Typedef with getIsNettype() == true and getResolutionFunc() holding
//     the 'with' function.
//   - Sec 6.6.7: "A user-defined resolution function for a net of a
//     user-defined nettype with a data type T shall be a function with a
//     return type of T and a single input argument whose type is a dynamic
//     array of elements of type T." my_function has NO arguments, so
//     "nettype bit net_bit with my_function;" (line 6) is illegal and must be
//     diagnosed. Tsum (line 28) meets every requirement, so wTsum (line 36)
//     is legal.
//   - Sec 6.6.7 valid data types: (c) real, (d) an unpacked structure whose
//     members are each valid -- T {real, bit} is a valid nettype data type,
//     so wT/wTsum must not be flagged.
//   - Sec 3.13: "Within a name space, it shall be illegal to redeclare a name
//     already declared by a prior declaration." my_real is declared at line
//     9 and again at line 39 in the same compilation-unit scope -> error.
//   - Sec 6.5: "Data shall be declared before they are used" --
//     some_other_type (line 18) is never declared -> error.
//   - Sec 6.7 / 6.7.2: "net_declaration ::= nettype_identifier [
//     delay_control ] list_of_net_decl_assignments ;" -- "my_real
//     my_real_net;" declares a NET of user-defined nettype my_real, not a
//     variable; my_real must resolve to the nettype declaration.
//   - Sec 6.18: "typedef some_other_type myalias;" and "typedef struct
//     {...} T;" are plain typedefs, not nettypes.
//
// What is NOT checked and why:
//   - Resolution behavior at simulation time (Sec 6.6.7 scheduling).
//   - my_function2's illegality is not considered: real/real[] matches.
//   - Body of Tsum beyond its signature: covered by 6.6.7 chapter tests.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_typespec.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <vector>

namespace hlc {

class NetTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NetType.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  // Returns all compilation-unit Typedefs named 'name' (in source order).
  static std::vector<const hldb::Typedef *> getTypedefs(std::string_view name) {
    std::vector<const hldb::Typedef *> result;
    if (m_design == nullptr || m_design->getTypedefs() == nullptr) return result;
    for (const hldb::Typedef *const td : *m_design->getTypedefs()) {
      if (td->getName() == name) result.emplace_back(td);
    }
    return result;
  }

  static const hldb::Typedef *getTypedef(std::string_view name) {
    const std::vector<const hldb::Typedef *> all = getTypedefs(name);
    return all.empty() ? nullptr : all.front();
  }

  static const hldb::Function *getFunction(std::string_view name) {
    if (m_design == nullptr || m_design->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>(name, m_design->getTaskFuncs());
  }
};

TEST_F(NetTypeTest, ModuleDutExists) { ASSERT_NE(getDut(), nullptr) << "module 'dut' not found"; }

// ---------------------------------------------------------------------------
// nettype bit net_bit with my_function;  (line 6)
// ---------------------------------------------------------------------------

TEST_F(NetTypeTest, NetBitIsNettypeOfBit) {
  const hldb::Typedef *const td = getTypedef("net_bit");
  ASSERT_NE(td, nullptr);
  EXPECT_TRUE(td->getIsNettype());
  ASSERT_NE(td->getAlias(), nullptr);
  EXPECT_NE(td->getAlias()->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(NetTypeTest, NetBitResolutionFuncIsMyFunction) {
  const hldb::Typedef *const td = getTypedef("net_bit");
  ASSERT_NE(td, nullptr);
  const hldb::RefObj *const fn = td->getResolutionFunc();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), std::string_view("my_function"));
  ASSERT_NE(fn->getActual(), nullptr);
  EXPECT_EQ(fn->getActual()->getAnyType(), hldb::AnyType::Function);
}

TEST_F(NetTypeTest, MyFunctionHasNoArgumentsSoIsIllegalResolutionFunction) {
  const hldb::Function *const fn = getFunction("my_function");
  ASSERT_NE(fn, nullptr);
  const size_t nbArgs = (fn->getIODecls() == nullptr) ? 0u : fn->getIODecls()->size();
  EXPECT_EQ(nbArgs, 0u);
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_NETTYPE_RESOLUTION_FUNCTION, 6), nullptr)
      << "Sec 6.6.7: resolution function must take a single input dynamic array of T";
}

// ---------------------------------------------------------------------------
// nettype real my_real;  (line 9) and its redeclaration at line 39
// ---------------------------------------------------------------------------

TEST_F(NetTypeTest, MyRealIsNettypeOfRealWithoutResolutionFunc) {
  const hldb::Typedef *const td = getTypedef("my_real");
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getStartLine(), 9u);
  EXPECT_TRUE(td->getIsNettype());
  ASSERT_NE(td->getAlias(), nullptr);
  EXPECT_NE(td->getAlias()->getActual<hldb::RealTypespec>(), nullptr);
  EXPECT_EQ(td->getResolutionFunc(), nullptr) << "no 'with' clause on line 9";
}

TEST_F(NetTypeTest, MyRealRedeclarationIsDiagnosed) {
  const bool found = (findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_TYPEDEF, "my_real") != nullptr) ||
                     (findError(ErrorDefinition::COMP_DUPLICATE_DECLARATION, "my_real") != nullptr);
  EXPECT_TRUE(found) << "Sec 3.13: 'my_real' is declared twice in the compilation-unit name space";
}

// ---------------------------------------------------------------------------
// typedef some_other_type myalias;  (line 18)
// ---------------------------------------------------------------------------

TEST_F(NetTypeTest, MyAliasIsPlainTypedef) {
  const hldb::Typedef *const td = getTypedef("myalias");
  ASSERT_NE(td, nullptr);
  EXPECT_FALSE(td->getIsNettype()) << "Sec 6.18: 'typedef', not 'nettype'";
  ASSERT_NE(td->getAlias(), nullptr);
  EXPECT_EQ(td->getAlias()->getName(), std::string_view("some_other_type"));
}

TEST_F(NetTypeTest, UndeclaredSomeOtherTypeIsDiagnosed) {
  const bool found = (findError(ErrorDefinition::COMP_FAILED_TO_BIND, "some_other_type") != nullptr) ||
                     (findError(ErrorDefinition::COMP_UNDEFINED_TYPE, "some_other_type") != nullptr);
  EXPECT_TRUE(found) << "Sec 6.5: 'some_other_type' is never declared";
}

// ---------------------------------------------------------------------------
// typedef struct { real field1; bit field2; } T;
// ---------------------------------------------------------------------------

TEST_F(NetTypeTest, TIsPlainTypedefOfUnpackedStruct) {
  const hldb::Typedef *const td = getTypedef("T");
  ASSERT_NE(td, nullptr);
  EXPECT_FALSE(td->getIsNettype());
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(s->getPacked());
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 2u);
  const hldb::TypespecMember *const f1 = s->getMembers()->at(0);
  const hldb::TypespecMember *const f2 = s->getMembers()->at(1);
  EXPECT_EQ(f1->getName(), std::string_view("field1"));
  ASSERT_NE(f1->getTypespec(), nullptr);
  EXPECT_NE(f1->getTypespec()->getActual<hldb::RealTypespec>(), nullptr);
  EXPECT_EQ(f2->getName(), std::string_view("field2"));
  ASSERT_NE(f2->getTypespec(), nullptr);
  EXPECT_NE(f2->getTypespec()->getActual<hldb::BitTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// nettype T wT;  nettype T wTsum with Tsum;
// ---------------------------------------------------------------------------

TEST_F(NetTypeTest, WTIsUnresolvedNettypeOfT) {
  const hldb::Typedef *const td = getTypedef("wT");
  ASSERT_NE(td, nullptr);
  EXPECT_TRUE(td->getIsNettype());
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::TypedefTypespec *const tts = td->getAlias()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tts, nullptr);
  EXPECT_EQ(tts->getName(), std::string_view("T"));
  EXPECT_EQ(td->getResolutionFunc(), nullptr);
}

TEST_F(NetTypeTest, WTsumIsNettypeOfTResolvedByTsum) {
  const hldb::Typedef *const td = getTypedef("wTsum");
  ASSERT_NE(td, nullptr);
  EXPECT_TRUE(td->getIsNettype());
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::TypedefTypespec *const tts = td->getAlias()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tts, nullptr);
  EXPECT_EQ(tts->getName(), std::string_view("T"));
  const hldb::RefObj *const fn = td->getResolutionFunc();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), std::string_view("Tsum"));
  ASSERT_NE(fn->getActual(), nullptr);
  EXPECT_EQ(fn->getActual()->getAnyType(), hldb::AnyType::Function);
}

TEST_F(NetTypeTest, TsumSignatureMatchesResolutionFunctionRules) {
  const hldb::Function *const fn = getFunction("Tsum");
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getAutomatic()) << "Sec 6.6.7: a resolution function shall be automatic";
  ASSERT_NE(fn->getReturn(), nullptr);
  const hldb::TypedefTypespec *const ret = fn->getReturn()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(ret, nullptr);
  EXPECT_EQ(ret->getName(), std::string_view("T"));
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const drv = fn->getIODecls()->at(0);
  EXPECT_EQ(drv->getName(), std::string_view("driver"));
  EXPECT_EQ(drv->getDirection(), vpiInput);
  ASSERT_NE(drv->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = drv->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiDynamicArray);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::TypedefTypespec *const elem = at->getElemTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_EQ(elem->getName(), std::string_view("T"));
}

TEST_F(NetTypeTest, NoDiagnosticForLegalNettypesWTAndWTsum) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_NETTYPE_RESOLUTION_FUNCTION, 36), nullptr)
      << "Sec 6.6.7: Tsum is a legal resolution function for T";
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_NETTYPE_DATA_TYPE, 34), nullptr)
      << "Sec 6.6.7 (d): unpacked struct of real/bit is a valid nettype data type";
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_NETTYPE_DATA_TYPE, 36), nullptr);
}

// ---------------------------------------------------------------------------
// module dut: my_real my_real_net;  (Sec 6.7.2 net of user-defined nettype)
// ---------------------------------------------------------------------------

TEST_F(NetTypeTest, MyRealNetIsNetNotVariable) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getNets(), nullptr) << "Sec 6.7: 'nettype_identifier net_identifier;' declares a net";
  EXPECT_NE(hldb::findByName<hldb::Net>("my_real_net", dut->getNets()), nullptr);
  if (dut->getVariables() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Variable>("my_real_net", dut->getVariables()), nullptr)
        << "my_real_net must not be modeled as a variable";
  }
}

TEST_F(NetTypeTest, MyRealNetTypeResolvesToMyRealNettype) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getNets(), nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("my_real_net", dut->getNets());
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_EQ(n->getTypespec()->getName(), std::string_view("my_real"));
  EXPECT_NE(n->getTypespec()->getActual(), nullptr) << "'my_real' must bind to the nettype declaration";
}

TEST_F(NetTypeTest, MyRealInDutBindsWithoutError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "my_real", 13), nullptr)
      << "Sec 6.7.2: 'my_real' is a declared nettype and must bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
