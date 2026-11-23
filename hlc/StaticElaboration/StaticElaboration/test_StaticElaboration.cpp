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

// Tests for dut.sv -- see that file for the full source. This file covers HLC's "static
// elaboration" (definition-level parameter specialization), across all four parameterizable
// definition kinds (module, interface, program, class), plus the harder scoped-lookup cases:
// a class nested in a package, a class nested in another class, the SAME nested-class name
// reused under two different outer scopes, and a parameterized module instantiated from a
// non-top-level module.
//
// IEEE 1800-2023 Sec 6.20 (Parameter and constant declarations) / Sec 23.3 (Module
// instantiation) require that a differently-parameterized use be bound against a distinct,
// independently specialized definition -- a reference to a parameter anywhere in the
// definition's own body (not just its own parameter port list) must resolve against the value
// supplied at THAT use, never the definition's own default.
//
// A per-instance/per-handle #(...) override lives on that use site's own <Kind>Typespec
// (the param assigns of ModuleTypespec/InterfaceTypespec/ProgramTypespec/ClassTypespec) -- NEVER on a RefInstance, which extends Any directly (not Scope),
// so it can never appear in a Typespec's own parent chain; this is also the only design that
// survives a single #(...) shared across a comma-separated hierarchical_instance list (IEEE
// 1800-2023 Annex A.4.1.1), since each instance already gets its own distinct typespec to hold
// its own copy. RefInstance is only ever used below as a private lookup step (find the named
// instance, then its typespec) -- never as the place override data is asserted against.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Database.h>
#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/program.h>
#include <hldb/program_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/type_parameter.h>
#include <hldb/typedef_typespec.h>
#include <hldb/unit.h>
#include <hldb/unsupported_typespec.h>
#include <hldb/variable.h>

#include <algorithm>

namespace hlc {

class StaticElaborationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "StaticElaboration.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // ---- top-level definition lookups ----

