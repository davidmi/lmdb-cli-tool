#include <malloc.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <arpa/inet.h>

#include "mdbx.h"

enum error_codes {
        ERR_ENV_OPEN = 1, /* Error opening env */
        ERR_LMDB, /* Internal LMDB error */
        ERR_INPUT, /* Invalid input */
};

enum action {
        ACTION_UNKNOWN=0,
        ACTION_HELP,
        ACTION_PRINT,
        ACTION_PRINTVALS,
        ACTION_WRITE,
        ACTION_DEL,
};

enum data_mode {
        MODE_STRING = 0,
        MODE_STRING_NULL,
        MODE_HEX,
        MODE_NUM,
        MODE_AUTO
};

struct mdbx_opts {
	enum action action;
	char *key;
	char *value;
	char *db_name;
	char *mdb_env_path;
	enum data_mode key_mode;
	enum data_mode val_mode;
};

struct mdbx_env {
	MDB_env *env;
	MDB_txn *txn;
	MDB_dbi dbi;
};

void sprint_hex(char *s, void *data, int len)
{
	for (int i = 0; i < len; i++) {
		snprintf(s, 3, "%02hhx", ((unsigned char *)data)[i]);
		s += 2;
	}
}

static inline void
print_hex(void *data, int len)
{
	char s[len * 2 + 1];
	sprint_hex(s, data, len);
	printf("%s\n", s);
}

static inline struct decoded_val *
decoded_val_create(int size)
{
	struct decoded_val *h = malloc(sizeof(struct decoded_val));
	*h = (struct decoded_val) {
		(struct MDB_val) {
			.mv_size = size,
			 .mv_data = calloc(size, 1),
		}
	};

	return h;
}

static inline void
decoded_val_destroy(struct decoded_val *x)
{
	free(x->val.mv_data);
	free(x);
}

/*
 * Decodes a string and initialized a struct decoded_val
 * Call decoded_val_destroy() to clean it up.
 */
static inline struct decoded_val *
decoded_val_decode(char *s)
{
	int len;
	void *data;
	struct decoded_val *hex;

	if (s == NULL) {
		return 0;
	}

	len = strlen(s) / 2;
	hex = decoded_val_create(len);
	data = hex->val.mv_data;

	for (int i = 0; i < len; i++) {
		if (sscanf(s, "%02hhx", (unsigned char *)data) != 1) {
			fprintf(stderr, "Failed parsing hex!\n");
			decoded_val_destroy(hex);
			return NULL;
		}
		data++;
		s += 2;
	}

	return hex;
}

struct decoded_val *
decode_num(char *s)
{
	long int parsed;
	int i = 0;
	struct decoded_val *h = decoded_val_create(sizeof(int));

	/* strtol doesn't differentiate between parse error and 0, so I guess
	 * we won't either..  */
	parsed = strtol(s, NULL, 10);
	if (errno == ERANGE) {
		fprintf(stderr, "Integer out of range!\n");
		decoded_val_destroy(h);
		return NULL;
	}

	if (parsed > UINT32_MAX) {
		fprintf(stderr, "Integer too large!\n");
		decoded_val_destroy(h);
		return NULL;
	}

	if (parsed < 0) {
		fprintf(stderr, "Negative integers not allowed!\n");
		decoded_val_destroy(h);
		return NULL;
	}

	i = htonl((uint32_t)parsed);

	memcpy((int *)h->val.mv_data, &i, sizeof(int));
	return h;
}


