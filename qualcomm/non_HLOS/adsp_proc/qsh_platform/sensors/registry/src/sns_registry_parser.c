/*=============================================================================
  @file sns_registry_parser.c

  JSON parser for registry and configuration files.

  A complete list of known registry groups (files) is created at boot-up, and
  maintained throughout the life of the SEE.  All read/parsed files and incoming
  write requests are first stored in "temporary" registry group objects, and
  then merged into the "main" registry group list.  This is to ensure parsing
  failures do not corrupt the registry state.

  All write requests require first reading and parsing the applicable file,
  making the requested changes, encoding the result, and writing it back to the
  file system.

  A potential optimization may be to read and parse all registry files at
  initialization, and keep the parsed version in memory.

  PEND: Use registry sensor as argument to PRINTF

  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
  All rights reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*=============================================================================
  Include Files
  ===========================================================================*/
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "sns_hw_info.h"
#ifdef SNS_USES_PLATFORM_FS
#include "reverserpc_arm.h"
#endif
#include "sns_file_service.h"
#include "sns_fw_file_service_internal.h"
#include "sns_printf_int.h"
#include "sns_mem_util.h"
#include "sns_memmgr.h"
#include "sns_types.h"
#include "sns_registry_parser.h"
#include "sns_registry_sensor.h"

/*=============================================================================
  Macros
  ===========================================================================*/

/**
 * When saving a registry group to file, the size of the string to initially
 * allocate. If the buffer turns-out to be too short, we'll re-allocate a
 * buffer twice as large, and try again.  We want this buffer size to be
 * sufficient 99% of the time (and we don't care if we waste memory, as this
 * allocation will only sit on the heap temporarily.
 */
#define SNS_REGISTRY_GRP_LEN_INIT 4000

#define SNS_REG_DEBUG_PRINT_TIME_THRESHOLD_TICKS                               \
  576000 // ticks in 30ms (ie 30*11200)

/* Maximum number of entries of the config file */
#define MAX_CONFIG_ENTRIES 30

#define DEBUG_REGISTRY_VERSION 0

#define SNS_REGISTRY_STRCPY(dst, src) copy_string((dst), (src))

// Use of system time can be disabled by adding USES_REG_DEBUG_NO_TIMESTAMP to
// por.py
#ifndef REG_DEBUG_NO_TIMESTAMP
#define SNS_REGISTRY_GET_TIME() sns_get_system_time()
#else
#define SNS_REGISTRY_GET_TIME() 0ULL
#endif

//#define ENABLE_TS_REG_DEBUG    // to enable registry timestamp logs, by
// default it is enabled. Please comment this to disable the logs
/*=============================================================================
  Type Definitions
  ===========================================================================*/

/* Function to be called on a particular file */
typedef sns_rc (*file_func)(char const *restrict dir,
                            char const *restrict file_name, size_t file_size,
                            bool is_config);

/*=============================================================================
  Static Variable Definitions
  ===========================================================================*/

/* List of all known registry groups */
static sns_list sns_registry_groups;

/* Registry elements to be picked from sns_reg_config file */
static sns_registry_hw *dynamic_hw_info = NULL;

/* Length of dynamic_hw_info */
static uint32_t dynamic_hw_info_len = 0;

/* line number of the registry file being processed */
static uint32_t current_line_number = 0;

/* The config group, frequently accessed */
static sns_registry_group *the_config_group = NULL;

static bool no_write_access = false;
static bool registry_init_complete = false;
static char *configfile_version = NULL;
static char *parsed_reg_files = NULL;
static size_t parsed_reg_files_buf_len = 0;

static char *json_files_2b_reparsed = NULL;
static size_t json_files_2b_reparsed_buf_len = 0;

static char *corrupted_parsed_files = NULL;
static size_t corrupted_parsed_files_buf_len = 0;

static char *json_list_name = NULL;
static char *registry_dir = NULL;
static char *registry_parent_dir = NULL;
static char *registry_grand_parent_dir = NULL;

/*=============================================================================
  External Variable Definitions
  ===========================================================================*/

// All defined in sns_registry_config.c

extern char const config_dir[];
extern char const parsed_file_list_file[];
extern char const corrupted_file_list_file[];
extern char const registry_config_file[];
extern char const current_version_file[];
extern char const current_sensors_list[];
extern const char config_group_name[];

/*=============================================================================
  Forward Declarations
  ===========================================================================*/
static void parsed_file_list_add(char const *filename);
static bool save_registry_file(char const *full_path, void const *data,
                               uint32_t len);

/*=============================================================================
  Static Function Definitions
  ===========================================================================*/

static void copy_string(char **dest, char const *source)
{
  if(NULL != source)
  {
    size_t str_size = strlen(source) + 1;
    if(NULL != *dest)
      sns_free(*dest);
    *dest = sns_malloc(SNS_HEAP_MAIN, str_size);
    if(NULL != *dest)
    {
      sns_strlcpy(*dest, source, str_size);
    }
  }
}

/**
 * Determine how many characters of whitespace/formatting are present from
 * the start of a string.  Does not check for null-terminating character.
 *
 * @param[i] str Character string whose whitespace we wish to skip
 * @param[i] str_len Length of str
 *
 * @return How many of the next characters should be considered whitespace
 *         0 if whitespace continues for str_len characters
 */
static int whitespace_len(char const *str, uint32_t str_len)
{
  uint32_t cidx = 0;

  while(0 != isspace(str[cidx]))
  {
    if('\n' == str[cidx])
    {
      current_line_number++;
    }
    if(cidx >= str_len)
      return 0;
    ++cidx;
  }

  return cidx;
}

/**
 * Given a JSON string representing a data type, determine the associated
 * enum value.
 *
 * @param[i] str Null-terminated JSON string
 *
 * @return True if parsing was successful; false otherwise
 */
static bool set_item_type(sns_registry_item *item, char const *str)
{
  if(0 == strcmp(str, "int"))
    item->type = SNS_REGISTRY_TYPE_INTEGER;
  else if(0 == strcmp(str, "flt"))
    item->type = SNS_REGISTRY_TYPE_FLOAT;
  else if(0 == strcmp(str, "grp"))
    item->type = SNS_REGISTRY_TYPE_GROUP;
  else if(0 == strcmp(str, "str"))
    item->type = SNS_REGISTRY_TYPE_STRING;
  else
  {
    SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Unknown data type '%s'", str);
    item->type = SNS_REGISTRY_TYPE_INVALID;
  }
  return (SNS_REGISTRY_TYPE_INVALID != item->type);
}

/**
 * Given a value read from a JSON file (and item->type) appropriately parse,
 * determine, and set the registry item value.
 *
 * We only create the subgroup if this item is parsed successfully.
 *
 * @param[io] item Temporary registry item to set
 * @param[i] value Item value to parse
 *
 * @return True if parsing was successful; false otherwise
 */
static bool set_item_data(sns_registry_item *item, char const *value)
{
  bool rv = true;
  if(SNS_REGISTRY_TYPE_INTEGER == item->type)
  {
    errno = 0;
    item->data.sint = strtoll(value, NULL, 0);
    if(0 != errno)
    {
      SNS_REG_SPRINTF(ERROR, sns_fw_printf, "stroll error %i (%s)", errno,
                      strerror(errno));
      rv = false;
    }
  }
  else if(SNS_REGISTRY_TYPE_FLOAT == item->type)
  {
    item->data.flt = atof(value);
  }
  else if(SNS_REGISTRY_TYPE_STRING == item->type)
  {
    item->data.str = (char *)value;
  }
  else if(SNS_REGISTRY_TYPE_GROUP == item->type)
  {
    item->data.group = NULL;
  }
  else
  {
    rv = false;
  }

  return rv;
}

/**
 * Update an existing registry item to a new value.  Follow all appropriate
 * versioning requirements.
 *
 * If this item is of type GROUP, and specifies a valid pointer to a nested
 * group, merge that group into the central registry.
 *
 * @param[i] source Name of source JSON file, if any
 * @param[i] item Existing item (within an existing registry group)
 * @param[i] new New value to be applied to the existing item
 * @param[i] sensor Registry Sensor handle to be used to enqueue groups
 *
 * @return
 *  SNS_RC_SUCCESS
 *  If type == GROUP, see sns_registry_group_merge()
 */
static sns_rc update_item(char *source, sns_registry_item *item,
                          sns_registry_item const *new, void *sensor)
{
  sns_rc rv = SNS_RC_SUCCESS;

  if(SNS_REGISTRY_TYPE_INVALID == item->type)
  {
    return SNS_RC_INVALID_TYPE;
  }
  if(0 == item->version || new->version > item->version)
  {
    /* Update the item type */
    item->type = new->type;

    if(SNS_REGISTRY_TYPE_INTEGER == item->type)
      item->data.sint = new->data.sint;
    else if(SNS_REGISTRY_TYPE_FLOAT == item->type)
      item->data.flt = new->data.flt;
    else if(SNS_REGISTRY_TYPE_STRING == item->type)
    {
      SNS_REGISTRY_STRCPY(&item->data.str, new->data.str);
      rv = (NULL != item->data.str) ? SNS_RC_SUCCESS : SNS_RC_NOT_AVAILABLE;
    }

    item->version = new->version;
  }

  if(SNS_REGISTRY_TYPE_GROUP == item->type)
  {
    if(NULL != new->data.group)
    {
      if(NULL != source)
      {
        SNS_REGISTRY_STRCPY(&((sns_registry_group *)new->data.group)->source,
                            source);
      }
      rv = sns_registry_group_merge((sns_registry_group *)new->data.group,
                                    sensor);

      item->data.group = sns_registry_group_find(new->data.group->name);
      rv |= (NULL != item->data.group) ? SNS_RC_SUCCESS : SNS_RC_NOT_AVAILABLE;
    }
    else
    {
      rv = SNS_RC_INVALID_VALUE;
    }
  }

  return rv;
}