  static const hldb::Module *getDut() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }
  static const hldb::Module *getMidLevel() {
    return hldb::findByDefName<hldb::Module>("mid_level", m_design->getAllModules());
  }

  // Nested-module/nested-interface shadowing coverage -- the module/interface counterparts of
  // getOuterCls()/getOtherOuterCls() below.
  static const hldb::Module *getOuterMod() {
    return hldb::findByDefName<hldb::Module>("OuterMod", m_design->getAllModules());
  }
  static const hldb::Module *getOtherOuterMod() {
    return hldb::findByDefName<hldb::Module>("OtherOuterMod", m_design->getAllModules());
  }
  static const hldb::Module *getOuterModWithIface() {
    return hldb::findByDefName<hldb::Module>("OuterModWithIface", m_design->getAllModules());
  }
  static const hldb::Module *getOtherOuterModWithIface() {
    return hldb::findByDefName<hldb::Module>("OtherOuterModWithIface", m_design->getAllModules());
  }
  static const hldb::Module *getNestedModule(const hldb::Module *outer, std::string_view name) {
    if ((outer == nullptr) || (outer->getModules() == nullptr)) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, outer->getModules());
  }
  static const hldb::Interface *getNestedInterface(const hldb::Module *outer, std::string_view name) {
    if ((outer == nullptr) || (outer->getInterfaces() == nullptr)) return nullptr;
    return hldb::findByDefName<hldb::Interface>(name, outer->getInterfaces());
  }

  // The template/base definitions -- never mutated by static elaboration; a specialization is
  // always a separate, additional object.
  static const hldb::Module *getParamMod() {
    return hldb::findByDefName<hldb::Module>("param_mod", m_design->getAllModules());
  }
  static const hldb::Interface *getParamIf() {
    return hldb::findByDefName<hldb::Interface>("param_if", m_design->getAllInterfaces());
  }
  static const hldb::Program *getParamProg() {
    return hldb::findByDefName<hldb::Program>("param_prog", m_design->getAllPrograms());
  }
  static const hldb::ClassDefn *getParamCls() {
    return (m_unit == nullptr) ? nullptr : hldb::findByDefName<hldb::ClassDefn>("param_cls", m_unit->getClassDefns());
  }
  static const hldb::Package *getPkgA() { return hldb::findByName<hldb::Package>("pkg_a", m_design->getAllPackages()); }
  static const hldb::Package *getPkgB() { return hldb::findByName<hldb::Package>("pkg_b", m_design->getAllPackages()); }

  // Package imports -- pkg_import_a/pkg_import_b each declare a class of the SAME
  // name, deliberately, so a bare reference is unresolvable without an import disambiguating
  // which one is meant; pkg_import_c is the wildcard-import counterpart.
  static const hldb::Package *getPkgImportA() {
    return hldb::findByName<hldb::Package>("pkg_import_a", m_design->getAllPackages());
  }
  static const hldb::Package *getPkgImportB() {
    return hldb::findByName<hldb::Package>("pkg_import_b", m_design->getAllPackages());
  }
  static const hldb::Package *getPkgImportC() {
    return hldb::findByName<hldb::Package>("pkg_import_c", m_design->getAllPackages());
  }
  static const hldb::ClassDefn *getOuterCls() {
    return (m_unit == nullptr) ? nullptr : hldb::findByDefName<hldb::ClassDefn>("OuterCls", m_unit->getClassDefns());
  }
  static const hldb::ClassDefn *getOtherOuterCls() {
    return (m_unit == nullptr) ? nullptr : hldb::findByDefName<hldb::ClassDefn>("OtherOuterCls", m_unit->getClassDefns());
  }
  static const hldb::ClassDefn *getNestedClass(const hldb::ClassDefn *outer, std::string_view name) {
    if ((outer == nullptr) || (outer->getClassDefns() == nullptr)) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>(name, outer->getClassDefns());
  }
  static const hldb::ClassDefn *getPkgClass(const hldb::Package *pkg, std::string_view name) {
    if ((pkg == nullptr) || (pkg->getClassDefns() == nullptr)) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>(name, pkg->getClassDefns());
  }
  static const hldb::Package *getPkgDeep() {
    return hldb::findByName<hldb::Package>("pkg_deep", m_design->getAllPackages());
  }
  static const hldb::Package *getPkgTd() { return hldb::findByName<hldb::Package>("pkg_td", m_design->getAllPackages()); }

  // TdOuter is declared directly inside `dut` (a Module), not at file/package scope like
  // param_cls/OuterCls/etc. -- m_design->getAllClasses() does NOT include it (that Design-level
  // aggregate only reflects file-scope class declarations). Search `owner`'s own class
  // declarations instead.
  static const hldb::ClassDefn *getModuleLocalClass(const hldb::Module *owner, std::string_view name) {
    if ((owner == nullptr) || (owner->getClassDefns() == nullptr)) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>(name, owner->getClassDefns());
  }

  // The only specialization of the class `defName` among `classes` (a specialization is parented
  // next to the class it was made from); nullptr if there is none or more than one.
  static const hldb::ClassDefn *getOnlySpecialization(const hldb::ClassDefnCollection *classes,
                                                      std::string_view defName) {
    if (classes == nullptr) return nullptr;
    const hldb::ClassDefn *found = nullptr;
    for (const hldb::ClassDefn *const cd : *classes) {
      if (!cd->getIsSpecialization() || (cd->getDefName() != defName)) continue;
      if (found != nullptr) return nullptr;
      found = cd;
    }
    return found;
  }

  // HLC models a scoped (`::`-qualified) class/typedef reference as ONE outer, chain-owning
  // RefTypespec whose own path_elem chain is
  // [RefObj(segment 0), ..., RefObj(segment N-2), RefTypespec(segment N-1)]. A VARIABLE's own
  // declared typespec (what handle->getTypespec<RefTypespec>() returns) is this OUTER
  // chain-owner for a multi-segment reference, but for a single-segment (unscoped) reference it
  // IS the terminal directly (no chain at all). resolveTerminalRefTypespec() normalizes both
  // cases to "the terminal RefTypespec whose own `actual` is the final resolved typespec."
  //
  // IMPORTANT: check `path_elem` first, NOT `getActual()` first -- the chain-owning
  // RefTypespec's own `actual` can ALSO be non-null (e.g. deep_handle's own outer RefTypespec
  // has both a 4-item path_elem AND its own `actual` set to the chain's final ClassTypespec).
  // A non-empty `path_elem` always means "I am the outer chain-owner, the real terminal is at
  // the back"; checking `getActual()` first would return the OUTER owner itself, whose own
  // `.parent` is the declaring Variable/member (not a RefTypespec), breaking every
  // getChainSegmentClassDefn()/`ownerRt` walk downstream.
  static const hldb::RefTypespec *resolveTerminalRefTypespec(const hldb::RefTypespec *declaredRt) {
    if (declaredRt == nullptr) return nullptr;
    const hldb::AnyCollection *const pathElems = declaredRt->getPathElems();
    if ((pathElems != nullptr) && !pathElems->empty()) {
      return any_cast<hldb::RefTypespec>(pathElems->back());
    }
    if (declaredRt->getActual() != nullptr) return declaredRt;
    return nullptr;
  }

  // Returns the ClassDefn a scoped chain's own segment[index] resolved to, given the chain's
  // TERMINAL RefTypespec (e.g. from getHandleRefTypespec()) -- walks up one level to the
  // chain-owning RefTypespec first. Segment 0..N-2 are plain RefObj (resolved directly via
  // RefObj::actual -- no wrapping ClassTypespec for an intermediate segment); segment N-1 (the
  // chain's own terminal)
  // is a RefTypespec wrapping a ClassTypespec.
  static const hldb::ClassDefn *getChainSegmentClassDefn(const hldb::RefTypespec *terminalRt, size_t index) {
    if (terminalRt == nullptr) return nullptr;
    const hldb::RefTypespec *const ownerRt = terminalRt->getParent<hldb::RefTypespec>();
    if ((ownerRt == nullptr) || (ownerRt->getPathElems() == nullptr) || (index >= ownerRt->getPathElems()->size())) {
      return nullptr;
    }
    hldb::Any *const segElem = ownerRt->getPathElems()->at(index);
    if (const hldb::RefObj *const ro = any_cast<hldb::RefObj>(segElem)) {
      return ro->getActual<hldb::ClassDefn>();
    }
    if (const hldb::RefTypespec *const segRt = any_cast<hldb::RefTypespec>(segElem)) {
      const hldb::ClassTypespec *const segCt = segRt->getActual<hldb::ClassTypespec>();
      return (segCt == nullptr) ? nullptr : segCt->getClassDefn();
    }
    return nullptr;
  }

  // ---- instance/handle typespec lookups ----
  //
  // RefInstance is used here purely as a private lookup step (find the named instance, then
  // the <Kind>Typespec its own RefTypespec::actual points at) -- never exposed to callers, and
  // never where override data is checked (see the file-level comment above for why).

  template <typename TypespecT>
  static const TypespecT *getInstanceTypespec(std::string_view instanceName, const hldb::Module *scope) {
    if ((scope == nullptr) || (scope->getRefInstances() == nullptr)) return nullptr;
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>(instanceName, scope->getRefInstances());
    if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return nullptr;
    return inst->getTypespec()->getActual<TypespecT>();
  }

  static const hldb::ModuleTypespec *getInstDefaultTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("inst_default", getDut());
  }
  static const hldb::ModuleTypespec *getInstWideTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("inst_wide", getDut());
  }
  static const hldb::ModuleTypespec *getMidInstTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("mid_inst", getDut());
  }
  static const hldb::ModuleTypespec *getMidNestedSameAsWideTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("inst_nested_same_as_wide", getMidLevel());
  }
  static const hldb::ModuleTypespec *getMidNestedDistinctTypespec() {
    return getInstanceTypespec<hldb::ModuleTypespec>("inst_nested_distinct", getMidLevel());
  }

  static const hldb::InterfaceTypespec *getIfDefaultTypespec() {
    return getInstanceTypespec<hldb::InterfaceTypespec>("if_default", getDut());
  }
  static const hldb::InterfaceTypespec *getIfWideTypespec() {
    return getInstanceTypespec<hldb::InterfaceTypespec>("if_wide", getDut());
  }

  static const hldb::ProgramTypespec *getProgDefaultTypespec() {
    return getInstanceTypespec<hldb::ProgramTypespec>("prog_default", getDut());
  }
  static const hldb::ProgramTypespec *getProgWideTypespec() {
    return getInstanceTypespec<hldb::ProgramTypespec>("prog_wide", getDut());
  }

  // A class handle is a plain Variable, not a RefInstance -- its own declared typespec is
  // reached via Variable::getTypespec<RefTypespec>(), matching the existing, already-passing
  // Google/chapter-8/8.5--parameters davtests convention; resolveTerminalRefTypespec() then
  // normalizes a (possibly scoped, possibly chain-owning) declared typespec down to the
  // chain's own terminal RefTypespec -- see that function's own comment.
  static const hldb::RefTypespec *getHandleRefTypespec(std::string_view handleName) {
    const hldb::Module *const dut = getDut();
    if ((dut == nullptr) || (dut->getVariables() == nullptr)) return nullptr;
    const hldb::Variable *const handle = hldb::findByName<hldb::Variable>(handleName, dut->getVariables());
    if ((handle == nullptr) || (handle->getTypespec() == nullptr)) return nullptr;
    return resolveTerminalRefTypespec(handle->getTypespec<hldb::RefTypespec>());
  }

  static const hldb::ClassTypespec *getHandleTypespec(std::string_view handleName) {
    const hldb::RefTypespec *const rt = getHandleRefTypespec(handleName);
    return (rt == nullptr) ? nullptr : rt->getActual<hldb::ClassTypespec>();
  }

  // A typedef'd handle's own terminal RefTypespec::actual is the TypedefTypespec itself --
  // HLC never dereferences through a typedef automatically (consumers follow
  // Typedef::getAlias() themselves, via the owning TypedefTypespec's own getTypedef()) -- so
  // getHandleTypespec() (which expects a ClassTypespec directly) does not apply to a typedef'd
  // handle. This is the typedef analog of it.
  static const hldb::TypedefTypespec *getHandleTypedefTypespec(std::string_view handleName) {
    const hldb::RefTypespec *const rt = getHandleRefTypespec(handleName);
    return (rt == nullptr) ? nullptr : rt->getActual<hldb::TypedefTypespec>();
  }

  // Follows a TypedefTypespec's own Typedef::getAlias() to whatever ClassTypespec it ultimately
  // resolved to (the aliased type's own use-site typespec, specialized if it carried an
  // override) -- nullptr if the alias itself resolved to something else (e.g. another typedef;
  // see getHandleTypedefTypespec() applied a second time for that case).
  static const hldb::ClassTypespec *getTypedefAliasClassTypespec(const hldb::TypedefTypespec *td) {
    if ((td == nullptr) || (td->getTypedef() == nullptr) || (td->getTypedef()->getAlias() == nullptr)) return nullptr;
    return td->getTypedef()->getAlias()->getActual<hldb::ClassTypespec>();
  }

  // Same idea as getHandleTypedefTypespec(), but for a plain data member of some OTHER class
  // (not dut itself) -- used to confirm a bare typedef reference resolves by walking up the
  // enclosing-scope chain, not just the member's own immediate scope.
  static const hldb::TypedefTypespec *getMemberTypedefTypespec(const hldb::ClassDefn *owner,
                                                                 std::string_view memberName) {
    if ((owner == nullptr) || (owner->getVariables() == nullptr)) return nullptr;
    const hldb::Variable *const member = hldb::findByName<hldb::Variable>(memberName, owner->getVariables());
    if ((member == nullptr) || (member->getTypespec() == nullptr)) return nullptr;
    const hldb::RefTypespec *const rt = member->getTypespec<hldb::RefTypespec>();
    if (rt == nullptr) return nullptr;
    return rt->getActual<hldb::TypedefTypespec>();
  }

  // Same idea as getHandleTypespec(), but for a plain data member of some OTHER class (not dut
  // itself) -- used to confirm a BARE class-name reference resolves by walking up the
  // enclosing-scope chain (local shadowing), not by a flat, global name->ClassDefn lookup.
  static const hldb::ClassTypespec *getMemberClassTypespec(const hldb::ClassDefn *owner, std::string_view memberName) {
    if ((owner == nullptr) || (owner->getVariables() == nullptr)) return nullptr;
    const hldb::Variable *const member = hldb::findByName<hldb::Variable>(memberName, owner->getVariables());
    if ((member == nullptr) || (member->getTypespec() == nullptr)) return nullptr;
    const hldb::RefTypespec *const rt = member->getTypespec<hldb::RefTypespec>();
    if (rt == nullptr) return nullptr;
    return rt->getActual<hldb::ClassTypespec>();
  }

  // ---- whole-graph counting helpers ----
  //
  // Database::getObjects<T>() is a flat, unconditional sweep of every object of that exact
  // type ever created anywhere in the WHOLE compilation -- unlike m_design->getAllClasses()
  // (which does NOT include module-nested classes), a count taken this way is a ground-truth
  // object-graph census, not a curated aggregate that could itself be incomplete.
  // Used below to assert exact object counts across the whole StaticElaboration compilation --
  // a coarser, whole-graph counterpart to the individual dedup/identity checks above: those
  // confirm a handful of SPECIFIC use sites resolve correctly, but a stray extra (or missing)
  // object anywhere else in the file -- e.g. an orphaned, wrongly-based duplicate ClassDefn
  // not reachable via any path_elems chain the other tests check -- would slip past every one
  // of them, since none of them inspects the graph as a whole.

  template <typename T>
  static size_t countAll() {
    return m_session->getDatabase().getObjects<T>().size();
  }

  // Counts only the objects of type T whose own vpiIsSpecialization flag is set -- i.e. the
  // ones HLC created as specializations, not a base definition nor an unspecialized
  // clone that merely came along for free as part of cloning some OTHER definition's own whole
  // subtree (e.g. pkg_deep::Level1's own nested Level2/Level3 get cloned when Level1 itself is
  // specialized, but neither clone is itself is_specialization==true unless ALSO independently
  // given its own #(...) override at some use site).
  template <typename T>
  static size_t countSpecializations() {
    size_t n = 0;
    for (hldb::Any *const source : m_session->getDatabase().getObjects<T>()) {
      if (const T *const t = any_cast<T>(source)) {
        if (t->getIsSpecialization()) ++n;
      }
    }
    return n;
  }

  // ---- generic per-definition helpers (Module/Interface/Program/ClassDefn all share this
  // shape, inherited from Scope) ----

  template <typename T>
  static const hldb::Parameter *getParameter(const T *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  template <typename T>
  static const hldb::Variable *getVariableByName(const T *scope, std::string_view name) {
    if ((scope == nullptr) || (scope->getVariables() == nullptr)) return nullptr;
    return hldb::findByName<hldb::Variable>(name, scope->getVariables());
  }

  // ---- extends / super helpers ----

  static const hldb::ClassDefn *getSuperBase() {
    return (m_unit == nullptr) ? nullptr : hldb::findByDefName<hldb::ClassDefn>("SuperBase", m_unit->getClassDefns());
  }
  static const hldb::ClassDefn *getSuperDerived() {
    return (m_unit == nullptr) ? nullptr : hldb::findByDefName<hldb::ClassDefn>("SuperDerived", m_unit->getClassDefns());
  }

  // The class `cd` extends: its extends clause's class typespec, followed to the class definition
  // it resolved to. A class extends exactly one class (IEEE 1800-2023 Sec 8.13), so the first
  // entry is the only one.
  static const hldb::ClassDefn *getExtendedClass(const hldb::ClassDefn *cd) {
    if ((cd == nullptr) || (cd->getExtends() == nullptr)) return nullptr;
    const hldb::RefTypespecCollection *const rts = cd->getExtends()->getClassTypespecs();
    if ((rts == nullptr) || rts->empty()) return nullptr;
    const hldb::ClassTypespec *const ct = rts->front()->getActual<hldb::ClassTypespec>();
    return (ct == nullptr) ? nullptr : ct->getClassDefn();
  }

  // The `super` reference written inside `owner` (SuperDerived's derived_fn() has exactly one).
  // Found by name among the references whose enclosing class is `owner`, so the test does not
  // depend on how the call `super.base_fn()` itself is modeled.
  static const hldb::RefObj *getSuperRef(const hldb::ClassDefn *owner) {
    if (owner == nullptr) return nullptr;
    for (hldb::Any *const source : m_session->getDatabase().getObjects<hldb::RefObj>()) {
      const hldb::RefObj *const ro = any_cast<hldb::RefObj>(source);
      if ((ro == nullptr) || (ro->getParent() == nullptr) || (ro->getName() != std::string_view("super"))) continue;
      if (hldb::getParent<hldb::ClassDefn>(ro) == owner) return ro;
    }
    return nullptr;
  }

  // The value of type parameter `name` in `cd`: what its vpiExpr is bound to (the default in a
  // written definition, the override in a specialization).
  static const hldb::Typespec *getTypeParameterValue(const hldb::ClassDefn *cd, std::string_view name) {
    if (cd == nullptr) return nullptr;
    const hldb::TypeParameter *const tp = hldb::findByName<hldb::TypeParameter>(name, cd->getParameters());
    if ((tp == nullptr) || (tp->getExpr() == nullptr)) return nullptr;
    return tp->getExpr()->getActual();
  }

  static bool containsClass(const hldb::ClassDefnCollection *classes, const hldb::ClassDefn *cd) {
    return (classes != nullptr) && (std::find(classes->cbegin(), classes->cend(), cd) != classes->cend());
  }

  // ---- canonical-key helpers ----

  // A class declared at compilation-unit scope.
  static const hldb::ClassDefn *getUnitClass(std::string_view name) {
    return (m_unit == nullptr) ? nullptr : hldb::findByDefName<hldb::ClassDefn>(name, m_unit->getClassDefns());
  }

  // The class a dut handle's declared type resolved to.
  static const hldb::ClassDefn *getHandleClass(std::string_view handleName) {
    const hldb::ClassTypespec *const ct = getHandleTypespec(handleName);
    return (ct == nullptr) ? nullptr : ct->getClassDefn();
  }

  // The type a typespec names, typedefs followed (a typedef names the same type, IEEE 1800-2023 Sec
  // 6.18).
  static const hldb::Typespec *followTypedefs(const hldb::Typespec *ts) {
    for (size_t guard = 0; (ts != nullptr) && (guard < 16); ++guard) {
      const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(ts);
      if (td == nullptr) return ts;
      if ((td->getTypedef() == nullptr) || (td->getTypedef()->getAlias() == nullptr)) return nullptr;
      ts = td->getTypedef()->getAlias()->getActual();
    }
    return nullptr;
  }
};

// =============================================================================================
// Module (baseline)
// =============================================================================================

TEST_F(StaticElaborationTest, DutModuleExists) { EXPECT_NE(getDut(), nullptr); }

TEST_F(StaticElaborationTest, ParamModModuleExists) { EXPECT_NE(getParamMod(), nullptr); }

TEST_F(StaticElaborationTest, ParamModHasOneParameterW) {
  const hldb::Module *const paramMod = getParamMod();
  ASSERT_NE(paramMod, nullptr);
  ASSERT_NE(paramMod->getParameters(), nullptr);
  ASSERT_EQ(paramMod->getParameters()->size(), 1u);
  const hldb::Parameter *const w = any_cast<hldb::Parameter>(paramMod->getParameters()->at(0));
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getName(), std::string_view("W"));
  EXPECT_FALSE(w->getLocalParam()) << "'parameter int W = 8' is a parameter, not a localparam";
}

