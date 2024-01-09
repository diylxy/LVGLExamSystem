/**
 * @file lv_cache.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_cache.h"
#include "../stdlib/lv_string.h"
#include "../osal/lv_os.h"
#include "../core/lv_global.h"

/*********************
 *      DEFINES
 *********************/
#define _cache_manager LV_GLOBAL_DEFAULT()->cache_manager

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void _lv_cache_init(void)
{
    lv_memzero(&_cache_manager, sizeof(lv_cache_manager_t));
    lv_mutex_init(&_cache_manager.mutex);
}

void _lv_cache_deinit(void)
{
    lv_mutex_delete(&_cache_manager.mutex);
}

void lv_cache_set_manager(lv_cache_manager_t * manager)
{
    LV_ASSERT(_cache_manager.locked);
    if(manager == NULL) return;

    if(_cache_manager.empty_cb != NULL) _cache_manager.empty_cb();
    else if(_cache_manager.set_max_size_cb != NULL) _cache_manager.set_max_size_cb(0);

    _cache_manager.add_cb = manager->add_cb;
    _cache_manager.find_by_data_cb = manager->find_by_data_cb;
    _cache_manager.invalidate_cb = manager->invalidate_cb;
    _cache_manager.get_data_cb = manager->get_data_cb;
    _cache_manager.release_cb = manager->release_cb;
    _cache_manager.set_max_size_cb = manager->set_max_size_cb;
    _cache_manager.empty_cb = manager->empty_cb;

    if(_cache_manager.set_max_size_cb != NULL) _cache_manager.set_max_size_cb(_cache_manager.max_size);
}

lv_cache_entry_t * lv_cache_add(const void * data, size_t data_size, uint32_t data_type, size_t memory_usage)
{
    LV_ASSERT(_cache_manager.locked);
    if(_cache_manager.add_cb == NULL) return NULL;

    return _cache_manager.add_cb(data, data_size, data_type, memory_usage);
}

lv_cache_entry_t * lv_cache_find_by_data(const void * data, size_t data_size, uint32_t data_type)
{
    LV_ASSERT(_cache_manager.locked);
    if(_cache_manager.find_by_data_cb == NULL) return NULL;

    return _cache_manager.find_by_data_cb(data, data_size, data_type);
}

lv_cache_entry_t * lv_cache_find_by_src(lv_cache_entry_t * entry, const void * src, lv_cache_src_type_t src_type)
{
    LV_ASSERT(_cache_manager.locked);
    if(_cache_manager.find_by_src_cb == NULL) return NULL;

    return _cache_manager.find_by_src_cb(entry, src, src_type);
}

void lv_cache_invalidate(lv_cache_entry_t * entry)
{
    LV_ASSERT(_cache_manager.locked);
    if(_cache_manager.invalidate_cb == NULL) return;

    _cache_manager.invalidate_cb(entry);
}

void lv_cache_invalidate_by_src(const void * src, lv_cache_src_type_t src_type)
{
    lv_cache_entry_t * next;
    LV_ASSERT(_cache_manager.locked);
    lv_cache_entry_t * entry = lv_cache_find_by_src(NULL, src, src_type);
    while(entry) {
        next = lv_cache_find_by_src(entry, src, src_type);
        lv_cache_invalidate(entry);
        entry = next;
    }
}

const void * lv_cache_get_data(lv_cache_entry_t * entry)
{
    LV_ASSERT(_cache_manager.locked);
    if(_cache_manager.get_data_cb == NULL) return NULL;

    const void * data = _cache_manager.get_data_cb(entry);
    return data;
}

void lv_cache_release(lv_cache_entry_t * entry)
{
    LV_ASSERT(_cache_manager.locked);
    if(_cache_manager.release_cb == NULL) return;

    _cache_manager.release_cb(entry);
}

void lv_cache_set_max_size(size_t size)
{
    LV_ASSERT(_cache_manager.locked);
    if(_cache_manager.set_max_size_cb == NULL) return;

    _cache_manager.set_max_size_cb(size);
    _cache_manager.max_size = size;
}

size_t lv_cache_get_max_size(void)
{
    return _cache_manager.max_size;
}

void lv_cache_lock(void)
{
    lv_mutex_lock(&_cache_manager.mutex);
    _cache_manager.locked = 1;
}

void lv_cache_unlock(void)
{
    _cache_manager.locked = 0;
    lv_mutex_unlock(&_cache_manager.mutex);
}

uint32_t lv_cache_register_data_type(void)
{
    _cache_manager.last_data_type++;
    return _cache_manager.last_data_type;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