/**
 * Parse a tuple, where that tuple is one of:
 * 1) "String":"String"
 *  Set 'key' to the beginning of the first string; 'value' to the second
 *
 * 2) "String":Object
 * 3) "String":Array
 *  Set 'key' to the beginning of the string.
 *
 * Null-terminate all strings, by replacing the closing '"' with '\0'.
 *
 * @param[io] json Beginning of the string to parse
 * @param[i] json_len Lenght of the json string
 * @param[o] key First string with quote symbols removed
 * @param[o] value Second string without quote symbols, or NULL
 *
 * @return Number of bytes processed; 0 upon any syntax error
 * 1) Includes key/value, colon, quotes, and whitespace, and the next character
 * will either be a ',' or '}' 2/3) Includes key, quote symbols, whitespace, and
 * colon.  Next character will be a '{' or '['
 */
static uint32_t parse_pair(char *json, uint32_t json_len, char **restrict key,
                           char **restrict value)
{
  uint32_t cidx = 0;
  char *end_string;
  bool failed = false;

  *value = NULL;
  if(cidx >= json_len || '\"' != json[cidx++])
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "parse_pair: Missing opening quote (%i)",
                   current_line_number);
    failed = true;
  }
  else if(NULL == (end_string = strchr(&json[cidx], '\"')))
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "parse_pair: Missing closing quote (%i)",
                   current_line_number);
    failed = true;
  }
  else
  {
    *key = &json[cidx];
    cidx += (uintptr_t)end_string - (uintptr_t)&json[cidx];
    json[cidx++] = '\0'; // We want to safely use this string later
    cidx += whitespace_len(&json[cidx], json_len - cidx);

    if(cidx >= json_len || ':' != json[cidx++])
    {
      SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                      "parse_pair: Missing colon '%s' %c (%i)", *key,
                      json[cidx], current_line_number);
      failed = true;
    }
    else
    {
      cidx += whitespace_len(&json[cidx], json_len - cidx);
      if(cidx < json_len && '\"' == json[cidx])
      {
        cidx++;
        *value = &json[cidx];
        end_string = strchr(&json[cidx], '\"');
        if(NULL == end_string)
        {
          SNS_REG_PRINTF(ERROR, sns_fw_printf,
                         "parse_pair: Missing closing quote (%i)",
                         current_line_number);
          failed = true;
        }
        else
        {
          cidx += (uintptr_t)end_string - (uintptr_t)&json[cidx];
          json[cidx++] = '\0';
          cidx += whitespace_len(&json[cidx], json_len - cidx);
        }
      }
      else if(cidx < json_len && '{' != json[cidx] && '[' != json[cidx])
      {
        SNS_REG_PRINTF(
            ERROR, sns_fw_printf,
            "parse_pair: Next element is not a quote or opening bracket (%i)",
            current_line_number);
        failed = true;
      }
    }
  }

  return failed ? 0 : cidx;
}

/**
 * Determine whether a configuration file should be loaded, based on whether
 * it has modified since the last time it was parsed.
 *
 * @param[i] config_name Name of the configuration .json file
 * @param[i] modified_name st_mtime as returned from stat()
 * @param[i] save Whether to save the modified_time into the registry
 *
 * @return true if file should be read/parsed; false otherwise
 */
static bool process_config_file(char const *config_name, time_t modified_time,
                                bool save)
{
  sns_registry_item *config_item = NULL;
  bool should_parse = false;

  if(NULL != the_config_group)
  {
    config_item = sns_registry_item_find(the_config_group, config_name);
  }
  if(NULL == config_item || (uint64_t)config_item->data.sint != modified_time)
  {
    if(save)
    {
      sns_registry_group *temp_group =
          sns_registry_group_create(config_group_name);
      sns_registry_item temp_item;

      temp_item.name = (char *)config_name;
      temp_item.version = 0;
      temp_item.type = SNS_REGISTRY_TYPE_INTEGER;
      temp_item.data.sint = modified_time;

      if(NULL == temp_group)
      {
        SNS_REG_SPRINTF(LOW, sns_fw_printf,
                        "config_file: '%s' save=%u parse=%u", config_name, save,
                        should_parse);
        return should_parse;
      }

      sns_registry_item_create(temp_group, &temp_item);
      // make it unwritable to avoid writing to file before all json files are
      // processed
      temp_group->writable = registry_init_complete;
      sns_registry_group_merge(temp_group, NULL);
      sns_registry_group_free(temp_group);
    }
    should_parse = true;
  }
  SNS_REG_SPRINTF(LOW, sns_fw_printf, "config_file: '%s' save=%u parse=%u",
                  config_name, save, should_parse);
  return should_parse;
}

/**
 * Called at the end of registry initialization process to save sns_reg_config
 * to file if necessary
 */
static void config_group_write(void)
{
  if(NULL != the_config_group && !the_config_group->writable)
  {
    the_config_group->writable = true;
    sns_registry_group_save(the_config_group, NULL);
  }
}

/**
 * Called after all parsed files have been processed to update sns_reg_config by
 * changing the timestamp of the json files that need to be reparsed.
 */
static void config_group_update(void)
{
  if(NULL != the_config_group && NULL != json_files_2b_reparsed &&
     0 < strlen(json_files_2b_reparsed))
  {
    char *unprocessed = json_files_2b_reparsed;
    char *json_file = NULL;
    uint32_t num_files_2_reparsed = 0;
    uint32_t num_files_found = 0;
    while(NULL != (json_file = strtok_r(unprocessed, "\n", &unprocessed)))
    {
      sns_registry_item *item;
      sns_list_iter iter;

      num_files_2_reparsed++;
      for(sns_list_iter_init(&iter, (sns_list *)&the_config_group->items, true);
          NULL != (item = sns_list_iter_get_curr_data(&iter));
          sns_list_iter_advance(&iter))
      {
        if(0 == strcmp(json_file, item->name))
        {
          num_files_found++;
          item->data.sint =
              1; // a number that's definitely not the original modified time
          SNS_REG_SPRINTF(LOW, sns_fw_printf, "config_group_update: '%s'",
                          json_file);
        }
      }
    }
    SNS_REG_PRINTF(LOW, sns_fw_printf,
                   "config_group_update: #files=%u #found=%u",
                   num_files_2_reparsed, num_files_found);
  }

  sns_free(json_files_2b_reparsed);
  json_files_2b_reparsed = NULL;
  json_files_2b_reparsed_buf_len = 0;
}

/**
 * Called when a parsed file is empty or corrupted.  If the parsed file was
 * generated from a JSON source it is added to the list of JSON files to be
 * reparsed.  The corrupted file is deleted whether it can be generated from
 * JSON or not.
 */
static void prepare_to_reparse(char *full_path, char *parsed_file_name)
{
  char *source_name_end;
  sns_file_service_internal *file_svc = sns_fs_get_internal_service();
  SNS_REG_SPRINTF(LOW, sns_fw_printf, "prepare_to_reparse: deleting '%s'",
                  full_path);
  file_svc->api->unlink(full_path);

  if(NULL != (source_name_end = strstr(parsed_file_name, "json.")))
  {
    size_t source_name_len = source_name_end + 5 - parsed_file_name;
    char source_name[SNS_REGISTRY_STR_MAX];
    sns_strlcpy(source_name, parsed_file_name, sizeof(source_name));
    source_name[source_name_len - 1] = 0;
    if(NULL == json_files_2b_reparsed)
    {
      json_files_2b_reparsed_buf_len = SNS_REGISTRY_GRP_LEN_INIT;
      json_files_2b_reparsed =
          sns_malloc(SNS_HEAP_MAIN, json_files_2b_reparsed_buf_len);
    }
    if(NULL != json_files_2b_reparsed &&
       NULL == strstr(json_files_2b_reparsed, source_name))
    {
      SNS_REG_SPRINTF(LOW, sns_fw_printf, "prepare_to_reparse: '%s'",
                      source_name);
      if(json_files_2b_reparsed_buf_len <=
         (strlen(json_files_2b_reparsed) + strlen(source_name) + 2))
      {
        char *temp_buf =
            sns_malloc(SNS_HEAP_MAIN, (json_files_2b_reparsed_buf_len << 1));
        if(NULL != temp_buf)
        {
          json_files_2b_reparsed_buf_len <<= 1;
          sns_strlcpy(temp_buf, json_files_2b_reparsed,
                      json_files_2b_reparsed_buf_len);
          sns_free(json_files_2b_reparsed);
          json_files_2b_reparsed = temp_buf;
        }
      }
      if(json_files_2b_reparsed_buf_len >
         (strlen(json_files_2b_reparsed) + strlen(source_name) + 2))
      {
        sns_strlcat(json_files_2b_reparsed, source_name,
                    json_files_2b_reparsed_buf_len);
        sns_strlcat(json_files_2b_reparsed, "\n",
                    json_files_2b_reparsed_buf_len);
      }
      else
      {
        SNS_REG_PRINTF(ERROR, sns_fw_printf, "prepare_to_reparse: OOM");
      }
    }
  }
  else
  {
    if(NULL == corrupted_parsed_files)
    {
      corrupted_parsed_files_buf_len = SNS_REGISTRY_GRP_LEN_INIT;
      corrupted_parsed_files =
          sns_malloc(SNS_HEAP_MAIN, corrupted_parsed_files_buf_len);
    }
    if(NULL != corrupted_parsed_files)
    {
      sns_strlcat(corrupted_parsed_files, parsed_file_name,
                  corrupted_parsed_files_buf_len);
      sns_strlcat(corrupted_parsed_files, "\n", corrupted_parsed_files_buf_len);
    }
  }
}

/**
 * Perform an action for every file found within the given directory.
 *
 * @param[i] dir_path Directory in which to search
 * @param[i] dir_file_list List of files in given directory
 * @param[i] cb_func Call this function for each file found
 * @param[i] is_config Whether the contained are configuration files
 *
 * @return
 *  SNS_RC_SUCCESS
 *  SNS_RC_INVALID_TYPE Directory cannot be found or is not available
 */
