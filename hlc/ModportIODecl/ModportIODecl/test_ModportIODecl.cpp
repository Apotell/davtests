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

// Validates modports as named groups of io decls (IEEE 1800-2023):
//   Sec 25.5     -- a modport lists interface items with the directions seen from the module
//   Sec 25.5.4   -- a simple modport port identifier is both the port name and a reference to
//                   the interface item; .P(expr) ports have their own name space per modport
//   Sec 25.5.5   -- "clocking cb" in a modport names a clocking block of the interface
//   Sec 25.10    -- through a modport, listable objects are accessible only if listed; types
//                   and parameters remain accessible
//   Sec 37.7     -- modport -> io decl
//   Sec 37.13 d3 -- the ref obj of a module's modport port names the modport declaration

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/clocking_block.h>
#include <hldb/clocking_io_decl.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/part_select.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ModportIODecl : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ModportIODecl.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Interface *findInterface(std::string_view name) const {
    return hldb::findByDefName<hldb::Interface>(name, m_design->getAllInterfaces());
  }

  const hldb::Module *findModule(std::string_view name) const {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  const hldb::Modport *findModport(std::string_view interfaceName, std::string_view modportName) const {
    const hldb::Interface *const i = findInterface(interfaceName);
    return (i == nullptr) ? nullptr : hldb::findByName<hldb::Modport>(modportName, i->getModports());
  }

  // The first statement of the module's first process (always/initial), unwrapping "@(...)".
  const hldb::Assignment *firstAssignment(const hldb::Module *m) const {
    if ((m == nullptr) || (m->getProcesses() == nullptr) || m->getProcesses()->empty()) return nullptr;
    const hldb::Process *const process = m->getProcesses()->front();
    if (process == nullptr) return nullptr;
    const hldb::Any *stmt = process->getStmt();
    if (const hldb::EventControl *const ec = any_cast<hldb::EventControl>(stmt)) stmt = ec->getStmt();
    return any_cast<hldb::Assignment>(stmt);
  }

  // Path element 'index' of a dotted reference such as "a.gnt".
  static const hldb::RefObj *pathElem(const hldb::Any *ref, size_t index) {
    const hldb::AnyCollection *elems = nullptr;
    if (const hldb::RefObj *const ro = any_cast<hldb::RefObj>(ref)) {
      elems = ro->getPathElems();
    } else if (const hldb::RefTypespec *const rt = any_cast<hldb::RefTypespec>(ref)) {
      elems = rt->getPathElems();
    }
    if ((elems == nullptr) || (elems->size() <= index)) return nullptr;
    return any_cast<hldb::RefObj>(elems->at(index));
  }

  // A simple modport port: listed with 'direction', its vpiExpr a ref obj naming the
  // interface item of the same name.
  void checkSimplePort(const hldb::Modport *mp, std::string_view name, int32_t direction,
                       hldb::AnyType itemType) const {
    ASSERT_NE(mp, nullptr);
    const hldb::IODecl *const iodecl = hldb::findByName<hldb::IODecl>(name, mp->getIODecls());
    ASSERT_NE(iodecl, nullptr) << "Modport '" << mp->getName() << "' has no io decl '" << name << "'";
    EXPECT_EQ(iodecl->getDirection(), direction) << name;
    const hldb::RefObj *const expr = iodecl->getExpr<hldb::RefObj>();
    ASSERT_NE(expr, nullptr) << "io decl '" << name << "' has no ref obj vpiExpr";
    EXPECT_EQ(expr->getName(), name);
    ASSERT_NE(expr->getActual(), nullptr) << "io decl '" << name << "' does not refer to the interface item";
    EXPECT_EQ(expr->getActual()->getAnyType(), itemType) << name;
    EXPECT_EQ(expr->getActual()->getName(), name);
    EXPECT_EQ(hldb::getParent<hldb::Interface>(expr->getActual()), mp->getInterface()) << name;
  }
};

// ----
// Sec 25.5, 25.5.1: a modport is a named group of io decls
// ----

TEST_F(ModportIODecl, TargetModportListsItsPortsWithDirections) {
  const hldb::Modport *const mp = findModport("simple_bus", "target");
  ASSERT_NE(mp, nullptr);
  ASSERT_NE(mp->getIODecls(), nullptr);
  EXPECT_EQ(mp->getIODecls()->size(), 8u);
  EXPECT_EQ(mp->getInterface(), findInterface("simple_bus"));

  checkSimplePort(mp, "req", vpiInput, hldb::AnyType::Variable);
  checkSimplePort(mp, "addr", vpiInput, hldb::AnyType::Variable);
  checkSimplePort(mp, "mode", vpiInput, hldb::AnyType::Variable);
  checkSimplePort(mp, "start", vpiInput, hldb::AnyType::Variable);
  checkSimplePort(mp, "clk", vpiInput, hldb::AnyType::Net);  // the interface's own port
  checkSimplePort(mp, "gnt", vpiOutput, hldb::AnyType::Variable);
  checkSimplePort(mp, "rdy", vpiOutput, hldb::AnyType::Variable);
  checkSimplePort(mp, "data", vpiRef, hldb::AnyType::Variable);
}

