#pragma once

// Shared configuration bridge for Hydromatic projects.
//
// This header lets the Status-display firmware reuse the same config, secrets,
// and MQTT definitions as Hydromatic-Server. Add the Hydromatic-Server
// include directory to your build flags (for example: -I../Hydromatic-Server/include),
// then provide config.h, secrets.h, and mqtt.h there.

#if defined(HYDROMATIC_CONFIG_PATH)
#include HYDROMATIC_CONFIG_PATH
#elif __has_include("config.h")
#include "config.h"
#elif __has_include("config.example.h")
#include "config.example.h"
#endif

#if defined(HYDROMATIC_SECRETS_PATH)
#include HYDROMATIC_SECRETS_PATH
#elif __has_include("secrets.h")
#include "secrets.h"
#endif

#if defined(HYDROMATIC_MQTT_PATH)
#include HYDROMATIC_MQTT_PATH
#elif __has_include("mqtt.h")
#include "mqtt.h"
#endif