static sns_rc foreach_in_dir(char const *restrict dir_path,
                             char *restrict dir_file_list, file_func cb_func,
                             bool is_config)
{
  sns_rc rv = SNS_RC_SUCCESS;
  sns_file_service_internal *file_svc = sns_fs_get_internal_service();
  char *reg_file = NULL;
  char *unprocessed = dir_file_list;
  int num_files_found = 0;
  int num_files_loaded = 0;
  int num_files_added = 0;
  sns_time start_ts = SNS_REGISTRY_GET_TIME();

  SNS_REG_SPRINTF(LOW, sns_fw_printf, "foreach: '%s'", dir_path);

  while(NULL != (reg_file = strtok_r(unprocessed, "\n", &unprocessed)))
  {
    struct stat stat_buf;
    char *carriage_return = strchr(reg_file, '\r');
    if(NULL != carriage_return)
    {
      *carriage_return = 0;
      SNS_REG_SPRINTF(LOW, sns_fw_printf, "foreach: '%s' %X %X", reg_file,
                      (uintptr_t)reg_file, (uintptr_t)carriage_return);
    }
    STR_CONCAT(full_path, total_size, dir_path, "/", reg_file);
    if(0 == file_svc->api->stat(full_path, &stat_buf))
    {
      sns_rc rc = SNS_RC_FAILED;
      num_files_found++;
      if(0 < stat_buf.st_size)
      {
        if(!is_config)
        {
          num_files_loaded++;
          rv |= rc = cb_func(dir_path, reg_file, stat_buf.st_size, false);
          if(SNS_RC_SUCCESS == rc)
          {
            num_files_added++;
            parsed_file_list_add(reg_file);
          }
        }
        else if(process_config_file(reg_file, stat_buf.st_mtime, false))
        {
          num_files_loaded++;
          if(SNS_RC_NOT_AVAILABLE !=
             cb_func(dir_path, reg_file, stat_buf.st_size, true))
          {
            num_files_added++;
            process_config_file(reg_file, stat_buf.st_mtime, true);
          }
        }
      }

      if(!is_config && SNS_RC_SUCCESS != rc)
      {
        prepare_to_reparse(full_path, reg_file);
      }
    }
    else
    {
      SNS_REG_SPRINTF(ERROR, sns_fw_printf, "stat error for '%s' %i (%s)",
                      reg_file, errno, strerror(errno));

      if(!is_config)
      {
        prepare_to_reparse(full_path, reg_file);
      }
    }
  }

  SNS_REG_PRINTF(LOW, sns_fw_printf,
                 "foreach: #found=%i #loaded=%u #added=%i duration=%i",
                 num_files_found, num_files_loaded, num_files_added,
                 (uint32_t)(SNS_REGISTRY_GET_TIME() - start_ts));

  return rv;
}

/**
 * Checks if registry directory is writable
 *
 * @return True if registry directory is writable
 */
static bool test_registry_dir(void)
{
  bool valid_reg_dir = false;
  static char test_string[] = "DIR";
  int test_string_len = strlen(test_string);
  STR_CONCAT(temp_file, file_path_len, registry_dir, "/", test_string);
  sns_file_service_internal *file_svc = sns_fs_get_internal_service();
  int temp_err;
  FILE *file;

  errno = 0;
  file = file_svc->api->fopen(temp_file, "w");
  temp_err = errno;
  if(NULL != file)
  {
    size_t len =
        file_svc->api->fwrite(test_string, sizeof(char), test_string_len, file);
    temp_err = errno;
    file_svc->api->fclose(file);
    if(len == test_string_len)
    {
      valid_reg_dir = true;
    }
  }
  else
  {
    SNS_REG_SPRINTF(HIGH, sns_fw_printf,
                    "test_registry_dir: '%s' not found/writable", registry_dir);
  }
  errno = temp_err;
  return valid_reg_dir;
}

/**
 * validate_registry_path
 * @note
 * Registry folder path should be of form
 * <persit_mount>sensors/registry/registry This fn checks if the path to the
 * registry is in this format
 * @param[i] dir path to registry folder
 *
 * @return True if path is path is valid
 */
static bool validate_registry_path(char const *dir)
{
  const char *reg_path_pattern = "sensors/registry/registry";
  return (NULL != strstr(dir, reg_path_pattern));
}

/**
 * Check if the persist registry directory exists.
 *
 */
static void check_registry_dir(void)
{
  if(validate_registry_path(registry_dir))
  {
    if(test_registry_dir())
    {
      char *ptr;
      SNS_REGISTRY_STRCPY(&registry_parent_dir, registry_dir);
      if(NULL != (ptr = strrchr(registry_parent_dir, '/')))
      {
        *ptr = 0;
        SNS_REGISTRY_STRCPY(&registry_grand_parent_dir, registry_parent_dir);
        if(NULL != (ptr = strrchr(registry_grand_parent_dir, '/')))
        {
          *ptr = 0;
        }
      }
    }
  }
  else if(NULL != strstr(config_dir, "/ramfs"))
  {
    no_write_access = true;
  }
  else
  {
    SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                    "check_registry_dir: invalid path '%s'", registry_dir);
  }
}

/**
 * Read and parse a registry item from a JSON string.
 *
 * This function should consume all text inclusive of the opening and closing
 * brackets.
 *
 * @param[io] group Already created registry group to which to add items/owner
 * @param[i] item_name Previously parsed item name; null-terminated
 * @param[i] json JSON text to parse; first character is the opening bracket
 * @param[i] json_len Length of the json string
 *
 * @return How many characters were consumed during processing;
 *         0 upon any syntax/parsing error
 */
static uint32_t parse_item(sns_registry_group *group, char const *item_name,
                           char *json, uint32_t json_len)
{
  uint32_t cidx = 0;
  uint32_t result = 0;
  sns_registry_item item = (sns_registry_item){
      .name = (char *)item_name, .type = SNS_REGISTRY_TYPE_INVALID};

  if(cidx >= json_len || '{' != json[cidx++])
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "parse_item: Missing opening bracket (%i)",
                   current_line_number);
  }
  else
  {
    do
    {
      char *key, *value;
      uint32_t len;

      cidx += whitespace_len(&json[cidx], json_len - cidx);
      len = parse_pair(&json[cidx], json_len - cidx, &key, &value);
      cidx += len;

      if(0 == len || NULL == key || NULL == value)
      {
        SNS_REG_PRINTF(ERROR, sns_fw_printf,
                       "parse_item: Unable to parse pair (%i)",
                       current_line_number);
        break;
      }
      else
      {
        if(0 == strcmp(key, "type"))
        {
          if(set_item_type(&item, value))
            result |= 1 << 0;
        }
        else if(0 == strcmp(key, "ver"))
        {
          errno = 0;
          item.version = strtol(value, NULL, 0);
          if(0 == errno)
            result |= 1 << 1;
          else
            SNS_REG_SPRINTF(ERROR, sns_fw_printf, "strol error %i (%s)", errno,
                            strerror(errno));
        }
        else if(0 == strcmp(key, "data"))
        {
          // We require that "type" comes before "data" in the JSON file
          if(set_item_data(&item, value))
            result |= 1 << 2;
        }
      }
      cidx += whitespace_len(&json[cidx], json_len - cidx);
    } while(cidx < json_len && ',' == json[cidx] && cidx++);

    if(cidx >= json_len || '}' != json[cidx++])
    {
      SNS_REG_PRINTF(ERROR, sns_fw_printf,
                     "parse_item: Missing closing bracket (%i)",
                     current_line_number);
      cidx = 0;
    }

    if(7 == result)
    {
      if(SNS_REGISTRY_TYPE_GROUP == item.type && NULL == item.data.group)
      {
        STR_CONCAT(full_name, total_size, group->name, ".", item.name);
        item.data.group = sns_registry_group_create(full_name);
        if(NULL != item.data.group)
        {
          item.data.group->fs_size = 1; // Don't write this empty group to file
        }
      }

      // PEND: Check for duplicate item name
      sns_registry_item_create(group, &item);
    }
  }

  return 7 != result ? 0 : cidx;
}

/**
 * Read and parse the JSON text representing a group.
 * From the opening bracket until the closing bracket.
 *
 * @param[i] grp_name Previously parsed group name; null-terminated
 * @param[i] json JSON text read from file
 * @param[i] json_len Length of the json character string
 * @param[o] len How many characters were consumed during processing;
 *               0 upon any syntax/parsing error
 *
 * @return Newly created registry group; NULL upon any error
 */
static sns_registry_group *parse_group(char const *grp_name, char *json,
                                       uint32_t json_len, uint32_t *len)
{
  uint32_t cidx = 0;
  bool failed = false;
  sns_registry_group *group = sns_registry_group_create(grp_name);

  *len = 0;

  if(cidx >= json_len || '{' != json[cidx++])
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "parse_group: Missing opening bracket (%i)",
                   current_line_number);
    failed = true;
  }
  else if(NULL != group)
  {
    do
    {
      char *key, *value;
      uint32_t len;

      cidx += whitespace_len(&json[cidx], json_len - cidx);
      len = parse_pair(&json[cidx], json_len - cidx, &key, &value);
      cidx += len;

      if(0 == len || NULL == key)
      {
        SNS_REG_PRINTF(ERROR, sns_fw_printf,
                       "parse_group: Unable to parse pair (%i)",
                       current_line_number);
        failed = true;
        break;
      }
      else if(NULL != value)
      {
        if(0 == strcmp(key, "owner"))
        {
          SNS_REGISTRY_STRCPY(&group->owner, value);
        }
        else
        {
          SNS_REG_SPRINTF(MED, sns_fw_printf,
                          "parse_group: Parsed unknown pair '%s' '%s' (%i)",
                          key, value, current_line_number);
        }
      }
      else
      {
        if('.' == key[0])
        {
          STR_CONCAT(full_name, total_size, grp_name, key, "");

          sns_registry_item item = (sns_registry_item){
              .name = &key[1], .type = SNS_REGISTRY_TYPE_GROUP, .version = 0};

          item.data.group =
              parse_group(full_name, &json[cidx], json_len - cidx, &len);

          if(NULL != item.data.group && 0 != len)
            sns_registry_item_create(group, &item);

          cidx += len;
        }
        else
        {
          len = parse_item(group, key, &json[cidx], json_len - cidx);
          cidx += len;
        }

        if(0 == len)
        {
          SNS_REG_SPRINTF(LOW, sns_fw_printf,
                          "parse_group: Error parsing item: '%s' (%i)", key,
                          current_line_number);
          failed = true;
          break;
        }
      }
      cidx += whitespace_len(&json[cidx], json_len - cidx);
    } while(',' == json[cidx] && cidx++);

    if(NULL == group->owner)
    {
      SNS_REG_PRINTF(ERROR, sns_fw_printf, "parse_group: Missing group owner");
      failed = true;
    }
    else if(cidx >= json_len || '}' != json[cidx++])
    {
      SNS_REG_PRINTF(ERROR, sns_fw_printf,
                     "parse_group: Missing closing bracket (%i)",
                     current_line_number);
      failed = true;
    }
  }

  if(failed && NULL != group)
  {
    sns_registry_group_free(group);
    group = NULL;
  }

  *len = cidx;
  return group;
}

