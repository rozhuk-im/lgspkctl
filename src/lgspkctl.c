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
 *
 */


#include <sys/param.h>
#ifdef __linux__ /* Linux specific code. */
#	define _GNU_SOURCE /* See feature_test_macros(7) */
#	define __USE_GNU 1
#endif /* Linux specific code. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h> /* malloc, exit */
#include <unistd.h> /* close, write, sysconf */
#include <fcntl.h> // open
#include <string.h> /* bcopy, bzero, memcpy, memmove, memset, strerror... */
#include <stdio.h> /* snprintf, fprintf */
#include <time.h>
#include <errno.h>
#include <getopt.h>

#include "lgspkctl.h"
#include "json.h"
#include "net/socket.h"
#include "net/socket_address.h"
#include "utils/mem_utils.h"
#include "utils/str2num.h"


#define sstrlen(__str)	((NULL == (__str)) ? 0 : strlen((__str)))


#define LOG_ERR(__error, __descr)					\
	    if (0 != (__error))						\
		fprintf(stderr, "%s , line: %i, error: %i - %s - %s\n",	\
		    __FUNCTION__, __LINE__, (__error), strerror((__error)), (__descr))
#define LOG_EV(__descr)							\
	    fprintf(stdout, "%s , line: %i: %s\n",			\
	        __FUNCTION__, __LINE__, (__descr))
#define LOG_INFO(__descr)						\
	    fprintf(stdout, "%s\n", (__descr))
