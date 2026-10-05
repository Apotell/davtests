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

// Tests for MiniAmiq (tags: MiniAmiq, UVM)
//
// MiniAmiq.hlc sets "-wd ../../../tests/MiniAmiq" and then a second
// "-wd ../../../third_party/UVM"; the source files actually compiled are
// the UVM 1.2 library and the AMIQ SVAUnit framework:
//   uvm-1.2/src/uvm_pkg.sv
//   svaunit/sv/svaunit_pkg.sv          (package svaunit_pkg; import uvm_pkg::*; ...)
//   svaunit/sv/svaunit_vpi_interface.sv
//
// Relevant excerpts:
//   uvm_misc.svh:       virtual class uvm_void; ... endclass
//                       typedef class uvm_object;   (also in uvm_factory.svh,
//                                                    uvm_event_callback.svh)
//   uvm_object.svh:     virtual class uvm_object extends uvm_void;
//   uvm_component.svh:  virtual class uvm_component extends uvm_report_object;
//   uvm_root.svh:       class uvm_root extends uvm_component;
//   svaunit_base.svh:   class svaunit_base extends uvm_test;
//   svaunit_test.svh:   class svaunit_test extends svaunit_base;
//   svaunit_sequence_test.svh:
//     class svaunit_sequence_test#(type SEQ_TYPE=svaunit_base_sequence)
//       extends svaunit_test;
//   svaunit_vpi_interface.sv:
//     interface svaunit_vpi_interface();
//       import svaunit_pkg::*;
//       import uvm_pkg::*;
//       import "DPI-C" context function void register_assertions_dpi(input int print_flag);
//       import "DPI-C" function int dpi_check_flag();
//       string test_name;
//       bit eots_flag = 0;
//       function bit sva_exists(string a_sva_name, string a_sva_path,
//                               svaunit_concurrent_assertion_info a_lof_sva[]);
//         int index[$];
//         index = a_lof_sva.find_index() with ((item.get_sva_name() == a_sva_name)
//                                           && (item.get_sva_path() == a_sva_path));
//         ...
//
// The input is very large; this file keeps to a focused set of
// standard-grounded checks on the class hierarchy, cross-package
// resolution, forward typedefs, and the DPI interface.
//
// What is checked:
//   - packages "uvm_pkg" and "svaunit_pkg" and interface
//     "svaunit_vpi_interface" exist
//   - Sec 8.21 (abstract classes): uvm_void, uvm_object, uvm_component are
//     declared "virtual class"; uvm_root is not
//   - Sec 8.13 (inheritance): uvm_void extends nothing; uvm_object extends
//     uvm_void; uvm_component extends uvm_report_object; uvm_root extends
//     uvm_component -- each "extends" resolves to the ClassDefn declared in
//     uvm_pkg
//   - Sec 26.3 (wildcard import): in svaunit_pkg, "class svaunit_base
//     extends uvm_test" resolves through "import uvm_pkg::*" to the
//     uvm_pkg ClassDefn "uvm_test"; "svaunit_test extends svaunit_base"
//     resolves within svaunit_pkg
//   - Sec 8.25 (parameterized classes): svaunit_sequence_test has a type
//     parameter "SEQ_TYPE"
//   - Sec 6.18: "It shall be legal to have multiple forward type
//     declarations for the same type identifier in the same scope" and
//     "either before or after the final type definition", so no
//     COMP_MULTIPLY_DEFINED_TYPEDEF for "uvm_object", "uvm_phase", or
//     "svaunit_test"
//   - Sec 6.16 / 6.11: the interface declares "string test_name" and
//     "bit eots_flag"
//   - Sec 35.5.4 (import declarations): "import "DPI-C" context function
//     ... register_assertions_dpi" is an imported (vpiDPIImportAcc),
//     context, "DPI-C" function of the interface; "dpi_check_flag" is
//     imported without "context"
//   - Sec 7.12: in "find_index() with (item.get_sva_name() ...)" the
//     iterator argument defaults to "item" (an element of the
//     svaunit_concurrent_assertion_info array), so its methods
//     get_sva_name / get_sva_path must bind: no COMP_FAILED_TO_BIND
//
// What is NOT checked and why:
//   - The tests/MiniAmiq/*.sv testbench files: the second "-wd" makes
//     them unreachable from this command line, so they are not compiled.
//   - Anything requiring elaboration (class specialization of
//     svaunit_sequence_test, uvm_root singleton, etc.): "-d 0" compile,
//     no elaboration.
//   - Exhaustive coverage of UVM: out of scope for a focused test.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/function.h>
#include <hldb/function_decl.h>
#include <hldb/interface.h>
#include <hldb/package.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/type_parameter.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MiniAmiqTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MiniAmiq.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPackage(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Interface *getVpiIf() {
    return hldb::findByDefName<hldb::Interface>("svaunit_vpi_interface", m_design->getAllInterfaces());
  }

  static const hldb::ClassDefn *getClass(std::string_view pkgName, std::string_view className) {
    const hldb::Package *const pkg = getPackage(pkgName);
    if (pkg == nullptr || pkg->getClassDefns() == nullptr) return nullptr;
    // Match on the class_identifier (name or definition name) so a
    // parameterized class is found regardless of how its name is decorated.
    for (const hldb::ClassDefn *const cls : *pkg->getClassDefns()) {
      if (cls != nullptr && (cls->getName() == className || cls->getDefName() == className)) return cls;
    }
    return nullptr;
  }

  // The ClassDefn a class's "extends" clause resolves to, or nullptr.
  static const hldb::ClassDefn *getBaseClass(const hldb::ClassDefn *cls) {
    if (cls == nullptr || cls->getExtends() == nullptr) return nullptr;
    const hldb::RefTypespecCollection *const tss = cls->getExtends()->getClassTypespecs();
    if (tss == nullptr || tss->size() != 1 || tss->at(0) == nullptr) return nullptr;
    const hldb::ClassTypespec *const ct = tss->at(0)->getActual<hldb::ClassTypespec>();
    return (ct == nullptr) ? nullptr : ct->getClassDefn();
  }

  // A DPI import (Sec 35.5.4) has no body, so it may be modeled either as a
  // prototype (FunctionDecl) or as a Function; both carry the DPI flags.
  struct DpiInfo {
    bool m_found = false;
    int32_t m_accessType = 0;
    bool m_context = false;
    int32_t m_cStr = 0;
  };

  static DpiInfo getIfDpiFunction(std::string_view name) {
    DpiInfo info;
    const hldb::Interface *const itf = getVpiIf();
    if (itf == nullptr) return info;
    if (itf->getTaskFuncDecls() != nullptr) {
      for (const hldb::TaskFuncDecl *const d : *itf->getTaskFuncDecls()) {
        if (d != nullptr && d->getName() == name && d->getAnyType() == hldb::AnyType::FunctionDecl) {
          info.m_found = true;
          info.m_accessType = d->getAccessType();
          info.m_context = d->getDPIContext();
          info.m_cStr = d->getDPICStr();
          return info;
        }
      }
    }
    if (itf->getTaskFuncs() != nullptr) {
      const hldb::Function *const f = hldb::findByName<hldb::Function>(name, itf->getTaskFuncs());
      if (f != nullptr) {
        info.m_found = true;
        info.m_accessType = f->getAccessType();
        info.m_context = f->getDPIContext();
        info.m_cStr = f->getDPICStr();
      }
    }
    return info;
  }
};