/**
 * Given a JSON array, parse and determine whether hw_value matches one of the
 * entries.
 *
 * @param[io] json JSON array in which to search
 * @param[i] json_len Length of json string
 * @param[i] hw_value Hardware value to match against the array;
 *
 * @return If match found : Total string length of the array
 *         (includes everything from the opening to closing brackets (inclusive)
 *         Otherwise 0.
 */
static uint32_t check_array(char *restrict json, uint32_t json_len,
                            char const *restrict hw_value)
{
  bool match_success = false;

  uint32_t cidx = 0;

  if(cidx >= json_len || '[' != json[cidx++])
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "check_array: Missing open bracket (%i)",
                   current_line_number);
    cidx = 0;
  }
  else
  {
    do
    {
      char *end_string;
      cidx += whitespace_len(&json[cidx], json_len - cidx);

      if(cidx >= json_len || '\"' != json[cidx++])
      {
        SNS_REG_PRINTF(ERROR, sns_fw_printf,
                       "check_array: Missing open parenthesis (%i)",
                       current_line_number);
        cidx = 0;
        break;
      }

      end_string = strchr(&json[cidx], '\"');
      if(NULL == end_string)
      {
        SNS_REG_PRINTF(ERROR, sns_fw_printf,
                       "check_array: Missing closing parenthesis (%i)",
                       current_line_number);
        cidx = 0;
        break;
      }
      else
      {
        char const *temp = &json[cidx];

        cidx += (uintptr_t)end_string - (uintptr_t)&json[cidx];
        json[cidx++] = '\0';
        cidx += whitespace_len(&json[cidx], json_len - cidx);

        if(NULL != hw_value && 0 == strcasecmp(temp, hw_value))
          match_success |= true;
      }
    } while(cidx < json_len && ',' == json[cidx] && cidx++);
  }

  if(0 != cidx && (cidx >= json_len || ']' != json[cidx++]))
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "check_array: Missing closing bracket (%i)",
                   current_line_number);
    cidx = 0;
  }

  return match_success ? cidx : 0;
}

/**
 * Parse the "config" block of configuration file.
 * This includes everything between the opening and closing brackets (inclusive)
 *
 * @param[i] json JSON text read from file
 * @param[i] json_len Length of the json character string
 * @param[i] hw_info Information regarding this device hardware
 * @param[i] hw_info_len length of the hw_info
 *
 * @return How many characters were consumed during processing;
 *         0 upon any syntax/parsing error
 *         -1 If config file is not applicable to this device
 */
static int32_t parse_config(char *json, uint32_t json_len,
                            sns_registry_hw const hw_info[],
                            uint32_t hw_info_len)
{
  int32_t cidx = 0;
  uint32_t len;
  char *key, *value;

  if(cidx >= json_len || '{' != json[cidx++])
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "parse_config: Missing open bracket (%i)",
                   current_line_number);
    cidx = 0;
  }
  else
  {
    do
    {
      cidx += whitespace_len(&json[cidx], json_len - cidx);

      if('}' == json[cidx])
        break;

      len = parse_pair(&json[cidx], json_len - cidx, &key, &value);
      cidx += len;
      if(0 == len || NULL == key || NULL != value)
      {
        SNS_REG_PRINTF(ERROR, sns_fw_printf, "parse_config: Parsing error");
        cidx = 0;
        break;
      }
      else
      {
#ifdef SNS_USES_PLATFORM_FS
        char *buf = NULL;
#endif
        char const *hw_value = NULL;

        for(int i = 0; i < hw_info_len; i++)
        {
          if(NULL == hw_info[i].name)
          {
            SNS_REG_PRINTF(ERROR, sns_fw_printf,
                           "parse_config: hw_info.name is NULL");
            cidx = 0;
            break;
          }
          else
          {
            if(0 == strcmp(key, hw_info[i].name))
            {
              hw_value = hw_info[i].value;
            }
          }
        }

#ifdef SNS_USES_PLATFORM_FS
        if(NULL == hw_value)
        {
          buf = sns_malloc(SNS_HEAP_MAIN, SNS_REGISTRY_DIR_LEN_MAX);
          if(NULL != buf &&
             0 == sns_registry_get_property(key, buf, SNS_REGISTRY_DIR_LEN_MAX))
          {
            hw_value = buf;
          }
          SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                          "parse_config: Android Prop: '%s' '%s'", key,
                          buf ? buf : "");
        }
#endif

        if(NULL == hw_value)
        {
          SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                          "parse_config: Unknown hw constraint: '%s'", key);
          cidx = 0;
          break;
        }

        len = check_array(&json[cidx], json_len - cidx, hw_value);
        cidx += len;
#ifdef SNS_USES_PLATFORM_FS
        sns_free(buf);
#endif
        if(0 == len)
        {
          SNS_REG_PRINTF(MED, sns_fw_printf,
                         "parse_config: Config file not applicable");
          cidx = -1;
          break;
        }
      }
    } while(cidx < json_len && ',' == json[cidx] && cidx++);
  }

  if(0 < cidx)
  {
    cidx += whitespace_len(&json[cidx], json_len - cidx);
  }
  if(0 < cidx && (cidx >= json_len || '}' != json[cidx++]))
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "parse_config: Missing closing bracket (%i)",
                   current_line_number);
    cidx = 0;
  }
  return cidx;
}

/**
 * Read and parse all text from a registry or configuration JSON file.
 * This includes everything between the opening and closing brackets (inclusive)
 *
 * @param[i] json JSON text read from file
 * @param[i] json_len Length of the json character string
 * @param[o] groups All groups contained within this file
 * @param[o] is_config Set to true if the parsed file is a config file
 *
 * @return
 *  SNS_RC_SUCCESS
 *  SNS_RC_INVALID_VALUE - Parsing/syntax error encountered
 *  SNS_RC_POLICY - Configuration file does not apply to this device
 */
static sns_rc parse_file(char *json, uint32_t json_len, sns_list_iter *groups,
                         bool *is_config)
{
  uint32_t cidx = 0;
  uint32_t len;
  int32_t ret;
  char *key, *value;
  sns_rc rv = SNS_RC_SUCCESS;

  current_line_number = 0;
  *is_config = false;
  cidx = whitespace_len(json, json_len);
  if(cidx >= json_len || '{' != json[cidx++])
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "parse_file: Missing open bracket (%i)",
                   current_line_number);
    rv = SNS_RC_INVALID_VALUE;
  }
  else
  {
    do
    {
      cidx += whitespace_len(&json[cidx], json_len - cidx);

      len = parse_pair(&json[cidx], json_len - cidx, &key, &value);
      if(0 == len || NULL == key || NULL != value)
      {
        rv = SNS_RC_INVALID_VALUE;
        break;
      }

      cidx += len;
      if(0 == strcmp(key, "config"))
      {
        ret = parse_config(&json[cidx], json_len - cidx, dynamic_hw_info,
                           dynamic_hw_info_len);

        *is_config = true;
        if(0 == ret)
        {
          rv = SNS_RC_INVALID_VALUE;
          break;
        }
        else if(-1 == ret)
        {
          rv = SNS_RC_POLICY;
          break;
        }
        else
        {
          cidx += ret;
        }
      }
      else
      {
        sns_registry_group *group =
            parse_group(key, &json[cidx], json_len - cidx, &len);
        if(NULL == group)
        {
          SNS_REG_SPRINTF(ERROR, sns_fw_printf, "Error parsing group: %s (%i)",
                          key, current_line_number);
          rv = SNS_RC_INVALID_VALUE;
          break;
        }
        else
        {
          sns_list_iter_insert(groups, &group->list_entry, true);
          cidx += len;
        }
      }
    } while(cidx < json_len && ',' == json[cidx] && cidx++);
  }

  return rv;
}

/**
 * Load a registry or config file, and apply its data to the central registry.
 *
 * @param[i] dir Directory in which file_name is located
 * @param[i] file_name Null-terminated file name string
 * @param[i] file_size File size as determined by stat()
 *
 * SNS_RC_NOT_AVAILABLE - File system error
 * SNS_RC_INVALID_VALUE - Error loading file
 * SNS_RC_SUCCESS - File loaded successfully
 */
