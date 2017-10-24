/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstSemanticChecker.cpp
 *
 * Implementation of the semantic checker pass.
 *
 ***********************************************************************/

#include "AstSemanticChecker.h"
#include "AstClause.h"
#include "AstComponent.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstTranslationUnit.h"
#include "AstTypeAnalysis.h"
#include "AstUtils.h"
#include "AstVisitor.h"
#include "ComponentModel.h"
#include "Global.h"
#include "GraphUtils.h"
#include "PrecedenceGraph.h"
#include "Util.h"

#include <set>
#include <vector>

namespace souffle {

class AstTranslationUnit;

bool AstSemanticChecker::transform(AstTranslationUnit& translationUnit) {
    const TypeEnvironment& typeEnv =
            translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->getTypeEnvironment();
    TypeAnalysis* typeAnalysis = translationUnit.getAnalysis<TypeAnalysis>();
    ComponentLookup* componentLookup = translationUnit.getAnalysis<ComponentLookup>();
    PrecedenceGraph* precedenceGraph = translationUnit.getAnalysis<PrecedenceGraph>();
    RecursiveClauses* recursiveClauses = translationUnit.getAnalysis<RecursiveClauses>();
    checkProgram(translationUnit.getErrorReport(), *translationUnit.getProgram(), typeEnv, *typeAnalysis,
            *componentLookup, *precedenceGraph, *recursiveClauses);
    return false;
}

void AstSemanticChecker::checkProgram(ErrorReport& report, const AstProgram& program,
        const TypeEnvironment& typeEnv, const TypeAnalysis& typeAnalysis,
        const ComponentLookup& componentLookup, const PrecedenceGraph& precedenceGraph,
        const RecursiveClauses& recursiveClauses) {
    // -- conduct checks --
    // TODO: re-write to use visitors
    checkTypes(report, program);
    checkRules(report, typeEnv, program, recursiveClauses);
    checkComponents(report, program, componentLookup);
    checkNamespaces(report, program);
    checkIODirectives(report, program);
    checkInlining(report, program, precedenceGraph);

    // get the list of components to be checked
    std::vector<const AstNode*> nodes;
    for (const auto& rel : program.getRelations()) {
        for (const auto& cls : rel->getClauses()) {
            nodes.push_back(cls);
        }
    }

    // -- check grounded variables --
    visitDepthFirst(nodes, [&](const AstClause& clause) {
        // only interested in rules
        if (clause.isFact()) {
            return;
        }

        // compute all grounded terms
        auto isGrounded = getGroundedTerms(clause);

        // all terms in head need to be grounded
        std::set<std::string> reportedVars;
        for (const AstVariable* cur : getVariables(clause)) {
            if (!isGrounded[cur] && reportedVars.insert(cur->getName()).second) {
                report.addError("Ungrounded variable " + cur->getName(), cur->getSrcLoc());
            }
        }

    });

    // -- type checks --

    // - variables -
    visitDepthFirst(nodes, [&](const AstVariable& var) {
        if (typeAnalysis.getTypes(&var).empty()) {
            report.addError("Unable to deduce type for variable " + var.getName(), var.getSrcLoc());
        }
    });

    // - constants -

    // all string constants are used as symbols
    visitDepthFirst(nodes, [&](const AstStringConstant& cnst) {
        TypeSet types = typeAnalysis.getTypes(&cnst);
        if (!isSymbolType(types)) {
            report.addError("Symbol constant (type mismatch)", cnst.getSrcLoc());
        }
    });

    // all number constants are used as numbers
    visitDepthFirst(nodes, [&](const AstNumberConstant& cnst) {
        TypeSet types = typeAnalysis.getTypes(&cnst);
        if (!isNumberType(types)) {
            report.addError("Number constant (type mismatch)", cnst.getSrcLoc());
        }
        AstDomain idx = cnst.getIndex();
        if (idx > 2147483647 || idx < -2147483648) {
            report.addError("Number constant not in range [-2^31, 2^31-1]", cnst.getSrcLoc());
        }
    });

    // all null constants are used as records
    visitDepthFirst(nodes, [&](const AstNullConstant& cnst) {
        TypeSet types = typeAnalysis.getTypes(&cnst);
        if (!isRecordType(types)) {
            // TODO (#467) remove the next line to enable subprogram compilation for record types
            if (Global::config().has("stratify")) Global::config().unset("stratify");
            report.addError("Null constant used as a non-record", cnst.getSrcLoc());
        }
    });

    // record initializations have the same size as their types
    visitDepthFirst(nodes, [&](const AstRecordInit& cnst) {
        TypeSet types = typeAnalysis.getTypes(&cnst);
        if (isRecordType(types)) {
            // TODO (#467) remove the next line to enable subprogram compilation for record types
            if (Global::config().has("stratify")) Global::config().unset("stratify");
            for (const Type& type : types) {
                if (cnst.getArguments().size() !=
                        dynamic_cast<const RecordType*>(&type)->getFields().size()) {
                    report.addError("Wrong number of arguments given to record", cnst.getSrcLoc());
                }
            }
        }
    });

    // - unary functors -
    visitDepthFirst(nodes, [&](const AstUnaryFunctor& fun) {

        // check arg
        auto arg = fun.getOperand();

        // check appropriate use use of a numeric functor
        if (fun.isNumerical() && !isNumberType(typeAnalysis.getTypes(&fun))) {
            report.addError("Non-numeric use for numeric functor", fun.getSrcLoc());
        }

        // check argument type of a numeric functor
        if (fun.acceptsNumbers() && !isNumberType(typeAnalysis.getTypes(arg))) {
            report.addError("Non-numeric argument for numeric functor", arg->getSrcLoc());
        }

        // check symbolic operators
        if (fun.isSymbolic() && !isSymbolType(typeAnalysis.getTypes(&fun))) {
            report.addError("Non-symbolic use for symbolic functor", fun.getSrcLoc());
        }

        // check symbolic operands
        if (fun.acceptsSymbols() && !isSymbolType(typeAnalysis.getTypes(arg))) {
            report.addError("Non-symbolic argument for symbolic functor", arg->getSrcLoc());
        }
    });

    // - binary functors -
    visitDepthFirst(nodes, [&](const AstBinaryFunctor& fun) {

        // check left and right side
        auto lhs = fun.getLHS();
        auto rhs = fun.getRHS();

        // check numeric types of result, first and second argument
        if (fun.isNumerical() && !isNumberType(typeAnalysis.getTypes(&fun))) {
            report.addError("Non-numeric use for numeric functor", fun.getSrcLoc());
        }
        if (fun.acceptsNumbers(0) && !isNumberType(typeAnalysis.getTypes(lhs))) {
            report.addError("Non-numeric first argument for functor", lhs->getSrcLoc());
        }
        if (fun.acceptsNumbers(1) && !isNumberType(typeAnalysis.getTypes(rhs))) {
            report.addError("Non-numeric second argument for functor", rhs->getSrcLoc());
        }

        // check symbolic types of result, first and second argument
        if (fun.isSymbolic() && !isSymbolType(typeAnalysis.getTypes(&fun))) {
            report.addError("Non-symbolic use for symbolic functor", fun.getSrcLoc());
        }
        if (fun.acceptsSymbols(0) && !isSymbolType(typeAnalysis.getTypes(lhs))) {
            report.addError("Non-symbolic first argument for functor", lhs->getSrcLoc());
        }
        if (fun.acceptsSymbols(1) && !isSymbolType(typeAnalysis.getTypes(rhs))) {
            report.addError("Non-symbolic second argument for functor", rhs->getSrcLoc());
        }

    });

    // - ternary functors -
    visitDepthFirst(nodes, [&](const AstTernaryFunctor& fun) {

        // check left and right side
        auto a0 = fun.getArg(0);
        auto a1 = fun.getArg(1);
        auto a2 = fun.getArg(2);

        // check numeric types of result, first and second argument
        if (fun.isNumerical() && !isNumberType(typeAnalysis.getTypes(&fun))) {
            report.addError("Non-numeric use for numeric functor", fun.getSrcLoc());
        }
        if (fun.acceptsNumbers(0) && !isNumberType(typeAnalysis.getTypes(a0))) {
            report.addError("Non-numeric first argument for functor", a0->getSrcLoc());
        }
        if (fun.acceptsNumbers(1) && !isNumberType(typeAnalysis.getTypes(a1))) {
            report.addError("Non-numeric second argument for functor", a1->getSrcLoc());
        }
        if (fun.acceptsNumbers(2) && !isNumberType(typeAnalysis.getTypes(a2))) {
            report.addError("Non-numeric third argument for functor", a2->getSrcLoc());
        }

        // check symbolic types of result, first and second argument
        if (fun.isSymbolic() && !isSymbolType(typeAnalysis.getTypes(&fun))) {
            report.addError("Non-symbolic use for symbolic functor", fun.getSrcLoc());
        }
        if (fun.acceptsSymbols(0) && !isSymbolType(typeAnalysis.getTypes(a0))) {
            report.addError("Non-symbolic first argument for functor", a0->getSrcLoc());
        }
        if (fun.acceptsSymbols(1) && !isSymbolType(typeAnalysis.getTypes(a1))) {
            report.addError("Non-symbolic second argument for functor", a1->getSrcLoc());
        }
        if (fun.acceptsSymbols(2) && !isSymbolType(typeAnalysis.getTypes(a2))) {
            report.addError("Non-symbolic third argument for functor", a2->getSrcLoc());
        }

    });

    // - binary relation -
    visitDepthFirst(nodes, [&](const AstConstraint& constraint) {

        // only interested in non-equal constraints
        auto op = constraint.getOperator();
        if (op == BinaryConstraintOp::EQ || op == BinaryConstraintOp::NE) {
            return;
        }

        // get left and right side
        auto lhs = constraint.getLHS();
        auto rhs = constraint.getRHS();

        if (constraint.isNumerical()) {
            // check numeric type
            if (!isNumberType(typeAnalysis.getTypes(lhs))) {
                report.addError("Non-numerical operand for comparison", lhs->getSrcLoc());
            }
            if (!isNumberType(typeAnalysis.getTypes(rhs))) {
                report.addError("Non-numerical operand for comparison", rhs->getSrcLoc());
            }
        } else if (constraint.isSymbolic()) {
            // check symbolic type
            if (!isSymbolType(typeAnalysis.getTypes(lhs))) {
                report.addError("Non-string operand for operation", lhs->getSrcLoc());
            }
            if (!isSymbolType(typeAnalysis.getTypes(rhs))) {
                report.addError("Non-string operand for operation", rhs->getSrcLoc());
            }
        }
    });

    // - stratification --

    // check for cyclic dependencies
    const Graph<const AstRelation*, AstNameComparison>& depGraph = precedenceGraph.graph();
    for (const AstRelation* cur : depGraph.vertices()) {
        if (depGraph.reaches(cur, cur)) {
            AstRelationSet clique = depGraph.clique(cur);
            for (const AstRelation* cyclicRelation : clique) {
                // Negations and aggregations need to be stratified
                const AstLiteral* foundLiteral = nullptr;
                bool hasNegation = hasClauseWithNegatedRelation(cyclicRelation, cur, &program, foundLiteral);
                if (hasNegation ||
                        hasClauseWithAggregatedRelation(cyclicRelation, cur, &program, foundLiteral)) {
                    std::string relationsListStr = toString(join(clique, ",",
                            [](std::ostream& out, const AstRelation* r) { out << r->getName(); }));
                    std::vector<DiagnosticMessage> messages;
                    messages.push_back(
                            DiagnosticMessage("Relation " + toString(cur->getName()), cur->getSrcLoc()));
                    std::string negOrAgg = hasNegation ? "negation" : "aggregation";
                    messages.push_back(
                            DiagnosticMessage("has cyclic " + negOrAgg, foundLiteral->getSrcLoc()));
                    report.addDiagnostic(Diagnostic(Diagnostic::ERROR,
                            DiagnosticMessage("Unable to stratify relation(s) {" + relationsListStr + "}"),
                            messages));
                    break;
                }
            }
        }
    }
}

void AstSemanticChecker::checkAtom(ErrorReport& report, const AstProgram& program, const AstAtom& atom) {
    // check existence of relation
    auto* r = program.getRelation(atom.getName());
    if (!r) {
        report.addError("Undefined relation " + toString(atom.getName()), atom.getSrcLoc());
    }

    // check arity
    if (r && r->getArity() != atom.getArity()) {
        report.addError("Mismatching arity of relation " + toString(atom.getName()), atom.getSrcLoc());
    }

    for (const AstArgument* arg : atom.getArguments()) {
        checkArgument(report, program, *arg);
    }
}

/* Check whether an unnamed variable occurs in an argument (expression) */
static bool hasUnnamedVariable(const AstArgument* arg) {
    if (dynamic_cast<const AstUnnamedVariable*>(arg)) {
        return true;
    }
    if (dynamic_cast<const AstVariable*>(arg)) {
        return false;
    }
    if (dynamic_cast<const AstConstant*>(arg)) {
        return false;
    }
    if (dynamic_cast<const AstCounter*>(arg)) {
        return false;
    }
    if (const AstUnaryFunctor* uf = dynamic_cast<const AstUnaryFunctor*>(arg)) {
        return hasUnnamedVariable(uf->getOperand());
    }
    if (const AstBinaryFunctor* bf = dynamic_cast<const AstBinaryFunctor*>(arg)) {
        return hasUnnamedVariable(bf->getLHS()) || hasUnnamedVariable(bf->getRHS());
    }
    if (const AstTernaryFunctor* tf = dynamic_cast<const AstTernaryFunctor*>(arg)) {
        return hasUnnamedVariable(tf->getArg(0)) || hasUnnamedVariable(tf->getArg(1)) ||
               hasUnnamedVariable(tf->getArg(2));
    }
    if (const AstRecordInit* ri = dynamic_cast<const AstRecordInit*>(arg)) {
        return any_of(ri->getArguments(), (bool (*)(const AstArgument*))hasUnnamedVariable);
    }
    if (dynamic_cast<const AstAggregator*>(arg)) {
        return false;
    }
    std::cout << "Unsupported Argument type: " << typeid(*arg).name() << "\n";
    ASSERT(false && "Unsupported Argument Type!");
    return false;
}

static bool hasUnnamedVariable(const AstLiteral* lit) {
    if (const AstAtom* at = dynamic_cast<const AstAtom*>(lit)) {
        return any_of(at->getArguments(), (bool (*)(const AstArgument*))hasUnnamedVariable);
    }
    if (const AstNegation* neg = dynamic_cast<const AstNegation*>(lit)) {
        return hasUnnamedVariable(neg->getAtom());
    }
    if (const AstConstraint* br = dynamic_cast<const AstConstraint*>(lit)) {
        return hasUnnamedVariable(br->getLHS()) || hasUnnamedVariable(br->getRHS());
    }
    std::cout << "Unsupported Literal type: " << typeid(lit).name() << "\n";
    ASSERT(false && "Unsupported Argument Type!");
    return false;
}

void AstSemanticChecker::checkLiteral(
        ErrorReport& report, const AstProgram& program, const AstLiteral& literal) {
    // check potential nested atom
    if (auto* atom = literal.getAtom()) {
        checkAtom(report, program, *atom);
    }

    if (const AstConstraint* constraint = dynamic_cast<const AstConstraint*>(&literal)) {
        checkArgument(report, program, *constraint->getLHS());
        checkArgument(report, program, *constraint->getRHS());
    }

    // check for invalid underscore utilization
    if (hasUnnamedVariable(&literal)) {
        if (dynamic_cast<const AstAtom*>(&literal)) {
            // nothing to check since underscores are allowed
        } else if (dynamic_cast<const AstNegation*>(&literal)) {
            // nothing to check since underscores are allowed
        } else if (dynamic_cast<const AstConstraint*>(&literal)) {
            report.addError("Underscore in binary relation", literal.getSrcLoc());
        } else {
            std::cout << "Unsupported Literal type: " << typeid(literal).name() << "\n";
            ASSERT(false && "Unsupported Argument Type!");
        }
    }
}

void AstSemanticChecker::checkAggregator(
        ErrorReport& report, const AstProgram& program, const AstAggregator& aggregator) {
    for (AstLiteral* literal : aggregator.getBodyLiterals()) {
        checkLiteral(report, program, *literal);
    }
}

void AstSemanticChecker::checkArgument(
        ErrorReport& report, const AstProgram& program, const AstArgument& arg) {
    if (const AstAggregator* agg = dynamic_cast<const AstAggregator*>(&arg)) {
        checkAggregator(report, program, *agg);
    } else if (const AstUnaryFunctor* unaryFunc = dynamic_cast<const AstUnaryFunctor*>(&arg)) {
        checkArgument(report, program, *unaryFunc->getOperand());
    } else if (const AstBinaryFunctor* binFunc = dynamic_cast<const AstBinaryFunctor*>(&arg)) {
        checkArgument(report, program, *binFunc->getLHS());
        checkArgument(report, program, *binFunc->getRHS());
    } else if (const AstTernaryFunctor* ternFunc = dynamic_cast<const AstTernaryFunctor*>(&arg)) {
        checkArgument(report, program, *ternFunc->getArg(0));
        checkArgument(report, program, *ternFunc->getArg(1));
        checkArgument(report, program, *ternFunc->getArg(2));
    }
}

static bool isConstantArithExpr(const AstArgument& argument) {
    if (dynamic_cast<const AstNumberConstant*>(&argument)) {
        return true;
    }
    if (const AstUnaryFunctor* unOp = dynamic_cast<const AstUnaryFunctor*>(&argument)) {
        return unOp->isNumerical() && isConstantArithExpr(*unOp->getOperand());
    }
    if (const AstBinaryFunctor* binOp = dynamic_cast<const AstBinaryFunctor*>(&argument)) {
        return binOp->isNumerical() && isConstantArithExpr(*binOp->getLHS()) &&
               isConstantArithExpr(*binOp->getRHS());
    }
    if (const AstTernaryFunctor* ternOp = dynamic_cast<const AstTernaryFunctor*>(&argument)) {
        return ternOp->isNumerical() && isConstantArithExpr(*ternOp->getArg(0)) &&
               isConstantArithExpr(*ternOp->getArg(1)) && isConstantArithExpr(*ternOp->getArg(2));
    }
    return false;
}

void AstSemanticChecker::checkConstant(ErrorReport& report, const AstArgument& argument) {
    if (const AstVariable* var = dynamic_cast<const AstVariable*>(&argument)) {
        report.addError("Variable " + var->getName() + " in fact", var->getSrcLoc());
    } else if (dynamic_cast<const AstUnnamedVariable*>(&argument)) {
        report.addError("Underscore in fact", argument.getSrcLoc());
    } else if (dynamic_cast<const AstUnaryFunctor*>(&argument)) {
        if (!isConstantArithExpr(argument)) {
            report.addError("Unary function in fact", argument.getSrcLoc());
        }
    } else if (dynamic_cast<const AstBinaryFunctor*>(&argument)) {
        if (!isConstantArithExpr(argument)) {
            report.addError("Binary function in fact", argument.getSrcLoc());
        }
    } else if (dynamic_cast<const AstTernaryFunctor*>(&argument)) {
        if (!isConstantArithExpr(argument)) {
            report.addError("Ternary function in fact", argument.getSrcLoc());
        }
    } else if (dynamic_cast<const AstCounter*>(&argument)) {
        report.addError("Counter in fact", argument.getSrcLoc());
    } else if (dynamic_cast<const AstConstant*>(&argument)) {
        // this one is fine - type checker will make sure of number and symbol constants
    } else if (auto* ri = dynamic_cast<const AstRecordInit*>(&argument)) {
        for (auto* arg : ri->getArguments()) {
            checkConstant(report, *arg);
        }
    } else {
        std::cout << "Unsupported Argument: " << typeid(argument).name() << "\n";
        ASSERT(false && "Unknown case");
    }
}

/* Check if facts contain only constants */
void AstSemanticChecker::checkFact(ErrorReport& report, const AstProgram& program, const AstClause& fact) {
    assert(fact.isFact());

    AstAtom* head = fact.getHead();
    if (head == nullptr) {
        return;  // checked by clause
    }

    AstRelation* rel = program.getRelation(head->getName());
    if (rel == nullptr) {
        return;  // checked by clause
    }

    // facts must only contain constants
    for (size_t i = 0; i < head->argSize(); i++) {
        checkConstant(report, *head->getArgument(i));
    }
}

void AstSemanticChecker::checkClause(ErrorReport& report, const AstProgram& program, const AstClause& clause,
        const RecursiveClauses& recursiveClauses) {
    // check head atom
    checkAtom(report, program, *clause.getHead());

    // check for absence of underscores in head
    if (hasUnnamedVariable(clause.getHead())) {
        report.addError("Underscore in head of rule", clause.getHead()->getSrcLoc());
    }

    // check body literals
    for (AstLiteral* lit : clause.getAtoms()) {
        checkLiteral(report, program, *lit);
    }
    for (AstNegation* neg : clause.getNegations()) {
        checkLiteral(report, program, *neg);
    }
    for (AstLiteral* lit : clause.getConstraints()) {
        checkLiteral(report, program, *lit);
    }

    // check facts
    if (clause.isFact()) {
        checkFact(report, program, clause);
    }

    // check for use-once variables
    std::map<std::string, int> var_count;
    std::map<std::string, const AstVariable*> var_pos;
    visitDepthFirst(clause, [&](const AstVariable& var) {
        var_count[var.getName()]++;
        var_pos[var.getName()] = &var;
    });

    // check for variables only occurring once
    if (!clause.isGenerated()) {
        for (const auto& cur : var_count) {
            if (cur.second == 1 && cur.first[0] != '_') {
                report.addWarning(
                        "Variable " + cur.first + " only occurs once", var_pos[cur.first]->getSrcLoc());
            }
        }
    }

    // check execution plan
    if (clause.getExecutionPlan()) {
        auto numAtoms = clause.getAtoms().size();
        for (const auto& cur : clause.getExecutionPlan()->getOrders()) {
            if (cur.second->size() != numAtoms || !cur.second->isComplete()) {
                report.addError("Invalid execution plan", cur.second->getSrcLoc());
            }
        }
    }
    // check auto-increment
    if (recursiveClauses.recursive(&clause)) {
        visitDepthFirst(clause, [&](const AstCounter& ctr) {
            report.addError("Auto-increment functor in a recursive rule", ctr.getSrcLoc());
        });
    }
}

void AstSemanticChecker::checkRelationDeclaration(ErrorReport& report, const TypeEnvironment& typeEnv,
        const AstProgram& program, const AstRelation& relation) {
    for (size_t i = 0; i < relation.getArity(); i++) {
        AstAttribute* attr = relation.getAttribute(i);
        AstTypeIdentifier typeName = attr->getTypeName();

        /* check whether type exists */
        if (typeName != "number" && typeName != "symbol" && !program.getType(typeName)) {
            report.addError("Undefined type in attribute " + attr->getAttributeName() + ":" +
                                    toString(attr->getTypeName()),
                    attr->getSrcLoc());
        }

        /* check whether name occurs more than once */
        for (size_t j = 0; j < i; j++) {
            if (attr->getAttributeName() == relation.getAttribute(j)->getAttributeName()) {
                report.addError("Doubly defined attribute name " + attr->getAttributeName() + ":" +
                                        toString(attr->getTypeName()),
                        attr->getSrcLoc());
            }
        }

        /* check whether type is a record type */
        if (typeEnv.isType(typeName)) {
            const Type& type = typeEnv.getType(typeName);
            if (isRecordType(type)) {
                if (relation.isInput()) {
                    report.addError(
                            "Input relations must not have record types. "
                            "Attribute " +
                                    attr->getAttributeName() + " has record type " +
                                    toString(attr->getTypeName()),
                            attr->getSrcLoc());
                }
                if (relation.isOutput()) {
                    report.addWarning(
                            "Record types in output relations are not printed verbatim: attribute " +
                                    attr->getAttributeName() + " has record type " +
                                    toString(attr->getTypeName()),
                            attr->getSrcLoc());
                }
            }
        }
    }
}

void AstSemanticChecker::checkRelation(ErrorReport& report, const TypeEnvironment& typeEnv,
        const AstProgram& program, const AstRelation& relation, const RecursiveClauses& recursiveClauses) {
    if (relation.isEqRel()) {
        if (relation.getArity() == 2) {
            if (relation.getAttribute(0)->getTypeName() != relation.getAttribute(1)->getTypeName()) {
                report.addError(
                        "Domains of equivalence relation " + toString(relation.getName()) + " are different",
                        relation.getSrcLoc());
            }
        } else {
            report.addError("Equivalence relation " + toString(relation.getName()) + " is not binary",
                    relation.getSrcLoc());
        }
    }

    // start with declaration
    checkRelationDeclaration(report, typeEnv, program, relation);

    // check clauses
    for (AstClause* c : relation.getClauses()) {
        checkClause(report, program, *c, recursiveClauses);
    }

    // check whether this relation is empty
    if (relation.clauseSize() == 0 && !relation.isInput()) {
        report.addWarning(
                "No rules/facts defined for relation " + toString(relation.getName()), relation.getSrcLoc());
    }
}

void AstSemanticChecker::checkRules(ErrorReport& report, const TypeEnvironment& typeEnv,
        const AstProgram& program, const RecursiveClauses& recursiveClauses) {
    for (AstRelation* cur : program.getRelations()) {
        checkRelation(report, typeEnv, program, *cur, recursiveClauses);
    }

    for (AstClause* cur : program.getOrphanClauses()) {
        checkClause(report, program, *cur, recursiveClauses);
    }
}

// ----- components --------

const AstComponent* AstSemanticChecker::checkComponentNameReference(ErrorReport& report,
        const AstComponent* enclosingComponent, const ComponentLookup& componentLookup,
        const std::string& name, const AstSrcLocation& loc, const TypeBinding& binding) {
    const AstTypeIdentifier& forwarded = binding.find(name);
    if (!forwarded.empty()) {
        // for forwarded types we do not check anything, because we do not know,
        // what the actual type will be
        return nullptr;
    }

    const AstComponent* c = componentLookup.getComponent(enclosingComponent, name, binding);
    if (!c) {
        report.addError("Referencing undefined component " + name, loc);
        return nullptr;
    }

    return c;
}

void AstSemanticChecker::checkComponentReference(ErrorReport& report, const AstComponent* enclosingComponent,
        const ComponentLookup& componentLookup, const AstComponentType& type, const AstSrcLocation& loc,
        const TypeBinding& binding) {
    // check whether targeted component exists
    const AstComponent* c = checkComponentNameReference(
            report, enclosingComponent, componentLookup, type.getName(), loc, binding);
    if (!c) {
        return;
    }

    // check number of type parameters
    if (c->getComponentType().getTypeParameters().size() != type.getTypeParameters().size()) {
        report.addError("Invalid number of type parameters for component " + type.getName(), loc);
    }
}

void AstSemanticChecker::checkComponentInit(ErrorReport& report, const AstComponent* enclosingComponent,
        const ComponentLookup& componentLookup, const AstComponentInit& init, const TypeBinding& binding) {
    checkComponentReference(
            report, enclosingComponent, componentLookup, init.getComponentType(), init.getSrcLoc(), binding);

    // Note: actual parameters can be atomic types like number, or anything declared with .type
    // The original semantic check permitted any identifier (existing or non-existing) to be actual parameter
    // In order to maintain the compatibility, we do not check the actual parameters

    // check the actual parameters:
    // const auto& actualParams = init.getComponentType().getTypeParameters();
    // for (const auto& param : actualParams) {
    //    checkComponentNameReference(report, scope, param, init.getSrcLoc(), binding);
    //}
}

void AstSemanticChecker::checkComponent(ErrorReport& report, const AstComponent* enclosingComponent,
        const ComponentLookup& componentLookup, const AstComponent& component, const TypeBinding& binding) {
    // -- inheritance --

    // Update type binding:
    // Since we are not compiling, i.e. creating concrete instance of the
    // components with type parameters, we are only interested in whether
    // component references refer to existing components or some type parameter.
    // Type parameter for us here is unknown type that will be bound at the template
    // instantiation time.
    auto parentTypeParameters = component.getComponentType().getTypeParameters();
    std::vector<AstTypeIdentifier> actualParams(parentTypeParameters.size(), "<type parameter>");
    TypeBinding activeBinding = binding.extend(parentTypeParameters, actualParams);

    // check parents of component
    for (const auto& cur : component.getBaseComponents()) {
        checkComponentReference(
                report, enclosingComponent, componentLookup, cur, component.getSrcLoc(), activeBinding);

        // Note: type parameters can also be atomic types like number, or anything defined through .type
        // The original semantic check permitted any identifier (existing or non-existing) to be actual
        // parameter
        // In order to maintain the compatibility, we do not check the actual parameters

        // for (const std::string& param : cur.getTypeParameters()) {
        //    checkComponentNameReference(report, scope, param, component.getSrcLoc(), activeBinding);
        //}
    }

    // get all parents
    std::set<const AstComponent*> parents;
    std::function<void(const AstComponent&)> collectParents = [&](const AstComponent& cur) {
        for (const auto& base : cur.getBaseComponents()) {
            auto c = componentLookup.getComponent(enclosingComponent, base.getName(), binding);
            if (!c) {
                continue;
            }
            if (parents.insert(c).second) {
                collectParents(*c);
            }
        }
    };
    collectParents(component);

    // check overrides
    for (const AstRelation* relation : component.getRelations()) {
        if (component.getOverridden().count(relation->getName().getNames()[0])) {
            report.addError("Override of non-inherited relation " + relation->getName().getNames()[0] +
                                    " in component " + component.getComponentType().getName(),
                    component.getSrcLoc());
        }
    }
    for (const AstComponent* parent : parents) {
        for (const AstRelation* relation : parent->getRelations()) {
            if (component.getOverridden().count(relation->getName().getNames()[0]) &&
                    !relation->isOverridable()) {
                report.addError("Override of non-overridable relation " + relation->getName().getNames()[0] +
                                        " in component " + component.getComponentType().getName(),
                        component.getSrcLoc());
            }
        }
    }

    // check for a cycle
    if (parents.find(&component) != parents.end()) {
        report.addError(
                "Invalid cycle in inheritance for component " + component.getComponentType().getName(),
                component.getSrcLoc());
    }

    // -- nested components --

    // check nested components
    for (const auto& cur : component.getComponents()) {
        checkComponent(report, &component, componentLookup, *cur, activeBinding);
    }

    // check nested instantiations
    for (const auto& cur : component.getInstantiations()) {
        checkComponentInit(report, &component, componentLookup, *cur, activeBinding);
    }
}

void AstSemanticChecker::checkComponents(
        ErrorReport& report, const AstProgram& program, const ComponentLookup& componentLookup) {
    for (AstComponent* cur : program.getComponents()) {
        checkComponent(report, nullptr, componentLookup, *cur, TypeBinding());
    }

    for (AstComponentInit* cur : program.getComponentInstantiations()) {
        checkComponentInit(report, nullptr, componentLookup, *cur, TypeBinding());
    }
}

// ----- types --------

void AstSemanticChecker::checkUnionType(
        ErrorReport& report, const AstProgram& program, const AstUnionType& type) {
    // check presence of all the element types
    for (const AstTypeIdentifier& sub : type.getTypes()) {
        if (sub != "number" && sub != "symbol" && !program.getType(sub)) {
            report.addError("Undefined type " + toString(sub) + " in definition of union type " +
                                    toString(type.getName()),
                    type.getSrcLoc());
        }
    }
}

void AstSemanticChecker::checkRecordType(
        ErrorReport& report, const AstProgram& program, const AstRecordType& type) {
    // check proper definition of all field types
    for (const auto& field : type.getFields()) {
        if (field.type != "number" && field.type != "symbol" && !program.getType(field.type)) {
            report.addError(
                    "Undefined type " + toString(field.type) + " in definition of field " + field.name,
                    type.getSrcLoc());
        }
    }

    // check that field names are unique
    auto& fields = type.getFields();
    std::size_t numFields = fields.size();
    for (std::size_t i = 0; i < numFields; i++) {
        const std::string& cur_name = fields[i].name;
        for (std::size_t j = 0; j < i; j++) {
            if (fields[j].name == cur_name) {
                report.addError("Doubly defined field name " + cur_name + " in definition of type " +
                                        toString(type.getName()),
                        type.getSrcLoc());
            }
        }
    }
}

void AstSemanticChecker::checkType(ErrorReport& report, const AstProgram& program, const AstType& type) {
    if (const AstUnionType* u = dynamic_cast<const AstUnionType*>(&type)) {
        checkUnionType(report, program, *u);
    } else if (const AstRecordType* r = dynamic_cast<const AstRecordType*>(&type)) {
        checkRecordType(report, program, *r);
    }
}

void AstSemanticChecker::checkTypes(ErrorReport& report, const AstProgram& program) {
    // check each type individually
    for (const auto& cur : program.getTypes()) {
        checkType(report, program, *cur);
    }
}

void AstSemanticChecker::checkIODirectives(ErrorReport& report, const AstProgram& program) {
    for (const auto& directive : program.getIODirectives()) {
        auto* r = program.getRelation(directive->getName());
        if (r == nullptr) {
            report.addError("Undefined relation " + toString(directive->getName()), directive->getSrcLoc());
        }
    }
}

void AstSemanticChecker::checkInlining(
        ErrorReport& report, const AstProgram& program, const PrecedenceGraph& precedenceGraph) {
    // Find all inlined relations
    AstRelationSet inlinedRelations;
    visitDepthFirst(program, [&](const AstRelation& relation) {
        if (relation.isInline()) {
            inlinedRelations.insert(&relation);

            // Inlined relations cannot be computed or input relations
            if (relation.isComputed()) {
                report.addError("Computed relation " + toString(relation.getName()) + " cannot be inlined",
                        relation.getSrcLoc());
            }

            if (relation.isInput()) {
                report.addError("Input relation " + toString(relation.getName()) + " cannot be inlined",
                        relation.getSrcLoc());
            }
        }
    });

    // Check 1:
    // Let G' be the subgraph of the precedence graph G containing only those nodes
    // which are marked with the inline directive.
    // If G' contains a cycle, then inlining cannot be performed.

    // Remember all inlined nodes that have been visited
    AstRelationSet visitedNodes;

    // Keep going until all nodes have been visited
    while (true) {
        // Grab an unvisited node
        const AstRelation* currNode = nullptr;
        for (const AstRelation* rel : inlinedRelations) {
            if (visitedNodes.find(rel) == visitedNodes.end()) {
                currNode = rel;
            }
        }

        // All nodes have been visited
        if (currNode == nullptr) {
            break;
        }

        // Check if a cycle exists in its connected component
        bool cyclic = false;
        const AstRelation* cyclicOrigin;   // origin of the cycle
        const AstRelation* cyclicWitness;  // predecessor of the origin of the cycle

        std::stack<const AstRelation*> fringe;                    // Nodes we are planning to visit
        std::set<const AstRelation*> visited;                     // Nodes we have already visited
        std::map<const AstRelation*, const AstRelation*> origin;  // Start node

        origin[currNode] = nullptr;
        fringe.push(currNode);

        // Keep going until we've visited all inlined nodes or found a cycle
        while (!fringe.empty() && !cyclic) {
            currNode = fringe.top();
            fringe.pop();
            visited.insert(currNode);

            const AstRelationSet& successors = precedenceGraph.graph().successors(currNode);
            for (const AstRelation* successor : successors) {
                // We only want to deal with the subgraph of the precedence graph containing
                // inlined relations
                if (successor->isInline()) {
                    if (visited.find(successor) != visited.end()) {
                        // Found a node that we've already visited!
                        cyclic = true;
                        cyclicOrigin = successor;
                        cyclicWitness = currNode;
                        break;
                    }

                    origin[successor] = currNode;
                    fringe.push(successor);
                }
            }
        }

        // Check if we found a cycle
        if (cyclic) {
            // Construct the string representation of the cycle
            std::stringstream cycle;
            cycle << "{" << cyclicOrigin->getName();
            const AstRelation* currRelation = cyclicWitness;
            while (currRelation != cyclicOrigin) {
                cycle << ", " << currRelation->getName();
                currRelation = origin[currRelation];
            }
            cycle << "}";
            report.addError("Inlined relations " + cycle.str() + " are cyclically dependent",
                    cyclicOrigin->getSrcLoc());
            break;
        }

        for (const AstRelation* vis : visited) {
            visitedNodes.insert(vis);
        }
    }

    // Check 2:
    // Cannot use the counter argument ('$') in inlined relations

    // Check if an inlined literal ever takes in a $
    visitDepthFirst(program, [&](const AstAtom& atom) {
        AstRelation* associatedRelation = program.getRelation(atom.getName());
        if (associatedRelation != nullptr && associatedRelation->isInline()) {
            visitDepthFirst(atom, [&](const AstArgument& arg) {
                if (dynamic_cast<const AstCounter*>(&arg)) {
                    report.addError(
                            "Cannot inline literal containing a counter argument '$'", arg.getSrcLoc());
                }
            });
        }
    });

    // Check if an inlined clause ever contains a $
    for (const AstRelation* rel : inlinedRelations) {
        for (AstClause* clause : rel->getClauses()) {
            visitDepthFirst(*clause, [&](const AstArgument& arg) {
                if (dynamic_cast<const AstCounter*>(&arg)) {
                    report.addError(
                            "Cannot inline clause containing a counter argument '$'", arg.getSrcLoc());
                }
            });
        }
    }

    // Check 3:
    // Suppose the relation b is marked with the inline directive, but appears negated
    // in a clause. Then, if b introduces a new variable in its body, we cannot inline
    // the relation b.

    // Find all relations with the inline declarative that introduce new variables in their bodies
    AstRelationSet nonNegatableRelations;
    for (const AstRelation* rel : inlinedRelations) {
        bool foundNonNegatable = false;
        for (const AstClause* clause : rel->getClauses()) {
            // Get the variables in the head
            std::set<std::string> headVariables;
            visitDepthFirst(
                    *clause->getHead(), [&](const AstVariable& var) { headVariables.insert(var.getName()); });

            // Get the variables in the body
            std::set<std::string> bodyVariables;
            visitDepthFirst(clause->getBodyLiterals(),
                    [&](const AstVariable& var) { bodyVariables.insert(var.getName()); });

            // Check if all body variables are in the head
            // Do this separately to the above so only one error is printed per variable
            for (std::string var : bodyVariables) {
                if (headVariables.find(var) == headVariables.end()) {
                    nonNegatableRelations.insert(rel);
                    foundNonNegatable = true;
                    break;
                }
            }

            if (foundNonNegatable) {
                break;
            }
        }
    }

    // Check that these relations never appear negated
    visitDepthFirst(program, [&](const AstNegation& neg) {
        AstRelation* associatedRelation = program.getRelation(neg.getAtom()->getName());
        if (nonNegatableRelations.find(associatedRelation) != nonNegatableRelations.end()) {
            report.addError(
                    "Inlined relation may introduce new variables but appears negated", neg.getSrcLoc());
        }
    });

    // Check 4:
    // Don't support inlining atoms within aggregators at this point.

    // Reasoning: Suppose we have an aggregator like `max X: a(X)`, where `a` is inlined to `a1` and `a2`.
    // Then, `max X: a(X)` will become `max( max X: a1(X),  max X: a2(X) )`. Suppose further that a(X) has
    // values X where it is true, while a2(X) does not. Then, the produced argument
    // `max( max X: a1(X),  max X: a2(X) )` will not return anything (as one of its arguments fails), while
    // `max X: a(X)` will.

    // This corner case prevents generalising aggregator inlining with the current set up.

    visitDepthFirst(program, [&](const AstAggregator& aggr) {
        visitDepthFirst(aggr, [&](const AstAtom& subatom) {
            if (program.getRelation(subatom.getName())->isInline()) {
                report.addError("Inlined relation appears in aggregator", subatom.getSrcLoc());
            }
        });
    });
}

// Check that type, relation, component names are disjoint sets.
void AstSemanticChecker::checkNamespaces(ErrorReport& report, const AstProgram& program) {
    std::map<std::string, AstSrcLocation> names;

    // Find all names and report redeclarations as we go.
    for (const auto& type : program.getTypes()) {
        const std::string name = toString(type->getName());
        if (names.count(name)) {
            report.addError("Name clash on type " + name, type->getSrcLoc());
        } else {
            names[name] = type->getSrcLoc();
        }
    }

    for (const auto& rel : program.getRelations()) {
        const std::string name = toString(rel->getName());
        if (names.count(name)) {
            report.addError("Name clash on relation " + name, rel->getSrcLoc());
        } else {
            names[name] = rel->getSrcLoc();
        }
    }

    // Note: Nested component and instance names are not obtained.
    for (const auto& comp : program.getComponents()) {
        const std::string name = toString(comp->getComponentType().getName());
        if (names.count(name)) {
            report.addError("Name clash on component " + name, comp->getSrcLoc());
        } else {
            names[name] = comp->getSrcLoc();
        }
    }

    for (const auto& inst : program.getComponentInstantiations()) {
        const std::string name = toString(inst->getInstanceName());
        if (names.count(name)) {
            report.addError("Name clash on instantiation " + name, inst->getSrcLoc());
        } else {
            names[name] = inst->getSrcLoc();
        }
    }
}

bool AstExecutionPlanChecker::transform(AstTranslationUnit& translationUnit) {
    RelationSchedule* relationSchedule = translationUnit.getAnalysis<RelationSchedule>();
    RecursiveClauses* recursiveClauses = translationUnit.getAnalysis<RecursiveClauses>();

    for (const RelationScheduleStep& step : relationSchedule->schedule()) {
        const std::set<const AstRelation*>& scc = step.computed();
        for (const AstRelation* rel : scc) {
            for (const AstClause* clause : rel->getClauses()) {
                if (!recursiveClauses->recursive(clause)) {
                    continue;
                }
                if (!clause->getExecutionPlan()) {
                    continue;
                }
                int version = 0;
                for (const AstAtom* atom : clause->getAtoms()) {
                    if (scc.count(getAtomRelation(atom, translationUnit.getProgram()))) {
                        version++;
                    }
                }
                if (version <= clause->getExecutionPlan()->getMaxVersion()) {
                    for (const auto& cur : clause->getExecutionPlan()->getOrders()) {
                        if (cur.first >= version) {
                            translationUnit.getErrorReport().addDiagnostic(Diagnostic(Diagnostic::ERROR,
                                    DiagnosticMessage(
                                            "execution plan for version " + std::to_string(cur.first),
                                            cur.second->getSrcLoc()),
                                    {DiagnosticMessage("only versions 0.." + std::to_string(version - 1) +
                                                       " permitted")}));
                        }
                    }
                }
            }
        }
    }
    return false;
}

}  // end of namespace souffle