TEST_F(ModportIODecl, InitiatorModportHasOppositeDirections) {
  const hldb::Modport *const mp = findModport("simple_bus", "initiator");
  checkSimplePort(mp, "gnt", vpiInput, hldb::AnyType::Variable);
  checkSimplePort(mp, "req", vpiOutput, hldb::AnyType::Variable);
  checkSimplePort(mp, "data", vpiRef, hldb::AnyType::Variable);
}

TEST_F(ModportIODecl, NetsCanBeListed) {
  const hldb::Modport *const mp = findModport("A_Bus", "DUT");
  checkSimplePort(mp, "req", vpiInput, hldb::AnyType::Net);
  checkSimplePort(mp, "gnt", vpiOutput, hldb::AnyType::Net);
  checkSimplePort(mp, "data", vpiInout, hldb::AnyType::Net);
}

// ----
// Sec 25.5.4: modport expressions
// ----

TEST_F(ModportIODecl, ModportExpressionPortsCarryTheirExpression) {
  const hldb::Modport *const a = findModport("I", "A");
  ASSERT_NE(a, nullptr);

  const hldb::IODecl *const p = hldb::findByName<hldb::IODecl>("P", a->getIODecls());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getDirection(), vpiOutput);
  const hldb::PartSelect *const ps = p->getExpr<hldb::PartSelect>();
  ASSERT_NE(ps, nullptr) << "'.P(r[3:0])' should be a part-select";
  const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  ASSERT_NE(prefix->getActual(), nullptr);
  EXPECT_EQ(prefix->getActual()->getName(), "r");

  const hldb::IODecl *const q = hldb::findByName<hldb::IODecl>("Q", a->getIODecls());
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getDirection(), vpiInput);
  const hldb::RefObj *const x = q->getExpr<hldb::RefObj>();
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getName(), "x");
  ASSERT_NE(x->getActual(), nullptr);
  EXPECT_EQ(x->getActual()->getName(), "x");

  checkSimplePort(a, "R", vpiInput, hldb::AnyType::Variable);

  const hldb::Modport *const b = findModport("I", "B");
  ASSERT_NE(b, nullptr);
  const hldb::IODecl *const bq = hldb::findByName<hldb::IODecl>("Q", b->getIODecls());
  ASSERT_NE(bq, nullptr);
  EXPECT_NE(bq->getExpr<hldb::Constant>(), nullptr) << "'.Q(2)' should be a constant";
}

TEST_F(ModportIODecl, ModportExpressionPortNameResolvesThroughModport) {
  // "initial i.P = i.Q;" in module M (I.A i): P and Q are modport A's ports, so they refer to
  // what those ports name (Sec 25.5.4): the part-select r[3:0] and the variable x. A ref obj's
  // vpiActual is never the io decl itself (Sec 37.15).
  const hldb::Modport *const a = findModport("I", "A");
  ASSERT_NE(a, nullptr);
  const hldb::IODecl *const p = hldb::findByName<hldb::IODecl>("P", a->getIODecls());
  const hldb::IODecl *const q = hldb::findByName<hldb::IODecl>("Q", a->getIODecls());
  ASSERT_NE(p, nullptr);
  ASSERT_NE(q, nullptr);
  const hldb::Assignment *const assign = firstAssignment(findModule("M"));
  ASSERT_NE(assign, nullptr);

  const hldb::RefObj *const lhsP = pathElem(assign->getLhs(), 1);
  ASSERT_NE(lhsP, nullptr);
  ASSERT_NE(lhsP->getActual(), nullptr);
  EXPECT_EQ(lhsP->getActual(), p->getExpr());
  EXPECT_EQ(lhsP->getActual()->getAnyType(), hldb::AnyType::PartSelect);

  const hldb::RefObj *const rhsQ = pathElem(assign->getRhs(), 1);
  ASSERT_NE(rhsQ, nullptr);
  ASSERT_NE(rhsQ->getActual(), nullptr);
  EXPECT_EQ(rhsQ->getActual(), q->getExpr<hldb::RefObj>()->getActual());
  EXPECT_EQ(rhsQ->getActual()->getName(), "x");
}