TEST_F(StaticElaborationTest, ParamModDefaultAssignForWIsEight) {
  const hldb::Parameter *const p = getParameter(getParamMod(), "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("8"));
}

TEST_F(StaticElaborationTest, BothInstanceTypespecsExist) {
  EXPECT_NE(getInstDefaultTypespec(), nullptr);
  EXPECT_NE(getInstWideTypespec(), nullptr);
}

TEST_F(StaticElaborationTest, InstDefaultHasNoOverrides) {
  const hldb::ModuleTypespec *const ts = getInstDefaultTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE((ts->getParamAssigns() == nullptr) || ts->getParamAssigns()->empty());
}

TEST_F(StaticElaborationTest, InstWideOverridesWToSixteen) {
  const hldb::ModuleTypespec *const ts = getInstWideTypespec();
  ASSERT_NE(ts, nullptr);
  ASSERT_NE(ts->getParamAssigns(), nullptr);
  ASSERT_EQ(ts->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = ts->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getLhs(), nullptr);
  EXPECT_EQ(pa->getLhs()->getName(), std::string_view("W"));
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("16"));
  EXPECT_TRUE(pa->getConnByName()) << "'.W(16)' is a by-name connection";
}

TEST_F(StaticElaborationTest, InstDefaultResolvesToBaseDefinition) {
  const hldb::ModuleTypespec *const ts = getInstDefaultTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->getModule(), getParamMod());
}

TEST_F(StaticElaborationTest, InstWideResolvesToDistinctSpecializedDefinition) {
  const hldb::Module *const modDefault = getInstDefaultTypespec()->getModule();
  const hldb::Module *const modWide = getInstWideTypespec()->getModule();
  ASSERT_NE(modDefault, nullptr);
  ASSERT_NE(modWide, nullptr) << "inst_wide's override must resolve to a specialized Module";
  EXPECT_NE(modWide, modDefault);
  EXPECT_NE(modWide, getParamMod());
}

TEST_F(StaticElaborationTest, SpecializedModuleHasWOverriddenToSixteen) {
  const hldb::Module *const modWide = getInstWideTypespec()->getModule();
  ASSERT_NE(modWide, nullptr);
  const hldb::Parameter *const p = getParameter(modWide, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("16"));
}

// Confirms the full-clone build strategy (as opposed to a delta/overlay one that shares
// parameter-independent children by pointer): a plain variable declaration elsewhere in the
// module body must be its OWN, independent object on the specialization, not shared with the
// template.
TEST_F(StaticElaborationTest, SpecializedModuleOwnsItsOwnCopyOfInternalReg) {
  const hldb::Module *const modWide = getInstWideTypespec()->getModule();
  ASSERT_NE(modWide, nullptr);
  const hldb::Variable *const baseReg = getVariableByName(getParamMod(), "internal_reg");
  const hldb::Variable *const specializedReg = getVariableByName(modWide, "internal_reg");
  ASSERT_NE(baseReg, nullptr);
  ASSERT_NE(specializedReg, nullptr);
  EXPECT_NE(specializedReg, baseReg);
}

TEST_F(StaticElaborationTest, CompilerReportsNoEvents) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
}

// =============================================================================================
// Interface
// =============================================================================================

TEST_F(StaticElaborationTest, ParamIfInterfaceExists) { EXPECT_NE(getParamIf(), nullptr); }

TEST_F(StaticElaborationTest, ParamIfHasOneParameterW) {
  const hldb::Interface *const paramIf = getParamIf();
  ASSERT_NE(paramIf, nullptr);
  ASSERT_NE(paramIf->getParameters(), nullptr);
  ASSERT_EQ(paramIf->getParameters()->size(), 1u);
  const hldb::Parameter *const w = any_cast<hldb::Parameter>(paramIf->getParameters()->at(0));
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(w->getName(), std::string_view("W"));
}

TEST_F(StaticElaborationTest, IfDefaultResolvesToBaseInterface) {
  const hldb::InterfaceTypespec *const ts = getIfDefaultTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->getInterface(), getParamIf());
}

TEST_F(StaticElaborationTest, IfWideResolvesToDistinctSpecializedInterface) {
  const hldb::InterfaceTypespec *const tsDefault = getIfDefaultTypespec();
  const hldb::InterfaceTypespec *const tsWide = getIfWideTypespec();
  ASSERT_NE(tsDefault, nullptr);
  ASSERT_NE(tsWide, nullptr);
  const hldb::Interface *const ifDefault = tsDefault->getInterface();
  const hldb::Interface *const ifWide = tsWide->getInterface();
  ASSERT_NE(ifDefault, nullptr);
  ASSERT_NE(ifWide, nullptr);
  EXPECT_NE(ifWide, ifDefault);
  EXPECT_NE(ifWide, getParamIf());
}

TEST_F(StaticElaborationTest, SpecializedInterfaceHasWOverriddenToEight) {
  const hldb::Interface *const ifWide = getIfWideTypespec()->getInterface();
  ASSERT_NE(ifWide, nullptr);
  const hldb::Parameter *const p = getParameter(ifWide, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("8"));
}

TEST_F(StaticElaborationTest, SpecializedInterfaceOwnsItsOwnCopyOfData) {
  const hldb::Interface *const ifWide = getIfWideTypespec()->getInterface();
  ASSERT_NE(ifWide, nullptr);
  const hldb::Variable *const baseData = getVariableByName(getParamIf(), "data");
  const hldb::Variable *const specializedData = getVariableByName(ifWide, "data");
  ASSERT_NE(baseData, nullptr);
  ASSERT_NE(specializedData, nullptr);
  EXPECT_NE(specializedData, baseData);
}

// =============================================================================================
// Program
// =============================================================================================

TEST_F(StaticElaborationTest, ParamProgProgramExists) { EXPECT_NE(getParamProg(), nullptr); }

TEST_F(StaticElaborationTest, ProgDefaultResolvesToBaseProgram) {
  const hldb::ProgramTypespec *const ts = getProgDefaultTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->getProgram(), getParamProg());
}

TEST_F(StaticElaborationTest, ProgWideResolvesToDistinctSpecializedProgram) {
  const hldb::ProgramTypespec *const tsDefault = getProgDefaultTypespec();
  const hldb::ProgramTypespec *const tsWide = getProgWideTypespec();
  ASSERT_NE(tsDefault, nullptr);
  ASSERT_NE(tsWide, nullptr);
  const hldb::Program *const progDefault = tsDefault->getProgram();
  const hldb::Program *const progWide = tsWide->getProgram();
  ASSERT_NE(progDefault, nullptr);
  ASSERT_NE(progWide, nullptr);
  EXPECT_NE(progWide, progDefault);
  EXPECT_NE(progWide, getParamProg());
}

TEST_F(StaticElaborationTest, SpecializedProgramHasWOverriddenToFour) {
  const hldb::Program *const progWide = getProgWideTypespec()->getProgram();
  ASSERT_NE(progWide, nullptr);
  const hldb::Parameter *const p = getParameter(progWide, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("4"));
}

// =============================================================================================
// Class -- plain, no package/nesting
// =============================================================================================

TEST_F(StaticElaborationTest, ParamClsClassExists) { EXPECT_NE(getParamCls(), nullptr); }

TEST_F(StaticElaborationTest, ClsDefaultHandleResolvesToBaseClass) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("cls_default_handle");
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getParamCls());
}

TEST_F(StaticElaborationTest, ClsWideHandleResolvesToDistinctSpecializedClass) {
  const hldb::ClassTypespec *const ctDefault = getHandleTypespec("cls_default_handle");
  const hldb::ClassTypespec *const ctWide = getHandleTypespec("cls_wide_handle");
  ASSERT_NE(ctDefault, nullptr);
  ASSERT_NE(ctWide, nullptr);
  const hldb::ClassDefn *const clsDefault = ctDefault->getClassDefn();
  const hldb::ClassDefn *const clsWide = ctWide->getClassDefn();
  ASSERT_NE(clsDefault, nullptr);
  ASSERT_NE(clsWide, nullptr);
  EXPECT_NE(clsWide, clsDefault);
  EXPECT_NE(clsWide, getParamCls());
}

TEST_F(StaticElaborationTest, SpecializedClsHasWOverriddenToNine) {
  const hldb::ClassDefn *const clsWide = getHandleTypespec("cls_wide_handle")->getClassDefn();
  ASSERT_NE(clsWide, nullptr);
  const hldb::Parameter *const p = getParameter(clsWide, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("9"));
}

TEST_F(StaticElaborationTest, SpecializedClsOwnsItsOwnCopyOfPayload) {
  const hldb::ClassDefn *const clsWide = getHandleTypespec("cls_wide_handle")->getClassDefn();
  ASSERT_NE(clsWide, nullptr);
  const hldb::Variable *const basePayload = getVariableByName(getParamCls(), "payload");
  const hldb::Variable *const specializedPayload = getVariableByName(clsWide, "payload");
  ASSERT_NE(basePayload, nullptr);
  ASSERT_NE(specializedPayload, nullptr);
  EXPECT_NE(specializedPayload, basePayload);
}

// =============================================================================================
// Class nested inside a package -- pkg_a::PkgWidget / pkg_b::PkgWidget, same
// nested class name under two different packages.
// =============================================================================================

TEST_F(StaticElaborationTest, PkgAAndPkgBExist) {
  EXPECT_NE(getPkgA(), nullptr);
  EXPECT_NE(getPkgB(), nullptr);
}

TEST_F(StaticElaborationTest, PkgAAndPkgBEachHaveTheirOwnDistinctPkgWidget) {
  const hldb::ClassDefn *const widgetA = getPkgClass(getPkgA(), "PkgWidget");
  const hldb::ClassDefn *const widgetB = getPkgClass(getPkgB(), "PkgWidget");
  ASSERT_NE(widgetA, nullptr);
  ASSERT_NE(widgetB, nullptr);
  EXPECT_NE(widgetA, widgetB) << "pkg_a::PkgWidget and pkg_b::PkgWidget share a name but must be "
                                 "distinct ClassDefn objects";
}

TEST_F(StaticElaborationTest, PkgAHandleResolvesToSpecializationOfPkgAsWidget) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("pkg_a_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "pkg_a::PkgWidget#(10) must resolve -- scoped lookup must "
                                     "search pkg_a's own class list, not a flat/global one";
  EXPECT_NE(specialized, getPkgClass(getPkgA(), "PkgWidget"))
      << "an override-bearing use must specialize, not bind straight to the template";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("10"));
}

TEST_F(StaticElaborationTest, PkgBHandleResolvesToSpecializationOfPkgBsWidget) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("pkg_b_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "pkg_b::PkgWidget#(20) must resolve against pkg_b's OWN "
                                     "PkgWidget, not be confused with pkg_a's same-named one";
  EXPECT_NE(specialized, getPkgClass(getPkgB(), "PkgWidget"));
  EXPECT_NE(specialized, getHandleTypespec("pkg_a_handle")->getClassDefn())
      << "pkg_a's and pkg_b's specializations must never collide, even though both PkgWidget "
         "classes share a name";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("20"));
}

// =============================================================================================
// Class nested inside another class -- OuterCls::InnerCls /
// OtherOuterCls::InnerCls, same nested class name under two different outer classes.
// =============================================================================================

TEST_F(StaticElaborationTest, OuterClsAndOtherOuterClsExist) {
  EXPECT_NE(getOuterCls(), nullptr);
  EXPECT_NE(getOtherOuterCls(), nullptr);
}

