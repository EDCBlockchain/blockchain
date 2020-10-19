#pragma once
#include <openssl/ec.h>
#include <openssl/crypto.h>
#include <openssl/dh.h>
#include <openssl/evp.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ecdsa.h>
#include <openssl/ecdh.h>
#include <openssl/sha.h>
#include <openssl/obj_mac.h>

/** 
 * @file openssl.hpp
 * Provides common utility calls for wrapping openssl c api.
 */
namespace fc 
{
  class path;

    template <typename ssl_type>
    struct ssl_wrapper
    {
        ssl_wrapper(ssl_type* obj):obj(obj) {}
        ssl_wrapper( ssl_wrapper& copy ) = delete;
        ssl_wrapper& operator=( ssl_wrapper& copy ) = delete;

        operator ssl_type*() { return obj; }
        operator const ssl_type*() const { return obj; }
        ssl_type* operator->() { return obj; }
        const ssl_type* operator->() const { return obj; }

        ssl_type* obj;
    };

    #define SSL_TYPE_DECL(name, ssl_type) \
        struct name  : public ssl_wrapper<ssl_type> \
        { \
            name( ssl_type* obj=nullptr ); \
            name( name&& move ); \
            ~name(); \
            name& operator=( name&& move ); \
        };

    SSL_TYPE_DECL(ec_group,       EC_GROUP)
    SSL_TYPE_DECL(ec_point,       EC_POINT)
    SSL_TYPE_DECL(ecdsa_sig,      ECDSA_SIG)
    SSL_TYPE_DECL(bn_ctx,         BN_CTX)
    SSL_TYPE_DECL(evp_cipher_ctx, EVP_CIPHER_CTX)
    SSL_TYPE_DECL(ssl_dh,         DH)

    /** allocates a bignum by default.. */
    struct ssl_bignum : public ssl_wrapper<BIGNUM>
    {
        ssl_bignum() : ssl_wrapper(BN_new()) {}
        ~ssl_bignum() { BN_free(obj); }
    };

    /** Allows to explicitly specify OpenSSL configuration file path to be loaded at OpenSSL library init.
        If not set OpenSSL will try to load the conf. file (openssl.cnf) from the path it was
        configured with what caused serious Keyhotee startup bugs on some Win7, Win8 machines.
        \warning to be effective this method should be used before any part using OpenSSL, especially
        before init_openssl call
    */
    void store_configuration_path(const path& filePath);
    int init_openssl();

} // namespace fc