static sns_rc load_file(char const *restrict dir,
                        char const *restrict file_name, size_t file_size,
                        bool is_config)
{
  FILE *file;
  sns_rc rv = SNS_RC_SUCCESS;
  sns_file_service_internal *file_svc = sns_fs_get_internal_service();
  STR_CONCAT(full_path, total_size, dir, "/", file_name);

  sns_time load_file_time = SNS_REGISTRY_GET_TIME();
  sns_time fread_ts = 0, load_file_end_ts = 0;

  if((NULL != strstr(full_path, ".bin")) || (NULL != strstr(full_path, ".BIN")))
  {
    file = file_svc->api->fopen(full_path, "rb");
  }
  else
  {
    file = file_svc->api->fopen(full_path, "r");
  }
  if(NULL == file)
  {
    SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                    "load_file: unable to open '%s' %i (%s)", full_path, errno,
                    strerror(errno));
    rv = SNS_RC_NOT_AVAILABLE;
  }
  else
  {
    size_t read_len = 0;
    sns_rc rc;
    char *buf = sns_malloc(SNS_HEAP_MAIN, file_size);
    if(NULL != buf)
    {
      read_len = file_svc->api->fread(buf, sizeof(uint8_t), file_size, file);
      fread_ts = SNS_REGISTRY_GET_TIME();
    }
    file_svc->api->fclose(file);

    if(0 == read_len)
    {
      SNS_REG_SPRINTF(ERROR, sns_fw_printf, "load_file: fread error %i (%s)",
                      errno, strerror(errno));
      rv = SNS_RC_INVALID_VALUE;
    }
    else
    {
      bool parsed_is_config;
      sns_list groups_list;
      sns_list_iter groups_iter;
      uint32_t group_count;

      sns_list_init(&groups_list);
      sns_list_iter_init(&groups_iter, &groups_list, true);
      rc = parse_file(buf, read_len, &groups_iter, &parsed_is_config);
      sns_list_iter_init(&groups_iter, &groups_list, true);
      group_count = sns_list_iter_len(&groups_iter);

      if(parsed_is_config != is_config)
      {
        SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                        "load_file: config file in invalid location: %s",
                        file_name);
        rv = SNS_RC_INVALID_VALUE;
      }
      else if(SNS_RC_POLICY == rc)
      {
      }
      else if(SNS_RC_SUCCESS != rc || 0 == group_count)
      {
        SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                        "load_file: error parsing file: %s (%i)", file_name,
                        rc);
        rv = SNS_RC_INVALID_VALUE;
      }
      else if(is_config)
      {
        for(sns_list_iter_init(&groups_iter, &groups_list, true);
            NULL != sns_list_iter_curr(&groups_iter);)
        {
          sns_registry_group *temp_group =
              (sns_registry_group *)sns_list_item_get_data(
                  sns_list_iter_remove(&groups_iter));

          SNS_REGISTRY_STRCPY(&temp_group->source, file_name);
          rv |= sns_registry_group_merge(temp_group, NULL);
          sns_registry_group_free(temp_group);
        }
      }
      else if(1 == group_count)
      {
        sns_registry_group *temp_group =
            (sns_registry_group *)sns_list_item_get_data(
                sns_list_iter_remove(&groups_iter));
        char source_name[SNS_REGISTRY_STR_MAX] = {0};
        char const *group_name;
        if(!is_config && NULL != (group_name = strstr(file_name, "json.")))
        {
          // extract source JSON file name from file_name
          size_t source_name_len = group_name + 5 - file_name;
          sns_memscpy(source_name, sizeof(source_name), file_name,
                      strlen(file_name) + 1);
          source_name[source_name_len - 1] = 0;
          group_name += 5; // group_name starts after "json."
        }
        else
        {
          group_name = file_name;
        }

        if(0 < strlen(source_name))
        {
          SNS_REGISTRY_STRCPY(&temp_group->source, source_name);
        }

        if(0 != strcasecmp(group_name, temp_group->name))
        {
          SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                          "Group/filename mismatch: '%s '%s' (%i)",
                          temp_group->name, file_name, rc);
          rv = SNS_RC_INVALID_VALUE;
        }
        else
        {
          temp_group->fs_size = file_size;
          rv = sns_registry_group_merge(temp_group, NULL);
        }
        sns_registry_group_free(temp_group);
      }
      else
      {
        SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                        "load_file: %s - group count = %u", file_name,
                        group_count);
        for(sns_list_iter_init(&groups_iter, &groups_list, true);
            NULL != sns_list_iter_curr(&groups_iter);)
        {
          sns_registry_group *temp_group =
              (sns_registry_group *)sns_list_item_get_data(
                  sns_list_iter_remove(&groups_iter));
          sns_registry_group_free(temp_group);
        }
        rv = SNS_RC_INVALID_VALUE;
      }

      SNS_REG_SPRINTF(LOW, sns_fw_printf,
                      "load_file: parsed '%s' (%i) with result %i", file_name,
                      file_size, rc);
      load_file_end_ts = SNS_REGISTRY_GET_TIME();
      if((load_file_end_ts - load_file_time) >=
         SNS_REG_DEBUG_PRINT_TIME_THRESHOLD_TICKS)
      {
        SNS_REG_PRINTF(HIGH, sns_fw_printf,
                        "load_file, start ts : %x, fread ts %x end ts %x,\
          full operation time %x",
                        load_file_time, fread_ts, load_file_end_ts,
                        (load_file_end_ts - load_file_time));
      }
    }

    sns_free(buf);
  }

  return rv;
}

/**
 * Convert and write an item to the json string; from the item name, to the
 * closing braces, inclusive.
 *
 * @note json_len was not large enough if rv == json_len
 *
 * @param[i] item Properly formed item to write
 * @param[o] json Active string buffer to write to
 * @param[i] json_len Maximum length of json
 *
 * @return Number of bytes written to json; -1 upon error
 */
static int write_item(sns_registry_item const *item, char *json,
                      uint32_t json_len)
{
  int cidx = -1;

  if(SNS_REGISTRY_TYPE_INTEGER == item->type)
    cidx = snprintf(json, json_len,
                    "\"%s\":{\"type\":\"int\",\"ver\":\"%" PRIu32
                    "\",\"data\":\"%" PRIi64 "\"}",
                    item->name, item->version, item->data.sint);
  else if(SNS_REGISTRY_TYPE_FLOAT == item->type)
    cidx = snprintf(json, json_len,
                    "\"%s\":{\"type\":\"flt\",\"ver\":\"%" PRIu32
                    "\",\"data\":\"%g\"}",
                    item->name, item->version, item->data.flt);
  else if(SNS_REGISTRY_TYPE_GROUP == item->type)
  {
    cidx = snprintf(json, json_len,
                    "\"%s\":{\"type\":\"grp\",\"ver\":\"%" PRIu32
                    "\",\"data\":\"\"}",
                    item->name, item->version);
  }
  else if(SNS_REGISTRY_TYPE_STRING == item->type)
    cidx = snprintf(json, json_len,
                    "\"%s\":{\"type\":\"str\",\"ver\":\"%" PRIu32
                    "\",\"data\":\"%s\"}",
                    item->name, item->version,
                    NULL != item->data.str ? item->data.str : "");
  else
    // PEND: Need to handle errors differently than buffer too small
    SNS_REG_PRINTF(ERROR, sns_fw_printf, "Invalid item type '%i'", item->type);

  return cidx;
}

/**
 * Convert and write an group to the json string; from the group name to the
 * closing braces, inclusive.
 *
 * @note json_len was not large enough if rv == json_len
 *
 * @param[i] group Properly formed group to write
 * @param[o] json Active string buffer to write to
 * @param[i] json_len Maximum length of json
 *
 * @return Number of bytes written to json; -1 upon error
 */
static int write_group(sns_registry_group const *group, char *json,
                       uint32_t json_len)
{
  int cidx;
  sns_list_iter iter;

  cidx = snprintf(json, json_len, "\"%s\":{\"owner\":\"%s\"", group->name,
                  NULL != group->owner ? group->owner : "NA");

  if(-1 == cidx || cidx + 2 >= json_len)
  {
    SNS_REG_PRINTF(MED, sns_fw_printf, "String buffer too short");
    cidx = -1;
  }
  else
  {
    sns_registry_item *item;
    for(sns_list_iter_init(&iter, (sns_list *)&group->items, true);
        NULL !=
        (item = (sns_registry_item *)sns_list_iter_get_curr_data(&iter));
        sns_list_iter_advance(&iter))
    {
      int len;

      json[cidx++] = ',';
      len = write_item(item, &json[cidx], json_len - cidx);

      // If write_item error, or not enough memory for comma/bracket
      if(-1 == len || len + cidx + 1 >= json_len)
      {
        SNS_REG_PRINTF(MED, sns_fw_printf, "String buffer too short");
        cidx = -1;
        break;
      }
      else
        cidx += len;
    }
    if(-1 != cidx)
      json[cidx++] = '}';
  }

  return cidx;
}

/**
 * Load a configuration file, and return a newly allocated string containing
 * its contents.
 *
 * @param[i] file_path Full file path to the platform file
 *
 * @return Data contained in the file
 */
static char *load_file_content(char const *file_path)
{
  struct stat stat_buf;
  char *rv = NULL;
  sns_file_service_internal *file_svc = sns_fs_get_internal_service();
  sns_time load_file_time = SNS_REGISTRY_GET_TIME();
  sns_time fread_ts = 0, fclose_ts = 0;

  SNS_REG_SPRINTF(HIGH, sns_fw_printf, "load_file_content: '%s'", file_path);

  errno = 0;
  if(0 == file_svc->api->stat(file_path, &stat_buf) && 0 < stat_buf.st_size)
  {
    FILE *file;
    if((NULL != strstr(file_path, ".bin")) ||
       (NULL != strstr(file_path, ".BIN")))
    {
      file = file_svc->api->fopen(file_path, "rb");
    }
    else
    {
      file = file_svc->api->fopen(file_path, "r");
    }
    if(NULL != file)
    {
      size_t read_len = 0;
      char *buf = sns_malloc(SNS_HEAP_MAIN, stat_buf.st_size + 1);
      if(NULL != buf)
      {
        read_len =
            file_svc->api->fread(buf, sizeof(uint8_t), stat_buf.st_size, file);
        fread_ts = SNS_REGISTRY_GET_TIME();
      }

      if(0 != read_len)
      {
        buf[read_len] = '\0';
        rv = buf;
      }
      else
      {
        SNS_REG_SPRINTF(HIGH, sns_fw_printf,
                        "load_file_content: fread() err=%d '%s'", errno,
                        strerror(errno));
        sns_free(buf);
      }
      file_svc->api->fclose(file);
      fclose_ts = SNS_REGISTRY_GET_TIME();

      if((fclose_ts - load_file_time) >=
         SNS_REG_DEBUG_PRINT_TIME_THRESHOLD_TICKS)
      {
        SNS_REG_PRINTF(HIGH, sns_fw_printf, "read ts %x total time for \
          load_file_content: %x ",
                       fread_ts, (fclose_ts - load_file_time));
      }
    }
    else
    {
      SNS_REG_SPRINTF(HIGH, sns_fw_printf,
                      "load_file_content: fopen() err=%d (%s)", errno,
                      strerror(errno));
    }
  }
  else
  {
    SNS_REG_SPRINTF(HIGH, sns_fw_printf, "load_file_content: '%s' st_size=%d",
                    strerror(errno), stat_buf.st_size);
  }
  return rv;
}

/**
 * Parses the file contents of sns_reg_config and updates dynamic_hw_info.
 * Handles config lines with the following formats:
 * file=input=xxx
 * file=output=xxx
 * property=xxx=yyy
 *
 * @param[i] config_contents configuration file data as string
 *
 * @return Number of configuration items saved in dynamic_hw_info[]
 */
