/*
 * Copyright (C) 2025 Tymphany. All rights reserved.
 * This source code is the property of Tymphany and contains confidential
 * information. Unauthorized copying, distribution, or disclosure is strictly
 * prohibited.
 */
/**
 * @file   tym-messages.h
 * @brief  Implements serialization, deserialization, and memory management
 *         functions for TymMessage protocol buffer objects.
 */

#ifndef TYM_MESSAGES_H
#define TYM_MESSAGES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tym.pb-c.h"

/**
 * @brief  Serializes a TymMessage object into a buffer.
 *
 * @param  msg              Pointer to the TymMessage object to serialize.
 * @param  serialize_buffer Pointer to the output buffer pointer, which
 *                          will be allocated inside the function.
 * @return size_t           The size of the serialized data.
 */
size_t tym_message_serialize(TymMessage *msg, unsigned char **serialize_buffer);

/**
 * @brief  Deserializes a buffer into a TymMessage object.
 *
 * @param  size               The size of the buffer.
 * @param  deserialize_buffer The buffer containing the serialized data.
 * @return TymMessage*        Pointer to the deserialized TymMessage object.
 */
TymMessage *tym_message_deserialize(size_t         size,
                                    unsigned char *deserialize_buffer);

/**
 * @brief  Releases the memory allocated for a TymMessage object.
 *
 * @param  msg Pointer to the TymMessage object to free.
 */
void tym_message_release(TymMessage *msg);

#ifdef __cplusplus
}
#endif

#endif /* TYM_MESSAGES_H */
