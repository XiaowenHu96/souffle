/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParserDriver.h
 *
 * Defines the parser driver.
 *
 ***********************************************************************/

#pragma once

#include "RelationTag.h"
#include "ast/Clause.h"
#include "ast/Component.h"
#include "ast/ComponentInit.h"
#include "ast/Directive.h"
#include "ast/FunctorDeclaration.h"
#include "ast/Pragma.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/SubsetType.h"
#include "ast/TranslationUnit.h"
#include "ast/Type.h"
#include "parser/SrcLocation.h"
#include "parser/parser.hh"
#include "reports/DebugReport.h"
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace souffle {

using yyscan_t = void*;

struct scanner_data {
    SrcLocation yylloc;

    /* Stack of parsed files */
    std::string yyfilename;
};

class ParserDriver {
public:
    virtual ~ParserDriver() = default;

    Own<AstTranslationUnit> translationUnit;

    void addRelation(Own<AstRelation> r);
    void addFunctorDeclaration(Own<AstFunctorDeclaration> f);
    void addDirective(Own<AstDirective> d);
    void addType(Own<AstType> type);
    void addClause(Own<AstClause> c);
    void addComponent(Own<AstComponent> c);
    void addInstantiation(Own<AstComponentInit> ci);
    void addPragma(Own<AstPragma> p);

    void addIoFromDeprecatedTag(AstRelation& r);
    Own<AstSubsetType> mkDeprecatedSubType(AstQualifiedName name, AstQualifiedName attr, SrcLocation loc);

    std::set<RelationTag> addReprTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addDeprecatedTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addTag(RelationTag tag, std::vector<RelationTag> incompatible, SrcLocation tagLoc,
            std::set<RelationTag> tags);

    bool trace_scanning = false;

    Own<AstTranslationUnit> parse(
            const std::string& filename, FILE* in, ErrorReport& errorReport, DebugReport& debugReport);
    Own<AstTranslationUnit> parse(
            const std::string& code, ErrorReport& errorReport, DebugReport& debugReport);
    static Own<AstTranslationUnit> parseTranslationUnit(
            const std::string& filename, FILE* in, ErrorReport& errorReport, DebugReport& debugReport);
    static Own<AstTranslationUnit> parseTranslationUnit(
            const std::string& code, ErrorReport& errorReport, DebugReport& debugReport);

    void warning(const SrcLocation& loc, const std::string& msg);
    void error(const SrcLocation& loc, const std::string& msg);
    void error(const std::string& msg);
};

}  // end of namespace souffle

#define YY_DECL yy::parser::symbol_type yylex(souffle::ParserDriver& driver, yyscan_t yyscanner)
YY_DECL;
