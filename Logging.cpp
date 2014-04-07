#include "Logging.h"

namespace signals {

	// let the signals log be quiet by default -- if you really want to read it, 
	// enable it explicitly via the command line
	logger::LogChannel signalslog("signalslog", "[signals] ", logger::Quiet);

} // namespace signals