// ----
// Sec 25.5.5: clocking blocks and modports
// ----

TEST_F(ModportIODecl, ClockingModportItemNamesTheClockingBlock) {
  const hldb::Modport *const mp = findModport("A_Bus", "STB");
  ASSERT_NE(mp, nullptr);
  const hldb::IODecl *const sb = hldb::findByName<hldb::IODecl>("sb", mp->getIODecls());
  ASSERT_NE(sb, nullptr);
  EXPECT_TRUE(sb->getClocking());
  const hldb::RefObj *const expr = sb->getExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  ASSERT_NE(expr->getActual(), nullptr);
  EXPECT_EQ(expr->getActual()->getAnyType(), hldb::AnyType::ClockingBlock);
  EXPECT_EQ(expr->getActual()->getName(), "sb");
}

TEST_F(ModportIODecl, ClockingSignalResolvesThroughModport) {
  // "initial b.sb.req <= 1'b1;" in module stb (A_Bus.STB b): req is clocking block sb's signal.
  const hldb::Assignment *const assign = firstAssignment(findModule("stb"));
  ASSERT_NE(assign, nullptr);

  const hldb::RefObj *const sb = pathElem(assign->getLhs(), 1);
  ASSERT_NE(sb, nullptr);
  ASSERT_NE(sb->getActual(), nullptr);
  EXPECT_EQ(sb->getActual()->getAnyType(), hldb::AnyType::ClockingBlock);
  EXPECT_EQ(sb->getActual()->getName(), "sb");

  const hldb::RefObj *const req = pathElem(assign->getLhs(), 2);
  ASSERT_NE(req, nullptr);
  ASSERT_NE(req->getActual(), nullptr);
  EXPECT_EQ(req->getActual()->getAnyType(), hldb::AnyType::ClockingIODecl);
  EXPECT_EQ(req->getActual()->getName(), "req");
}

// ----
// Sec 37.13 detail 3: a module's modport port refers to the modport declaration
// ----

TEST_F(ModportIODecl, ModulePortIODeclRefersToModport) {
  struct Expected {
    std::string_view m_module, m_port, m_interface, m_modport;
  };
  for (const Expected &e : {Expected{"memMod", "a", "simple_bus", "target"}, Expected{"M", "i", "I", "A"},
                            Expected{"stb", "b", "A_Bus", "STB"}, Expected{"sub", "i", "ebus_i", "mp"}}) {
    const hldb::Module *const m = findModule(e.m_module);
    ASSERT_NE(m, nullptr) << e.m_module;
    const hldb::IODecl *const iodecl = hldb::findByName<hldb::IODecl>(e.m_port, m->getIODecls());
    ASSERT_NE(iodecl, nullptr) << e.m_module;
    const hldb::RefObj *const expr = iodecl->getExpr<hldb::RefObj>();
    ASSERT_NE(expr, nullptr) << e.m_module;
    EXPECT_EQ(expr->getActual(), findModport(e.m_interface, e.m_modport)) << e.m_module;

    // Sec 37.14 detail 5: the port's vpiLowConn is a ref obj, naming the same declaration.
    const hldb::Port *const port = hldb::findByName<hldb::Port>(e.m_port, m->getPorts());
    ASSERT_NE(port, nullptr) << e.m_module;
    const hldb::RefObj *const lowConn = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(lowConn, nullptr) << e.m_module;
    EXPECT_EQ(lowConn->getActual(), findModport(e.m_interface, e.m_modport)) << e.m_module;
  }
}

TEST_F(ModportIODecl, MemberUseResolvesToInterfaceItem) {
  // "a.gnt <= a.req & avail;" in memMod (simple_bus.target a): 'a' names modport target and
  // 'gnt' the interface variable its modport port refers to (Sec 25.5.4, 37.15).
  const hldb::Modport *const target = findModport("simple_bus", "target");
  ASSERT_NE(target, nullptr);
  const hldb::Assignment *const assign = firstAssignment(findModule("memMod"));
  ASSERT_NE(assign, nullptr);

  const hldb::RefObj *const a = pathElem(assign->getLhs(), 0);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getActual(), target);

  const hldb::RefObj *const gnt = pathElem(assign->getLhs(), 1);
  ASSERT_NE(gnt, nullptr);
  ASSERT_NE(gnt->getActual(), nullptr);
  EXPECT_EQ(gnt->getActual()->getAnyType(), hldb::AnyType::Variable);
  EXPECT_EQ(gnt->getActual()->getName(), "gnt");
  EXPECT_EQ(hldb::getParent<hldb::Interface>(gnt->getActual()), findInterface("simple_bus"));
}

// ----
// Sec 25.10: access to interface objects through a modport
// ----

