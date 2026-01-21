/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License
 * Agreement (SLA) with Amlogic Inc ("Amlogic"). This software may be
 * used only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission
 * from Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TEE_CLIENT_SOCKET_H_
#define TEE_CLIENT_SOCKET_H_

#include <tee_client_api.h>

typedef void* TEEC_SocketHandle;

typedef struct {
	uint32_t magic;
	TEEC_Context teec_ctx;
	uint32_t pipe_id;
	uint32_t pipe_type;
} data_pipe_descriptor_t;

/**
 * TEEC_SocketOpen() - Open a socket handle for communication
 *
 * @param flag, reserved for future
 *
 * @param handle, out parameter return a socket handle
 *
 * @return TEEC_SUCCESS on success
 * @return other value is failed
 */
TEEC_Result TEEC_SocketOpen(int flag, TEEC_SocketHandle *handle);

/**
 * TEEC_SocketClose() - Close the socket handle that opened by TEEC_SocketOpen()
 *
 * @param handle, the socket handle
 *
 * @return TEEC_SUCCESS on success
 * @return other value is failed
 */
TEEC_Result TEEC_SocketClose(TEEC_SocketHandle handle);

/**
 * TEEC_SocketRead() - Read data from the socket handle into the buf
 *
 * @param handle, the socket handle
 *
 * @param buf, the read buffer pointer
 *
 * @param len, in/out parameter, the read buffer length, return the number of bytes read
 *
 * @return TEEC_SUCCESS on success
 * @return other value is failed
 */
TEEC_Result TEEC_SocketRead(TEEC_SocketHandle handle, void *buf, size_t *len);

/**
 * TEEC_SocketWrite() - Write data to the socket handle from the buf
 *
 * @param handle, the socket handle
 *
 * @param buf, the write buffer pointer
 *
 * @param len, in/out parameter, the write buffer length, return the number of bytes write
 *
 * @return TEEC_SUCCESS on success
 * @return other value is failed
 */
TEEC_Result TEEC_SocketWrite(TEEC_SocketHandle handle, const void *buf, size_t *len);

/**
 * TEEC_SocketListen() - Start listen
 *
 * @backlog, max connection count
 *
 * @return TEEC_SUCCESS on success
 * @return other value is failed
 */
TEEC_Result TEEC_SocketListen(uint32_t backlog);

/**
 * TEEC_SocketAccept() - Accept a socket handle for communication
 *
 * @param handle, out parameter return a socket handle
 *
 * @return TEEC_SUCCESS on success
 * @return other value is failed
 */
TEEC_Result TEEC_SocketAccept(TEEC_SocketHandle *handle);

#endif
