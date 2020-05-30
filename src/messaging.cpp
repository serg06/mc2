#include <future>
#include <iostream>
#include <string>

#include "messaging.h"

#include "zmq_addon.hpp"

#include <Windows.h>

namespace msg
{
	zmq::context_t ctx(0);
}