struct mdbx_opts
parse_args(int argc, char *argv[])
{
	int option_index;
	int c;
	struct mdbx_opts ret = (struct mdbx_opts) {
		0
	};
	static char *names[] = {
		"",
		"help",
		"print",
		"printvals",
		"write"
		"delete"
	};

	static char *short_names[] = {
		"",
		"h",
		"p",
		"a",
		"w",
		"d"
	};

	/*
	 * Get subcommand/action
	 */

	if (argc < 3) {
		return ret;
	}
	for (uint8_t i = 0; i < (sizeof(short_names) / sizeof(char *)); i++) {
		if (strcmp(short_names[i], argv[1]) == 0) {
			ret.action = (enum action)i;
			break;
		}
	}

	for (uint8_t i = 0; i < (sizeof(names) / sizeof(char *)); i++) {
		if (strcmp(names[i], argv[1]) == 0) {
			ret.action = (enum action)i;
			break;
		}
	}

	if (ret.action == 0) {
		fprintf(stderr, "Unknown action %s\n\n", argv[1]);
		return ret;
	}

	optind = 2;

	static struct option long_options[] = {
		{"db-name", no_argument, NULL, 'd'},
		{"key", no_argument, NULL, 'k'},
		{"value", no_argument, NULL, 'v'},
		{"hex-key", no_argument, NULL, 'x'},
		{"num-key", no_argument, NULL, 'n'},
		{"hex-data", no_argument, NULL, 'X'},
		{"num-data", no_argument, NULL, 'N'},
		{NULL, 0, NULL, 0}
	};

	for (;;) {
		option_index = 0;
		c = getopt_long(
		            argc,
		            argv,
		            "hpnNxXd:k:v:",
		            long_options,
		            &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			/*
			 * Return here since if there's a -h anywhere in the
			 * options, we want to print the default help message.
			 * TODO: Action-specific help
			 */
			return (struct mdbx_opts) {
				.action = ACTION_HELP,
			};
			break;
		case 'd':
			ret.db_name = optarg;
			break;
		case 'k':
			ret.key = optarg;
			break;
		case 'v':
			ret.value = optarg;
			break;
		case 'x':
			ret.key_mode = MODE_HEX;
			break;
		case 'X':
			ret.val_mode = MODE_HEX;
			break;
		case 'n':
			ret.key_mode = MODE_NUM;
			break;
		case 'N':
			ret.val_mode = MODE_NUM;
			break;
		default:
			fprintf(stderr, "Unknown option %c\n", c);
			return (struct mdbx_opts) {
				.action = ACTION_UNKNOWN,
			};
		}
	}

	if (optind < argc) {
		ret.mdb_env_path = argv[optind];
	} else {
		fprintf(stderr, "Expected path argument! %d\n", optind);
		return (struct mdbx_opts) {
			0
		};
	}

	return ret;
}

int _print_keys(MDB_cursor *cursor, MDB_val key, enum data_mode val_mode)
{
	MDB_val db_key;
	MDB_val data;
	int err;

	if (key.mv_data == NULL) {
		for (;;) {
			err = mdb_cursor_get(cursor, &db_key, &data, MDB_NEXT);
			if (err == MDB_NOTFOUND) {
				break;
			} else if (err != 0) {
				fprintf(stderr, "Error reading DB! %d\n", err);
				return err;
			}

			switch (val_mode) {
			case MODE_HEX:
				print_hex(db_key.mv_data, db_key.mv_size);
				break;
			case MODE_NUM:
				if (db_key.mv_size == sizeof(uint32_t)) {
					printf("%u\n", ntohl(*((uint32_t *)db_key.mv_data)));
				}
				break;
			case MODE_AUTO:
			case MODE_STRING:
			default:
				fwrite(db_key.mv_data, 1, db_key.mv_size, stdout);
				printf("\n");
			}
		}
	} else {
		db_key = key;
		err = mdb_cursor_get(cursor, &db_key, &data, MDB_SET_KEY);
		if (err == MDB_NOTFOUND) {
			fprintf(stderr, "Key not found!\n");
			return err;
		} else if (err != 0) {
			fprintf(stderr, "Error reading DB! %d\n", err);
			return err;
		}

		switch (val_mode) {
		case MODE_HEX:
			print_hex(data.mv_data, data.mv_size);
			break;
		case MODE_NUM:
			if (data.mv_size == sizeof(uint32_t)) {
				printf("%u\n", ntohl(*((uint32_t *)data.mv_data)));
			}
			break;
		case MODE_AUTO:
		case MODE_STRING:
		default:
			fwrite(data.mv_data, 1, data.mv_size, stdout);
		}
	}

	fflush(stdout);

	return 0;
}

