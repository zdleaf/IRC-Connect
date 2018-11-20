# IRC Connect

	IRC Connect - zincldn.co.uk

	Basic IRC connection and client on linux using BSD sockets.

	Receives 512 bytes at a time, splitting into lines delimited by CR-LF. If we have a partial line at the end of recv() this is kept in memory to prefix to the next recv().
