/*
 * Repository:  https://github.com/kingkybel/ThreadUtilities
 * File Name:   test/threadutil_tests.cc
 * Description: Unit tests for thread utilities.
 *
 * Copyright (C) 2024 Dieter J Kybelksties <github@kybelksties.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * @date: 2024-12-20
 * @author: Dieter J Kybelksties
 */
#include "threadutil.h"

#include <gtest/gtest.h>
#include <iostream>
#include <string>

using namespace std;
using namespace util;

class ThreadutilTest : public ::testing::Test
{
    protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

double somefunc(int x, double y)
{
    if(y < 0.0)
        throw std::exception();
    return static_cast<double>(x) + y;
}

TEST_F(ThreadutilTest, future_with_exception_test)
{
    int    x             = 5;
    double y             = 6.4;
    auto   result_future = make_exception_safe_future(somefunc, x, y);
    double result{};
    try
    {
        // Get the result or handle exceptions
        ASSERT_NO_THROW(result = result_future.get());
        ASSERT_EQ(result, 11.4);
    }
    catch(const std::exception& ex)
    {
        FAIL() << "Unexpected exception: " << ex.what() << std::endl;
    }

    y             = -5.0;
    result_future = make_exception_safe_future(somefunc, x, y);
    ASSERT_THROW(result_future.get(), std::exception);
}
