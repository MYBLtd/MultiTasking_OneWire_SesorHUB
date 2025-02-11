// SslTest.h
#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <mbedtls/ssl.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <ETH.h>
#include "Logger.h"
#include "SystemHealth.h"
#include "PreferencesManager.h"
#include "SharedDefinitions.h"
#include "MqttManager.h"
#include "certificates.h"  // Add this to include root CA certificates

class SslTest {
public:
    static bool runTests();
    static bool prewarmConnection(const char* host, uint16_t port);
    static void cleanupSession();
    
private:
    // Single declaration of static members
    static mbedtls_ssl_session* cached_session;
    static mbedtls_entropy_context entropy;
    static mbedtls_ctr_drbg_context ctr_drbg;

    static bool testSslHandshake(bool use_session_cache);
    static bool testCertificateLoading();
    static bool testMemoryUsage();
    static void logMbedtlsError(int error_code);
    
    // Test configuration
    static constexpr int MAX_ERROR_STRING_SIZE = 100;
    static constexpr size_t MIN_FREE_HEAP = 32768;
    static constexpr size_t SSL_OVERHEAD = 16384;
    static constexpr uint32_t SSL_TIMEOUT = 10000;
};
