#pragma once

#include <tl/int_types.hpp>
#include <tl/span.hpp>

namespace tg
{

char* checkCompileErrors(u32 shad, tl::Span<char> buffer);
char* checkLinkErrors(u32 prog, tl::Span<char> buffer);

}