static uint32_t parse_device_hw_info(char *config_contents)
{
  char *rest_of_config = config_contents;
  char *config_line;
  uint8_t hw_info_idx = 0;

  while(hw_info_idx < MAX_CONFIG_ENTRIES &&
        NULL != (config_line = strtok_r(rest_of_config, "\n", &rest_of_config)))
  {
    char *rest_of_line = config_line;
    char *token[3] = {NULL, NULL, NULL};
    uint8_t token_idx = 0;

    SNS_REG_SPRINTF(LOW, sns_fw_printf, "parse_device_hw_info: '%s'",
                    config_line);

    while(token_idx < ARR_SIZE(token) &&
          NULL !=
              (token[token_idx] = strtok_r(rest_of_line, "=", &rest_of_line)))
    {
      token_idx++;
    }

    if(NULL != token[2])
    {
      if(0 == strcmp(token[0], "file"))
      {
        if(0 == strcmp(token[1], "input"))
        {
          SNS_REGISTRY_STRCPY(&json_list_name, token[2]);
        }
        else if(0 == strcmp(token[1], "output"))
        {
          SNS_REGISTRY_STRCPY(&registry_dir, token[2]);
        }
      }
      else if(0 == strcmp(token[0], "property"))
      {
        SNS_REGISTRY_STRCPY((char **)&dynamic_hw_info[hw_info_idx].name,
                            token[1]);
        SNS_REGISTRY_STRCPY((char **)&dynamic_hw_info[hw_info_idx].path,
                            token[2]);
        hw_info_idx++;
      }
    }
  }

  if(MAX_CONFIG_ENTRIES == hw_info_idx)
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf, "dynamic_hw_info exceeded length");
  }

  return hw_info_idx;
}

/**
 *
 * PEND: sns_reg_config is not supposed to have a version #; either needs
 * to be removed, or update these comments.
 *
 * sns_reg_config has version line at top to control versioning,
 * change in the version can remove existing sensors_list to let
 * it populate afresh again during boot
 */
static void handle_config_change(void)
{
  if(NULL != registry_parent_dir && NULL != configfile_version)
  {
    bool changed = false;
    STR_CONCAT(full_path, total_size, registry_parent_dir, "/",
               current_version_file);
    char *version_stored = load_file_content(full_path);

    if(NULL != version_stored)
    {
      if(0 == strcmp(version_stored, configfile_version))
      {
        SNS_REG_PRINTF(LOW, sns_fw_printf,
                       "no update of sns_reg_config version");
      }
      else if(NULL != registry_grand_parent_dir)
      {
        sns_file_service_internal *file_svc = sns_fs_get_internal_service();
        STR_CONCAT(parsed_file_list, reg_list_file_size, registry_parent_dir,
                   "/", parsed_file_list_file);
        STR_CONCAT(sensor_list, total_size, registry_grand_parent_dir, "/",
                   current_sensors_list);
        SNS_REG_SPRINTF(LOW, sns_fw_printf,
                        "update of sns_reg_config version "
                        "%s:=(expected)/NOT_EQUAL/(current)=:%s",
                        configfile_version, version_stored);
        file_svc->api->unlink(sensor_list);
        file_svc->api->unlink(parsed_file_list);
        changed = true;
      }
      sns_free(version_stored);
    }
    else
    {
      /*could be first boot after flash if sns_reg_version part of build*/
      changed = true;
    }

    if(changed)
    {
      save_registry_file(full_path, configfile_version,
                         strlen(configfile_version));
    }
  }
}

/**
 * sns_reg_config has version line at top to control versioning.
 * read the first line and get the version
 *
 *  @param[i] config_contents - configuration file data as string
 */
static void get_version(char *config_contents)
{
  char *end_version_line = strchr(config_contents, '\n');
  if(NULL != end_version_line)
  {
    int version_strlen = end_version_line - config_contents;
    config_contents[version_strlen] = '\0';
    SNS_REGISTRY_STRCPY(&configfile_version, config_contents);
    config_contents[version_strlen] = '\n';
  }
}

static void get_hw_info_from_core(void)
{
  sns_registry_hw *hw;
  static char platform_name[] = "hw_platform";
  static char platform_value[] = "MTPSURFQRD";
  static char platform_subtype_id_name[] = "platform_subtype_id";
  static char platform_subtype_id_value[] = "123";
  static char platform_version_name[] = "platform_version";
  static char platform_version_value[] = "123456789";
  static char soc_id_name[] = "soc_id";
  static char soc_id_value[] = "12345";
  static char platform_family_name[] = "platform_family";
  static char platform_family_value[] = "12345";

  sns_hw_chip_id chip_id;
  sns_hw_platform_info platform_info;
  sns_hw_platform_type info_text;
  sns_hw_chip_family_id chip_family;

  if(SNS_RC_SUCCESS ==
     sns_get_hw_info(SNS_HW_INFO_TYPE_PLATFORM_INFO, &platform_info))
  {
    hw = &dynamic_hw_info[dynamic_hw_info_len++];
    info_text.platform = platform_info.platform;
    sns_get_hw_info(SNS_HW_INFO_TYPE_PLATFORM_TYPE, &info_text);
    snprintf(platform_value, sizeof(platform_value), "%s", info_text.text);
    hw->name = platform_name;
    hw->value = platform_value;

    hw = &dynamic_hw_info[dynamic_hw_info_len++];
    snprintf(platform_subtype_id_value, sizeof(platform_subtype_id_value),
             "%" PRIu32, platform_info.subtype);
    hw->name = platform_subtype_id_name;
    hw->value = platform_subtype_id_value;

    hw = &dynamic_hw_info[dynamic_hw_info_len++];
    snprintf(platform_version_value, sizeof(platform_version_value), "%" PRIu32,
             platform_info.version);
    hw->name = platform_version_name;
    hw->value = platform_version_value;
  }

  hw = &dynamic_hw_info[dynamic_hw_info_len++];
  sns_get_hw_info(SNS_HW_INFO_TYPE_CHIP_ID, &chip_id);
  snprintf(soc_id_value, sizeof(soc_id_value), "%" PRIu32, chip_id.id);
  hw->name = soc_id_name;
  hw->value = soc_id_value;
  hw = &dynamic_hw_info[dynamic_hw_info_len++];
  sns_get_hw_info(SNS_HW_INFO_TYPE_CHIP_FAMILY_ID, &chip_family);
  snprintf(platform_family_value, sizeof(platform_family_value), "%" PRIu32,
           chip_family.id);
  hw->name = platform_family_name;
  hw->value = platform_family_value;

  for(int i = 0; i < dynamic_hw_info_len; i++)
  {
    hw = &dynamic_hw_info[i];
    SNS_REG_SPRINTF(HIGH, sns_fw_printf, "meta: #%u - %s = %s = %s", i,
                    hw->name, hw->path ? hw->path : "",
                    hw->value ? hw->value : "");
  }
}

/**
 * Read and parse the current device configuration, to be used to determine
 * which configuration files are applicable.  Updates dynamic_hw_info.
 *
 * No-op if dynamic config file is not present.  May block up to 400ms for
 * control file (i.e. for dynamic config file to be ready).
 */
static void load_meta_config(void)
{
  dynamic_hw_info_len = 0;
  dynamic_hw_info =
      sns_malloc(SNS_HEAP_MAIN, MAX_CONFIG_ENTRIES * sizeof(sns_registry_hw));

  if(NULL != dynamic_hw_info)
  {
    char *config_contents = load_file_content(registry_config_file);
    if(NULL != config_contents)
    {
      get_version(config_contents);
      dynamic_hw_info_len = parse_device_hw_info(config_contents);
      sns_free(config_contents);
    }
    else
    {
      SNS_REGISTRY_STRCPY(&json_list_name, "json.lst");
      SNS_REGISTRY_STRCPY(&registry_dir, "");
    }
    get_hw_info_from_core();
  }
  else
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf, "load_meta_config: OOM");
  }
}

/**
 * Initializes registry group list
 */
static void registry_group_init(void)
{
  the_config_group = sns_registry_group_create(config_group_name);
  if(NULL != the_config_group)
  {
    sns_list_iter iter;
    sns_list_init(&sns_registry_groups);
    sns_list_iter_init(&iter, &sns_registry_groups, false);
    sns_list_iter_insert(&iter, &the_config_group->list_entry, true);
  }
}

/**
 * Save a string to disk.
 *
 * @param[i] full_path File path to write to
 * @param[i] json Data to write
 * @param[i] json_len Length of the json string
 *
 * @return true if write succeeded; false otherwise
 */
static bool save_registry_file(char const *full_path, void const *json,
                               uint32_t json_len)
{
  bool rv = false;
  FILE *file;
  sns_file_service_internal *file_svc = sns_fs_get_internal_service();
  sns_time save_reg_time = SNS_REGISTRY_GET_TIME();

  errno = 0;
  file = file_svc->api->fopen(full_path, "w");

  if(NULL != file)
  {
    size_t write_len =
        file_svc->api->fwrite(json, sizeof(char), json_len, file);
    sns_time write_ts = SNS_REGISTRY_GET_TIME();
    rv = (write_len == json_len);
    SNS_REG_SPRINTF(LOW, sns_fw_printf,
                    "save_registry_file: fwrite() '%s' '%s'", full_path,
                    strerror(errno));
    if((write_ts - save_reg_time) >= SNS_REG_DEBUG_PRINT_TIME_THRESHOLD_TICKS)
    {
      SNS_REG_SPRINTF(HIGH, sns_fw_printf, "start ts %x fwrite ts %x, total\
        duration %x",
                      save_reg_time, write_ts, (write_ts - save_reg_time));
    }
    file_svc->api->fclose(file);
  }
  else
  {
    SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                    "save_registry_file: fopen() '%s' '%s'", full_path,
                    strerror(errno));
  }

  return rv;
}

/**
 * Initializes parsed_reg_files
 *
 * @param[i] found_list Previously saved list of parsed files, if any
 *
 */
static void parsed_file_list_init(const char *found_list)
{
  parsed_reg_files_buf_len = SNS_REGISTRY_GRP_LEN_INIT;
  if(NULL != found_list && SNS_REGISTRY_GRP_LEN_INIT < strlen(found_list))
  {
    parsed_reg_files_buf_len <<= 1;
  }
  parsed_reg_files = sns_malloc(SNS_HEAP_MAIN, parsed_reg_files_buf_len);
}

/**
 * Writes the list of parsed files to file
 */