TEST_F(StaticElaborationTest, OuterClsAndOtherOuterClsEachHaveTheirOwnDistinctInnerCls) {
  const hldb::ClassDefn *const innerA = getNestedClass(getOuterCls(), "InnerCls");
  const hldb::ClassDefn *const innerB = getNestedClass(getOtherOuterCls(), "InnerCls");
  ASSERT_NE(innerA, nullptr);
  ASSERT_NE(innerB, nullptr);
  EXPECT_NE(innerA, innerB) << "OuterCls::InnerCls and OtherOuterCls::InnerCls share a name but "
                               "must be distinct ClassDefn objects";
}

TEST_F(StaticElaborationTest, OuterInnerHandleResolvesToSpecializationOfOuterClsInnerCls) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("outer_inner_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "OuterCls::InnerCls#(12) must resolve -- scoped lookup must "
                                     "search OuterCls's own nested class list";
  EXPECT_NE(specialized, getNestedClass(getOuterCls(), "InnerCls"));
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("12"));
}

TEST_F(StaticElaborationTest, OtherOuterInnerHandleResolvesToSpecializationOfOtherOuterClsInnerCls) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("other_outer_inner_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "OtherOuterCls::InnerCls#(24) must resolve against "
                                     "OtherOuterCls's OWN InnerCls, not be confused with "
                                     "OuterCls's same-named one";
  EXPECT_NE(specialized, getNestedClass(getOtherOuterCls(), "InnerCls"));
  EXPECT_NE(specialized, getHandleTypespec("outer_inner_handle")->getClassDefn())
      << "OuterCls's and OtherOuterCls's specializations must never collide, even though both "
         "InnerCls classes share a name";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("24"));
}

// =============================================================================================
// Local shadowing for a BARE class-name reference (the class counterpart of the typedef
// walk-up tested further below). "InnerCls" is NOT globally unique (OuterCls and OtherOuterCls both
// declare their own), so a flat, scope-blind lookup can never resolve a BARE reference to it
// at all -- it must walk outward from the reference's own scope, finding OuterCls's OWN
// InnerCls directly, without ever needing (or reaching) the ambiguous global name.
// =============================================================================================

TEST_F(StaticElaborationTest, ShadowTestHandleResolvesToOuterClsOwnInnerClsViaBareReference) {
  const hldb::ClassTypespec *const ct = getMemberClassTypespec(getOuterCls(), "shadow_test_handle");
  ASSERT_NE(ct, nullptr) << "a bare (unqualified) 'InnerCls' reference, written from within "
                             "OuterCls's own scope, must resolve by walking up the enclosing "
                             "scopes -- OuterCls's own InnerCls is declared directly in the "
                             "current scope, so no `::` qualification is needed to see it";
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  EXPECT_NE(specialized, getNestedClass(getOuterCls(), "InnerCls"))
      << "an override-bearing use (#(15)) must specialize, not bind straight to the template";
  EXPECT_NE(specialized, getHandleTypespec("outer_inner_handle")->getClassDefn())
      << "#(15) and #(12) are different overrides of the same OuterCls::InnerCls base -- must "
         "be distinct specializations";
  EXPECT_NE(specialized, getNestedClass(getOtherOuterCls(), "InnerCls"))
      << "must never be confused with OtherOuterCls's own, identically-named InnerCls -- the "
         "whole point of walking up from shadow_test_handle's own scope (OuterCls) rather than "
         "falling back to some global, name-only lookup";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("15"));
}

// The module/interface counterparts of the class-shadowing test above -- IEEE 1800-2023 Sec
// 3.13/3.14.2.3 confirm modules and interfaces can be lexically nested the same way classes can
// (unlike programs and packages, which cannot), so the identical shadowing hazard applies:
// OuterMod's own InnerMod must be found before OtherOuterMod's identically-named one, by
// walking up the enclosing scopes, not by a flat/global lookup.
TEST_F(StaticElaborationTest, ShadowModInstResolvesToOuterModOwnInnerModViaBareReference) {
  const hldb::ModuleTypespec *const mt = getInstanceTypespec<hldb::ModuleTypespec>("shadow_mod_inst", getOuterMod());
  ASSERT_NE(mt, nullptr) << "a bare (unqualified) 'InnerMod' reference, written from within "
                             "OuterMod's own scope, must resolve by walking up the enclosing "
                             "scopes -- OuterMod's own InnerMod is declared directly in the "
                             "current scope, so no lookup outside it should ever be needed";
  const hldb::Module *const specialized = mt->getModule();
  ASSERT_NE(specialized, nullptr);
  EXPECT_NE(specialized, getNestedModule(getOuterMod(), "InnerMod"))
      << "an override-bearing use (#(15)) must specialize, not bind straight to the template";
  EXPECT_NE(specialized, getNestedModule(getOtherOuterMod(), "InnerMod"))
      << "must never be confused with OtherOuterMod's own, identically-named InnerMod -- the "
         "whole point of walking up from shadow_mod_inst's own scope (OuterMod) rather than "
         "falling back to some global, name-only lookup";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("15"));
}

TEST_F(StaticElaborationTest, ShadowIfaceInstResolvesToOuterModWithIfaceOwnInnerIfaceViaBareReference) {
  const hldb::InterfaceTypespec *const it =
      getInstanceTypespec<hldb::InterfaceTypespec>("shadow_iface_inst", getOuterModWithIface());
  ASSERT_NE(it, nullptr) << "a bare (unqualified) 'InnerIface' reference, written from within "
                             "OuterModWithIface's own scope, must resolve by walking up the "
                             "enclosing scopes";
  const hldb::Interface *const specialized = it->getInterface();
  ASSERT_NE(specialized, nullptr);
  EXPECT_NE(specialized, getNestedInterface(getOuterModWithIface(), "InnerIface"))
      << "an override-bearing use (#(17)) must specialize, not bind straight to the template";
  EXPECT_NE(specialized, getNestedInterface(getOtherOuterModWithIface(), "InnerIface"))
      << "must never be confused with OtherOuterModWithIface's own, identically-named InnerIface";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("17"));
}

// =============================================================================================
// Deeply nested class chain -- pkg_deep::Level1::Level2::Level3,
// a 4-segment chain with an independent override at every level. IEEE 1800-2023 places no cap
// on scope-resolution nesting depth (PkgA::ClassA::ClassB::ClassC is exactly as legal as
// PkgA::ClassA); confirms HLC resolves and specializes every segment of an arbitrarily long
// chain, not just a single Scope::Target pair.
// =============================================================================================

TEST_F(StaticElaborationTest, PkgDeepAndNestedChainClassesExist) {
  const hldb::Package *const pkgDeep = getPkgDeep();
  ASSERT_NE(pkgDeep, nullptr);
  const hldb::ClassDefn *const level1 = getPkgClass(pkgDeep, "Level1");
  ASSERT_NE(level1, nullptr);
  const hldb::ClassDefn *const level2 = getNestedClass(level1, "Level2");
  ASSERT_NE(level2, nullptr);
  const hldb::ClassDefn *const level3 = getNestedClass(level2, "Level3");
  EXPECT_NE(level3, nullptr);
}

TEST_F(StaticElaborationTest, DeepHandleResolvesToSpecializedLevel3) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("deep_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr) << "pkg_deep::Level1#(11)::Level2#(22)::Level3#(33) must fully "
                                      "resolve -- a 4-segment chain, not just a 2-segment one";
  const hldb::ClassDefn *const baseLevel1 = getPkgClass(getPkgDeep(), "Level1");
  const hldb::ClassDefn *const baseLevel2 = getNestedClass(baseLevel1, "Level2");
  const hldb::ClassDefn *const baseLevel3 = getNestedClass(baseLevel2, "Level3");
  ASSERT_NE(baseLevel3, nullptr);
  EXPECT_NE(specialized, baseLevel3) << "an override-bearing use must specialize, not bind "
                                         "straight to the template";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("33"));
}

TEST_F(StaticElaborationTest, DeepHandleLevel1SegmentIsSpecialized) {
  const hldb::RefTypespec *const rt = getHandleRefTypespec("deep_handle");
  ASSERT_NE(rt, nullptr);
  // path_elems (on rt's own chain-owning parent): [0] = pkg_deep scope, [1] = Level1,
  // [2] = Level2, [3] = Level3's own RefTypespec (== rt itself).
  const hldb::ClassDefn *const specializedLevel1 = getChainSegmentClassDefn(rt, 1);
  ASSERT_NE(specializedLevel1, nullptr);
  EXPECT_NE(specializedLevel1, getPkgClass(getPkgDeep(), "Level1"))
      << "an intermediate segment's own #(...) must specialize it too, not just the leaf";
  const hldb::Parameter *const p = getParameter(specializedLevel1, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("11"));
}

TEST_F(StaticElaborationTest, DeepHandleLevel2SegmentIsSpecializedWithinLevel1sSpecialization) {
  const hldb::RefTypespec *const rt = getHandleRefTypespec("deep_handle");
  ASSERT_NE(rt, nullptr);
  const hldb::ClassDefn *const specializedLevel1 = getChainSegmentClassDefn(rt, 1);
  ASSERT_NE(specializedLevel1, nullptr);
  const hldb::ClassDefn *const specializedLevel2 = getChainSegmentClassDefn(rt, 2);
  ASSERT_NE(specializedLevel2, nullptr);
  EXPECT_NE(specializedLevel2, getNestedClass(getPkgClass(getPkgDeep(), "Level1"), "Level2"))
      << "must not be Level1's BASE own Level2 -- Level2 must be looked up within Level1's own "
         "SPECIALIZATION, since a nested class's own fields could depend on the enclosing "
         "specialization's parameter";
  // getNestedClass(specializedLevel1, "Level2") is Level2 as found WITHIN Level1's own
  // specialization -- i.e. what Level2's own #(22) override still needed to be applied to.
  // specializedLevel2 is that same lookup's result AFTER its own override was applied, so the
  // two must be distinct objects too -- confirming Level2's own #(...) specialized it a second,
  // independent time on top of being resolved in the right (specialized) scope.
  EXPECT_NE(specializedLevel2, getNestedClass(specializedLevel1, "Level2"));
  const hldb::Parameter *const p = getParameter(specializedLevel2, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("22"));
}

TEST_F(StaticElaborationTest, DeepHandleLastSegmentSelfReferencesTheChainsOwnResult) {
  const hldb::RefTypespec *const rt = getHandleRefTypespec("deep_handle");
  ASSERT_NE(rt, nullptr);
  ASSERT_NE(rt->getActual<hldb::ClassTypespec>(), nullptr);
  const hldb::RefTypespec *const ownerRt = rt->getParent<hldb::RefTypespec>();
  ASSERT_NE(ownerRt, nullptr);
  ASSERT_NE(ownerRt->getPathElems(), nullptr);
  ASSERT_EQ(ownerRt->getPathElems()->size(), 4u);
  EXPECT_EQ(ownerRt->getPathElems()->back(), rt)
      << "for the LAST segment of a scoped chain, its own resolution IS the whole reference's "
         "resolution -- path_elems[N-1] must be the chain's own terminal RefTypespec itself";
}

// =============================================================================================
// Two independent references to overrides deep_handle above already uses -- regression
// coverage for duplicate specialization: each segment of a scoped chain must be resolved in
// the scope of the (specialized) segment before it. If a segment were resolved on its own, its
// override would be lost -- falling back to the UNSPECIALIZED base and corrupting every later
// segment's own search scope (e.g. Level2 specialized TWICE: once nested under Level1's own
// specialization, once wrongly under Level1's own base). The tests above only inspect the
// chain's own FINAL path_elems, so they would not see such an orphaned duplicate.
// level1_alias_handle/level2_alias_handle must dedupe to the EXACT SAME Level1/Level2
// specializations deep_handle's own chain already resolved to.
// =============================================================================================

