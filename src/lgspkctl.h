/*-
 * Copyright (c) 2019 - 2020 Rozhuk Ivan <rozhuk.im@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Rozhuk Ivan <rozhuk.im@gmail.com>
 * Original reverse: https://github.com/google/python-temescal/blob/master/temescal/__init__.py
 */


#ifndef __LG_SPK_CONTROL_PROTO_H__
#define __LG_SPK_CONTROL_PROTO_H__

#include <sys/types.h>
#include <inttypes.h>

#include <stdlib.h>
#include <openssl/aes.h> /* Requires: -lcrypto from OpenSSL/LibreSSL. */


#define LG_AES_IV_SIZE		AES_BLOCK_SIZE
/* "\'%^Ur7gy$~t+f)%@" */
static const uint8_t lg_aes_iv[LG_AES_IV_SIZE] = {
	0x27, 0x25, 0x5e, 0x55, 0x72, 0x37, 0x67, 0x79,
	0x24, 0x7e, 0x74, 0x2b, 0x66, 0x29, 0x25, 0x40
};

#define LG_AES_KEY_SIZE		32
/* "T^&*J%^7tr~4^%^&I(o%^!jIJ__+a0 k" */
static const uint8_t lg_aes_key[LG_AES_KEY_SIZE] = {
	0x54, 0x5e, 0x26, 0x2a, 0x4a, 0x25, 0x5e, 0x37,
	0x74, 0x72, 0x7e, 0x34, 0x5e, 0x25, 0x5e, 0x26,
	0x49, 0x28, 0x6f, 0x25, 0x5e, 0x21, 0x6a, 0x49,
	0x4a, 0x5f, 0x5f, 0x2b, 0x61, 0x30, 0x20, 0x6b
};
#define LG_CTL_TCP_PORT		9741

#define LG_CTL_PKT_HDR_MAGIC	0x10
typedef struct lg_ctl_pkt_hdr_s {
	uint8_t		magic;		/* LG_CTL_PKT_HDR_MAGIC. */
	uint32_t	length;		/* Length in network byte order. */
	/* Payload... */
} __attribute__((__packed__)) lg_ctl_pkt_hdr_t, *lg_ctl_pkt_hdr_p;


static const char *lg_ctl_msg[] = {
	"EQ_VIEW_INFO",
	"SPK_LIST_VIEW_INFO",
	"PLAY_INFO",
	"FUNC_VIEW_INFO",
	"SETTING_VIEW_INFO",
	"PRODUCT_INFO",
	"C4A_SETTING_INFO",
	"RADIO_VIEW_INFO",
	"SHARE_AP_INFO",
	"UPDATE_VIEW_INFO",
	"BUILD_INFO_DEV",
	"OPTION_INFO_DEV",
	"MAC_INFO_DEV",
	"MEM_MON_DEV",
	"TEST_DEV",
	"TEST_TONE_REQ",
	"FACTORY_SET_REQ"
};

/* EQ_VIEW_INFO: i_curr_eq, ai_eq_list */
static const char *lg_ctl_equalisers[] = {
	"Standard",
	"Bass",
	"Flat",
	"Boost",
	"Treble and Bass",
	"User",
	"Music",
	"Cinema",
	"Night",
	"News",
	"Voice",
	"ia_sound",
	"Adaptive Sound Control",
	"Movie",
	"Bass Blast",
	"Dolby Atmos",
	"DTS Virtual X",
	"Bass Boost Plus"
};

/* FUNC_VIEW_INFO: i_curr_func, ai_func_list */
static const char *lg_ctl_functions[] = {
	"Wifi",
	"Bluetooth",
	"Portable",
	"Aux",
	"Optical",
	"CP",
	"HDMI",
	"ARC",
	"Spotify",
	"Optical2",
	"HDMI2",
	"HDMI3",
	"LG TV",
	"Mic",
	"Chromecast",
	"Optical/HDMI ARC",
	"LG Optical",
	"FM",
	"USB"
};


