// Copyright (c) Gurux Ltd

#include <stdlib.h>	// malloc and free needs this or error is generated
#include <stdio.h>
#include <string.h>	// string function definitions
#include <unistd.h>	// UNIX standard function definitions
#include <fcntl.h>	// File control definitions
#include <errno.h>	// Error number definitions
#include <termios.h>	// POSIX terminal control definitions
#include <time.h>	// time calls

#include "connection.h"

void con_initializeBuffers(connection* connection, int size) {
    if (size == 0)
        bb_clear(&connection->data);
    else
        bb_capacity(&connection->data, size);
}

void con_init(connection* con, GX_TRACE_LEVEL trace) {
    con->trace = trace;
    con->waitTime = 5000;	// Reply wait time is 5 seconds.
    con->comPort = -1;
    con->socket = -1;
    con->receiverThread = -1;
    con->closing = 0;
    bb_init(&con->data);
    bb_capacity(&con->data, 500);
}

void con_close(connection* con) {
    if (con->comPort != -1) {
        if (close(con->comPort) < 0)
            printf("Failed to close port.\r\n");
        con->comPort = -1;
    }
    con->socket = -1;
    bb_clear(&con->data);
    con->closing = 0;
    con_initializeBuffers(con, 0);
}