TEST_F(StaticElaborationTest, Level1AliasHandleDedupesWithDeepHandlesOwnLevel1Segment) {
  const hldb::RefTypespec *const deepRt = getHandleRefTypespec("deep_handle");
  ASSERT_NE(deepRt, nullptr);
  const hldb::ClassDefn *const expectedLevel1 = getChainSegmentClassDefn(deepRt, 1);
  ASSERT_NE(expectedLevel1, nullptr);

  const hldb::ClassTypespec *const aliasCt = getHandleTypespec("level1_alias_handle");
  ASSERT_NE(aliasCt, nullptr);
  const hldb::ClassDefn *const actualLevel1 = aliasCt->getClassDefn();
  ASSERT_NE(actualLevel1, nullptr);
  EXPECT_EQ(actualLevel1, expectedLevel1)
      << "pkg_deep::Level1#(11), referenced independently here, must resolve to the exact same "
         "specialization deep_handle's own chain already produced for the identical override -- "
         "not a second, separately-minted (and, per the bug this guards against, potentially "
         "wrongly-based) clone";
}

TEST_F(StaticElaborationTest, Level2AliasHandleDedupesWithDeepHandlesOwnLevel2Segment) {
  const hldb::RefTypespec *const deepRt = getHandleRefTypespec("deep_handle");
  ASSERT_NE(deepRt, nullptr);
  const hldb::ClassDefn *const expectedLevel2 = getChainSegmentClassDefn(deepRt, 2);
  ASSERT_NE(expectedLevel2, nullptr);

  const hldb::RefTypespec *const aliasRt = getHandleRefTypespec("level2_alias_handle");
  ASSERT_NE(aliasRt, nullptr);
  const hldb::ClassTypespec *const aliasCt = aliasRt->getActual<hldb::ClassTypespec>();
  ASSERT_NE(aliasCt, nullptr);
  const hldb::ClassDefn *const actualLevel2 = aliasCt->getClassDefn();
  ASSERT_NE(actualLevel2, nullptr);
  EXPECT_EQ(actualLevel2, expectedLevel2)
      << "pkg_deep::Level1#(11)::Level2#(22), referenced independently here, must resolve its "
         "own Level2 segment to the exact same specialization deep_handle's own chain already "
         "produced -- not a duplicate parented under Level1's own BASE (not its "
         "specialization), which would mean the segment's own override was lost";

  // Also confirm the intermediate Level1 segment reached THIS WAY dedupes too -- same
  // specialization as both the direct level1_alias_handle test above and deep_handle's own
  // chain, regardless of which of the three independent references is resolved first.
  const hldb::ClassDefn *const aliasLevel1 = getChainSegmentClassDefn(aliasRt, 1);
  ASSERT_NE(aliasLevel1, nullptr);
  const hldb::ClassDefn *const deepLevel1 = getChainSegmentClassDefn(deepRt, 1);
  ASSERT_NE(deepLevel1, nullptr);
  EXPECT_EQ(aliasLevel1, deepLevel1);
}

// =============================================================================================
// Typedefs in scope resolution. Three shapes: a typedef reached through a package scope
// (`pkg_td::pkg_alias_t`), a bare/unscoped typedef referenced with no `::` at all
// (`bare_alias_t`), and a typedef aliasing ANOTHER bare, unscoped typedef in the same scope
// (`chained_alias_t` -- a typedef-of-typedef chain, whose two references must both resolve
// regardless of the order in which HLC visits them).
//
// A typedef never carries its own #(...) override -- the override lives on, and is resolved
// through, the typedef's own underlying type (Typedef::getAlias(), reached via the owning
// TypedefTypespec's own getTypedef()) instead, so a typedef'd handle's own RefTypespec::actual
// is the TypedefTypespec itself, never dereferenced automatically; every test below follows
// getTypedef()->getAlias() explicitly to reach the (possibly specialized) ClassTypespec
// underneath.
// =============================================================================================