static void parsed_file_list_write(void)
{
  size_t len;
  if(NULL != registry_parent_dir && NULL != parsed_reg_files &&
     0 < (len = strlen(parsed_reg_files)))
  {
    sns_file_service_internal *file_svc = sns_fs_get_internal_service();
    STR_CONCAT(full_path, reg_list_file_size, registry_parent_dir, "/",
               parsed_file_list_file);
    if(0 == file_svc->api->fwritesafe(full_path, registry_parent_dir,
                                      parsed_reg_files, len))
    {
      SNS_REG_SPRINTF(HIGH, sns_fw_printf,
                      "parsed_file_list_write: '%s' len=%u", full_path, len);
    }
    else
    {
      save_registry_file(full_path, parsed_reg_files, len);
    }
  }
}

/**
 * Adds the given filename to list of parsed files
 *
 * @param[i] filename The file to add if not already on the list
 *
 */
static void parsed_file_list_add(char const *filename)
{
  char file_entry[SNS_REGISTRY_STR_MAX];
  snprintf(file_entry, sizeof(file_entry), "%s\n", filename);
  if(NULL != parsed_reg_files && NULL == strstr(parsed_reg_files, file_entry))
  {
    size_t list_len = strlen(parsed_reg_files);
    size_t name_len = strlen(filename) + 1; // includes "\n"

    if(list_len + name_len >= parsed_reg_files_buf_len)
    {
      size_t len = parsed_reg_files_buf_len + SNS_REGISTRY_GRP_LEN_INIT;
      char *new_buf = sns_malloc(SNS_HEAP_MAIN, len);
      if(NULL != new_buf)
      {
        parsed_reg_files_buf_len = len;
        sns_strlcpy(new_buf, parsed_reg_files, len);
        sns_free(parsed_reg_files);
        parsed_reg_files = new_buf;
      }
    }
    if(list_len + name_len < parsed_reg_files_buf_len)
    {
      sns_strlcat(parsed_reg_files, filename, parsed_reg_files_buf_len);
      sns_strlcat(parsed_reg_files, "\n", parsed_reg_files_buf_len);

      if(registry_init_complete)
      {
        SNS_REG_SPRINTF(LOW, sns_fw_printf, "parsed_file_list_add: '%s'",
                        filename);
        parsed_file_list_write();
      }
      // else, list is not written to file until the end of registry init
      // process
    }
  }
}

/**
 * Writes the list of corrupted files to file
 */
static void corrupted_file_list_write(void)
{
  size_t len;
  if(NULL != registry_parent_dir && NULL != corrupted_parsed_files &&
     0 < (len = strlen(corrupted_parsed_files)))
  {
    sns_file_service_internal *file_svc = sns_fs_get_internal_service();
    STR_CONCAT(full_path, file_size, registry_parent_dir, "/",
               corrupted_file_list_file);
    if(0 == file_svc->api->fwritesafe(full_path, registry_parent_dir,
                                      corrupted_parsed_files, len))
    {
      SNS_REG_SPRINTF(HIGH, sns_fw_printf,
                      "corrupted_file_list_write: '%s' len=%u", full_path, len);
    }
    else
    {
      save_registry_file(full_path, corrupted_parsed_files, len);
    }
  }
  if(NULL != corrupted_parsed_files)
  {
    sns_free(corrupted_parsed_files);
    corrupted_parsed_files = NULL;
    corrupted_parsed_files_buf_len = 0;
  }
}

/**
 * Loads and parses existing parsed registry files
 *
 * @note Loads existing parsed_file_list.csv, if found, to get the names of
 *       existing parsed files in output folder and one by one loads and parses
 *       each parsed file and creates corresponding registry group for it. The
 *       parsed file list in RAM is rebuilt with successfully loaded and parsed
 *       files.  This list is then written to file once at the end of registry
 *       init process and whenever updated afterward.
 *
 */
static void process_registry_dir(void)
{
  sns_time start_ts = SNS_REGISTRY_GET_TIME();
  char *dir_file_list = NULL;
  if(NULL != registry_parent_dir)
  {
    STR_CONCAT(reg_list_file, reg_list_file_size, registry_parent_dir, "/",
               parsed_file_list_file);
    dir_file_list = load_file_content(reg_list_file);
  }
  parsed_file_list_init(dir_file_list);
  if(NULL != dir_file_list)
  {
    foreach_in_dir(registry_dir, dir_file_list, &load_file, false);
    sns_free(dir_file_list);
    config_group_update();
  }
  SNS_REG_PRINTF(HIGH, sns_fw_printf, "process_registry_dir: took %u ms",
                 (uint32_t)((SNS_REGISTRY_GET_TIME() - start_ts) *
                            sns_get_time_tick_resolution() * 1E-6));
}

/**
 * Loads and parses registry config (.json) files
 *
 * @note Unlike parsed file list which need not be present at boot json list
 *       must exist.
 *
 * @return
 * SNS_RC_SUCCESS if successfully loads and parses all config files
 * SNS_RC_FAILED if config directory is empty or not accessible
 */
static sns_rc process_config_dir(void)
{
  sns_rc rc = SNS_RC_FAILED;
  sns_time start_ts = sns_get_system_time();
  char *dir_file_list = NULL;
  if(NULL != json_list_name)
  {
    STR_CONCAT(json_list_file, json_list_file_size, config_dir, "/",
               json_list_name);
    dir_file_list = load_file_content(json_list_file);
    sns_free(json_list_name);
    json_list_name = NULL;
  }
  if(NULL != dir_file_list)
  {
    foreach_in_dir(config_dir, dir_file_list, &load_file, true);
    sns_free(dir_file_list);
    config_group_write();
    parsed_file_list_write();
    corrupted_file_list_write();
    rc = SNS_RC_SUCCESS;
  }
  else
  {
    SNS_REG_PRINTF(ERROR, sns_fw_printf,
                   "process_config_dir: no JSON file list");
  }
  SNS_REG_PRINTF(HIGH, sns_fw_printf, "process_config_dir: took %u ms",
                 (uint32_t)((sns_get_system_time() - start_ts) *
                            sns_get_time_tick_resolution() * 1E-6));
  return rc;
}

/*=============================================================================
  Public Function Definitions
  ===========================================================================*/

sns_rc sns_registry_init(sns_sensor *const this)
{
  UNUSED_VAR(this);
  sns_rc rv = SNS_RC_FAILED;
  sns_time sns_reg_start = SNS_REGISTRY_GET_TIME();
  SNS_REG_PRINTF(HIGH, sns_fw_printf, "REG INIT START...ts=%X",
                 sns_get_system_time());

  load_meta_config();
  registry_group_init();

  check_registry_dir(); // will work even if output registry dir is not
                        // present/writable
  handle_config_change();
  process_registry_dir(); // must be done before config dir is processed

  rv = process_config_dir();

  sns_free(registry_grand_parent_dir);
  registry_grand_parent_dir = NULL;
  sns_free(configfile_version);
  configfile_version = NULL;
  registry_init_complete = true;

  if(NULL != dynamic_hw_info)
  {
    for(uint8_t i = 0; i < MAX_CONFIG_ENTRIES; i++)
    {
      if(NULL != dynamic_hw_info[i].name)
      {
        sns_free((void *)dynamic_hw_info[i].name);
      }
      if(NULL != dynamic_hw_info[i].path)
      {
        sns_free((void *)dynamic_hw_info[i].path);
      }
      if(NULL != dynamic_hw_info[i].value)
      {
        sns_free(dynamic_hw_info[i].value);
      }
    }
    sns_free(dynamic_hw_info);
    dynamic_hw_info = NULL;
  }

  SNS_REG_PRINTF(HIGH, sns_fw_printf, "REG INIT DONE total duration %x",
                 (SNS_REGISTRY_GET_TIME() - sns_reg_start));

  sns_reg_debug_buf_dump();

  return rv;
}

sns_registry_group *sns_registry_group_create(char const *name)
{
  uint32_t group_size = sizeof(sns_registry_group) + strlen(name) + 1;
  sns_registry_group *group = sns_malloc(SNS_HEAP_MAIN, group_size);
  if(NULL != group)
  {
    group->name = (char *)((uintptr_t)group + sizeof(*group));
    sns_strlcpy(group->name, name, strlen(name) + 1);

    group->fs_size = 0;
    group->source = NULL;
    group->owner = NULL;
    group->writable = true;

    sns_list_item_init(&group->list_entry, group);
    sns_list_init(&group->items);
  }

  return group;
}

sns_registry_group *sns_registry_group_find(char const *grp_name)
{
  sns_list_iter iter;
  sns_registry_group *group;

  for(sns_list_iter_init(&iter, &sns_registry_groups, true);
      NULL !=
      (group = (sns_registry_group *)sns_list_iter_get_curr_data(&iter));
      sns_list_iter_advance(&iter))
  {
    if(0 == strcmp(group->name, grp_name))
      return group;
  }

  return NULL;
}

sns_rc sns_registry_group_merge(sns_registry_group *group, void *sensor)
{
  sns_rc rv = SNS_RC_FAILED;
  sns_list_iter iter;
  sns_registry_item *item;
  sns_registry_group *perm_group = sns_registry_group_find(group->name);

  if(perm_group == group)
  {
    return rv;
  }

  if(NULL == perm_group)
  {
    perm_group = sns_registry_group_create(group->name);
    if(NULL != perm_group)
    {
      perm_group->fs_size = group->fs_size;

      sns_list_iter_init(&iter, &sns_registry_groups, false);
      sns_list_iter_insert(&iter, &perm_group->list_entry, true);
    }
  }
  if(NULL != perm_group)
  {
    rv = SNS_RC_SUCCESS;

    if(NULL == perm_group->owner && NULL != group->owner)
    {
      SNS_REGISTRY_STRCPY(&perm_group->owner, group->owner);
    }
    if(NULL != group->source &&
       (NULL == perm_group->source || 0 == perm_group->items.cnt))
    {
      SNS_REGISTRY_STRCPY(&perm_group->source, group->source);
    }

    if(!group->writable)
    {
      perm_group->writable = false;
    }

    for(sns_list_iter_init(&iter, &group->items, true);
        SNS_RC_SUCCESS == rv &&
        NULL !=
            (item = (sns_registry_item *)sns_list_iter_get_curr_data(&iter));
        sns_list_iter_advance(&iter))
    {
      if(SNS_REGISTRY_TYPE_INVALID != item->type)
      {
        sns_registry_item *perm_item =
            sns_registry_item_find(perm_group, item->name);
        if(NULL == perm_item)
        {
          perm_item = sns_registry_item_create(perm_group, item);
        }
        if(NULL != perm_item)
        {
          rv |= update_item(group->source, perm_item, item, sensor);
        }
      }
    }

    // Don't save to file if we just read it from file
    if(0 == group->fs_size && perm_group->writable)
    {
      rv |= sns_registry_group_save(perm_group, sensor);
    }
    else if(!perm_group->writable)
    {
      // if can't write data to file system because it's not writable
      // treat this case as error
      // the other cases are not error (fs_size is not 0)
      rv |= SNS_RC_FAILED;
    }
  }

  return rv;
}

