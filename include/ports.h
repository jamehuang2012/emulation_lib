/*
 * Copyright (C) 2012 MapleLeaf Software, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ports.h
 *
 * Author: djozwiak@mlsw.biz
 *
 * Description: server socket port numbers
 *
 */

#ifndef __PORTS_H__
#define __PORTS_H__

typedef enum
{
    SOCKET_PORT_LOG = 5000,
    SOCKET_PORT_CALLBACK,
    SOCKET_PORT_BDP_TX,
    SOCKET_PORT_BDP_RX,
    SOCKET_PORT_KR,
    SOCKET_PORT_SVS,

    SOCKET_PORT_WIM = 5010,

    SOCKET_PORT_POWER_MGR = 5020,
    SOCKET_PORT_POWER_MGR_PDM,
    SOCKET_PORT_POWER_MGR_SCM,

    SOCKET_PORT_PAYMENT_MGR = 5030,

    SOCKET_PORT_UPGRADE_MGR = 5040,

    SOCKET_PORT_DIO_MGR = 5050,

    SOCKET_PORT_END
} socket_port;

#endif /* __PORTS_H__ */
