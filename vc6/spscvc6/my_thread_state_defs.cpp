// my_thread_state_defs.cpp
///////////////////////////////////////////////////////////
// Damn VC6 REQUIRES that static const class members are
// declared in the header, but defined in a cpp file.
///////////////////////////////////////////////////////////
#include "../../my_concurrent.h"

#ifdef MSVC6

namespace concurrent
{

    //const int FOO::Fooey_ = 5; 
	const LONG STATES::STATE_NONE  = 0;
    const LONG STATES::STATE_AWAKE = 1;
    const LONG STATES::STATE_ASLEEP = 2;
    const LONG STATES::STATE_QUITTING = 4;
    const LONG STATES::STATE_ABORTED = 8;
    const LONG STATES::STATE_QUIT = 16;

};
#endif