// PEND: Use strchr to reject any request to store a '"' symbol in the registry
// To avoid multiple RT latencies, we first produce the entire string,
// then subsequently write it to file
sns_rc sns_registry_group_save(sns_registry_group *group, void *sensor)
{
  uint32_t json_len = SNS_REGISTRY_GRP_LEN_INIT;
  sns_rc rv = SNS_RC_NOT_AVAILABLE;

  do
  {
    int len;
    uint32_t cidx = 0;
    char *json = sns_malloc(SNS_HEAP_MAIN, json_len);
    if(NULL == json)
    {
      rv = SNS_RC_NOT_AVAILABLE;
      break;
    }

    json[cidx++] = '{';
    len = write_group(group, &json[cidx], json_len - cidx);
    if(-1 == len || len + cidx + 1 >= json_len)
    {
      SNS_REG_SPRINTF(MED, sns_fw_printf,
                      "group_save: '%s' l=%d jl=%d i=%d; doubling", group->name,
                      len, json_len, cidx);
      json_len <<= 1;
      sns_free(json);
    }
    else
    {
      cidx += len;
      json[cidx++] = '}';

      if(NULL == sensor)
      {
        char file_name[SNS_REGISTRY_STR_MAX];
        if(NULL != group->source && NULL != group->owner &&
           0 != strcmp(group->owner, "NA"))
        {
          snprintf(file_name, sizeof(file_name), "%s.%s", group->source,
                   group->name);
        }
        else
        {
          snprintf(file_name, sizeof(file_name), "%s", group->name);
        }
        rv = sns_registry_group_write(file_name, json, cidx);
      }
      else // If called from instance (after init is done), we enqueue the write
      {
        rv = sns_registry_group_enqueue(group->source, group->name, json, cidx,
                                        sensor);
      }

      if(rv == SNS_RC_SUCCESS)
        group->fs_size = len;

      sns_free(json);
      break;
    }
  } while(true);

  return rv;
}

// write a json string to file
sns_rc sns_registry_group_write(char const *name, void const *json,
                                uint32_t length)
{
  sns_rc rv = SNS_RC_SUCCESS;
  sns_time group_write_time = SNS_REGISTRY_GET_TIME();
  sns_time end_ts = 0;

  if(!no_write_access && NULL != registry_dir && NULL != registry_parent_dir)
  {
    sns_file_service_internal *file_svc = sns_fs_get_internal_service();
    STR_CONCAT(full_path, total_size, registry_dir, "/", name);

    rv = SNS_RC_NOT_AVAILABLE;
    if(0 ==
       file_svc->api->fwritesafe(full_path, registry_parent_dir, json, length))
    {
      SNS_REG_SPRINTF(HIGH, sns_fw_printf, "group_write: '%s' len=%u", name,
                      length);
      rv = SNS_RC_SUCCESS;
    }
    else if(ENOTSUP == errno && save_registry_file(full_path, json, length))
    {
      rv = SNS_RC_SUCCESS;
    }

    if(SNS_RC_SUCCESS == rv)
    {
      /* append this new file name to list of generated files */
      parsed_file_list_add(name);

      if((NULL != strstr(name, ".bin")) || (NULL != strstr(name, ".BIN")))
      {
        SNS_REG_SPRINTF(ERROR, sns_fw_printf,
                        "group_write: *** bin file '%s' ***", name);
      }
    }
  }

  end_ts = SNS_REGISTRY_GET_TIME();
  if((end_ts - group_write_time) >= SNS_REG_DEBUG_PRINT_TIME_THRESHOLD_TICKS)
  {
    SNS_REG_SPRINTF(HIGH, sns_fw_printf, "start ts %x end ts %x,\
          total duration %x",
                    group_write_time, end_ts, (end_ts - group_write_time));
  }

  return rv;
}

void sns_registry_group_free(sns_registry_group *group)
{
  sns_list_iter iter;

  sns_list_iter_init(&iter, &group->items, true);
  while(NULL != sns_list_iter_curr(&iter))
  {
    sns_registry_item *item = (sns_registry_item *)sns_list_item_get_data(
        sns_list_iter_remove(&iter));

    if(SNS_REGISTRY_TYPE_STRING == item->type)
      sns_free(item->data.str);
    else if(SNS_REGISTRY_TYPE_GROUP == item->type)
      sns_registry_group_free(item->data.group);

    sns_free(item);
  }

  sns_free(group->owner);
  sns_free(group->source);
  sns_free(group);
}

/* When a subgroup is first read, we don't set its pointer.  Now when we are
 * merging it into the registry itself, we need to find it, and create it if
 * necessary. */
sns_registry_item *sns_registry_item_create(sns_registry_group *group,
                                            sns_registry_item *item)
{
  sns_registry_item *new_item = NULL;
  uint32_t item_size = 0;
  if(NULL != group && NULL != item)
  {
    item_size = sizeof(*item) + strlen(item->name) + 1;
    new_item = sns_malloc(SNS_HEAP_MAIN, item_size);
  }

  if(NULL != new_item)
  {
    sns_list_iter iter;
    new_item->name = (char *)((uintptr_t)new_item + sizeof(*item));
    sns_strlcpy(new_item->name, item->name, strlen(item->name) + 1);

    if(SNS_REGISTRY_TYPE_STRING == item->type)
    {
      SNS_REGISTRY_STRCPY(&new_item->data.str, item->data.str);
    }
    else
    {
      new_item->data = item->data;
    }

    new_item->version = item->version;
    new_item->type = item->type;

    sns_list_item_init(&new_item->list_entry, new_item);
    sns_list_iter_init(&iter, &group->items, false);
    sns_list_iter_insert(&iter, &new_item->list_entry, true);
  }
  else
  {
    SNS_REG_SPRINTF(
        ERROR, sns_fw_printf, "item_create: group='%s' item='%s' size=%u",
        group ? group->name : "", item ? item->name : "", item_size);
  }

  return new_item;
}

sns_registry_item *sns_registry_item_find(sns_registry_group const *group,
                                          char const *item_name)
{
  sns_list_iter iter;
  sns_registry_item *item;

  for(sns_list_iter_init(&iter, &((sns_registry_group *)group)->items, true);
      NULL != (item = (sns_registry_item *)sns_list_iter_get_curr_data(&iter));
      sns_list_iter_advance(&iter))
  {
    if(0 == strcmp(item->name, item_name))
      return item;
  }

  return NULL;
}

#ifdef SNS_REGISTRY_DEBUG
// Buffer for storing DEBUG strings and its current offset
static char reg_debug_buf[SNS_REG_DEBUG_BUFFER_SIZE]
                         [SNS_REG_DEBUG_MESSAGE_MAX_LEN];
static unsigned int reg_debug_buf_offset = 0;

void sns_reg_sensor_sprintf(uint16_t ssid, const sns_sensor *sensor,
                            sns_message_priority prio, const char *file,
                            uint32_t line, const char *format, ...)
{
  unsigned int offset = reg_debug_buf_offset++ % SNS_REG_DEBUG_BUFFER_SIZE;
  char *cur_buf = reg_debug_buf[offset];
  size_t timestamp_len;
  va_list args;
  va_start(args, format);
  timestamp_len = snprintf(cur_buf, SNS_REG_DEBUG_MESSAGE_MAX_LEN,
                           "%llX:: ", SNS_REGISTRY_GET_TIME());
  vsnprintf(cur_buf + timestamp_len,
            SNS_REG_DEBUG_MESSAGE_MAX_LEN - timestamp_len, format, args);
  va_end(args);

  sns_diag_sensor_sprintf(ssid, sensor, prio, file, line, "%s",
                          reg_debug_buf[offset]);
}
#endif /* SNS_REGISTRY_DEBUG */

void sns_reg_debug_buf_dump(void)
{
#ifdef SNS_REGISTRY_DEBUG_DUMP
  if(NULL != registry_parent_dir)
  {
    static unsigned int dump_start_offset = 0;
    sns_time start_ts = SNS_REGISTRY_GET_TIME();
    STR_CONCAT(reg_dbg_file, file_path_len, registry_parent_dir, "/",
               "reg_dbg.txt");
    sns_file_service_internal *file_svc = sns_fs_get_internal_service();
    FILE *file = file_svc->api->fopen(reg_dbg_file,
                                      (dump_start_offset == 0) ? "w" : "a");
    if(NULL != file)
    {
      unsigned int num_lines = 0;
      char line[SNS_REGISTRY_STR_MAX];
      for(unsigned int i = dump_start_offset; i < ARR_SIZE(reg_debug_buf); i++)
      {
        size_t line_len = strlen(reg_debug_buf[i]);
        if(line_len > 0)
        {
          snprintf(line, sizeof(line), "%s\n", reg_debug_buf[i]);
          file_svc->api->fwrite(line, sizeof(char), strlen(line), file);
          num_lines++;
        }
        else
        {
          sns_time log_time = SNS_REGISTRY_GET_TIME();
          unsigned int cost =
              (unsigned int)((SNS_REGISTRY_GET_TIME() - start_ts) *
                             sns_get_time_tick_resolution() * 1E-6);
          snprintf(line, sizeof(line),
                   "%llX:: reg_dbg_dump: #lines=%u/%u cost=%u ms\n", log_time,
                   num_lines, reg_debug_buf_offset, cost);
          file_svc->api->fwrite(line, sizeof(char), strlen(line), file);
          dump_start_offset = i;
          break;
        }
      }
      file_svc->api->fclose(file);
    }
  }
#endif /* SNS_REGISTRY_DEBUG_DUMP */
}