TEST_F(StaticElaborationTest, PkgTdExistsWithItsOwnAlias) {
  const hldb::Package *const pkgTd = getPkgTd();
  ASSERT_NE(pkgTd, nullptr);
  ASSERT_NE(pkgTd->getTypespecs(), nullptr);
  bool found = false;
  for (const hldb::Typespec *const ts : *pkgTd->getTypespecs()) {
    if (const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(ts)) {
      if (td->getName() == std::string_view("pkg_alias_t")) found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(StaticElaborationTest, PkgAliasHandleResolvesThroughPackageScopedTypedef) {
  const hldb::TypedefTypespec *const td = getHandleTypedefTypespec("pkg_alias_handle");
  ASSERT_NE(td, nullptr) << "pkg_td::pkg_alias_t must bind to the TypedefTypespec itself, not "
                             "leave the reference as an unresolved UnsupportedTypespec";
  EXPECT_EQ(td->getName(), std::string_view("pkg_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(td);
  ASSERT_NE(underlying, nullptr) << "the typedef's own underlying type (param_cls#(40)) must "
                                     "also resolve";
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  EXPECT_NE(specialized, getParamCls()) << "param_cls#(40) must specialize, not bind to the base";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("40"));
}

TEST_F(StaticElaborationTest, BareAliasHandleResolvesThroughFlatCaseTypedefFallback) {
  const hldb::TypedefTypespec *const td = getHandleTypedefTypespec("bare_alias_handle");
  ASSERT_NE(td, nullptr) << "a bare, unqualified typedef reference (no '::' at all) must resolve "
                             "to the TypedefTypespec, not stay unresolved";
  EXPECT_EQ(td->getName(), std::string_view("bare_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(td);
  ASSERT_NE(underlying, nullptr);
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  EXPECT_NE(specialized, getParamCls());
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("41"));
}

TEST_F(StaticElaborationTest, ChainedAliasHandleResolvesThroughTypedefOfTypedef) {
  const hldb::TypedefTypespec *const chainedTd = getHandleTypedefTypespec("chained_alias_handle");
  ASSERT_NE(chainedTd, nullptr);
  EXPECT_EQ(chainedTd->getName(), std::string_view("chained_alias_t"));

  ASSERT_NE(chainedTd->getTypedef(), nullptr);
  ASSERT_NE(chainedTd->getTypedef()->getAlias(), nullptr);
  const hldb::TypedefTypespec *const bareTd = chainedTd->getTypedef()->getAlias()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(bareTd, nullptr) << "chained_alias_t's own underlying type is bare_alias_t itself -- "
                                 "another typedef, not a class -- and must resolve to it, not "
                                 "stay an unresolved UnsupportedTypespec";
  EXPECT_EQ(bareTd->getName(), std::string_view("bare_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(bareTd);
  ASSERT_NE(underlying, nullptr) << "bare_alias_t's OWN underlying type (param_cls#(41)) must "
                                     "also be fully resolved -- regardless of which of these two "
                                     "independent references is resolved first";
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("41"));
}

// =============================================================================================
// Typedef whose own underlying type is a specialized class with further-parameterized nested
// classes -- TdOuterAlias::TdMid#(81)::TdInner#(82). TdOuterAlias's own aliased class
// (TdOuter#(80)) must be resolved AND SPECIALIZED before TdMid can even be searched for -- it
// must be looked up within TdOuter#(80)'s own cloned nested-class list, not TdOuter's base
// one -- while TdOuter#(80) is itself an independent use site (the typedef's own alias). The
// chain must resolve whichever of the two HLC happens to visit first.
// =============================================================================================

TEST_F(StaticElaborationTest, TdOuterTdMidTdInnerClassesExist) {
  const hldb::ClassDefn *const tdOuter = getModuleLocalClass(getDut(), "TdOuter");
  ASSERT_NE(tdOuter, nullptr);
  const hldb::ClassDefn *const tdMid = getNestedClass(tdOuter, "TdMid");
  ASSERT_NE(tdMid, nullptr);
  const hldb::ClassDefn *const tdInner = getNestedClass(tdMid, "TdInner");
  EXPECT_NE(tdInner, nullptr);
}

TEST_F(StaticElaborationTest, TdOuterAliasIsATypedefResolvingToSpecializedTdOuter) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getTypespecs(), nullptr);
  const hldb::TypedefTypespec *aliasTd = nullptr;
  for (const hldb::Typespec *const ts : *dut->getTypespecs()) {
    if (const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(ts)) {
      if (td->getName() == std::string_view("TdOuterAlias")) aliasTd = td;
    }
  }
  ASSERT_NE(aliasTd, nullptr);

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(aliasTd);
  ASSERT_NE(underlying, nullptr) << "TdOuterAlias's own underlying type (TdOuter#(80)) must "
                                     "resolve -- and specialize";
  const hldb::ClassDefn *const specializedOuter = underlying->getClassDefn();
  ASSERT_NE(specializedOuter, nullptr);
  EXPECT_NE(specializedOuter, getModuleLocalClass(getDut(), "TdOuter"));
  const hldb::Parameter *const p = getParameter(specializedOuter, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("80"));
}

TEST_F(StaticElaborationTest, TdChainHandleResolvesThroughSpecializedTypedefLeadingSegment) {
  const hldb::RefTypespec *const rt = getHandleRefTypespec("td_chain_handle");
  ASSERT_NE(rt, nullptr) << "TdOuterAlias::TdMid#(81)::TdInner#(82) must fully resolve -- a "
                             "its leading typedef segment must be resolved and specialized "
                             "first (see the section comment above)";
  const hldb::ClassTypespec *const ct = rt->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  const hldb::RefTypespec *const ownerRt = rt->getParent<hldb::RefTypespec>();
  ASSERT_NE(ownerRt, nullptr);
  ASSERT_NE(ownerRt->getPathElems(), nullptr);
  ASSERT_EQ(ownerRt->getPathElems()->size(), 3u);

  const hldb::ClassDefn *const specializedInner = ct->getClassDefn();
  ASSERT_NE(specializedInner, nullptr);
  const hldb::Parameter *const p = getParameter(specializedInner, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("82"));
}

TEST_F(StaticElaborationTest, TdChainHandleMidSegmentIsSpecializedWithinTdOuterAliasSpecialization) {
  const hldb::RefTypespec *const rt = getHandleRefTypespec("td_chain_handle");
  ASSERT_NE(rt, nullptr);
  // path_elems (on rt's own chain-owning parent): [0] = TdOuterAlias (a typedef, not a class --
  // getChainSegmentClassDefn doesn't apply to it, see
  // TdOuterAliasIsATypedefResolvingToSpecializedTdOuter instead), [1] = TdMid, [2] = TdInner's
  // own RefTypespec (== rt itself).
  const hldb::ClassDefn *const specializedMid = getChainSegmentClassDefn(rt, 1);
  ASSERT_NE(specializedMid, nullptr);

  const hldb::ClassDefn *const baseTdOuter = getModuleLocalClass(getDut(), "TdOuter");
  const hldb::ClassDefn *const baseTdMid = getNestedClass(baseTdOuter, "TdMid");
  ASSERT_NE(baseTdMid, nullptr);
  EXPECT_NE(specializedMid, baseTdMid) << "TdMid must be looked up (and re-specialized) within "
                                          "TdOuterAlias's own specialization's nested class "
                                          "list, not TdOuter's base one";

  const hldb::Parameter *const p = getParameter(specializedMid, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("81"));
}

// =============================================================================================
// Lexical (walk-up) scoping for a bare, unqualified typedef reference. A `::`-qualified segment is
// bound EXACTLY to its resolved scope (see the tests above), but an unqualified name follows
// ordinary SV name resolution: visible in its own immediate scope, then every scope enclosing
// that, all the way out to file/compilation-unit ("$unit") scope. Two shapes: a typedef
// declared on a class, referenced bare from a NESTED class scope (TdMid seeing TdOuter's own
// outer_scope_alias_t), and a typedef declared outside any module/package/class at all,
// referenced bare from deep inside dut (walking all the way out to the compilation unit, $unit,
// modeled as the file's hldb::Unit).
// =============================================================================================

TEST_F(StaticElaborationTest, ScopeWalkMemberResolvesTypedefFromEnclosingClassScope) {
  // Followed through what dut actually uses (TdOuterAlias::TdMid#(81)::...): TdOuter#(80) and, in
  // it, TdMid#(81). The written TdOuter is never used itself, so its body is bound at the definition
  // level only and its param_cls #(95) is not specialized from there (static elaboration specializes
  // only what is reachable).
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::ClassDefn *const tdOuter = getOnlySpecialization(dut->getClassDefns(), "TdOuter");
  ASSERT_NE(tdOuter, nullptr) << "TdOuterAlias names TdOuter #(80)";
  ASSERT_NE(getParameter(tdOuter, "W"), nullptr);
  ASSERT_NE(getParameter(tdOuter, "W")->getExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(getParameter(tdOuter, "W")->getExpr<hldb::Constant>()->getDecompile(), std::string_view("80"));
  const hldb::ClassDefn *const tdMid = getOnlySpecialization(tdOuter->getClassDefns(), "TdMid");
  ASSERT_NE(tdMid, nullptr) << "TdOuterAlias::TdMid#(81) is specialized within TdOuter #(80)";
  ASSERT_NE(getParameter(tdMid, "W"), nullptr);
  ASSERT_NE(getParameter(tdMid, "W")->getExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(getParameter(tdMid, "W")->getExpr<hldb::Constant>()->getDecompile(), std::string_view("81"));

  const hldb::TypedefTypespec *const td = getMemberTypedefTypespec(tdMid, "scope_walk_member");
  ASSERT_NE(td, nullptr) << "outer_scope_alias_t is declared on TdOuter (TdMid's own enclosing "
                             "scope), not on TdMid itself -- resolving a bare reference to it "
                             "from within TdMid requires walking UP the enclosing scopes, not "
                             "just checking TdMid's own immediate scope";
  EXPECT_EQ(td->getName(), std::string_view("outer_scope_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(td);
  ASSERT_NE(underlying, nullptr);
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("95"));
}

TEST_F(StaticElaborationTest, UnitScopeHandleResolvesTypedefWalkedAllTheWayToDesign) {
  const hldb::TypedefTypespec *const td = getHandleTypedefTypespec("unit_scope_handle");
  ASSERT_NE(td, nullptr) << "unit_scope_alias_t is declared outside any module/package/class -- "
                             "in the compilation unit ($unit) scope -- so "
                             "resolving a bare reference to it from inside dut requires "
                             "walking the enclosing scopes all the way out to the compilation "
                             "unit";
  EXPECT_EQ(td->getName(), std::string_view("unit_scope_alias_t"));

  const hldb::ClassTypespec *const underlying = getTypedefAliasClassTypespec(td);
  ASSERT_NE(underlying, nullptr);
  const hldb::ClassDefn *const specialized = underlying->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("96"));
}

// =============================================================================================
// Package imports -- IEEE 1800-2023 Sec 26.5. pkg_import_a/pkg_import_b each declare
// a class of the SAME name ("ImportShadowCls"), so a bare reference from dut's own scope is
// genuinely unresolvable without an import (neither candidate is visible via ordinary lexical
// scoping -- dut is not lexically inside either package); the explicit import disambiguates
// which one is meant. pkg_import_c's own typedef is reachable ONLY via a wildcard import.
// =============================================================================================

TEST_F(StaticElaborationTest, ImportExplicitHandleResolvesViaExplicitImport) {
  // Reached through a typedef (import_explicit_alias_t), not a direct class-typed variable
  // declaration -- see dut.sv's own comment on `import_explicit_alias_t` for why: a direct
  // `ImportShadowCls #(31) import_explicit_handle;` is genuinely ambiguous at the GRAMMAR level
  // with a net declaration's own delay-control syntax, and HLC does not yet disambiguate it for
  // a class declared in a package -- an unrelated limitation, not something this test exercises.
  const hldb::TypedefTypespec *const td = getHandleTypedefTypespec("import_explicit_handle");
  ASSERT_NE(td, nullptr) << "'import pkg_import_a::ImportShadowCls;' must disambiguate the "
                             "otherwise-ambiguous bare 'ImportShadowCls' reference (IEEE "
                             "1800-2023 Sec 26.3)";
  const hldb::ClassTypespec *const ct = getTypedefAliasClassTypespec(td);
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const specialized = ct->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  EXPECT_NE(specialized, getPkgClass(getPkgImportA(), "ImportShadowCls"))
      << "an override-bearing use (#(31)) must specialize, not bind straight to the template";
  EXPECT_NE(specialized, getPkgClass(getPkgImportB(), "ImportShadowCls"))
      << "must never resolve to pkg_import_b's own, identically-named, NOT-imported class";
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("31"));
}

TEST_F(StaticElaborationTest, ImportWildcardAliasHandleResolvesViaWildcardImport) {
  const hldb::TypedefTypespec *const td = getHandleTypedefTypespec("import_wildcard_alias_handle");
  ASSERT_NE(td, nullptr) << "'import pkg_import_c::*;' must make the bare "
                             "'import_wildcard_alias_t' reference resolvable through the "
                             "wildcard import -- pkg_import_c is otherwise completely unrelated to dut's "
                             "own lexical scope";
  const hldb::ClassTypespec *const aliasCt = getTypedefAliasClassTypespec(td);
  ASSERT_NE(aliasCt, nullptr);
  const hldb::ClassDefn *const specialized = aliasCt->getClassDefn();
  ASSERT_NE(specialized, nullptr);
  const hldb::Parameter *const p = getParameter(specialized, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("42"));
}

// =============================================================================================
// Module within module -- a parameterized module instantiated from a non-top-level module.
// Confirms specialization (and its cross-scope dedup) is independent of hierarchy depth.
// =============================================================================================

TEST_F(StaticElaborationTest, MidLevelModuleExists) { EXPECT_NE(getMidLevel(), nullptr); }

TEST_F(StaticElaborationTest, MidInstResolvesToBaseMidLevel) {
  // 'mid_level mid_inst ();' supplies no #(...) -- mid_level itself is never specialized.
  const hldb::ModuleTypespec *const ts = getMidInstTypespec();
  ASSERT_NE(ts, nullptr);
  EXPECT_EQ(ts->getModule(), getMidLevel());
}

TEST_F(StaticElaborationTest, MidNestedSameAsWideDedupesWithDutInstWide) {
  // mid_level's own inst_nested_same_as_wide uses the SAME override value (W=16) as dut's
  // inst_wide -- despite living in a completely different enclosing scope, both must resolve
  // to the exact same specialization object.
  const hldb::ModuleTypespec *const tsFromDut = getInstWideTypespec();
  const hldb::ModuleTypespec *const tsFromMid = getMidNestedSameAsWideTypespec();
  ASSERT_NE(tsFromDut, nullptr);
  ASSERT_NE(tsFromMid, nullptr);
  const hldb::Module *const modFromDut = tsFromDut->getModule();
  const hldb::Module *const modFromMid = tsFromMid->getModule();
  ASSERT_NE(modFromDut, nullptr);
  ASSERT_NE(modFromMid, nullptr);
  EXPECT_EQ(modFromMid, modFromDut) << "the same override value (W=16), requested from two "
                                        "different enclosing scopes, must dedupe to one "
                                        "specialization";
}

TEST_F(StaticElaborationTest, MidNestedDistinctIsItsOwnSpecialization) {
  const hldb::ModuleTypespec *const tsDistinct = getMidNestedDistinctTypespec();
  const hldb::ModuleTypespec *const tsSameAsWide = getMidNestedSameAsWideTypespec();
  ASSERT_NE(tsDistinct, nullptr);
  ASSERT_NE(tsSameAsWide, nullptr);
  const hldb::Module *const modDistinct = tsDistinct->getModule();
  const hldb::Module *const modSameAsWide = tsSameAsWide->getModule();
  ASSERT_NE(modDistinct, nullptr);
  ASSERT_NE(modSameAsWide, nullptr);
  EXPECT_NE(modDistinct, modSameAsWide);
  EXPECT_NE(modDistinct, getParamMod());

  const hldb::Parameter *const p = getParameter(modDistinct, "W");
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const expr = p->getExpr<hldb::Constant>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getDecompile(), std::string_view("24"));
}

// =============================================================================================
// extends whose override depends on the deriving class's own parameter -- `class SuperDerived
// #(type T = int) extends SuperBase #(T);`. IEEE 1800-2023 Sec 8.15: `super` is the class the
// current class extends; Sec 8.25: each specialization is its own class type. So SuperDerived
// #(bit) extends SuperBase #(bit) and its `super` is SuperBase #(bit) -- never SuperDerived itself
// (a specialization does not extend the definition it was built from), and never SuperBase
// specialized with SuperDerived's DEFAULT for T. The default SuperDerived extends SuperBase #(int),
// SuperBase's own default specialization, which is SuperBase itself.
// =============================================================================================

TEST_F(StaticElaborationTest, SuperBaseAndSuperDerivedExist) {
  EXPECT_NE(getSuperBase(), nullptr);
  EXPECT_NE(getSuperDerived(), nullptr);
}

TEST_F(StaticElaborationTest, SuperDefaultHandleResolvesToBaseSuperDerived) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("super_default_handle");
  ASSERT_NE(ct, nullptr);
  ASSERT_NE(getSuperDerived(), nullptr);
  EXPECT_EQ(ct->getClassDefn(), getSuperDerived());
}

TEST_F(StaticElaborationTest, SuperBitHandleResolvesToSuperDerivedSpecializedWithBit) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("super_bit_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const derivedBit = ct->getClassDefn();
  ASSERT_NE(derivedBit, nullptr);
  ASSERT_NE(getSuperDerived(), nullptr);
  EXPECT_NE(derivedBit, getSuperDerived());
  EXPECT_TRUE(derivedBit->getIsSpecialization());
  EXPECT_EQ(derivedBit->getSpecializedFrom(), getSuperDerived());
  EXPECT_EQ(derivedBit->getDefName(), std::string_view("SuperDerived"));

  const hldb::Typespec *const t = getTypeParameterValue(derivedBit, "T");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getAnyType(), hldb::AnyType::BitTypespec);
}

TEST_F(StaticElaborationTest, BaseSuperDerivedExtendsSuperBaseItself) {
  ASSERT_NE(getSuperDerived(), nullptr);
  ASSERT_NE(getSuperBase(), nullptr);
  // SuperBase #(int), with int SuperBase's own default for T: SuperBase's default specialization.
  EXPECT_EQ(getExtendedClass(getSuperDerived()), getSuperBase());
}

TEST_F(StaticElaborationTest, SpecializedSuperDerivedExtendsSuperBaseSpecializedWithBit) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("super_bit_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const derivedBit = ct->getClassDefn();
  ASSERT_NE(derivedBit, nullptr);

  const hldb::ClassDefn *const baseBit = getExtendedClass(derivedBit);
  ASSERT_NE(baseBit, nullptr);
  // Not the definition the specialization was built from: a specialization never extends it.
  EXPECT_NE(baseBit, getSuperDerived());
  EXPECT_NE(baseBit, derivedBit);
  // Not SuperBase specialized with SuperDerived's default (int), which is SuperBase itself.
  EXPECT_NE(baseBit, getSuperBase());
  EXPECT_TRUE(baseBit->getIsSpecialization());
  EXPECT_EQ(baseBit->getSpecializedFrom(), getSuperBase());

  const hldb::Typespec *const t = getTypeParameterValue(baseBit, "T");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getAnyType(), hldb::AnyType::BitTypespec);
}

TEST_F(StaticElaborationTest, SuperInBaseSuperDerivedBindsToSuperBase) {
  const hldb::RefObj *const superRef = getSuperRef(getSuperDerived());
  ASSERT_NE(superRef, nullptr);
  ASSERT_NE(superRef->getActual(), nullptr);
  EXPECT_EQ(superRef->getActual(), getSuperBase());
}

TEST_F(StaticElaborationTest, SuperInSpecializedSuperDerivedBindsToSuperBaseSpecializedWithBit) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("super_bit_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const derivedBit = ct->getClassDefn();
  ASSERT_NE(derivedBit, nullptr);
  const hldb::ClassDefn *const baseBit = getExtendedClass(derivedBit);
  ASSERT_NE(baseBit, nullptr);

  // The specialization owns its own copy of derived_fn(), and so of its `super` reference.
  const hldb::RefObj *const superRef = getSuperRef(derivedBit);
  ASSERT_NE(superRef, nullptr);
  EXPECT_NE(superRef, getSuperRef(getSuperDerived()));
  ASSERT_NE(superRef->getActual(), nullptr);
  EXPECT_EQ(superRef->getActual(), baseBit);
}

TEST_F(StaticElaborationTest, DerivedClassesFollowWhatEachClassExtends) {
  const hldb::ClassTypespec *const ct = getHandleTypespec("super_bit_handle");
  ASSERT_NE(ct, nullptr);
  const hldb::ClassDefn *const derivedBit = ct->getClassDefn();
  ASSERT_NE(derivedBit, nullptr);
  const hldb::ClassDefn *const baseBit = getExtendedClass(derivedBit);
  ASSERT_NE(baseBit, nullptr);
  ASSERT_NE(getSuperBase(), nullptr);

  // SuperBase is extended by SuperDerived (default); SuperBase #(bit) by SuperDerived #(bit).
  EXPECT_TRUE(containsClass(getSuperBase()->getDerivedClasses(), getSuperDerived()));
  EXPECT_FALSE(containsClass(getSuperBase()->getDerivedClasses(), derivedBit));
  EXPECT_TRUE(containsClass(baseBit->getDerivedClasses(), derivedBit));
  EXPECT_FALSE(containsClass(baseBit->getDerivedClasses(), getSuperDerived()));
  // A specialization is not a subclass of the definition it was built from.
  EXPECT_FALSE(containsClass(getSuperDerived()->getDerivedClasses(), derivedBit));
}

// =============================================================================================
// Canonical specialization keys. A specialization is identified by its parameter values (IEEE
// 1800-2023 Sec 8.25), not by how a use site spells them: defaults omitted or written, ordered or
// named (in any order, Sec 23.10.2), a type written directly or through any typedef of it (Sec
// 6.18). Inside a class, the class with its own parameters (this_type) is the current
// specialization; a dependent name P::item_t names item_t in the class P stands for.
// =============================================================================================

TEST_F(StaticElaborationTest, KeyClassesExist) {
  EXPECT_NE(getUnitClass("KeyCls"), nullptr);
  EXPECT_NE(getUnitClass("KeySelf"), nullptr);
  EXPECT_NE(getUnitClass("KeyDepHost"), nullptr);
  EXPECT_NE(getUnitClass("KeyItemProvider"), nullptr);
  EXPECT_NE(getUnitClass("KeyItemProviderWide"), nullptr);
}

TEST_F(StaticElaborationTest, KeyEverySpellingOfTheDefaultsNamesTheBaseClass) {
  const hldb::ClassDefn *const keyCls = getUnitClass("KeyCls");
  ASSERT_NE(keyCls, nullptr);
  for (std::string_view handle : {"key_bare_handle", "key_ordered_default_handle", "key_named_default_handle",
                                  "key_named_w_default_handle"}) {
    EXPECT_EQ(getHandleClass(handle), keyCls) << handle;
  }
}

TEST_F(StaticElaborationTest, KeyEverySpellingOfByteNamesOneSpecialization) {
  const hldb::ClassDefn *const keyCls = getUnitClass("KeyCls");
  ASSERT_NE(keyCls, nullptr);
  const hldb::ClassDefn *const keyByte = getHandleClass("key_byte_handle");
  ASSERT_NE(keyByte, nullptr);
  EXPECT_NE(keyByte, keyCls);
  EXPECT_TRUE(keyByte->getIsSpecialization());
  EXPECT_EQ(keyByte->getSpecializedFrom(), keyCls);

  // Named vs. ordered, the default W spelled out, and two different typedefs of byte.
  for (std::string_view handle :
       {"key_named_byte_handle", "key_byte_full_handle", "key_typedef_a_handle", "key_typedef_b_handle"}) {
    EXPECT_EQ(getHandleClass(handle), keyByte) << handle;
  }
}

TEST_F(StaticElaborationTest, KeyByteSpecializationHasTheValuesItWasRequestedWith) {
  const hldb::ClassDefn *const keyByte = getHandleClass("key_byte_handle");
  ASSERT_NE(keyByte, nullptr);

  const hldb::Typespec *const t = followTypedefs(getTypeParameterValue(keyByte, "T"));
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getAnyType(), hldb::AnyType::ByteTypespec);

  const hldb::Parameter *const w = getParameter(keyByte, "W");
  ASSERT_NE(w, nullptr);
  const hldb::Constant *const wValue = w->getExpr<hldb::Constant>();
  ASSERT_NE(wValue, nullptr);
  EXPECT_EQ(wValue->getDecompile(), std::string_view("4"));
}