// ---------------------------------------------------------------------------
// Top-level design units
// ---------------------------------------------------------------------------

TEST_F(MiniAmiqTest, PackagesAndInterfaceExist) {
  EXPECT_NE(getPackage("uvm_pkg"), nullptr) << "package 'uvm_pkg' not found";
  EXPECT_NE(getPackage("svaunit_pkg"), nullptr) << "package 'svaunit_pkg' not found";
  EXPECT_NE(getVpiIf(), nullptr) << "interface 'svaunit_vpi_interface' not found";
}

// ---------------------------------------------------------------------------
// Sec 8.21 / 8.13: UVM base class hierarchy
// ---------------------------------------------------------------------------

TEST_F(MiniAmiqTest, UvmVoidIsAbstractRoot) {
  const hldb::ClassDefn *const v = getClass("uvm_pkg", "uvm_void");
  ASSERT_NE(v, nullptr) << "class 'uvm_void' not found in uvm_pkg";
  EXPECT_TRUE(v->getVirtual()) << "Sec 8.21: 'virtual class uvm_void'";
  EXPECT_EQ(v->getExtends(), nullptr) << "'uvm_void' has no extends clause";
}

TEST_F(MiniAmiqTest, UvmObjectExtendsUvmVoid) {
  const hldb::ClassDefn *const obj = getClass("uvm_pkg", "uvm_object");
  ASSERT_NE(obj, nullptr) << "class 'uvm_object' not found in uvm_pkg";
  EXPECT_TRUE(obj->getVirtual()) << "Sec 8.21: 'virtual class uvm_object'";
  ASSERT_NE(obj->getExtends(), nullptr);
  const hldb::ClassDefn *const base = getBaseClass(obj);
  ASSERT_NE(base, nullptr) << "'extends uvm_void' must resolve to a ClassDefn";
  EXPECT_EQ(base, getClass("uvm_pkg", "uvm_void"));
}