static int
lg_ctl_pkt_create(const uint8_t *data, const size_t data_size,
    uint8_t *buf, size_t *buf_size_ret) {
	AES_KEY enc_key;
	const size_t pad_size = (AES_BLOCK_SIZE - (data_size % AES_BLOCK_SIZE));
	const size_t payload_size = (data_size + pad_size);
	const uint32_t payload32n_size = htonl((uint32_t)payload_size);
	uint8_t *plain_buf, iv[AES_BLOCK_SIZE];

	if (NULL != buf_size_ret) {
		(*buf_size_ret) = ((sizeof(lg_ctl_pkt_hdr_t) + payload_size));
	}
	if (NULL == buf)
		return (ENOBUFS); /* Allow delayed mem alloc. */
	if (NULL == data || 0 == data_size || NULL == buf_size_ret)
		return (EINVAL);

	/* Prepare data to crypt: PADd it. */
	plain_buf = malloc(payload_size);
	if (NULL == plain_buf)
		return (ENOMEM);
	memcpy(plain_buf, data, data_size);
	memset((plain_buf + data_size), (uint8_t)pad_size, pad_size);

	/* Write pcaket header: magic + size. */
	buf[0] = LG_CTL_PKT_HDR_MAGIC;
	memcpy((buf + 1), &payload32n_size, sizeof(uint32_t));

	/* Encrypt peyload data. */
	AES_set_encrypt_key(lg_aes_key, (LG_AES_KEY_SIZE * 8), &enc_key);
	memcpy(iv, lg_aes_iv, AES_BLOCK_SIZE);
	AES_cbc_encrypt(plain_buf, (buf + sizeof(lg_ctl_pkt_hdr_t)),
	    payload_size, &enc_key, iv, AES_ENCRYPT);
	free(plain_buf);

	return (0);
}

static int
lg_ctl_pkt_data_get(size_t *buf_off, const uint8_t *buf, const size_t buf_size,
    uint8_t *data, const size_t data_size, size_t *data_size_ret) {
	AES_KEY dec_key;
	size_t pkt_size, pad_size;
	uint32_t payload32n_size;
	uint8_t *ptr, iv[AES_BLOCK_SIZE];

	if (NULL == buf || NULL == buf_off || (*buf_off) >= buf_size)
		return (EINVAL);

	/* Looking for packet header start. */
	ptr = memchr((buf + (*buf_off)), LG_CTL_PKT_HDR_MAGIC,
	    (buf_size - (*buf_off)));
	if (NULL == ptr) {
		(*buf_off) = buf_size;
		return (EAGAIN);
	}
	(*buf_off) = (size_t)(ptr - buf); /* Remember packet offset in buf. */
	ptr ++;

	/* Read packet size from header. */
	if ((buf_size - (size_t)(ptr - buf)) < sizeof(uint32_t))
		return (EAGAIN); /* Not enough data received. */
	memcpy(&payload32n_size, ptr, sizeof(uint32_t));
	pkt_size = ntohl(payload32n_size);
	ptr += sizeof(uint32_t);

	/* Check available data and buffer space. */
	if ((buf_size - (size_t)(ptr - buf)) < pkt_size)
		return (EAGAIN); /* Not enough data received. */
	if (data_size < pkt_size || NULL == data) {
		if (NULL != data_size_ret) {
			(*data_size_ret) = pkt_size;
		}
		return (ENOBUFS); /* Allow delayed mem alloc. */
	}

	(*buf_off) = (size_t)((ptr + pkt_size) - buf); /* Set next packet offset. */

	/* Decrypt peyload data. */
	AES_set_decrypt_key(lg_aes_key, (LG_AES_KEY_SIZE * 8), &dec_key);
	memcpy(iv, lg_aes_iv, AES_BLOCK_SIZE);
	AES_cbc_encrypt(ptr, data, pkt_size, &dec_key, iv, AES_DECRYPT);

	/* Process PADding. */
	pad_size = data[(pkt_size - 1)];
	if (AES_BLOCK_SIZE < pad_size)
		return (EBADMSG); /* Bad payload. */
	/* Zeroize end, we always have space for that. */
	data[(pkt_size - pad_size)] = 0;
	if (NULL != data_size_ret) { /* Decrease PADding size. */
		(*data_size_ret) = (pkt_size - pad_size);
	}

	return (0);
}


#endif /* __LG_SPK_CONTROL_PROTO_H__ */
