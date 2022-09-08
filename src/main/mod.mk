#
# mod.mk
#
# Copyright (C) 2010 Creytiv.com
#

SRCS	+= main/init.c
SRCS	+= main/main.c
SRCS	+= main/method.c

ifneq ($(USE_OPENSSL),)
SRCS    += main/openssl.c
endif
ifeq ($(USE_MBEDTLS),yes)
SRCS    += main/mbedtls.c
endif