struct decoded_val *
decode(char *val, enum data_mode mode)
{
	struct decoded_val *ret;

	if (val == NULL) {
		fprintf(stderr, "No value to decode provided!\n");
		return NULL;
	}

	switch (mode) {
	case (MODE_HEX):
		ret = decoded_val_decode(val);
		if (ret == NULL) {
			fprintf(stderr, "Failed to decode hex string!\n");
		}
		break;
	case (MODE_NUM):
		ret = decode_num(val);
		if (ret == NULL) {
			fprintf(stderr, "Failed to parse number!\n");
		}
		break;
	case (MODE_STRING):
	case (MODE_STRING_NULL):
	default:
		ret = decoded_val_create(strlen(val) + 1);
		strcpy(ret->val.mv_data, val);
		break;
	}

	return ret;
}

int
print_keys(struct mdbx_env *e, char *key, enum data_mode key_mode, enum data_mode val_mode)
{
	int err, ret;
	struct decoded_val *key_val;
	MDB_cursor *cursor = NULL;
	MDB_txn *txn = e->txn;
	MDB_dbi dbi = e->dbi;

	err = mdb_cursor_open(txn, dbi, &cursor);
	if (err != 0) {
		fprintf(stderr, "Failed to open cursor!\n %d", err);
		return ERR_LMDB;
	}

	if (key != NULL) {
		key_val = decode(key, key_mode);
		if (key_val == NULL) {
			return 1;
		}
		ret = _print_keys(cursor, key_val->val, val_mode);
		decoded_val_destroy(key_val);
	} else {
		ret = _print_keys(cursor, (MDB_val) {
			0
		}, val_mode);
	}

	mdb_cursor_close(cursor);
	return ret;
}

int
print_vals(struct mdbx_env *e)
{
	int err;
	MDB_cursor *cursor = NULL;
	MDB_txn *txn = e->txn;
	MDB_dbi dbi = e->dbi;

	MDB_val db_key;
	MDB_val data;

	err = mdb_cursor_open(txn, dbi, &cursor);
	if (err != 0) {
		fprintf(stderr, "Failed to open cursor!\n %d", err);
		return ERR_LMDB;
	}

	for (;;) {
		err = mdb_cursor_get(cursor, &db_key, &data, MDB_NEXT);
		if (err == MDB_NOTFOUND) {
			break;
		} else if (err != 0) {
			fprintf(stderr, "Error reading DB! %d\n", err);
			return err;
		}

		fwrite(data.mv_data, 1, data.mv_size, stdout);
	}

	mdb_cursor_close(cursor);
	return 0;
}

int delete_key(struct mdbx_env *e, char *key, char *data, enum data_mode key_mode, enum data_mode val_mode)
{
	int err;
	struct decoded_val *key_val;
	struct decoded_val *data_val = NULL;
	MDB_txn *txn = e->txn;
	MDB_dbi dbi = e->dbi;

	key_val = decode(key, key_mode);
	if (key_val == NULL) {
		return 1;
	}

	if (data) {
		data_val = decode(data, val_mode);
		if (data_val == NULL) {
			decoded_val_destroy(key_val);
			return 1;
		}
	}

	/**
	 * mdb_del will ignore val if MDB_DUPSORT is not enabled on the DB
	 * If it is, it will delete the key with the specified value, or
	 * all of them if NULL is specified.
	 */
	err = mdb_del(txn, dbi, &key_val->val, &data_val->val);

	decoded_val_destroy(key_val);
	if (data_val) {
		decoded_val_destroy(data_val);
	}
	return err;
}

int write_val(struct mdbx_env *e, char *key, char *data, enum data_mode key_mode, enum data_mode val_mode)
{
	int err;
	struct decoded_val *key_val;
	struct decoded_val *data_val;
	MDB_txn *txn = e->txn;
	MDB_dbi dbi = e->dbi;

	key_val = decode(key, key_mode);
	if (key_val == NULL) {
		return 1;
	}

	data_val = decode(data, val_mode);
	if (data_val == NULL) {
		decoded_val_destroy(key_val);
		return 1;
	}

	err = mdb_put(txn, dbi, &key_val->val, &data_val->val, 0);

	decoded_val_destroy(key_val);
	decoded_val_destroy(data_val);
	return err;
}

