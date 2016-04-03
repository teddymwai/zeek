%include binpac.pac
%include bro.pac

%extern{
	#include "types.bif.h"
	#include "events.bif.h"
%}

analyzer DCE_RPC withcontext {
	connection : DCE_RPC_Conn;
	flow       : DCE_RPC_Flow;
};

connection DCE_RPC_Conn(bro_analyzer: BroAnalyzer) {
	upflow   = DCE_RPC_Flow(true);
	downflow = DCE_RPC_Flow(false);
};

%include dce_rpc-protocol.pac

# Now we define the flow:
flow DCE_RPC_Flow(is_orig: bool) {
	flowunit = DCE_RPC_PDU(is_orig) withcontext(connection, this);
};

%include endpoint-atsvc.pac
%include endpoint-epmapper.pac
%include dce_rpc-analyzer.pac
