/*
 * Copyright (C) 2025 Tymphany. All rights reserved.
 * This source code is the property of Tymphany and contains confidential
 * information. Unauthorized copying, distribution, or disclosure is strictly
 * prohibited.
 */

/**
 * @file    tymipc-service.h
 * @brief   Tymipc D-Bus client API for Cast system
 *          Provides initialization, subscription, and message
 *          communication interfaces for Cast modules to interact
 *          with the Tymipc D-Bus server.
 *
 */
#ifndef TYMIPC_SERVICE_H
#define TYMIPC_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration of Tymipc structure
typedef struct _Tymipc Tymipc;

// Forward declaration of Accessory structure
typedef struct _Accessory Accessory;

/**
 * @brief Enum for Tymipc group types.
 */
typedef enum {
    TYMIPC_GROUP_TYPE_AUDIO = 0,    ///< Audio group
    TYMIPC_GROUP_TYPE_BUTTON,       ///< Button group
    TYMIPC_GROUP_TYPE_CONNECTIVITY, ///< Connectivity group
    TYMIPC_GROUP_TYPE_DEVICES,      ///< Devices group
    TYMIPC_GROUP_TYPE_LED,          ///< LED group
    TYMIPC_GROUP_TYPE_SOURCE,       ///< Source group
    TYMIPC_GROUP_TYPE_SYSTEM,       ///< System group
    TYMIPC_GROUP_TYPE_VOICE         ///< Voice group
} TYMIPC_GROUP_TYPE;

/**
 * @brief Callback type for message subscription.
 *
 * @param obj        User object pointer.
 * @param submitter  Name of the message submitter.
 * @param group      Group type.
 * @param content    Message content (serialized).
 * @param size       Size of the message content.
 * @param userdata   User data pointer.
 */
typedef void (*subscribe_cb)(void *obj, const char *submitter, int group,
                             const char *content, int size, void *userdata);

/**
 * @brief Tymipc main structure.
 */
struct _Tymipc {
    Accessory *accessory; ///< Accessory pointer

    /**
     * @brief Initialize the Tymipc instance.
     * @param priv Tymipc instance pointer.
     * @param name Name of the submitter/module.
     */
    void (*Initialize)(Tymipc *priv, const char *name);

    /**
     * @brief Subscribe to a message group.
     * @param priv     Tymipc instance pointer.
     * @param type     Group type to subscribe.
     * @param callback Callback function for received messages.
     */
    void (*Subscribe)(Tymipc *priv, TYMIPC_GROUP_TYPE type,
                      subscribe_cb callback);

    /**
     * @brief Run the Tymipc main loop.
     * @param priv Tymipc instance pointer.
     */
    void (*Run)(Tymipc *priv);

    /**
     * @brief Release the Tymipc instance and resources.
     * @param priv Tymipc instance pointer.
     */
    void (*Release)(Tymipc *priv);

    /**
     * @brief Send a message to a group.
     * @param priv    Tymipc instance pointer.
     * @param group   Group type to send to.
     * @param content Serialized message content.
     * @param size    Size of the message content.
     */
    void (*Send)(Tymipc *priv, TYMIPC_GROUP_TYPE group, const char *content,
                 int size);
};

/**
 * @brief Get the singleton instance of Tymipc.
 * @return Tymipc* Pointer to the Tymipc instance.
 */
Tymipc *Instance();

#ifdef __cplusplus
}
#endif

#endif // TYMIPC_SERVICE_H
