// SslTest.cpp
#include "SslTest.h"
#include <mbedtls/error.h>
#include <mbedtls/platform.h>

// Static member definition
mbedtls_ssl_session* SslTest::cached_session = nullptr;
mbedtls_entropy_context SslTest::entropy;
mbedtls_ctr_drbg_context SslTest::ctr_drbg;


bool SslTest::runTests() {
    Logger::info("Starting SSL stack tests");
    
    // Record initial memory state to track SSL overhead
    size_t initial_heap = ESP.getFreeHeap();
    Logger::info("Initial free heap: " + String(initial_heap) + " bytes");

    // First test: Can we load the certificate into memory?
    if (!testCertificateLoading()) {
        Logger::error("Certificate loading test failed");
        return false;
    }
    Logger::info("Certificate loading test passed");

    // Second test: Do we have enough memory for multiple SSL connections?
    if (!testMemoryUsage()) {
        Logger::error("Memory usage test failed");
        return false;
    }
    Logger::info("Memory usage test passed");

    // Final test: Can we perform a real SSL handshake?
    if (!testSslHandshake(true)) {  // Explicitly pass boolean
        Logger::error("SSL handshake test failed");
        return false;
    }
    Logger::info("SSL handshake test passed");

    // Check final memory state for leaks
    size_t final_heap = ESP.getFreeHeap();
    Logger::info("Final free heap: " + String(final_heap) + " bytes");
    size_t memory_used = initial_heap - final_heap;
    Logger::info("SSL testing used " + String(memory_used) + " bytes");

    return true;
}

bool SslTest::testSslHandshake(bool use_session_cache) {
    WiFiClientSecure client;
    
    Logger::info("Starting SSL handshake test");
    
    // Use getRootCAChain() instead of direct reference
    client.setCACert(getRootCAChain());
    client.setTimeout(SSL_TIMEOUT / 1000);
    
    // Note: Direct session management might not be supported 
    // You may need to use mbedtls functions directly or modify WiFiClientSecure
    const char* test_host = "valid-isrgrootx2.letsencrypt.org";
    const uint16_t test_port = 443;

    unsigned long start = millis();
    
    if (!client.connect(test_host, test_port)) {
        Logger::error("SSL connection failed");
        return false;
    }

    Logger::info("SSL handshake completed in " + String(millis() - start) + "ms");
    
    // Simple HTTP request
    client.println("HEAD / HTTP/1.1");
    client.println("Host: " + String(test_host));
    client.println("Connection: close");
    client.println();
    
    String response = client.readStringUntil('\n');
    Logger::info("Received response: " + response);
    
    client.stop();
    return true;
}

bool SslTest::testCertificateLoading() {
    WiFiClientSecure client;
    
    try {
        // Use the function to get the certificate
        client.setCACert(getLetsEncryptRootCA());
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to load certificate: " + String(e.what()));
        return false;
    }
}


bool SslTest::testMemoryUsage() {
    size_t initial_heap = ESP.getFreeHeap();
    
    // Test with multiple clients to ensure we have headroom
    const int NUM_TEST_CLIENTS = 3;  // We'll need at least 2 for server+MQTT
    WiFiClientSecure* clients[NUM_TEST_CLIENTS];
    
    for (int i = 0; i < NUM_TEST_CLIENTS; i++) {
        clients[i] = new WiFiClientSecure();
        
        if (!clients[i]) {
            Logger::error("Failed to allocate SSL client " + String(i));
            // Clean up any clients we did manage to create
            for (int j = 0; j < i; j++) {
                delete clients[j];
            }
            return false;
        }
        
        // Use getLetsEncryptRootCA() instead of direct reference
        clients[i]->setCACert(getLetsEncryptRootCA());
        
        size_t current_heap = ESP.getFreeHeap();
        Logger::info("Heap after client " + String(i) + ": " + String(current_heap));
        
        // Check if we're getting too low on memory
        if (current_heap < MIN_FREE_HEAP) {
            Logger::error("Insufficient heap remaining: " + String(current_heap));
            for (int j = 0; j <= i; j++) {
                delete clients[j];
            }
            return false;
        }
    }
    
    // Clean up all test clients
    for (int i = 0; i < NUM_TEST_CLIENTS; i++) {
        delete clients[i];
    }
    
    // Check for memory leaks
    size_t final_heap = ESP.getFreeHeap();
    size_t leak_check = initial_heap - final_heap;
    
    if (leak_check > 1024) {  // Allow some small variations
        Logger::warning("Possible memory leak detected: " + String(leak_check) + " bytes");
    }
    
    return true;
}

