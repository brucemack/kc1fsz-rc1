#include "TxControl.h"

namespace kc1fsz {

TxControl::TxControl(Clock& clock, Tx& tx)
:   _clock(clock),
    _tx(tx)
{
}

void TxControl::run() { 
}


}