#define LOG_ERR_FMT(__error, fmt, args...)				\
	    if (0 != (__error))						\
		fprintf(stderr, "%s , line: %i, error: %i - %s" fmt "\n", \
		    __FUNCTION__, __LINE__, (__error), strerror((__error)), ##args)
#define LOG_EV_FMT(fmt, args...)					\
	    fprintf(stdout, "%s , line: %i: " fmt "\n",			\
	        __FUNCTION__, __LINE__, ##args)
#define LOG_INFO_FMT(fmt, args...)					\
	    fprintf(stdout, fmt"\n", ##args)



static int
lg_ctl_pkt_send(const uintptr_t skt, const uint8_t *data, const size_t data_size) {
	int error;
	uint8_t *buf;
	size_t buf_size, off;
	ssize_t ios;

	if (((uintptr_t)-1) == skt)
		return (EINVAL);

	/* Get buf size for packet. */
	error = lg_ctl_pkt_create(data, data_size, NULL, &buf_size);
	if (ENOBUFS != error)
		return (error);
	/* Allocate buf. */
	buf = malloc(buf_size);
	if (NULL == buf)
		return (ENOMEM);
	/* Make packet. */
	error = lg_ctl_pkt_create(data, data_size, buf, &buf_size);
	if (0 != error)
		goto err_out;
	/* Send it. */
	for (off = 0; off < buf_size;) {
		ios = send((int)skt, (buf + off), (buf_size - off),
		    MSG_NOSIGNAL);
		if (-1 == ios) {
			error = errno;
			break;
		}
		off += (size_t)ios;
	}

err_out:
	free(buf);

	return (error);
}

static int
lg_ctl_pkt_recv(const uintptr_t skt, uint8_t *data, const size_t data_size,
    size_t *buf_size_ret) {
	int error;
	const size_t buf_size = (data_size * 2);
	size_t off, rcvd = 0;
	ssize_t ios;
	uint8_t *buf;

	if (((uintptr_t)-1) == skt || NULL == data || 0 == data_size)
		return (EINVAL);

	/* Allocate buf. */
	buf = malloc(buf_size);
	if (NULL == buf)
		return (ENOMEM);
	for (;;) {
		ios = recv((int)skt, (buf + rcvd), (buf_size - rcvd),
		    MSG_NOSIGNAL);
		if (0 >= ios) {
			error = ((-1 == ios) ? errno : ECONNRESET);
			break;
		}
		rcvd += (size_t)ios;
		off = 0;
		error = lg_ctl_pkt_data_get(&off, buf, rcvd,
		    data, data_size, buf_size_ret);
		if (EAGAIN != error)
			break;
		/* Clean up space to receive packet data. */
		rcvd -= off;
		memmove(buf, (buf + off), rcvd);
	}
	free(buf);

	return (error);
}


static struct json_object_element_s *
json_object_element_by_name(struct json_object_element_s *start_elem,
    const char *name, const size_t name_size) {
	struct json_object_element_s *elem;

	if (NULL == start_elem)
		return (NULL);

	for (elem = start_elem; NULL != elem; elem = elem->next) {
		if (elem->name->string_size != name_size ||
		    0 != memcmp(elem->name->string, name, name_size))
			continue;
		return (elem);
	}

	return (NULL);
}

static int
lg_spk_responce_is_ok(const char *ctl_msg, const size_t ctl_msg_size,
    struct json_object_element_s *start_elem) {
	struct json_object_element_s *elem;
	struct json_string_s *string;

	if (NULL == ctl_msg || NULL == start_elem)
		return (0);

	/* Check: msg. */
	elem = json_object_element_by_name(start_elem, "msg", 3);
	if (NULL == elem || elem->value->type != json_type_string)
		return (0);
	string = elem->value->payload;
	if (ctl_msg_size != string->string_size ||
	    0 != memcmp(string->string, ctl_msg, ctl_msg_size))
		return (0);

	/* Check: cmd. */
	/* "cmd": "notibyget" */

	/* Check: result. */
	elem = json_object_element_by_name(start_elem, "result", 6);
	if (NULL == elem)
		return (0);
	switch (elem->value->type) {
	case json_type_string:
		string = elem->value->payload;
		if (string->string_size == 2 &&
		    0 == memcmp(string->string, "ok", 2))
			return (1);
		return (0);
	case json_type_true:
		return (1);
	}

	return (0);
}


static int
lg_spk_array_dump(const char *ctl_msg, const size_t ctl_msg_size,
    const char *array_name, const size_t array_name_size,
    struct json_array_s *array, const int level) {
	struct json_array_element_s *elem;
	struct json_string_s *string;
	struct json_number_s *number;
	const char *tabs = "						";
	const char **array_descr = NULL;
	size_t array_descr_size = 0, num;

	if (NULL == array)
		return (EINVAL);

	if (0 == mem_cmpn_cstr("EQ_VIEW_INFO", ctl_msg, ctl_msg_size) &&
	    0 == mem_cmpn_cstr("ai_eq_list", array_name, array_name_size)) {
		array_descr = lg_ctl_equalisers;
		array_descr_size = nitems(lg_ctl_equalisers);
	} else if (0 == mem_cmpn_cstr("FUNC_VIEW_INFO", ctl_msg, ctl_msg_size) &&
	    0 == mem_cmpn_cstr("ai_func_list", array_name, array_name_size)) {
		array_descr = lg_ctl_functions;
		array_descr_size = nitems(lg_ctl_functions);
	}

	for (elem = array->start; NULL != elem; elem = elem->next) {
		switch (elem->value->type) {
		case json_type_string:
			string = elem->value->payload;
			LOG_INFO_FMT("%.*s%s",
			    level, tabs,
			    string->string);
			break;
		case json_type_number:
			number = elem->value->payload;
			num = str2usize(number->number, number->number_size);
			if (NULL == array_descr ||
			    array_descr_size <= num) {
				LOG_INFO_FMT("%.*s%s",
				    level, tabs,
				    number->number);
				break;
			}
			LOG_INFO_FMT("%.*s%-2s: %s",
			    level, tabs,
			    number->number, array_descr[num]);
			break;
		case json_type_object:
			LOG_INFO_FMT("%.*s%s",
			    level, tabs,
			    "object");
			break;
		case json_type_array:
			LOG_INFO_FMT("%.*s%s",
			    level, tabs,
			    "array");
			break;
		case json_type_true:
			LOG_INFO_FMT("%.*s%s",
			    level, tabs,
			    "true");
			break;
		case json_type_false:
			LOG_INFO_FMT("%.*s%s",
			    level, tabs,
			    "false");
			break;
		case json_type_null:
			LOG_INFO_FMT("%.*s%s",
			    level, tabs,
			    "null");
			break;
		}
	}

	return (0);
}

static int
lg_spk_object_dump(const char *ctl_msg, const size_t ctl_msg_size,
    struct json_object_s *obj, const int level) {
	int error;
	struct json_object_element_s *elem;
	struct json_string_s *string;
	struct json_number_s *number;
	const char *tabs = "						";
	const char **array_descr = NULL;
	size_t array_descr_size = 0, num;

	if (NULL == obj)
		return (EINVAL);

	if (0 == mem_cmpn_cstr("EQ_VIEW_INFO", ctl_msg, ctl_msg_size)) {
		array_descr = lg_ctl_equalisers;
		array_descr_size = nitems(lg_ctl_equalisers);
	} else if (0 == mem_cmpn_cstr("FUNC_VIEW_INFO", ctl_msg, ctl_msg_size)) {
		array_descr = lg_ctl_functions;
		array_descr_size = nitems(lg_ctl_functions);
	}

	for (elem = obj->start; NULL != elem; elem = elem->next) {
		switch (elem->value->type) {
		case json_type_string:
			string = elem->value->payload;
			LOG_INFO_FMT("%.*s%s: %s",
			    level, tabs,
			    elem->name->string, string->string);
			break;
		case json_type_number:
			number = elem->value->payload;
			num = str2usize(number->number, number->number_size);
			if (array_descr_size <= num ||
			    (0 != mem_cmpn_cstr("i_curr_eq",
			    elem->name->string, elem->name->string_size) &&
			    0 != mem_cmpn_cstr("i_curr_func",
			    elem->name->string, elem->name->string_size))) {
				LOG_INFO_FMT("%.*s%s: %s",
				    level, tabs,
				    elem->name->string, number->number);
				    break;
			}
			LOG_INFO_FMT("%.*s%s: %s - %s",
			    level, tabs,
			    elem->name->string, number->number,
			    array_descr[num]);
			break;
		case json_type_object:
			LOG_INFO_FMT("%.*s%s: %s",
			    level, tabs,
			    elem->name->string, "object");
			error = lg_spk_object_dump(ctl_msg, ctl_msg_size,
			    (struct json_object_s*)elem->value->payload,
			    (level + 1));
			if (0 != error)
				return (error);
			break;
		case json_type_array:
			LOG_INFO_FMT("%.*s%s: %s",
			    level, tabs,
			    elem->name->string, "array");
			error = lg_spk_array_dump(ctl_msg, ctl_msg_size,
			    elem->name->string, elem->name->string_size,
			    (struct json_array_s*)elem->value->payload,
			    (level + 1));
			if (0 != error)
				return (error);
			break;
		case json_type_true:
			LOG_INFO_FMT("%.*s%s: %s",
			    level, tabs,
			    elem->name->string, "true");
			break;
		case json_type_false:
			LOG_INFO_FMT("%.*s%s: %s",
			    level, tabs,
			    elem->name->string, "false");
			break;
		case json_type_null:
			LOG_INFO_FMT("%.*s%s: %s",
			    level, tabs,
			    elem->name->string, "null");
			break;
		}
	}

	return (0);
}

static int
lg_spk_handle_responce(const char *ctl_msg, const size_t ctl_msg_size,
    const uint8_t *data, const size_t data_size) {
	int error = EBADMSG;
	struct json_value_s *root;
	struct json_object_s *obj;
	struct json_object_element_s *joe_data;

	if (NULL == ctl_msg || NULL == data || 0 == data_size)
		return (EINVAL);

	root = json_parse(data, data_size);
	obj = root->payload;

	if (0 == lg_spk_responce_is_ok(ctl_msg, ctl_msg_size, obj->start))
		goto err_out;
	joe_data = json_object_element_by_name(obj->start, "data", 4);
	if (NULL == joe_data || json_type_object != joe_data->value->type)
		goto err_out;

	error = lg_spk_object_dump(ctl_msg, ctl_msg_size,
	    (struct json_object_s*)joe_data->value->payload, 1);

err_out:
	free(root);
	return (error);
}











#if 0
#define MAX_ARGS_COUNT	64

typedef struct command_line_options_s {
	int		quiet;
	int		cmd; /* Raw, get, set. */
	const char	*raw_data[MAX_ARGS_COUNT];
	size_t		raw_data_count;


	uint32_t	write_flags;
	int		post_wr_verify;
	off_t		file_offset;
	const char	*chip_name;
	uint32_t	chip_id;
	uint8_t		chip_id_size;
	uint32_t	address;
	size_t		size;
	int		page;
	uint8_t		icsp;
	const char	*db_file_name;
	int		chip_id_check_no_fail;
	int		chip_id_check_disable;
	int		size_error;
	int		size_error_no_warn;
} cmd_opts_t, *cmd_opts_p;


static struct option long_options[] = {
	{ "help",	no_argument,		NULL,	'?'	},
	{ "quiet",	no_argument,		NULL,	'q'	},
	{ "raw",	required_argument,	NULL,	'R'	},
	{ "get",	no_argument,		NULL,	'g'	},
	{ "set",	no_argument,		NULL,	's'	},

	{ "no-pre-unprotect",	no_argument,	NULL,	'u'	},
	{ "no-post-protect",	no_argument,	NULL,	'P'	},
	{ "no-post-verify",	no_argument,	NULL,	'v'	},
	{ "file-offset", required_argument,	NULL,	0	},
	{ "chip",	required_argument,	NULL,	'p'	},
	{ "chip-id",	required_argument,	NULL,	0	},
	{ "addr",	required_argument,	NULL,	0	},
	{ "size",	required_argument,	NULL,	0	},
	{ "page",	required_argument,	NULL,	'c'	},
	{ "icsp",	no_argument,		NULL,	'I'	},
	{ "icsp-vcc",	no_argument,		NULL,	'i'	},
	{ "db-file",	required_argument,	NULL,	'b'	},
	{ "chip-id-check-no-fail", no_argument,	NULL,	'y'	},
	{ "chip-id-check-disable", no_argument,	NULL,	'f'	},
	{ "no-size-error", no_argument,		NULL,	's'	},
	{ "no-size-error-warn", no_argument,	NULL,	'S'	},
	{ NULL,		0,			NULL,	0	}
};

static const char *long_options_descr[] = {
	"			Show help",
	"				Less verboce",
	"<json_data>		Send raw JSON request",
	"			Get - read soundbar props",
	"			Set - write soundbar props",

	"<file_name>		Verify memory",
	"<file_name>		Write memory",
	"		Do NOT disable write-protect before write",
	"		Do NOT enable write-protect after write",
	"		Do NOT verify after write",
	"<offset>		File offset (hex)",
	"<chip>		Specify chip (use quotes)",
	"<chip_id>		Specify chip ID (hex)",
	"<chip_addr>		Start pos for read/verify/write",
	"<size>			Size to read/verify/write (hex)",
	"<page>		Specify memory page type (optional)\n"
	"					Possible values: code, data, config",
	"			Use ICSP (without enabling Vcc)",
	"			Use ICSP",
	"	Do NOT error on chip ID mismatch",
	"	Disable all chip ID checks and reading",
	"		Do NOT error on file size mismatch (only a warning)",
	"	No warning message for file size mismatch (can't combine with -s)",
	NULL
};


static int
cmd_opts_parse(int argc, char **argv, struct option *opts,
    cmd_opts_p cmd_opts) {
	int i, ch, opt_idx;
	char opts_str[1024];


	memset(cmd_opts, 0x00, sizeof(cmd_opts_t));
	cmd_opts->action = -1;
	cmd_opts->post_wr_verify = 1;
	cmd_opts->size_error = 1;

	/* Process command line. */
	/* Generate opts string from long options. */
	for (i = 0, opt_idx = 0;
	    NULL != opts[i].name && (int)(sizeof(opts_str) - 1) > opt_idx;
	    i ++) {
		if (0 == opts[i].val)
			continue;
		opts_str[opt_idx ++] = (char)opts[i].val;
		switch (opts[i].has_arg) {
		case optional_argument:
			opts_str[opt_idx ++] = ':';
			/* PASSTROUTH. */
		case required_argument:
			opts_str[opt_idx ++] = ':';
		}
	}

	opts_str[opt_idx] = 0;
	opt_idx = -1;
	while ((ch = getopt_long_only(argc, argv, opts_str, opts,
	    &opt_idx)) != -1) {
restart_opts:
		switch (opt_idx) {
		case -1: /* Short option to index. */
			for (opt_idx = 0;
			    NULL != opts[opt_idx].name;
			    opt_idx ++) {
				if (ch == opts[opt_idx].val)
					goto restart_opts;
			}
			/* Unknown option. */
			break;
		case 0: /* help */
			return (EINVAL);
		case 1: /* quiet */
			cmd_opts->quiet = 1;
			break;
		case 2: /* raw */
			cmd_opts->raw_data[cmd_opts->raw_data_count] = optarg;
			cmd_opts->raw_data_count ++;
			break;
		case 2: /* write */
		case 3: /* hwtest */
			if (-1 != cmd_opts->action) {
				fprintf(stderr,
				    "write / read / verify / hwtest - can not be "
				    "combined, select one of them.\n");
				return (EINVAL);
			}
			cmd_opts->action = opt_idx;
			cmd_opts->file_name = optarg;
			break;
		case 7: /* no-post-verify */
			cmd_opts->post_wr_verify = 0;
			break;
		case 9: /* chip */
			cmd_opts->chip_name = optarg;
			break;
		case 16: /* db-file */
			cmd_opts->db_file_name = optarg;
			break;
		case 17: /* chip-id-check-no-fail */
			cmd_opts->chip_id_check_no_fail = 1;
			break;
		case 18: /* chip-id-check-disable */
			cmd_opts->chip_id_check_disable = 1;
			break;
		case 19: /* no-size-error */
			cmd_opts->size_error = 0;
			break;
		case 20: /* no-size-error-warn */
			cmd_opts->size_error = 0;
			cmd_opts->size_error_no_warn = 1;
			break;
		default:
			return (EINVAL);
		}
		opt_idx = -1;
	}

	return (0);
}

static void
print_usage(const char *progname, struct option *opts,
    const char **opts_descr) {
	size_t i;
	/*const char *usage =
		PACKAGE_STRING"     "PACKAGE_DESCRIPTION"\n"
		"Usage: %s [options]\n"
		"options:\n";
	fprintf(stderr, usage, basename(progname));*/

	for (i = 0; NULL != opts[i].name; i ++) {
		if (0 == opts[i].val) {
			fprintf(stderr, "	-%s %s\n",
			    opts[i].name, opts_descr[i]);
		} else {
			fprintf(stderr, "	-%s, -%c %s\n",
			    opts[i].name, opts[i].val, opts_descr[i]);
		}
	}
}
#endif

int
main(int argc, char *argv[]) {
	int error = 0;
	uintptr_t skt = (uintptr_t)-1;
	struct sockaddr_storage addr;
	const char *ctl_addr = "172.16.0.227";
	//const char *ctl_addr = "[2001:470:1f15:3d8:9a93:ccff:fece:16a1]";
	uint8_t buf[4096];
	size_t i, buf_size;
#if 0
	cmd_opts_t cmd_opts;


	error = cmd_opts_parse(argc, argv, long_options, &cmd_opts);
	if (0 != error) {
		if (-1 == error)
			return (0); /* Handled action. */
		print_usage(argv[0], long_options, long_options_descr);
		return (error);
	}

	if (optind >= argc) {
		fprintf(stderr, "Expected argument after options\n");
		//exit(EXIT_FAILURE);
	}
#endif



	error = sa_addr_port_from_str(&addr, ctl_addr, sstrlen(ctl_addr));
	if (0 != error) {
		LOG_ERR(error, "sa_addr_port_from_str()");
		goto err_out;
	}
	if (0 == sa_port_get(&addr)) { /* Set def port. */
		sa_port_set(&addr, LG_CTL_TCP_PORT);
	}

	error = skt_connect(&addr, SOCK_STREAM, 0, 0, &skt);
	if (0 != error) {
		LOG_ERR(error, "skt_connect()");
		goto err_out;
	}

	for (i = 0; i < 14; i ++) {
		buf_size = (size_t)snprintf((char*)buf, sizeof(buf),
		    "{\"cmd\": \"get\", \"msg\": \"%s\"}",
		    lg_ctl_msg[i]);

		error = lg_ctl_pkt_send(skt, buf, buf_size);
		if (0 != error) {
			LOG_ERR(error, "lg_ctl_pkt_send()");
			goto err_out;
		}
		error = lg_ctl_pkt_recv(skt, buf, sizeof(buf), &buf_size);
		if (0 != error) {
			LOG_ERR(error, "lg_ctl_pkt_recv()");
			goto err_out;
		}
		//LOG_INFO(buf);
		LOG_INFO(lg_ctl_msg[i]);

		lg_spk_handle_responce(lg_ctl_msg[i], strlen(lg_ctl_msg[i]),
		    buf, buf_size);
		LOG_INFO("");
	}

err_out:
	close((int)skt);

	return (error);
}
