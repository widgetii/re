/**
 * @file main.h  Internal interface to main polling loop
 *
 * Copyright (C) 2010 Creytiv.com
 */


#ifdef __cplusplus
extern "C" {
#endif

#if defined(USE_OPENSSL)
int  openssl_init(void);
#elif defined(USE_MBEDTLS)
int  mbedtls_init(void);
#endif

#ifdef __cplusplus
}
#endif
