#include "stdafx.h"
#include "cpp_utils.h"

namespace cpp_utils {
	std::string str_toupper(std::string src) {
		std::transform(src.begin(), src.end(), src.begin(), ::toupper);
		return src;
	}
}
