#pragma once

#ifdef _MSC_VER
#define LPG_USE_RESULT _Check_return_
#else
#define LPG_USE_RESULT __attribute__((warn_unused_result))
#endif