int
mdbx_env_make(char *mdb_env_path, char *db_name, unsigned int env_flags, unsigned int txn_flags, unsigned int dbi_flags, struct mdbx_env *env)
{
	int err;

	*env = (const struct mdbx_env) {
		0
	};

	err = mdb_env_create(&env->env);
	if (err != 0) {
		fprintf(stderr, "Failed to create lmbd env! %d\n", err);
		return ERR_LMDB;
	}

	err = mdb_env_set_maxdbs(env->env, 1024);
	if (err != 0) {
		fprintf(stderr, "Could not set maxdbs!\n");
		return ERR_LMDB;
	}

	err = mdb_env_open(env->env, mdb_env_path, env_flags, 0644);
	if (err != 0) {
		fprintf(stderr, "Could not open mdb env %s! %d\n", mdb_env_path, err);
		mdbx_env_destroy(env, false);
		return ERR_ENV_OPEN;
	}

	err = mdb_txn_begin(env->env, NULL, txn_flags, &env->txn);
	if (err != 0) {
		fprintf(stderr, "Failed to create lmdb txn! %d\n", err);
		mdbx_env_destroy(env, false);
		return ERR_LMDB;
	}

	/* The NULL db contains a list of database names */
	err = mdb_dbi_open(env->txn, db_name, dbi_flags, &env->dbi);
	if (err != 0) {
		fprintf(stderr, "Failed to open db! %d\n", err);
		mdbx_env_destroy(env, false);
		return ERR_LMDB;
	}

	return 0;
}

void mdbx_env_destroy(struct mdbx_env *e, bool commit)
{
	if (commit && e->txn) {
		mdb_txn_commit(e->txn);
	}

	else {
		mdb_txn_abort(e->txn);
	}
	if (e->env) {
		mdb_env_close(e->env);
	}
}

int mdbx_main(int argc, char *argv[])
{
	struct mdbx_opts op = parse_args(argc, argv);
	struct mdbx_env env;
	int err;
	int ret = 0;

	switch (op.action) {
	case ACTION_UNKNOWN:
		ret = 1;
		/* fallthrough */
	case ACTION_HELP:
		printf("mdbx (lMDB eXplorer).\n\n"
		       "USAGE: mdbx ACTION [OPTION [ARG] [OPTION [ARG] ...]]  <lmdb env path>\n\n"
		       "ACTIONS\n" "	w|write\n" "	p|print\n" "	a|printall\n"
		       "	d|delete\n" "	h|help]\n\n"
		       "OPTIONS\n"
		       "	[-d|--db-name] <name>\n"
		       "	[-k|--key] <key> -- string key\n"
		       "	[-N|--num-data] -- print/input data as number\n"
		       "	[-n|--num-key]  -- input key as number\n"
		       "	[-X|--hex-data] -- print/input data as number\n"
		       "	[-x|--hex-key]  -- input key as number\n");
		break;
	case ACTION_PRINT:
		err = mdbx_env_make(op.mdb_env_path, op.db_name, MDB_RDONLY | MDB_NOLOCK, MDB_RDONLY, 0, &env);
		if (err) {
			return err;
		}
		ret = print_keys(&env, op.key, op.key_mode, op.val_mode);
		mdbx_env_destroy(&env, false);
		break;
	case ACTION_PRINTVALS:
		err = mdbx_env_make(op.mdb_env_path, op.db_name, MDB_RDONLY | MDB_NOLOCK, MDB_RDONLY, 0, &env);
		if (err) {
			return err;
		}
		ret = print_vals(&env);
		mdbx_env_destroy(&env, false);
		break;
	case ACTION_WRITE:
		err = mdbx_env_make(op.mdb_env_path, op.db_name, 0, 0, MDB_CREATE, &env);
		if (err) {
			return err;
		}
		ret = write_val(&env, op.key, op.value, op.key_mode, op.val_mode);
		mdbx_env_destroy(&env, true);
		if (ret != 0) {
			fprintf(stderr, "Failed to write value!\n");
		}
		break;
	case ACTION_DEL:
		err = mdbx_env_make(op.mdb_env_path, op.db_name, 0, 0, 0, &env);
		if (err) {
			return err;
		}
		ret = delete_key(&env, op.key, NULL, op.key_mode, op.val_mode);
		mdbx_env_destroy(&env, true);
		if (ret != 0) {
			fprintf(stderr, "Failed to delete key!\n");
		}
		break;
	}

	return ret;
}
