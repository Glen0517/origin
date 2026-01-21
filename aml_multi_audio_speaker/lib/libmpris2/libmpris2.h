
/*
 * Copyright (C) 2025 Tymphany. All rights reserved.
 * This source code is the property of Tymphany and contains confidential
 * information. Unauthorized copying, distribution, or disclosure is strictly
 * prohibited.
 */

/**
 * @file    libmpris2.h
 * @brief   MPRIS2 client library API for Cast system
 *          Provides type definitions and function prototypes for MPRIS2 D-Bus integration.
 *
 *          This header defines the main structures and API for initializing, subscribing,
 *          running, and releasing the MPRIS2 client, enabling Cast modules to expose and manage
 *          MPRIS2 interfaces in a unified way.
 */
#ifndef LIBMPRIS2_H
#define LIBMPRIS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mpris2-api.h"

/**
 * @brief Opaque handle for the MPRIS2 client object.
 */
typedef struct Mpris2 Mpris2_t;
/**
 * @brief Opaque handle for the MPRIS2 implementation details.
 */
typedef struct Impl Impl_t;
/**
 * @brief Enumeration for main loop running mode.
 */
typedef enum Mode Mode_e;
/**
 * @brief Enumeration for MPRIS2 method calls.
 */
typedef enum InvokeMethod InvokeMethod_e;
/**
 * @brief Enumeration for MPRIS2 property setting.
 */
typedef enum Property Property_e;

/**
 * @brief Callback type for signal/method subscription.
 *
 * @param obj      The GObject instance emitting the signal or method call.
 * @param context  Context pointer (e.g., GDBusMethodInvocation* or GParamSpec*).
 * @param userdata User data pointer passed to the callback.
 */
typedef void (*subscribe_cb)(void *obj, gpointer context, gpointer userdata);

/**
 * @brief Main loop running mode.
 */
enum Mode {
    MPRIS2_MODE_NON_BLOCK = 0, /**< Run the main loop in non-blocking (threaded) mode. */
    MPRIS2_MODE_BLOCK,         /**< Run the main loop in blocking mode (current thread). */
};

/**
 * @brief Enumeration for MPRIS2 method calls.
 */
enum InvokeMethod {
    METHOD_PLAY,     /**< Invoke the Play method. */
    METHOD_PAUSE,    /**< Invoke the Pause method. */
    METHOD_STOP,     /**< Invoke the Stop method. */
    METHOD_NEXT,     /**< Invoke the Next method. */
    METHOD_PREVIOUS, /**< Invoke the Previous method. */
};

/**
 * @brief Enumeration for MPRIS2 property setting.
 */
enum Property {
    PROP_VOLUME, /**< Volume property. */
};

/**
 * @brief Implementation structure for MPRIS2 client (internal use).
 */
struct Impl {
    GObject         *main_iface;       /**< Skeleton for org.mpris.MediaPlayer2 interface. */
    GObject         *player_iface;     /**< Skeleton for org.mpris.MediaPlayer2.Player interface. */
    GObject         *track_list_iface; /**< Skeleton for org.mpris.MediaPlayer2.TrackList interface. */
    GObject         *playlists_iface;  /**< Skeleton for org.mpris.MediaPlayer2.Playlists interface. */
    GObject         *proxy;            /**< Proxy for org.mpris.MediaPlayer2.Player interface. */
    GDBusConnection *conn;             /**< D-Bus connection used for both skeleton and proxy. */
    GMainLoop       *loop;             /**< Main loop for D-Bus event handling. */
};

/**
 * @brief MPRIS2 client object structure.
 *
 * Provides function pointers for API operations.
 */
struct Mpris2 {
    Impl_t *impl;       /**< Pointer to internal implementation structure. */
    gchar  *owner_name; /**< D-Bus name owner string. */

    /**
     * @brief Initialize the MPRIS2 client, set up D-Bus skeletons and connection.
     * @param priv Mpris2_t object pointer.
     * @param name Service name used for identification.
     */
    void (*Initialize)(Mpris2_t *priv, const gchar *name);

    /**
     * @brief Register a callback for receiving MPRIS2 signals or method calls.
     * @param obj      GObject pointer (skeleton interface).
     * @param signal   Signal or method name to subscribe.
     * @param callback Callback function to handle the event.
     * @param userdata User data pointer passed to the callback.
     */
    void (*Subscribe)(GObject *obj, const gchar *signal, subscribe_cb callback, gpointer userdata);

    /**
     * @brief Invoke a MPRIS2 method (e.g., Play, Pause, Stop, Next, Previous).
     * @param priv Mpris2_t object pointer.
     * @param method Method to invoke (see InvokeMethod_e).
     */
    void (*Invoke)(Mpris2_t *priv, InvokeMethod_e method);

    /**
     * @brief Set a property on the MPRIS2 interface (e.g., volume).
     * @param priv Mpris2_t object pointer.
     * @param prop Property to set (see Property_e).
     * @param value Value to set (as GValue).
     */
    void (*SetProperty)(Mpris2_t *priv, Property_e prop, GValue *value);

    /**
     * @brief Get a property from the MPRIS2 interface (e.g., volume).
     * @param priv Mpris2_t object pointer.
     * @param prop Property to get (see Property_e).
     * @param value Output value (as GValue).
     */
    void (*GetProperty)(Mpris2_t *priv, Property_e prop, GValue *value);

    /**
     * @brief Enter the main loop to process MPRIS2 events (blocking or non-blocking).
     * @param priv Mpris2_t object pointer.
     * @param mode Mode_e, blocking or non-blocking.
     */
    void (*Run)(Mpris2_t *priv, Mode_e mode);

    /**
     * @brief Request to quit the main loop and release resources.
     * @param priv Mpris2_t object pointer.
     */
    void (*Release)(Mpris2_t *priv);
};

/**
 * @brief Create and initialize a MPRIS2 client object.
 *
 * Allocates the Mpris2_t structure and assigns API function pointers.
 *
 * @return Mpris2_t* Pointer to the created MPRIS2 object.
 */
Mpris2_t *Mpris2Instance(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBMPRIS2_H */