TEST_F(StaticElaborationTest, KeyDifferentValuesNameDifferentSpecializations) {
  const hldb::ClassDefn *const keyCls = getUnitClass("KeyCls");
  const hldb::ClassDefn *const keyByte = getHandleClass("key_byte_handle");
  const hldb::ClassDefn *const keyW8 = getHandleClass("key_w8_handle");
  ASSERT_NE(keyCls, nullptr);
  ASSERT_NE(keyByte, nullptr);
  ASSERT_NE(keyW8, nullptr);
  EXPECT_NE(keyW8, keyCls);
  EXPECT_NE(keyW8, keyByte);
  EXPECT_TRUE(keyW8->getIsSpecialization());
  EXPECT_EQ(keyW8->getSpecializedFrom(), keyCls);

  const hldb::Parameter *const w = getParameter(keyW8, "W");
  ASSERT_NE(w, nullptr);
  const hldb::Constant *const wValue = w->getExpr<hldb::Constant>();
  ASSERT_NE(wValue, nullptr);
  EXPECT_EQ(wValue->getDecompile(), std::string_view("8"));
}

TEST_F(StaticElaborationTest, KeyNamedOverridesMatchFormalsInAnyOrder) {
  const hldb::ClassDefn *const keyW8 = getHandleClass("key_w8_handle");
  ASSERT_NE(keyW8, nullptr);
  EXPECT_EQ(getHandleClass("key_w8_ordered_handle"), keyW8);
  EXPECT_EQ(getHandleClass("key_w8_reversed_handle"), keyW8);
}

TEST_F(StaticElaborationTest, KeyThisTypeInTheBaseNamesTheBase) {
  const hldb::ClassDefn *const keySelf = getUnitClass("KeySelf");
  ASSERT_NE(keySelf, nullptr);
  EXPECT_EQ(getHandleClass("key_self_default_handle"), keySelf);

  const hldb::TypedefTypespec *const thisTypedef = getMemberTypedefTypespec(keySelf, "self_handle");
  const hldb::ClassTypespec *const thisType = getTypedefAliasClassTypespec(thisTypedef);
  ASSERT_NE(thisType, nullptr);
  EXPECT_EQ(thisType->getClassDefn(), keySelf);
}

TEST_F(StaticElaborationTest, KeyThisTypeInASpecializationNamesThatSpecialization) {
  const hldb::ClassDefn *const keySelf = getUnitClass("KeySelf");
  const hldb::ClassDefn *const keySelfByte = getHandleClass("key_self_byte_handle");
  ASSERT_NE(keySelf, nullptr);
  ASSERT_NE(keySelfByte, nullptr);
  EXPECT_NE(keySelfByte, keySelf);
  EXPECT_TRUE(keySelfByte->getIsSpecialization());
  EXPECT_EQ(keySelfByte->getSpecializedFrom(), keySelf);

  // The specialization's own this_type typedef (each specialization owns its members) names it.
  const hldb::TypedefTypespec *const thisTypedef = getMemberTypedefTypespec(keySelfByte, "self_handle");
  ASSERT_NE(thisTypedef, nullptr);
  EXPECT_NE(thisTypedef, getMemberTypedefTypespec(keySelf, "self_handle"));
  const hldb::ClassTypespec *const thisType = getTypedefAliasClassTypespec(thisTypedef);
  ASSERT_NE(thisType, nullptr);
  EXPECT_EQ(thisType->getClassDefn(), keySelfByte);
}

TEST_F(StaticElaborationTest, KeyDependentNameWithTheDefaultProviderNamesKeyClsOfByte) {
  const hldb::ClassDefn *const keyDepHost = getUnitClass("KeyDepHost");
  ASSERT_NE(keyDepHost, nullptr);
  EXPECT_EQ(getHandleClass("key_dep_default_handle"), keyDepHost);

  // P::item_t with P = KeyItemProvider is byte: the same specialization as KeyCls #(byte).
  const hldb::ClassTypespec *const depType = getMemberClassTypespec(keyDepHost, "dep_handle");
  ASSERT_NE(depType, nullptr);
  ASSERT_NE(getHandleClass("key_byte_handle"), nullptr);
  EXPECT_EQ(depType->getClassDefn(), getHandleClass("key_byte_handle"));
}

TEST_F(StaticElaborationTest, KeyDependentNameWithAnOverriddenProviderNamesKeyClsOfShortint) {
  const hldb::ClassDefn *const keyCls = getUnitClass("KeyCls");
  const hldb::ClassDefn *const keyDepHost = getUnitClass("KeyDepHost");
  const hldb::ClassDefn *const keyDepWide = getHandleClass("key_dep_wide_handle");
  ASSERT_NE(keyCls, nullptr);
  ASSERT_NE(keyDepHost, nullptr);
  ASSERT_NE(keyDepWide, nullptr);
  EXPECT_NE(keyDepWide, keyDepHost);
  EXPECT_TRUE(keyDepWide->getIsSpecialization());
  EXPECT_EQ(keyDepWide->getSpecializedFrom(), keyDepHost);

  // In the specialization, P is KeyItemProviderWide, so P::item_t is shortint.
  const hldb::ClassTypespec *const depType = getMemberClassTypespec(keyDepWide, "dep_handle");
  ASSERT_NE(depType, nullptr);
  const hldb::ClassDefn *const keyShortint = depType->getClassDefn();
  ASSERT_NE(keyShortint, nullptr);
  EXPECT_TRUE(keyShortint->getIsSpecialization());
  EXPECT_EQ(keyShortint->getSpecializedFrom(), keyCls);
  EXPECT_NE(keyShortint, getHandleClass("key_byte_handle"));

  const hldb::Typespec *const t = followTypedefs(getTypeParameterValue(keyShortint, "T"));
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getAnyType(), hldb::AnyType::ShortIntTypespec);
}

// =============================================================================================
// Whole-graph object counts. A coarser, complementary check to every dedup/identity assertion
// above: those each confirm a handful of SPECIFIC use sites resolve to the right (shared, or
// correctly distinct) object, but none of them notices a stray EXTRA object sitting elsewhere
// in the database, unreachable from any of the specific paths they happen to check -- e.g. an
// orphaned, wrongly-based duplicate ClassDefn (a second "Level2" specialization parented under
// Level1's own BASE instead of its specialization), invisible to every test that only walks
// the chain's own path_elems. An unexpected total count here catches that directly, with no
// need to already know which specific object to check.
//
// Every number below was derived by hand from dut.sv's own declarations, walking through
// EVERY override-bearing use site and the specialization it requests (documented inline per
// definition kind) -- NOT read back from a prior run's own dump (which would just re-lock in
// whatever the tool happened to produce, bug included). If a count below ever needs to change, re-derive it by
// hand against dut.sv's own current declarations first -- never adjust it to match a dump
// without first confirming the ADDITIONAL declaration/use-site that justifies the new number.
// =============================================================================================

