#ifndef DBOX_STORAGE_H
#define DBOX_STORAGE_H

#include "index-storage.h"
#include "mailbox-list-private.h"

#define DBOX_STORAGE_NAME "dbox"
#define DBOX_SUBSCRIPTION_FILE_NAME ".dbox-subscriptions"
#define DBOX_UIDVALIDITY_FILE_NAME ".dbox-uidvalidity"
#define DBOX_INDEX_PREFIX "dovecot.index"

#define DBOX_MAILDIR_NAME "dbox-Mails"
#define DBOX_GLOBAL_INDEX_PREFIX "dovecot.map.index"
#define DBOX_GLOBAL_DIR_NAME "storage"
#define DBOX_MAIL_FILE_MULTI_PREFIX "m."
#define DBOX_MAIL_FILE_UID_PREFIX "u."
#define DBOX_MAIL_FILE_MULTI_FORMAT DBOX_MAIL_FILE_MULTI_PREFIX"%u"
#define DBOX_MAIL_FILE_UID_FORMAT DBOX_MAIL_FILE_UID_PREFIX"%u"
#define DBOX_GUID_BIN_LEN (128/8)

/* How often to scan for stale temp files (based on dir's atime) */
#define DBOX_TMP_SCAN_SECS (8*60*60)
/* Delete temp files having ctime older than this. */
#define DBOX_TMP_DELETE_SECS (36*60*60)

/* Default rotation settings */
#define DBOX_DEFAULT_ROTATE_SIZE (2*1024*1024)
#define DBOX_DEFAULT_ROTATE_MIN_SIZE (1024*16)
#define DBOX_DEFAULT_ROTATE_DAYS 0
#define DBOX_DEFAULT_MAX_OPEN_FILES 64

/* Flag specifies if the message should be in primary or alternative storage */
#define DBOX_INDEX_FLAG_ALT MAIL_INDEX_MAIL_FLAG_BACKEND

struct dbox_index_header {
	uint32_t map_uid_validity;
	uint32_t highest_maildir_uid;
};

struct dbox_storage {
	struct mail_storage storage;
	union mailbox_list_module_context list_module_ctx;

	/* root path for alt directory */
	const char *alt_dir;
	/* paths for storage directories */
	const char *storage_dir, *alt_storage_dir;
	struct dbox_map *map;

	/* mode/gid to use for new dbox storage files */
	mode_t create_mode;
	gid_t create_gid;

	uoff_t rotate_size, rotate_min_size;
	unsigned int rotate_days;
	unsigned int max_open_files;
	ARRAY_DEFINE(open_files, struct dbox_file *);

	unsigned int sync_rebuild:1;
	unsigned int have_multi_msgs:1;
};

struct dbox_mail_index_record {
	uint32_t map_uid;
};

struct dbox_mailbox {
	struct index_mailbox ibox;
	struct dbox_storage *storage;

	struct maildir_uidlist *maildir_uidlist;
	uint32_t highest_maildir_uid;
	uint32_t map_uid_validity;

	uint32_t dbox_ext_id, dbox_hdr_ext_id, guid_ext_id;

	const char *path, *alt_path;
};

struct dbox_transaction_context {
	struct index_transaction_context ictx;
	union mail_index_transaction_module_context module_ctx;

	uint32_t first_saved_mail_seq;
	struct dbox_save_context *save_ctx;
};

extern struct mail_vfuncs dbox_mail_vfuncs;

void dbox_transaction_class_init(void);
void dbox_transaction_class_deinit(void);

struct mailbox *
dbox_mailbox_open(struct mail_storage *storage, const char *name,
		  struct istream *input, enum mailbox_open_flags flags);

struct mail *
dbox_mail_alloc(struct mailbox_transaction_context *t,
		enum mail_fetch_field wanted_fields,
		struct mailbox_header_lookup_ctx *wanted_headers);

/* Get map_uid for wanted message. */
int dbox_mail_lookup(struct dbox_mailbox *mbox, struct mail_index_view *view,
		     uint32_t seq, uint32_t *map_uid_r);
uint32_t dbox_get_uidvalidity_next(struct mail_storage *storage);

struct mail_save_context *
dbox_save_alloc(struct mailbox_transaction_context *_t);
int dbox_save_begin(struct mail_save_context *ctx, struct istream *input);
int dbox_save_continue(struct mail_save_context *ctx);
int dbox_save_finish(struct mail_save_context *ctx);
void dbox_save_cancel(struct mail_save_context *ctx);

int dbox_transaction_save_commit_pre(struct dbox_save_context *ctx);
void dbox_transaction_save_commit_post(struct dbox_save_context *ctx);
void dbox_transaction_save_rollback(struct dbox_save_context *ctx);

int dbox_copy(struct mail_save_context *ctx, struct mail *mail);

#endif
