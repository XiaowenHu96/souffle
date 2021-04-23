/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file False.h
 *
 * Defines a class for evaluating conditions in the Relational Algebra
 * Machine.
 *
 ***********************************************************************/

#pragma once

#include "ram/Condition.h"
#include <ostream>

namespace souffle::ram {

/**
 * @class True
 * @brief False value condition
 *
 * Output is "false"
 */
class False : public Condition {
public:
    False* clone() const override {
        return new False();
    }

protected:
    void print(std::ostream& os) const override {
        os << "false";
    }
};

}  // namespace souffle::ram