bool SslTest::prewarmConnection(const char* host, uint16_t port) {
    Logger::info("Pre-warming SSL connection to " + String(host));
    
    // Initialize mbedTLS contexts if not already done
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    // Seed the random number generator
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, 
                                    mbedtls_entropy_func, 
                                    &entropy, 
                                    nullptr, 
                                    0);
    if (ret != 0) {
        Logger::error("Failed to seed RNG: " + String(ret));
        return false;
    }

    // Manually create SSL context for low-level operations
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);

    // Configure SSL 
    ret = mbedtls_ssl_config_defaults(&conf, 
                                      MBEDTLS_SSL_IS_CLIENT, 
                                      MBEDTLS_SSL_TRANSPORT_STREAM, 
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        Logger::error("SSL config defaults failed: " + String(ret));
        return false;
    }

    // Create and parse the root CA certificate chain
    mbedtls_x509_crt root_crt;
    mbedtls_x509_crt_init(&root_crt);
    
    // Use getRootCAChain() instead of direct reference
    ret = mbedtls_x509_crt_parse(&root_crt, 
        reinterpret_cast<const unsigned char*>(getRootCAChain()), 
        strlen(getRootCAChain()) + 1);
    
    if (ret != 0) {
        Logger::error("Failed to parse root CA certificate: " + String(ret));
        return false;
    }

    // Setup certificate verification
    mbedtls_ssl_conf_ca_chain(&conf, &root_crt, nullptr);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    // If we have a cached session, attempt to resume
    if (cached_session) {
        ret = mbedtls_ssl_set_session(&ssl, cached_session);
        if (ret != 0) {
            Logger::warning("Failed to set cached session: " + String(ret));
        }
    }

    // Cleanup
    mbedtls_x509_crt_free(&root_crt);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);

    return true;
}
void SslTest::cleanupSession() {
    if (cached_session) {
        // Properly free the session
        mbedtls_ssl_session_free(cached_session);
        free(cached_session);
        cached_session = nullptr;
    }

    // Clean up additional contexts
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
}

void SslTest::logMbedtlsError(int error_code) {
    char error_string[MAX_ERROR_STRING_SIZE];
    
    // Clear the error string buffer to ensure proper null termination
    memset(error_string, 0, sizeof(error_string));
    
    // Convert the mbedTLS error code to a human-readable string
    mbedtls_strerror(error_code, error_string, MAX_ERROR_STRING_SIZE);
    
    // Log both the error string and the numeric code for debugging
    Logger::error("MbedTLS error: " + String(error_string) + " (code: " + String(error_code) + ")");
    
    // Additional error context based on common error codes
    switch(error_code) {
        case MBEDTLS_ERR_X509_CERT_VERIFY_FAILED:
            Logger::error("Certificate verification failed - check certificate chain");
            break;
        case MBEDTLS_ERR_SSL_WANT_READ:
            Logger::error("SSL operation incomplete - more data needed");
            break;
        case MBEDTLS_ERR_SSL_TIMEOUT:
            Logger::error("SSL operation timed out");
            break;
        case MBEDTLS_ERR_SSL_ALLOC_FAILED:
            Logger::error("SSL memory allocation failed");
            break;
    }
}