TEST_F(MiniAmiqTest, UvmComponentExtendsUvmReportObject) {
  const hldb::ClassDefn *const comp = getClass("uvm_pkg", "uvm_component");
  ASSERT_NE(comp, nullptr) << "class 'uvm_component' not found in uvm_pkg";
  EXPECT_TRUE(comp->getVirtual()) << "Sec 8.21: 'virtual class uvm_component'";
  const hldb::ClassDefn *const base = getBaseClass(comp);
  ASSERT_NE(base, nullptr) << "'extends uvm_report_object' must resolve to a ClassDefn";
  EXPECT_EQ(base, getClass("uvm_pkg", "uvm_report_object"));
}

TEST_F(MiniAmiqTest, UvmRootIsConcreteAndExtendsUvmComponent) {
  const hldb::ClassDefn *const root = getClass("uvm_pkg", "uvm_root");
  ASSERT_NE(root, nullptr) << "class 'uvm_root' not found in uvm_pkg";
  EXPECT_FALSE(root->getVirtual()) << "'class uvm_root' is not declared virtual";
  const hldb::ClassDefn *const base = getBaseClass(root);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base, getClass("uvm_pkg", "uvm_component"));
}

// ---------------------------------------------------------------------------
// Sec 26.3: cross-package resolution via "import uvm_pkg::*"
// ---------------------------------------------------------------------------

TEST_F(MiniAmiqTest, SvaunitBaseExtendsImportedUvmTest) {
  const hldb::ClassDefn *const sb = getClass("svaunit_pkg", "svaunit_base");
  ASSERT_NE(sb, nullptr) << "class 'svaunit_base' not found in svaunit_pkg";
  const hldb::ClassDefn *const base = getBaseClass(sb);
  ASSERT_NE(base, nullptr) << "'extends uvm_test' must resolve through 'import uvm_pkg::*'";
  ASSERT_NE(getClass("uvm_pkg", "uvm_test"), nullptr);
  EXPECT_EQ(base, getClass("uvm_pkg", "uvm_test"));
}

TEST_F(MiniAmiqTest, SvaunitTestExtendsSvaunitBase) {
  const hldb::ClassDefn *const st = getClass("svaunit_pkg", "svaunit_test");
  ASSERT_NE(st, nullptr) << "class 'svaunit_test' not found in svaunit_pkg";
  const hldb::ClassDefn *const base = getBaseClass(st);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base, getClass("svaunit_pkg", "svaunit_base"));
}

// ---------------------------------------------------------------------------
// Sec 8.25: class svaunit_sequence_test#(type SEQ_TYPE=svaunit_base_sequence)
// ---------------------------------------------------------------------------