TEST_F(ModportIODecl, ListedObjectIsAccessibleThroughModport) {
  // "assign P = i.Q;" in sub (ebus_i.mp i).
  const hldb::Module *const m = findModule("sub");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getContAssigns(), nullptr);
  ASSERT_FALSE(m->getContAssigns()->empty());
  const hldb::RefObj *const q = pathElem(m->getContAssigns()->front()->getRhs(), 1);
  ASSERT_NE(q, nullptr);
  ASSERT_NE(q->getActual(), nullptr);
  EXPECT_EQ(q->getActual()->getAnyType(), hldb::AnyType::Variable);
  EXPECT_EQ(q->getActual()->getName(), "Q");
  EXPECT_EQ(hldb::getParent<hldb::Interface>(q->getActual()), findInterface("ebus_i"));
}

TEST_F(ModportIODecl, TypeIsAccessibleThroughModport) {
  // "typedef i.choice yes_no;" -- a type cannot be listed in a modport, so it stays accessible.
  const hldb::Module *const m = findModule("sub");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getTypedefs(), nullptr);
  const hldb::Typedef *const yesNo = hldb::findByName<hldb::Typedef>("yes_no", m->getTypedefs());
  ASSERT_NE(yesNo, nullptr);
  ASSERT_NE(yesNo->getAlias(), nullptr);
  const hldb::RefObj *const choice = pathElem(yesNo->getAlias(), 1);
  ASSERT_NE(choice, nullptr);
  ASSERT_NE(choice->getActual(), nullptr);
  EXPECT_EQ(choice->getActual()->getName(), "choice");
}

TEST_F(ModportIODecl, UnlistedObjectIsNotAccessibleThroughModport) {
  // "initial i.I = 0;" -- I is a variable of ebus_i not listed in modport mp.
  const hldb::Assignment *const assign = firstAssignment(findModule("sub"));
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const i = pathElem(assign->getLhs(), 1);
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getName(), "I");
  EXPECT_EQ(i->getActual(), nullptr) << "'I' is not in modport mp and must not resolve through it";
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "I"), nullptr);
}

// ----
// Sec 30.4.1, Syntax 30-3: a module path source through a modport must be an input or inout
// ----

TEST_F(ModportIODecl, SpecifyPathSourceMustBeModportInputOrInout) {
  // "(p.b => y)" with p a "sp_if.src" port: b is an output of modport src.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SPECIFY_PATH, "b"), nullptr);
  // "(p.a => y)": a is an input of modport src.
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_SPECIFY_PATH, "a"), nullptr);
}

// ----
// Sec 25.10, 6.17: an event is a variable, so a modport restricts it like one
// ----

TEST_F(ModportIODecl, UnlistedEventIsNotAccessibleThroughModport) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "e"), nullptr)
      << "'e' is not in modport src and must not resolve through it";
}

// ----
// Sec 25.5.4, 37.15: a ".Q(2)" member is legal, though no object for a ref obj to refer to
// ----

TEST_F(ModportIODecl, ConstantModportExpressionPortIsALegalMember) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "Q"), nullptr);
}

// ----
// Sec 6.10: a modport may list a net its interface declares implicitly
// ----

TEST_F(ModportIODecl, ImplicitNetFromInstanceConnectionCanBeListed) {
  // "leaf_if l (.w(imp)); modport m (input imp);" in implicit_if.
  const hldb::Modport *const mp = findModport("implicit_if", "m");
  ASSERT_NE(mp, nullptr);
  const hldb::IODecl *const imp = hldb::findByName<hldb::IODecl>("imp", mp->getIODecls());
  ASSERT_NE(imp, nullptr);
  EXPECT_EQ(imp->getDirection(), vpiInput);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "imp", 100, 20), nullptr)
      << "the modport port 'imp' (dut.sv:100) names an implicitly declared net";
}

// ----
// Sec 23.2.2: an interface_port_header has no port_direction, so none is missing
// ----

TEST_F(ModportIODecl, InterfacePortIsNotWarnedMissingDirection) {
  for (std::string_view name : {"a", "i", "b"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_PORT_MISSING_DIRECTION, name), nullptr) << name;
  }
}

// ----
// Sec 23.2.2.4: a default port value is only allowed on an input port
// ----

TEST_F(ModportIODecl, InterfacePortDefaultValueIsIllegal) {
  // "module bad_default (simple_bus x = 1'b0);"
  const hldb::Module *const m = findModule("bad_default");
  ASSERT_NE(m, nullptr);
  EXPECT_NE(hldb::findByName<hldb::IODecl>("x", m->getIODecls()), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DEFAULT_PORT_VALUE, "x"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
