/*
 *    strmap version 2.0.1
 *
 *    ANSI C hash table for strings.
 *
 *	  Version history:
 *	  1.0.0 - initial release
 *	  2.0.0 - changed function prefix from strmap to sm to ensure
 *	      ANSI C compatibility
 *	  2.0.1 - improved documentation 
 *
 *    strmap.h
 *
 *    Copyright (c) 2009, 2011, 2013 Per Ola Kristensson.
 *
 *    Per Ola Kristensson <pok21@cam.ac.uk> 
 *    Inference Group, Department of Physics
 *    University of Cambridge
 *    Cavendish Laboratory
 *    JJ Thomson Avenue
 *    CB3 0HE Cambridge
 *    United Kingdom
 *
 *    strmap is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    strmap is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with strmap.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _STRMAP_H_
#define _STRMAP_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <string.h>

typedef struct StrMap StrMap;

/*
 * This callback function is called once per key-value when iterating over
 * all keys associated to values.
 *
 * Parameters:
 *
 * key: A pointer to a null-terminated C string. The string must not
 * be modified by the client.
 *
 * value: A pointer to a null-terminated C string. The string must
 * not be modified by the client.
 *
 * obj: A pointer to a client-specific object. This parameter may be
 * null.
 *
 * Return value: None.
 */
typedef void(*sm_enum_func)(const char *key, const char *value, const void *obj);

/*
 * Creates a string map.
 *
 * Parameters:
 *
 * capacity: The number of top-level slots this string map
 * should allocate. This parameter must be > 0.
 *
 * Return value: A pointer to a string map object, 
 * or null if a new string map could not be allocated.
 */
StrMap * sm_new(unsigned int capacity);

/*
 * Releases all memory held by a string map object.
 *
 * Parameters:
 *
 * map: A pointer to a string map. This parameter cannot be null.
 * If the supplied string map has been previously released, the
 * behaviour of this function is undefined.
 *
 * Return value: None.
 */
void sm_delete(StrMap *map);

/*
 * Returns the value associated with the supplied key.
 *
 * Parameters:
 *
 * map: A pointer to a string map. This parameter cannot be null.
 *
 * key: A pointer to a null-terminated C string. This parameter cannot
 * be null.
 *
 * out_buf: A pointer to an output buffer which will contain the value,
 * if it exists and fits into the buffer.
 *
 * n_out_buf: The size of the output buffer in bytes.
 *
 * Return value: If out_buf is set to null and n_out_buf is set to 0 the return
 * value will be the number of bytes required to store the value (if it exists)
 * and its null-terminator. For all other parameter configurations the return value
 * is 1 if an associated value was found and completely copied into the output buffer,
 * 0 otherwise.
 */
int sm_get(const StrMap *map, const char *key, char *out_buf, unsigned int n_out_buf);

/*
 * Queries the existence of a key.
 *
 * Parameters:
 *
 * map: A pointer to a string map. This parameter cannot be null.
 *
 * key: A pointer to a null-terminated C string. This parameter cannot
 * be null.
 *
 * Return value: 1 if the key exists, 0 otherwise.
 */
int sm_exists(const StrMap *map, const char *key);

/*
 * Associates a value with the supplied key. If the key is already
 * associated with a value, the previous value is replaced.
 *
 * Parameters:
 *
 * map: A pointer to a string map. This parameter cannot be null.
 *
 * key: A pointer to a null-terminated C string. This parameter
 * cannot be null. The string must have a string length > 0. The
 * string will be copied.
 *
 * value: A pointer to a null-terminated C string. This parameter
 * cannot be null. The string must have a string length > 0. The
 * string will be copied.
 *
 * Return value: 1 if the association succeeded, 0 otherwise.
 */
int sm_put(StrMap *map, const char *key, const char *value);

/*
 * Returns the number of associations between keys and values.
 *
 * Parameters:
 *
 * map: A pointer to a string map. This parameter cannot be null.
 *
 * Return value: The number of associations between keys and values.
 */
int sm_get_count(const StrMap *map);

/*
 * An enumerator over all associations between keys and values.
 *
 * Parameters:
 *
 * map: A pointer to a string map. This parameter cannot be null.
 *
 * enum_func: A pointer to a callback function that will be
 * called by this procedure once for every key associated
 * with a value. This parameter cannot be null.
 *
 * obj: A pointer to a client-specific object. This parameter will be
 * passed back to the client's callback function. This parameter can
 * be null.
 *
 * Return value: 1 if enumeration completed, 0 otherwise.
 */
int sm_enum(const StrMap *map, sm_enum_func enum_func, const void *obj);

#ifdef __cplusplus
}
#endif

#endif