TEST_F(MiniAmiqTest, SvaunitSequenceTestHasTypeParameter) {
  const hldb::ClassDefn *const sst = getClass("svaunit_pkg", "svaunit_sequence_test");
  ASSERT_NE(sst, nullptr) << "class 'svaunit_sequence_test' not found in svaunit_pkg";
  ASSERT_NE(sst->getParameters(), nullptr) << "'#(type SEQ_TYPE=...)' parameter list missing";
  const hldb::TypeParameter *found = nullptr;
  for (const hldb::Any *const p : *sst->getParameters()) {
    const hldb::TypeParameter *const tp = any_cast<hldb::TypeParameter>(p);
    if (tp != nullptr && tp->getName() == "SEQ_TYPE") {
      found = tp;
      break;
    }
  }
  EXPECT_NE(found, nullptr) << "type parameter 'SEQ_TYPE' not found";
  EXPECT_EQ(getBaseClass(sst), getClass("svaunit_pkg", "svaunit_test"));
}

// ---------------------------------------------------------------------------
// Sec 6.18: repeated forward typedefs are legal
// ---------------------------------------------------------------------------

TEST_F(MiniAmiqTest, RepeatedForwardTypedefsAreLegal) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_TYPEDEF, "uvm_object"), nullptr)
      << "Sec 6.18: multiple 'typedef class uvm_object;' are legal";
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_TYPEDEF, "uvm_phase"), nullptr)
      << "Sec 6.18: multiple 'typedef class uvm_phase;' are legal";
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_TYPEDEF, "svaunit_test"), nullptr)
      << "Sec 6.18: a forward typedef plus the class definition is legal";
}

// ---------------------------------------------------------------------------
// svaunit_vpi_interface contents
// ---------------------------------------------------------------------------

TEST_F(MiniAmiqTest, InterfaceDeclaresTestNameAndEotsFlag) {
  const hldb::Interface *const itf = getVpiIf();
  ASSERT_NE(itf, nullptr);
  ASSERT_NE(itf->getVariables(), nullptr);

  const hldb::Variable *const tn = hldb::findByName<hldb::Variable>("test_name", itf->getVariables());
  ASSERT_NE(tn, nullptr) << "'string test_name' not found";
  ASSERT_NE(tn->getTypespec(), nullptr);
  ASSERT_NE(tn->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(tn->getTypespec()->getActual()->getAnyType(), hldb::AnyType::StringTypespec);

  const hldb::Variable *const ef = hldb::findByName<hldb::Variable>("eots_flag", itf->getVariables());
  ASSERT_NE(ef, nullptr) << "'bit eots_flag' not found";
  ASSERT_NE(ef->getTypespec(), nullptr);
  ASSERT_NE(ef->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(ef->getTypespec()->getActual()->getAnyType(), hldb::AnyType::BitTypespec);
}

TEST_F(MiniAmiqTest, DpiImportsInInterface) {
  const DpiInfo reg = getIfDpiFunction("register_assertions_dpi");
  ASSERT_TRUE(reg.m_found) << "imported DPI function 'register_assertions_dpi' not found";
  EXPECT_EQ(reg.m_accessType, vpiDPIImportAcc) << "Sec 35.5.4: 'import \"DPI-C\"'";
  EXPECT_TRUE(reg.m_context) << "declared with 'context'";
  EXPECT_EQ(reg.m_cStr, vpiDPIC) << "\"DPI-C\" spec string";

  const DpiInfo chk = getIfDpiFunction("dpi_check_flag");
  ASSERT_TRUE(chk.m_found) << "imported DPI function 'dpi_check_flag' not found";
  EXPECT_EQ(chk.m_accessType, vpiDPIImportAcc);
  EXPECT_FALSE(chk.m_context) << "declared without 'context'";
}

TEST_F(MiniAmiqTest, FindIndexIteratorItemMethodsBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "get_sva_name"), nullptr)
      << "Sec 7.12: 'item' is the default iterator; its class methods must resolve";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "get_sva_path"), nullptr)
      << "Sec 7.12: 'item' is the default iterator; its class methods must resolve";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
