// DO NOT COMPILE ME!!!

#include "Includes-inl.h"
#include "Wrong-inl.h"

#include "common/base/Base.h"
#include "StringUtil.h"

#include <foo/c++14/bar.hpp>

#include <string>
#include <cstdlib>

// Should generate include order error
#include "Includes.hpp"