// ---- Module: 3 base (param_mod, mid_level, dut) + 6 base (OuterMod, OuterMod::InnerMod,
// OtherOuterMod, OtherOuterMod::InnerMod, OuterModWithIface, OtherOuterModWithIface -- the
// nested-module-declaration shadowing regression) + 3 specializations (param_mod#16, shared by
// inst_wide and mid_level's own inst_nested_same_as_wide; param_mod#24, mid_level's own
// inst_nested_distinct; OuterMod::InnerMod#15, shadow_mod_inst's own bare, shadow-disambiguated
// reference) = 12 total. param_mod and InnerMod both have no nested module declarations of
// their own, so specializing either clones nothing further. mid_level/dut/OuterMod/
// OtherOuterMod/OuterModWithIface/OtherOuterModWithIface carry no parameters, so none of them is
// ever itself specialized (module INSTANTIATION, unlike a nested class DECLARATION, never mints
// a new Module ClassDefn-equivalent of its own).

TEST_F(StaticElaborationTest, TotalModuleCountInGraph) { EXPECT_EQ(countAll<hldb::Module>(), 12u); }

TEST_F(StaticElaborationTest, TotalModuleSpecializationCount) {
  EXPECT_EQ(countSpecializations<hldb::Module>(), 3u);
}

// ---- Interface: 1 base (param_if) + 2 base (OuterModWithIface::InnerIface,
// OtherOuterModWithIface::InnerIface -- the nested-interface-declaration shadowing regression) +
// 2 specializations (param_if#8, if_wide; InnerIface#17, shadow_iface_inst's own bare,
// shadow-disambiguated reference) = 5 total. InnerIface has no nested interface declarations of
// its own, so specializing it clones nothing further.

TEST_F(StaticElaborationTest, TotalInterfaceCountInGraph) { EXPECT_EQ(countAll<hldb::Interface>(), 5u); }

TEST_F(StaticElaborationTest, TotalInterfaceSpecializationCount) {
  EXPECT_EQ(countSpecializations<hldb::Interface>(), 2u);
}

// ---- Program: 1 base (param_prog) + 1 specialization (param_prog#4, prog_wide) = 2 total.

TEST_F(StaticElaborationTest, TotalProgramCountInGraph) { EXPECT_EQ(countAll<hldb::Program>(), 2u); }

TEST_F(StaticElaborationTest, TotalProgramSpecializationCount) {
  EXPECT_EQ(countSpecializations<hldb::Program>(), 1u);
}

// ---- Package: pkg_a, pkg_b, pkg_deep, pkg_td, pkg_import_a, pkg_import_b, pkg_import_c (the
// import-resolution coverage) = 7 total. Packages have no parameter
// mechanism at all in this model -- never specialized, so no companion specialization-count
// test.

TEST_F(StaticElaborationTest, TotalPackageCountInGraph) { EXPECT_EQ(countAll<hldb::Package>(), 7u); }

// ---- ClassDefn: the complex one -- 22 base declarations, 25 specializations (one per distinct
// (baseDef, parameter values) pair actually requested anywhere in the file), and 6 further,
// UNSPECIALIZED clones that come along for free as a byproduct of cloning a WHOLE subtree
// whenever ITS OWN enclosing class gets specialized (a clone is only counted as one of the 25
// "specializations" above if it is ITSELF the direct target of a use site's override -- an
// intermediate clone reached only by walking INTO an already-specialized parent is not). 22 + 25 + 6 = 53 total.
//
// Base declarations (22): param_cls; pkg_a::PkgWidget; pkg_b::PkgWidget; OuterCls;
// OuterCls::InnerCls; OtherOuterCls; OtherOuterCls::InnerCls; pkg_deep::Level1;
// pkg_deep::Level1::Level2; pkg_deep::Level1::Level2::Level3; TdOuter; TdOuter::TdMid;
// TdOuter::TdMid::TdInner; pkg_import_a::ImportShadowCls; pkg_import_b::ImportShadowCls (the
// import shadowing coverage -- pkg_import_b's own copy is declared but never referenced
// with an override, so it never itself gets specialized); SuperBase; SuperDerived; KeyCls;
// KeySelf; KeyItemProvider; KeyItemProviderWide; KeyDepHost.
//
// Specializations (25), grouped by base:
//   param_cls -- 6 distinct override values requested across the file: #(9) (cls_wide_handle),
//     #(40) (pkg_alias_t's own underlying type), #(41) (bare_alias_t's own underlying type --
//     chained_alias_t aliases bare_alias_t itself, not param_cls directly, so it dedupes to
//     this SAME specialization rather than adding a 7th), #(95) (outer_scope_alias_t's own
//     underlying type), #(96) (unit_scope_alias_t's own underlying type), #(42)
//     (import_wildcard_alias_t's own underlying type -- the wildcard-import case).
//     None of these clone anything further -- param_cls has no nested classes.
//   pkg_a::PkgWidget -- #(10) (pkg_a_handle). pkg_b::PkgWidget -- #(20) (pkg_b_handle). Two
//     DIFFERENT bases (different packages), so two separate specializations despite the shared
//     name and shared override VALUE -- confirmed already by PkgAHandle.../PkgBHandle... above.
//   OuterCls::InnerCls -- #(12) (outer_inner_handle) AND #(15) (shadow_test_handle, a BARE
//     reference to InnerCls written from WITHIN OuterCls's own scope -- regression coverage for
//     the class-name local-shadowing fix; a different override value from outer_inner_handle's
//     #(12), so it's its own, separate specialization despite sharing the same base).
//     OtherOuterCls::InnerCls -- #(24) (other_outer_inner_handle). Same base-disambiguation shape
//     as PkgWidget above, now with OuterCls::InnerCls itself contributing two specializations.
//   pkg_deep's own 3-level chain -- Level1#(11) (shared by deep_handle, level1_alias_handle,
//     level2_alias_handle); Level2#(22), specializing the CLONE of Level2 that lives inside
//     Level1#(11)'s own clone (shared by deep_handle, level2_alias_handle); Level3#(33),
//     specializing the clone of Level3 that lives inside THAT Level2#(22) specialization
//     (deep_handle only) = 3 specializations.
//   TdOuter's own 3-level chain, structurally identical to pkg_deep's -- TdOuter#(80) (via
//     TdOuterAlias's own underlying type); TdMid#(81) (td_chain_handle); TdInner#(82)
//     (td_chain_handle) = 3 specializations.
//   pkg_import_a::ImportShadowCls -- #(31) (import_explicit_handle, resolved via the explicit
//     import) = 1 specialization. pkg_import_b's own, identically-named
//     ImportShadowCls contributes zero (never referenced with an override anywhere).
//   SuperDerived -- #(bit) (super_bit_handle) = 1 specialization. SuperBase -- #(bit), from the
//     extends clause of THAT specialization (SuperBase #(T) with T=bit there) = 1 specialization.
//     The default SuperDerived's own extends clause is SuperBase #(int), int being SuperBase's own
//     default for T: SuperBase's default specialization, i.e. SuperBase itself, adding nothing.
//   KeyCls -- 3: T=byte (key_byte_handle, and every other spelling of it: key_named_byte_handle,
//     key_byte_full_handle, both typedef handles, and KeyDepHost's own dep_handle, P::item_t being
//     byte there); W=8 (key_w8_handle, key_w8_ordered_handle, key_w8_reversed_handle); T=shortint
//     (the dep_handle of KeyDepHost #(KeyItemProviderWide)). The four spellings of the defaults add
//     nothing: they name KeyCls itself.
//   KeySelf -- #(byte) (key_self_byte_handle) = 1; its own this_type names it, adding nothing, and
//     the base's this_type names the base.
//   KeyDepHost -- #(KeyItemProviderWide) (key_dep_wide_handle) = 1.
//   6 + 1 + 1 + 2 + 1 + 3 + 3 + 1 + 1 + 1 + 3 + 1 + 1 = 25.
//
// Unspecialized transitive clones (6): Level1#(11)'s own clone step clones Level2 AND (nested
// within that) Level3 = 2; Level2#(22)'s own clone step clones Level3 (a SECOND, independent
// Level3 clone, nested inside the Level2 specialization rather than inside the bare Level1
// clone) = 1; Level3#(33) has no nested classes to clone = 0. Same shape for TdOuter's own
// chain: TdOuter#(80) clones TdMid and (nested) TdInner = 2; TdMid#(81) clones TdInner again
// (nested inside the TdMid specialization) = 1; TdInner#(82) clones nothing = 0. OuterCls::InnerCls,
// pkg_import_a::ImportShadowCls, SuperBase, SuperDerived, KeyCls, KeySelf and KeyDepHost have no
// nested classes of their own, so none of their specializations adds any further transitive clone.
// 2 + 1 + 0 + 2 + 1 + 0 = 6.

TEST_F(StaticElaborationTest, TotalClassDefnCountInGraph) { EXPECT_EQ(countAll<hldb::ClassDefn>(), 53u); }

TEST_F(StaticElaborationTest, TotalClassDefnSpecializationCount) {
  EXPECT_EQ(countSpecializations<hldb::ClassDefn>(), 25u);
}

// ---- TypedefTypespec: pkg_alias_t, unit_scope_alias_t, bare_alias_t, chained_alias_t,
// outer_scope_alias_t, TdOuterAlias, import_wildcard_alias_t (the wildcard-import case),
// import_explicit_alias_t (the explicit-import case -- wrapped in a
// typedef specifically to sidestep a real, separate grammar-level ambiguity between a
// class-typed declaration and a net declaration's own delay control; see its own comment in
// dut.sv), key_byte_a_t, key_byte_b_t, KeySelf::this_type, KeyItemProvider::item_t,
// KeyItemProviderWide::item_t = 13 written. A typedef declared in a parameterized definition is a
// member of each specialization of it -- its aliased type may depend on that specialization's
// parameters -- so each specialization owns its own copy: outer_scope_alias_t is declared in
// TdOuter, and TdOuterAlias specializes TdOuter#(80), which owns a second outer_scope_alias_t;
// this_type is declared in KeySelf, and KeySelf #(byte) owns a second this_type. 13 + 2 = 15. The
// typedefs declared outside any parameterized definition (packages, $unit, module dut, the
// unparameterized KeyItemProvider classes) are never copied.

TEST_F(StaticElaborationTest, TotalTypedefTypespecCountInGraph) {
  EXPECT_EQ(countAll<hldb::TypedefTypespec>(), 15u);
}

// ---- Sanity check: every UnsupportedTypespec HLC built as a placeholder for this file (a name
// whose kind -- class, module, interface, program -- is not known while parsing) should have
// been resolved to its concrete typespec and orphaned (setParent(nullptr)) by the end of
// compilation. A non-zero count here means something was left unresolved. Deliberately checks
// "still has a live parent", not "count is zero" -- the ORIGINAL placeholder objects still exist
// (orphaned, not destroyed; left for hldb::GarbageCollector), so a raw total would never reach zero.

TEST_F(StaticElaborationTest, NoUnresolvedScopedTypespecsRemain) {
  size_t stillLive = 0;
  for (hldb::Any *const source : m_session->getDatabase().getObjects<hldb::UnsupportedTypespec>()) {
    if (const hldb::UnsupportedTypespec *const ut = any_cast<hldb::UnsupportedTypespec>(source)) {
      if (ut->getParent() != nullptr) ++stillLive;
    }
  }
  EXPECT_EQ(stillLive, 0u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
