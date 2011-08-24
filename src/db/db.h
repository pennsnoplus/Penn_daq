#ifndef DB_H
#define DB_H

#include <stdint.h>

#include "json.h"
#include "db_types.h"

#define DB_ADDRESS "localhost"
#define DB_PORT "5984"
#define DB_USERNAME ""
#define DB_PASSWORD ""
#define DB_BASE_NAME "penndb1"
#define DB_VIEWDOC "_design/view_doc/_view"

int parse_fec_hw(JsonNode* value,mb_t* mb);
int parse_fec_debug(JsonNode* value,mb_t* mb);
int parse_test(JsonNode* test,mb_t* mb,int cbal,int zdisc,int all);
int swap_fec_db(mb_t* mb);
int parse_mtc(JsonNode* value,mtc_t* mtc);

int post_debug_doc(int crate, int card, JsonNode* doc);
int post_debug_doc_with_id(int crate, int card, char *id, JsonNode* doc);

#endif

