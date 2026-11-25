// vi: ts=4:sts=4:sw=4:et:noai:nosi:nu:syn=off

#ifndef __IO_Stream_H__
#define __IO_Stream_H__

typedef struct ios {
    FILE *fp[2];
} IOS;

RS485_init(RS485 *io, FILE *fp) {
    io
}

#endif
