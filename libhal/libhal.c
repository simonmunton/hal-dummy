/***************************************************************************
 *
 * libhal.c : HAL daemon C convenience library
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 *
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>

#include "libhal.h"

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(String) dgettext (GETTEXT_PACKAGE, String)
# ifdef gettext_noop
#   define N_(String) gettext_noop (String)
# else
#   define N_(String) (String)
# endif
#else
/* Stubs that do something close enough.  */
# define textdomain(String) (String)
# define gettext(String) (String)
# define dgettext(Domain,Message) (Message)
# define dcgettext(Domain,Message,Type) (Message)
# define bindtextdomain(Domain,Directory) (Domain)
# define _(String)
# define N_(String) (String)
#endif

#include <sys/time.h>
#include <stdarg.h>


static void hal_logger(char *fmt, ...)
{
  struct timeval tv;
  FILE *fp;
  va_list ap;

  fp = fopen ("/tmp/libhal.log", "a");
  if (fp)
  {
    gettimeofday(&tv, NULL);
    fprintf(fp, "%ld.%06ld ", tv.tv_sec, tv.tv_usec);
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fprintf(fp, "\n");
    fclose(fp);
  }
}

/**
 * LIBHAL_CHECK_PARAM_VALID:
 * @_param_: the prameter to check for 
 * @_name_:  the name of the prameter (for debug output) 
 * @_ret_:   what to use for return value if the prameter is NULL
 *
 * Handy macro for checking whether a parameter is valid and not NULL.
 */
#define LIBHAL_CHECK_PARAM_VALID(_param_,_name_,_ret_)				\
	do {									\
		if (_param_ == NULL) {						\
			fprintf (stderr,					\
				 "%s %d : invalid paramater. %s is NULL.\n",  	\
				 __FILE__, __LINE__, _name_);	 		\
			return _ret_;						\
		}								\
	} while(0)

/**
 * LIBHAL_CHECK_UDI_VALID:
 * @_udi_: the UID to check for 
 * @_ret_:   what to use for return value if udi is invalid 
 *
 * Handy macro for checking whether a UID is valid and not NULL.
 */
#define LIBHAL_CHECK_UDI_VALID(_udi_,_ret_)						\
	do {										\
		if (_udi_ == NULL) {							\
			fprintf (stderr,						\
				 "%s %d : invalid udi %s. udi is NULL.\n",  		\
				 __FILE__, __LINE__, _udi_);	 			\
			return _ret_;							\
		} else {								\
			if(strncmp(_udi_, "/org/freedesktop/Hal/devices/", 29) != 0) {	\
				fprintf (stderr,					\
                                 	 "%s %d : invalid udi: %s doesn't start"	\
					 "with '/org/freedesktop/Hal/devices/'. \n",    \
	                                 __FILE__, __LINE__, _udi_);			\
				return _ret_;						\
			}								\
		}									\
	} while(0)


/**
 * libhal_free_string_array:
 * @str_array: the array to be freed
 *
 * Frees a NULL-terminated array of strings. If passed NULL, does nothing.
 */
void
libhal_free_string_array (char **str_array)
{
hal_logger("%s", __func__);
	if (str_array != NULL) {
		int i;

		for (i = 0; str_array[i] != NULL; i++) {
			free (str_array[i]);
			str_array[i] = NULL;
		}
		free (str_array);
		str_array = NULL;
	}
}


/**
 * libhal_free_string:
 * @str: the nul-terminated sting to free
 *
 * Used to free strings returned by libhal.
 */
void
libhal_free_string (char *str)
{
hal_logger("%s %p %s", __func__, str, str);

	if (str != NULL) {
		free (str);
		str = NULL;
	}
}


/**
 * LibHalPropertySet:
 *
 * Represents a set of properties. Opaque; use the
 * libhal_property_set_*() family of functions to access it.
 */
struct LibHalPropertySet_s {
	LibHalProperty *properties;
};

/**
 * LibHalProperty:
 *
 * Represents a property. Opaque.
 */
struct LibHalProperty_s {
	LibHalPropertyType type;	     	/**< Type of property */
	char *key;		     		/**< ASCII string */

	/** Possible values of the property */
	union {
		char *str_value;     		/**< UTF-8 zero-terminated string */
		dbus_int32_t int_value;         /**< 32-bit signed integer */
		dbus_uint64_t uint64_value;     /**< 64-bit unsigned integer */
		double double_value; 		/**< IEEE754 double precision float */
		dbus_bool_t bool_value;		/**< Truth value */
		char **strlist_value; 		/**< List of UTF-8 zero-terminated strings */
	} v;
};

/**
 * LibHalContext:
 *
 * Context for connection to the HAL daemon. Opaque, use the
 * libhal_ctx_*() family of functions to access it.
 */
struct LibHalContext_s {
        void *connection;                     /**< D-BUS connection */
        dbus_bool_t is_initialized;           /**< Are we initialised */
        dbus_bool_t is_shutdown;              /**< Have we been shutdown */
        dbus_bool_t cache_enabled;            /**< Is the cache enabled */
        dbus_bool_t is_direct;                /**< Whether the connection to hald is direct */

	/** Device added */
	LibHalDeviceAdded device_added;

	/** Device removed */
	LibHalDeviceRemoved device_removed;

	/** Device got a new capability */
	LibHalDeviceNewCapability device_new_capability;

	/** Device got a new capability */
	LibHalDeviceLostCapability device_lost_capability;

	/** A property of a device changed  */
	LibHalDevicePropertyModified device_property_modified;

	/** A non-continous event on the device occured  */
	LibHalDeviceCondition device_condition;
        
        /** A global interface lock is acquired  */
        LibHalGlobalInterfaceLockAcquired global_interface_lock_acquired;

        /** A global interface lock is released  */
        LibHalGlobalInterfaceLockReleased global_interface_lock_released;

        /** An interface lock is acquired  */
        LibHalInterfaceLockAcquired interface_lock_acquired;

        /** An interface lock is released  */
        LibHalInterfaceLockReleased interface_lock_released;

	/** Singleton device added */
	LibHalSingletonDeviceAdded singleton_device_added;

	/** Singleton device removed*/
	LibHalSingletonDeviceRemoved singleton_device_removed;

	void *user_data;                      /**< User data */
};

/**
 * libhal_ctx_set_user_data:
 * @ctx: the context for the connection to hald
 * @user_data: user data
 *
 * Set user data for the context.
 *
 * Returns: TRUE if user data was successfully set, FALSE if otherwise
 */
dbus_bool_t
libhal_ctx_set_user_data(LibHalContext *ctx, void *user_data)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	ctx->user_data = user_data;
	return TRUE;
}

/**
 * libhal_ctx_get_user_data:
 * @ctx: the context for the connection to hald
 *
 * Get user data for the context.
 *
 * Returns: opaque pointer stored through libhal_ctx_set_user_data() or NULL if not set.
 */
void*
libhal_ctx_get_user_data(LibHalContext *ctx)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
	return ctx->user_data;
}



/**
 * libhal_device_get_all_properties:
 * @ctx: the context for the connection to hald
 * @udi: the Unique id of device
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Retrieve all the properties on a device.
 *
 * Returns: An object represent all properties. Must be freed with libhal_free_property_set().
 */
LibHalPropertySet *
libhal_device_get_all_properties (LibHalContext *ctx, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
	LIBHAL_CHECK_UDI_VALID(udi, NULL);

	return NULL;
}


/**
 * libhal_property_set_sort:
 * @set: property-set to sort
 *
 * Sort all properties according to property name.
 */
void 
libhal_property_set_sort (LibHalPropertySet *set)
{
hal_logger("%s", __func__);
}

/**
 * libhal_free_property_set:
 * @set: property-set to free
 *
 * Free a property set earlier obtained with libhal_device_get_all_properties().
 */
void
libhal_free_property_set (LibHalPropertySet * set)
{
hal_logger("%s", __func__);
}

/**
 * libhal_property_set_get_num_elems: 
 * @set: property set to consider
 *
 * Get the number of properties in a property set.
 *
 * Returns: number of properties in given property set
 */
unsigned int 
libhal_property_set_get_num_elems (LibHalPropertySet *set)
{
hal_logger("%s", __func__);
	return 0;
}

/**
 * libhal_ps_get_type:
 * @set: property set
 * @key: name of property to inspect
 *
 * Get the type of a given property. 
 *
 * Returns: the #LibHalPropertyType of the given property, 
 * LIBHAL_PROPERTY_TYPE_INVALID if property is not in the set
 */
LibHalPropertyType
libhal_ps_get_type (const LibHalPropertySet *set, const char *key)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_PARAM_VALID(set, "*set", LIBHAL_PROPERTY_TYPE_INVALID);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", LIBHAL_PROPERTY_TYPE_INVALID);

	return LIBHAL_PROPERTY_TYPE_INVALID;
}

/**
 * libhal_ps_get_string:
 * @set: property set
 * @key: name of property to inspect
 *
 * Get the value of a property of type string.
 *
 * Returns: UTF8 nul-terminated string. This pointer is only valid
 * until libhal_free_property_set() is invoked on the property set
 * this property belongs to. NULL if property is not in the set or not a string
 */
const char *
libhal_ps_get_string  (const LibHalPropertySet *set, const char *key)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_PARAM_VALID(set, "*set", NULL);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", NULL);

	return NULL;
}

/**
 * libhal_ps_get_int:
 * @set: property set
 * @key: name of property to inspect
 *
 * Get the value of a property of type signed integer. 
 *
 * Returns: property value (32-bit signed integer)
 */
dbus_int32_t
libhal_ps_get_int32 (const LibHalPropertySet *set, const char *key)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_PARAM_VALID(set, "*set", 0);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", 0);

	return 0;
}

/**
 * libhal_ps_get_uint64:
 * @set: property set
 * @key: name of property to inspect
 *
 * Get the value of a property of type unsigned integer. 
 *
 * Returns: property value (64-bit unsigned integer)
 */
dbus_uint64_t
libhal_ps_get_uint64 (const LibHalPropertySet *set, const char *key)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_PARAM_VALID(set, "*set", 0);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", 0);

	return 0;
}

/**
 * libhal_ps_get_double:
 * @set: property set
 * @key: name of property to inspect
 *
 * Get the value of a property of type double.
 *
 * Returns: property value (IEEE754 double precision float)
 */
double
libhal_ps_get_double (const LibHalPropertySet *set, const char *key)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_PARAM_VALID(set, "*set", 0.0);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", 0.0);

	return 0.0;
}

/**
 * libhal_ps_get_bool:
 * @set: property set
 * @key: name of property to inspect
 *
 * Get the value of a property of type bool. 
 *
 * Returns: property value (bool)
 */
dbus_bool_t
libhal_ps_get_bool (const LibHalPropertySet *set, const char *key)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_PARAM_VALID(set, "*set", FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	return FALSE;
}

/**
 * libhal_ps_get_strlist:
 * @set: property set
 * @key: name of property to inspect
 *
 * Get the value of a property of type string list. 
 *
 * Returns: pointer to array of strings, this is owned by the property set
 */
const char *const *
libhal_ps_get_strlist (const LibHalPropertySet *set, const char *key)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_PARAM_VALID(set, "*set", NULL);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", NULL);

	return NULL;
}


/**
 * libhal_psi_init:
 * @iter: iterator object
 * @set: property set to iterate over
 *
 * Initialize a property set iterator.
 *
 */
void
libhal_psi_init (LibHalPropertySetIterator * iter, LibHalPropertySet * set)
{
hal_logger("%s", __func__);
	if (set == NULL)
		return;
}


/**
 * libhal_psi_has_more:
 * @iter: iterator object
 *
 * Determine whether there are more properties to iterate over.
 *
 * Returns: TRUE if there are more properties, FALSE otherwise.
 */
dbus_bool_t
libhal_psi_has_more (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return FALSE;
}

/**
 * libhal_psi_next:
 * @iter: iterator object
 *
 * Advance iterator to next property.
 */
void
libhal_psi_next (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
}

/**
 * libhal_psi_get_type:
 * @iter: iterator object
 *
 * Get type of property.
 *
 * Returns: the property type at the iterator's position
 */
LibHalPropertyType
libhal_psi_get_type (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return 0;
}

/**
 * libhal_psi_get_key:
 * @iter: iterator object
 *
 * Get the key of a property. 
 *
 * Returns: ASCII nul-terminated string. This pointer is only valid
 * until libhal_free_property_set() is invoked on the property set
 * this property belongs to.
 */
char *
libhal_psi_get_key (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return NULL;
}

/**
 * libhal_psi_get_string:
 * @iter: iterator object
 *
 * Get the value of a property of type string.
 *
 * Returns: UTF8 nul-terminated string. This pointer is only valid
 * until libhal_free_property_set() is invoked on the property set
 * this property belongs to.
 */
char *
libhal_psi_get_string (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return NULL;
}

/**
 * libhal_psi_get_int:
 * @iter: iterator object
 *
 * Get the value of a property of type signed integer. 
 *
 * Returns: property value (32-bit signed integer)
 */
dbus_int32_t
libhal_psi_get_int (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return 0;
}

/**
 * libhal_psi_get_uint64:
 * @iter: iterator object
 *
 * Get the value of a property of type unsigned integer. 
 *
 * Returns: property value (64-bit unsigned integer)
 */
dbus_uint64_t
libhal_psi_get_uint64 (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return 0;
}

/**
 * libhal_psi_get_double:
 * @iter: iterator object
 *
 * Get the value of a property of type double.
 *
 * Returns: property value (IEEE754 double precision float)
 */
double
libhal_psi_get_double (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return 0.0;
}

/**
 * libhal_psi_get_bool:
 * @iter: iterator object
 *
 * Get the value of a property of type bool. 
 *
 * Returns: property value (bool)
 */
dbus_bool_t
libhal_psi_get_bool (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return FALSE;
}

/**
 * libhal_psi_get_strlist:
 * @iter: iterator object
 *
 * Get the value of a property of type string list. 
 *
 * Returns: pointer to array of strings
 */
char **
libhal_psi_get_strlist (LibHalPropertySetIterator * iter)
{
hal_logger("%s", __func__);
	return NULL;
}


/**
 * libhal_get_all_devices:
 * @ctx: the context for the connection to hald
 * @num_devices: the number of devices will be stored here
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get all devices in the Global Device List (GDL).
 *
 * Returns: An array of device identifiers terminated with NULL. It is
 * the responsibility of the caller to free with
 * libhal_free_string_array(). If an error occurs NULL is returned.
 */
char **
libhal_get_all_devices (LibHalContext *ctx, int *num_devices, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);

	*num_devices = 0;

	return NULL;
}

/**
 * libhal_device_get_property_type:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Query a property type of a device.
 *
 * Returns: A LibHalPropertyType. LIBHAL_PROPERTY_TYPE_INVALID is
 * return if the property doesn't exist.
 */
LibHalPropertyType
libhal_device_get_property_type (LibHalContext *ctx, const char *udi, const char *key, DBusError *error)
{
hal_logger("%s %s %s", __func__, udi, key);

	LIBHAL_CHECK_LIBHALCONTEXT(ctx, LIBHAL_PROPERTY_TYPE_INVALID); /* or return NULL? */
	LIBHAL_CHECK_UDI_VALID(udi, LIBHAL_PROPERTY_TYPE_INVALID);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", LIBHAL_PROPERTY_TYPE_INVALID);

	if (strcmp(udi, "/org/freedesktop/Hal/devices/computer") == 0 &&
	    strcmp(key, "system.hardware.serial") == 0)
	    return DBUS_TYPE_STRING;

	return LIBHAL_PROPERTY_TYPE_INVALID;
}

/**
 * libhal_device_get_property_strlist:
 * @ctx: the context for the connection to hald
 * @udi: unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get the value of a property of type string list. 
 *
 * Returns: Array of pointers to UTF8 nul-terminated strings
 * terminated by NULL. The caller is responsible for freeing this
 * string array with the function libhal_free_string_array(). Returns
 * NULL if the property didn't exist or we are OOM
 */
char **
libhal_device_get_property_strlist (LibHalContext *ctx, const char *udi, const char *key, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
	LIBHAL_CHECK_UDI_VALID(udi, NULL);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", NULL);

	return NULL;
}

/**
 * libhal_device_get_property_string:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: the name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 * 
 * Get the value of a property of type string. 
 *
 * Returns: UTF8 nul-terminated string. The caller is responsible for
 * freeing this string with the function libhal_free_string(). Returns
 * NULL if the property didn't exist or we are OOM.
 */
char *
libhal_device_get_property_string (LibHalContext *ctx,
				   const char *udi, const char *key, DBusError *error)
{
hal_logger("%s %s %s", __func__, udi, key);

	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
	LIBHAL_CHECK_UDI_VALID(udi, NULL);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", NULL);

	if (strcmp(udi, "/org/freedesktop/Hal/devices/computer") == 0 &&
	    strcmp(key, "system.hardware.serial") == 0)
	    return strdup("System Serial Number");

	return NULL;
}

/**
 * libhal_device_get_property_int:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get the value of a property of type integer. 
 *
 * Returns: Property value (32-bit signed integer)
 */
dbus_int32_t
libhal_device_get_property_int (LibHalContext *ctx, 
				const char *udi, const char *key, DBusError *error)
{
hal_logger("%s %s %s", __func__, udi, key);

	LIBHAL_CHECK_LIBHALCONTEXT(ctx, -1);
	LIBHAL_CHECK_UDI_VALID(udi, -1);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", -1);

	return -1;
}

/**
 * libhal_device_get_property_uint64:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get the value of a property of type signed integer.
 *
 * Returns: Property value (64-bit unsigned integer)
 */
dbus_uint64_t
libhal_device_get_property_uint64 (LibHalContext *ctx, 
				   const char *udi, const char *key, DBusError *error)
{
hal_logger("%s %s %s", __func__, udi, key);

	LIBHAL_CHECK_LIBHALCONTEXT(ctx, -1);
	LIBHAL_CHECK_UDI_VALID(udi, -1);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", -1);

	return -1;
}

/**
 * libhal_device_get_property_double:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get the value of a property of type double.
 *
 * Returns: Property value (IEEE754 double precision float)
 */
double
libhal_device_get_property_double (LibHalContext *ctx, 
				   const char *udi, const char *key, DBusError *error)
{
hal_logger("%s %s %s", __func__, udi, key);

	LIBHAL_CHECK_LIBHALCONTEXT(ctx, -1.0);
	LIBHAL_CHECK_UDI_VALID(udi, -1.0);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", -1.0);

	return -1.0f;
}

/**
 * libhal_device_get_property_bool:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Get the value of a property of type bool. 
 *
 * Returns: Property value (boolean)
 */
dbus_bool_t
libhal_device_get_property_bool (LibHalContext *ctx, 
				 const char *udi, const char *key, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	return FALSE;
}


/**
 * libhal_device_set_property_string:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @value: value of the property; a UTF8 string
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Set a property of type string.
 *
 * Returns: TRUE if the property was set, FALSE if the device didn't
 * exist or the property had a different type.
 */
dbus_bool_t
libhal_device_set_property_string (LibHalContext *ctx, 
				   const char *udi,
				   const char *key, 
				   const char *value,
				   DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	LIBHAL_CHECK_PARAM_VALID(value, "*value", FALSE);

	return FALSE;
}

/**
 * libhal_device_set_property_int:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @value: value of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Set a property of type signed integer.
 *
 * Returns: TRUE if the property was set, FALSE if the device didn't
 * exist or the property had a different type.
 */
dbus_bool_t
libhal_device_set_property_int (LibHalContext *ctx, const char *udi,
				const char *key, dbus_int32_t value, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	return FALSE;
}

/**
 * libhal_device_set_property_uint64:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @value: value of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Set a property of type unsigned integer.
 *
 * Returns: TRUE if the property was set, FALSE if the device didn't
 * exist or the property had a different type.
 */
dbus_bool_t
libhal_device_set_property_uint64 (LibHalContext *ctx, const char *udi,
				   const char *key, dbus_uint64_t value, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	
	return FALSE;
}

/**
 * libhal_device_set_property_double:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @value: value of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Set a property of type double.
 *
 * Returns: TRUE if the property was set, FALSE if the device didn't
 * exist or the property had a different type.
 */
dbus_bool_t
libhal_device_set_property_double (LibHalContext *ctx, const char *udi,
				   const char *key, double value, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	
	return FALSE;
}

/**
 * libhal_device_set_property_bool:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @value: value of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Set a property of type bool.
 *
 * Returns: TRUE if the property was set, FALSE if the device didn't
 * exist or the property had a different type.
 */
dbus_bool_t
libhal_device_set_property_bool (LibHalContext *ctx, const char *udi,
				 const char *key, dbus_bool_t value, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	
	return FALSE;
}


/**
 * libhal_device_remove_property:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Remove a property.
 *
 * Returns: TRUE if the property was set, FALSE if the device didn't
 * exist
 */
dbus_bool_t
libhal_device_remove_property (LibHalContext *ctx, 
			       const char *udi, const char *key, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	
	return FALSE;
}

/**
 * libhal_device_property_strlist_append:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @value: value to append to property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Append to a property of type strlist.
 *
 * Returns: TRUE if the value was appended, FALSE if the device didn't
 * exist or the property had a different type.
 */
dbus_bool_t
libhal_device_property_strlist_append (LibHalContext *ctx, 
				       const char *udi,
				       const char *key,
				       const char *value,
				       DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	LIBHAL_CHECK_PARAM_VALID(value, "*value", FALSE);

	return FALSE;
}

/**
 * libhal_device_property_strlist_prepend:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @value: value to prepend to property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Prepend to a property of type strlist.
 *
 * Returns: TRUE if the value was prepended, FALSE if the device
 * didn't exist or the property had a different type.
 */
dbus_bool_t
libhal_device_property_strlist_prepend (LibHalContext *ctx, 
					const char *udi,
					const char *key,
					const char *value, 
					DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	LIBHAL_CHECK_PARAM_VALID(value, "*value", FALSE);

	return FALSE;
}

/**
 * libhal_device_property_strlist_remove_index:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @idx: index of string to remove in the strlist
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Remove a specified string from a property of type strlist.
 *
 * Returns: TRUE if the string was removed, FALSE if the device didn't
 * exist or the property had a different type.
 */
dbus_bool_t
libhal_device_property_strlist_remove_index (LibHalContext *ctx, 
					     const char *udi,
					     const char *key,
					     unsigned int idx,
					     DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	return FALSE;
}

/**
 * libhal_device_property_strlist_remove:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @key: name of the property
 * @value: the string to remove
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Remove a specified string from a property of type strlist.
 *
 * Returns: TRUE if the string was removed, FALSE if the device didn't
 * exist or the property had a different type.
 */
dbus_bool_t
libhal_device_property_strlist_remove (LibHalContext *ctx, 
				       const char *udi,
				       const char *key,
				       const char *value, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	LIBHAL_CHECK_PARAM_VALID(value, "*value", FALSE);

	return FALSE;
}


/**
 * libhal_device_lock:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @reason_to_lock: a user-presentable reason why the device is locked.
 * @reason_why_locked: a pointer to store the reason why the device cannot be locked on failure, or NULL
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Take an advisory lock on the device.
 *
 * Returns: TRUE if the lock was obtained, FALSE otherwise
 */
dbus_bool_t
libhal_device_lock (LibHalContext *ctx,
		    const char *udi,
		    const char *reason_to_lock,
		    char **reason_why_locked, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	if (reason_why_locked != NULL)
		*reason_why_locked = NULL;

	return FALSE;
}

/**
 * libhal_device_unlock:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Release an advisory lock on the device.
 *
 * Returns: TRUE if the device was successfully unlocked,
 *                              FALSE otherwise
 */
dbus_bool_t
libhal_device_unlock (LibHalContext *ctx,
		      const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}


/**
 * libhal_new_device:
 * @ctx: the context for the connection to hald
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Create a new device object which will be hidden from applications
 * until the CommitToGdl(), ie. libhal_device_commit_to_gdl(), method
 * is called. Note that the program invoking this method needs to run
 * with super user privileges.
 *
 * Returns: Temporary device unique id or NULL if there was a
 * problem. This string must be freed by the caller.
 */
char *
libhal_new_device (LibHalContext *ctx, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);

	return NULL;
}


/**
 * libhal_device_commit_to_gdl:
 * @ctx: the context for the connection to hald
 * @temp_udi: the temporary unique device id as returned by libhal_new_device()
 * @udi: the new unique device id.
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * When a hidden device has been built using the NewDevice method,
 * ie. libhal_new_device(), and the org.freedesktop.Hal.Device
 * interface this function will commit it to the global device list.
 *
 * This means that the device object will be visible to applications
 * and the HAL daemon will possibly attempt to boot the device
 * (depending on the property RequireEnable).
 *
 * Note that the program invoking this method needs to run with super
 * user privileges.
 *
 * Returns: FALSE if the given unique device id is already in use.
 */
dbus_bool_t
libhal_device_commit_to_gdl (LibHalContext *ctx, 
			     const char *temp_udi, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(temp_udi, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}

/**
 * libhal_remove_device:
 * @ctx: the context for the connection to hald
 * @udi: the Unique device id.
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * This method can be invoked when a device is removed. The HAL daemon
 * will shut down the device. Note that the device may still be in the
 * device list if the Persistent property is set to true.
 *
 * Note that the program invoking this method needs to run with super
 * user privileges.
 *
 * Returns: TRUE if the device was removed, FALSE otherwise
 */
dbus_bool_t
libhal_remove_device (LibHalContext *ctx, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}

/**
 * libhal_device_exists:
 * @ctx: the context for the connection to hald
 * @udi: the Unique device id.
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Determine if a device exists.
 *
 * Returns: TRUE if the device exists
 */
dbus_bool_t
libhal_device_exists (LibHalContext *ctx, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}

/**
 * libhal_device_property_exists:
 * @ctx: the context for the connection to hald
 * @udi: the Unique device id.
 * @key: name of the property
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Determine if a property on a device exists.
 *
 * Returns: TRUE if the device exists, FALSE otherwise
 */
dbus_bool_t
libhal_device_property_exists (LibHalContext *ctx, 
			       const char *udi, const char *key, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	return FALSE;
}

/**
 * libhal_merge_properties:
 * @ctx: the context for the connection to hald
 * @target_udi: the Unique device id of target device to merge to
 * @source_udi: the Unique device id of device to merge from
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Merge properties from one device to another.
 *
 * Returns: TRUE if the properties were merged, FALSE otherwise
 */
dbus_bool_t
libhal_merge_properties (LibHalContext *ctx, 
			 const char *target_udi, const char *source_udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(target_udi, FALSE);
	LIBHAL_CHECK_UDI_VALID(source_udi, FALSE);

	return FALSE;
}

/**
 * libhal_device_matches:
 * @ctx: the context for the connection to hald
 * @udi1: the Unique Device Id for device 1
 * @udi2: the Unique Device Id for device 2
 * @property_namespace: the namespace for set of devices, e.g. "usb"
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Check a set of properties for two devices matches. 
 *
 * Checks that all properties where keys, starting with a given value
 * (namespace), of the first device is in the second device and that
 * they got the same value and type.
 *
 * Note that the other inclusion isn't tested, so there could be
 * properties (from the given namespace) in the second device not
 * present in the first device.
 *
 * Returns: TRUE if all properties starting with the given namespace
 * parameter from one device is in the other and have the same value.
 */
dbus_bool_t
libhal_device_matches (LibHalContext *ctx, 
		       const char *udi1, const char *udi2,
		       const char *property_namespace, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi1, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi2, FALSE);
	LIBHAL_CHECK_PARAM_VALID(property_namespace, "*property_namespace", FALSE);

	return FALSE;
}

/**
 * libhal_device_print:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Print a device to stdout; useful for debugging.
 *
 * Returns: TRUE if device's information could be obtained, FALSE otherwise
 */
dbus_bool_t
libhal_device_print (LibHalContext *ctx, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}

/**
 * libhal_manager_find_device_string_match:
 * @ctx: the context for the connection to hald
 * @key: name of the property
 * @value: the value to match
 * @num_devices: pointer to store number of devices
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Find a device in the GDL where a single string property matches a
 * given value.
 *
 * Returns: UDI of devices; free with libhal_free_string_array()
 */
char **
libhal_manager_find_device_string_match (LibHalContext *ctx, 
					 const char *key,
					 const char *value, int *num_devices, DBusError *error)
{
hal_logger("%s %s %s", __func__, key, value);

	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", NULL);
	LIBHAL_CHECK_PARAM_VALID(value, "*value", NULL);

	*num_devices = 0;

	return NULL;
}


/**
 * libhal_device_add_capability:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @capability: the capability name to add
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Assign a capability to a device.
 *
 * Returns: TRUE if the capability was added, FALSE if the device didn't exist
 */
dbus_bool_t
libhal_device_add_capability (LibHalContext *ctx, 
			      const char *udi, const char *capability, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(capability, "*capability", FALSE);

	return FALSE;
}

/**
 * libhal_device_query_capability:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @capability: the capability name
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Check if a device has a capability. The result is undefined if the
 * device doesn't exist.
 *
 * Returns: TRUE if the device has the capability, otherwise FALSE
 */
dbus_bool_t
libhal_device_query_capability (LibHalContext *ctx, const char *udi, const char *capability, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(capability, "*capability", FALSE);

	return FALSE;
}

/**
 * libhal_find_device_by_capability:
 * @ctx: the context for the connection to hald
 * @capability: the capability name
 * @num_devices: pointer to store number of devices
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Find devices with a given capability.
 *
 * Returns: UDI of devices; free with libhal_free_string_array()
 */
char **
libhal_find_device_by_capability (LibHalContext *ctx, 
				  const char *capability, int *num_devices, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
	LIBHAL_CHECK_PARAM_VALID(capability, "*capability", NULL);

	return NULL;
}

/**
 * libhal_device_property_watch_all:
 * @ctx: the context for the connection to hald
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Watch all devices, ie. the device_property_changed callback is
 * invoked when the properties on any device changes.
 *
 * Returns: TRUE only if the operation succeeded
 */
dbus_bool_t
libhal_device_property_watch_all (LibHalContext *ctx, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	return FALSE;
}


dbus_bool_t
libhal_device_property_remove_watch_all (LibHalContext *ctx, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	return FALSE;
}


/**
 * libhal_device_add_property_watch:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Add a watch on a device, so the device_property_changed callback is
 * invoked when the properties on the given device changes.
 *
 * The application itself is responsible for deleting the watch, using
 * libhal_device_remove_property_watch, if the device is removed.
 *
 * Returns: TRUE only if the operation succeeded
 */
dbus_bool_t
libhal_device_add_property_watch (LibHalContext *ctx, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}


/**
 * libhal_device_remove_property_watch:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Remove a watch on a device.
 *
 * Returns: TRUE only if the operation succeeded
 */
dbus_bool_t
libhal_device_remove_property_watch (LibHalContext *ctx, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	return FALSE;
}


/**
 * libhal_ctx_new:
 *
 * Create a new LibHalContext
 *
 * Returns: a new uninitialized LibHalContext object
 */
LibHalContext *
libhal_ctx_new (void)
{
	LibHalContext *ctx;
hal_logger("%s", __func__);

	ctx = calloc (1, sizeof (LibHalContext));
	if (ctx == NULL) {
		fprintf (stderr, 
			 "%s %d : Failed to allocate %lu bytes\n",
			 __FILE__, __LINE__, (unsigned long) sizeof (LibHalContext));
		return NULL;
	}

	ctx->is_initialized = FALSE;
	ctx->is_shutdown = FALSE;
	ctx->connection = NULL;
	ctx->is_direct = FALSE;

	return ctx;
}

/**
 * libhal_ctx_set_cache:
 * @ctx: context to enable/disable cache for
 * @use_cache: whether or not to use cache
 *
 * Enable or disable caching. Note: Caching is not actually
 * implemented yet.
 *
 * Returns: TRUE if cache was successfully enabled/disabled, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_cache (LibHalContext *ctx, dbus_bool_t use_cache)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->cache_enabled = use_cache;
	return TRUE;
}

/**
 * libhal_ctx_set_dbus_connection:
 * @ctx: context to set connection for
 * @conn: DBus connection to use
 *
 * Set DBus connection to use to talk to hald.
 *
 * Returns: TRUE if connection was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_dbus_connection (LibHalContext *ctx, DBusConnection *conn)
{
hal_logger("%s %p %p", __func__, ctx, conn);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	if (conn == NULL)
		return FALSE;

	ctx->connection = conn;
	return TRUE;
}

/**
 * libhal_ctx_get_dbus_connection:
 * @ctx: context to get connection for
 *
 * Get DBus connection used for talking to hald.
 *
 * Returns: DBus connection to use or NULL
 */
DBusConnection *
libhal_ctx_get_dbus_connection (LibHalContext *ctx)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);

	return ctx->connection;
}


/**
 * libhal_ctx_init:
 * @ctx: Context for connection to hald (D-BUS connection should be set with libhal_ctx_set_dbus_connection)
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Initialize the connection to hald.
 *
 * Returns: TRUE if initialization succeeds, FALSE otherwise
 */
dbus_bool_t 
libhal_ctx_init (LibHalContext *ctx, DBusError *error)
{
hal_logger("%s %p %p", __func__, ctx, error);

	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	if (ctx->connection == NULL)
		return FALSE;

	ctx->is_initialized = TRUE;
	ctx->is_direct = FALSE;

	return TRUE;
}

/**
 * libhal_ctx_init_direct:
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Create an already initialized connection to hald. This function should only be used by HAL helpers.
 *
 * Returns: A pointer to an already initialized LibHalContext
 */
LibHalContext *
libhal_ctx_init_direct (DBusError *error)
{
	LibHalContext *ctx;

hal_logger("%s", __func__);
	ctx = libhal_ctx_new ();
	if (ctx == NULL)
		goto out;

	if (getenv ("HALD_DIRECT_ADDR") == NULL) {
		libhal_ctx_free (ctx);
		ctx = NULL;
		goto out;
	}

	ctx->is_initialized = TRUE;
	ctx->is_direct = TRUE;

out:
	return ctx;
}

/**
 * libhal_ctx_shutdown:
 * @ctx: the context for the connection to hald
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Shut down a connection to hald.
 *
 * Returns: TRUE if connection successfully shut down, FALSE otherwise
 */
dbus_bool_t    
libhal_ctx_shutdown (LibHalContext *ctx, DBusError *error)
{
hal_logger("%s %p %p", __func__, ctx, error);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->is_initialized = FALSE;

	return TRUE;
}

/**
 * libhal_ctx_free:
 * @ctx: pointer to a LibHalContext
 *
 * Free a LibHalContext resource.
 *
 * Returns: TRUE
 */
dbus_bool_t    
libhal_ctx_free (LibHalContext *ctx)
{
hal_logger("%s %p", __func__, ctx);
	free (ctx);
	return TRUE;
}

/**
 * libhal_ctx_set_device_added:
 * @ctx: the context for the connection to hald
 * @callback: the function to call when a device is added
 *
 * Set the callback for when a device is added
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_device_added (LibHalContext *ctx, LibHalDeviceAdded callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->device_added = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_device_removed:
 * @ctx: the context for the connection to hald
 * @callback: the function to call when a device is removed
 *
 * Set the callback for when a device is removed.
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_device_removed (LibHalContext *ctx, LibHalDeviceRemoved callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->device_removed = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_device_new_capability:
 * @ctx: the context for the connection to hald
 * @callback: the function to call when a device gains a new capability
 *
 * Set the callback for when a device gains a new capability.
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_device_new_capability (LibHalContext *ctx, LibHalDeviceNewCapability callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->device_new_capability = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_device_lost_capability:
 * @ctx: the context for the connection to hald
 * @callback: the function to call when a device loses a capability
 *
 * Set the callback for when a device loses a capability
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_device_lost_capability (LibHalContext *ctx, LibHalDeviceLostCapability callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->device_lost_capability = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_device_property_modified:
 * @ctx: the context for the connection to hald
 * @callback: the function to call when a property is modified on a device
 *
 * Set the callback for when a property is modified on a device.
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_device_property_modified (LibHalContext *ctx, LibHalDevicePropertyModified callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->device_property_modified = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_device_condition:
 * @ctx: the context for the connection to hald
 * @callback: the function to call when a device emits a condition
 *
 * Set the callback for when a device emits a condition
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_device_condition (LibHalContext *ctx, LibHalDeviceCondition callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->device_condition = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_singleton_device_added:
 * @ctx: the context for the connection to hald
 * @callback: the function to call when a device emits a condition
 *
 * Set the callback for when a singleton should handle a new device
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_singleton_device_added (LibHalContext *ctx, LibHalSingletonDeviceAdded callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->singleton_device_added = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_singleton_device_removed:
 * @ctx: the context for the connection to hald
 * @callback: the function to call when a device emits a condition
 *
 * Set the callback for when a singleton should discard a device
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_singleton_device_removed (LibHalContext *ctx, LibHalSingletonDeviceRemoved callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);

	ctx->singleton_device_removed = callback;
	return TRUE;
}

/**
 * libhal_string_array_length:
 * @str_array: array of strings to consider
 *
 * Get the length of an array of strings.
 *
 * Returns: Number of strings in array
 */
unsigned int 
libhal_string_array_length (char **str_array)
{
	unsigned int i;
hal_logger("%s", __func__);

	if (str_array == NULL)
		return 0;

	for (i = 0; str_array[i] != NULL; i++)
		;

	return i;
}


/**
 * libhal_device_rescan:
 * @ctx: the context for the connection to hald
 * @udi: the Unique id of device
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * TODO document me.
 *
 * Returns: Whether the operation succeeded
 */
dbus_bool_t 
libhal_device_rescan (LibHalContext *ctx, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}

/**
 * libhal_device_reprobe:
 * @ctx: the context for the connection to hald
 * @udi: the Unique id of device
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * TODO document me.
 *
 * Returns: Whether the operation succeeded
 */
dbus_bool_t
libhal_device_reprobe (LibHalContext *ctx, const char *udi, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}

/**
 * libhal_device_emit_condition:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @condition_name: user-readable name of condition
 * @condition_details: user-readable details of condition
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Emit a condition from a device. Can only be used from hald helpers.
 *
 * Returns: TRUE if condition successfully emitted,
 *                              FALSE otherwise
 */
dbus_bool_t libhal_device_emit_condition (LibHalContext *ctx,
					  const char *udi,
					  const char *condition_name,
					  const char *condition_details,
					  DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(condition_name, "*condition_name", FALSE);
	LIBHAL_CHECK_PARAM_VALID(condition_details, "*condition_details", FALSE);

	return FALSE;
}


/**
 * libhal_device_addon_is_ready:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id this addon is handling
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * HAL addon's must call this method when they are done initializing the device object. The HAL
 * daemon will wait for all addon's to call this.
 *
 * Can only be used from hald helpers.
 *
 * Returns: TRUE if the HAL daemon received the message, FALSE otherwise
 */
dbus_bool_t
libhal_device_addon_is_ready (LibHalContext *ctx,
			      const char *udi,
			      DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);

	return FALSE;
}

/**
 * libhal_device_singleton_addon_is_ready:
 * @ctx: the context for the connection to hald
 * @command_line: commandline singleton was started with
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * HAL singleton addon's must call this method when they are done initializing the device object. The HAL
 * daemon will wait for all addon's to call this.
 *
 * Can only be used from hald helpers.
 *
 * Returns: TRUE if the HAL daemon received the message, FALSE otherwise
 */
dbus_bool_t
libhal_device_singleton_addon_is_ready (LibHalContext *ctx,
					const char *command_line,
					DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_PARAM_VALID(command_line, "*command_line", FALSE);

	return FALSE;
}


/**
 * libhal_device_claim_interface:
 * @ctx: the context for the connection to hald
 * @udi: the Unique Device Id
 * @interface_name: Name of interface to claim, e.g. org.freedesktop.Hal.Device.FoobarKindOfThing
 * @introspection_xml: Introspection XML containing what would be inside the interface XML tag
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 *
 * Claim an interface for a device. All messages to this interface
 * will be forwarded to the helper. Can only be used from hald
 * helpers.
 *
 * Returns: TRUE if interface was claimed, FALSE otherwise
 */
dbus_bool_t
libhal_device_claim_interface (LibHalContext *ctx,
			       const char *udi,
			       const char *interface_name,
			       const char *introspection_xml,
			       DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(interface_name, "*interface_name", FALSE);

	return FALSE;
}



struct LibHalChangeSetElement_s;

typedef struct LibHalChangeSetElement_s LibHalChangeSetElement;

struct LibHalChangeSetElement_s {
	char *key;
	int change_type;
	union {
		char *val_str;
		dbus_int32_t val_int;
		dbus_uint64_t val_uint64;
		double val_double;
		dbus_bool_t val_bool;
		char **val_strlist;
	} value;
	LibHalChangeSetElement *next;
	LibHalChangeSetElement *prev;
};

struct LibHalChangeSet_s {
	char *udi;
	LibHalChangeSetElement *head;
	LibHalChangeSetElement *tail;
};

/**
 * libhal_device_new_changeset:
 * @udi: unique device identifier
 *
 * Request a new changeset object. Used for changing multiple properties at once. Useful when
 * performance is critical and also for atomically updating several properties.
 * 
 * Returns: A new changeset object or NULL on error
 */
LibHalChangeSet *
libhal_device_new_changeset (const char *udi)
{
	LibHalChangeSet *changeset;
hal_logger("%s", __func__);

	LIBHAL_CHECK_UDI_VALID(udi, NULL);

	changeset = calloc (1, sizeof (LibHalChangeSet));
	if (changeset == NULL)
		goto out;

	changeset->udi = strdup (udi);
	if (changeset->udi == NULL) {
		free (changeset);
		changeset = NULL;
		goto out;
	}

	changeset->head = NULL;
	changeset->tail = NULL;

out:
	return changeset;
}

static void
libhal_changeset_append (LibHalChangeSet *changeset, LibHalChangeSetElement *elem)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_PARAM_VALID(changeset, "*changeset", (void) NULL );
	LIBHAL_CHECK_PARAM_VALID(elem, "*elem", (void) NULL);

	if (changeset->head == NULL) {
		changeset->head = elem;
		changeset->tail = elem;
		elem->next = NULL;
		elem->prev = NULL;
	} else {
		elem->prev = changeset->tail;
		elem->next = NULL;
		elem->prev->next = elem;
		changeset->tail = elem;
	}
}


/**
 * libhal_changeset_set_property_string:
 * @changeset: the changeset
 * @key: key of property 
 * @value: the value to set
 * 
 * Set a property.
 * 
 * Returns: FALSE on OOM
 */
dbus_bool_t
libhal_changeset_set_property_string (LibHalChangeSet *changeset, const char *key, const char *value)
{
	LibHalChangeSetElement *elem;

hal_logger("%s", __func__);

	LIBHAL_CHECK_PARAM_VALID(changeset, "*changeset", FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);
	LIBHAL_CHECK_PARAM_VALID(value, "*value", FALSE);

	elem = calloc (1, sizeof (LibHalChangeSetElement));
	if (elem == NULL)
		goto out;
	elem->key = strdup (key);
	if (elem->key == NULL) {
		free (elem);
		elem = NULL;
		goto out;
	}

	elem->change_type = LIBHAL_PROPERTY_TYPE_STRING;
	elem->value.val_str = strdup (value);
	if (elem->value.val_str == NULL) {
		free (elem->key);
		free (elem);
		elem = NULL;
		goto out;
	}

	libhal_changeset_append (changeset, elem);
out:
	return elem != NULL;
}

/**
 * libhal_changeset_set_property_int:
 * @changeset: the changeset
 * @key: key of property 
 * @value: the value to set
 * 
 * Set a property.
 * 
 * Returns: FALSE on OOM
 */
dbus_bool_t
libhal_changeset_set_property_int (LibHalChangeSet *changeset, const char *key, dbus_int32_t value)
{
	LibHalChangeSetElement *elem;

hal_logger("%s", __func__);

	LIBHAL_CHECK_PARAM_VALID(changeset, "*changeset", FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	elem = calloc (1, sizeof (LibHalChangeSetElement));
	if (elem == NULL)
		goto out;
	elem->key = strdup (key);
	if (elem->key == NULL) {
		free (elem);
		elem = NULL;
		goto out;
	}

	elem->change_type = LIBHAL_PROPERTY_TYPE_INT32;
	elem->value.val_int = value;

	libhal_changeset_append (changeset, elem);
out:
	return elem != NULL;
}

/**
 * libhal_changeset_set_property_uint64:
 * @changeset: the changeset
 * @key: key of property 
 * @value: the value to set
 * 
 * Set a property.
 * 
 * Returns: FALSE on OOM
 */
dbus_bool_t
libhal_changeset_set_property_uint64 (LibHalChangeSet *changeset, const char *key, dbus_uint64_t value)
{
	LibHalChangeSetElement *elem;

hal_logger("%s", __func__);

	LIBHAL_CHECK_PARAM_VALID(changeset, "*changeset", FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	elem = calloc (1, sizeof (LibHalChangeSetElement));
	if (elem == NULL)
		goto out;
	elem->key = strdup (key);
	if (elem->key == NULL) {
		free (elem);
		elem = NULL;
		goto out;
	}

	elem->change_type = LIBHAL_PROPERTY_TYPE_UINT64;
	elem->value.val_uint64 = value;

	libhal_changeset_append (changeset, elem);
out:
	return elem != NULL;
}

/**
 * libhal_changeset_set_property_double:
 * @changeset: the changeset
 * @key: key of property 
 * @value: the value to set
 * 
 * Set a property.
 * 
 * Returns: FALSE on OOM
 */
dbus_bool_t
libhal_changeset_set_property_double (LibHalChangeSet *changeset, const char *key, double value)
{
	LibHalChangeSetElement *elem;

hal_logger("%s", __func__);

	LIBHAL_CHECK_PARAM_VALID(changeset, "*changeset", FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	elem = calloc (1, sizeof (LibHalChangeSetElement));
	if (elem == NULL)
		goto out;
	elem->key = strdup (key);
	if (elem->key == NULL) {
		free (elem);
		elem = NULL;
		goto out;
	}

	elem->change_type = LIBHAL_PROPERTY_TYPE_DOUBLE;
	elem->value.val_double = value;

	libhal_changeset_append (changeset, elem);
out:
	return elem != NULL;
}

/**
 * libhal_changeset_set_property_bool:
 * @changeset: the changeset
 * @key: key of property 
 * @value: the value to set
 * 
 * Set a property.
 * 
 * Returns: FALSE on OOM
 */
dbus_bool_t
libhal_changeset_set_property_bool (LibHalChangeSet *changeset, const char *key, dbus_bool_t value)
{
	LibHalChangeSetElement *elem;

hal_logger("%s", __func__);

	LIBHAL_CHECK_PARAM_VALID(changeset, "*changeset", FALSE);
	LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	elem = calloc (1, sizeof (LibHalChangeSetElement));
	if (elem == NULL)
		goto out;
	elem->key = strdup (key);
	if (elem->key == NULL) {
		free (elem);
		elem = NULL;
		goto out;
	}

	elem->change_type = LIBHAL_PROPERTY_TYPE_BOOLEAN;
	elem->value.val_bool = value;

	libhal_changeset_append (changeset, elem);
out:
	return elem != NULL;
}

/**
 * libhal_changeset_set_property_strlist:
 * @changeset: the changeset
 * @key: key of property 
 * @value: the value to set - NULL terminated array of strings
 * 
 * Set a property.
 * 
 * Returns: FALSE on OOM
 */
dbus_bool_t
libhal_changeset_set_property_strlist (LibHalChangeSet *changeset, const char *key, const char **value)
{
	LibHalChangeSetElement *elem;
	char **value_copy;
	int len;
	int i, j;

hal_logger("%s", __func__);

        LIBHAL_CHECK_PARAM_VALID(changeset, "*changeset", FALSE);
        LIBHAL_CHECK_PARAM_VALID(key, "*key", FALSE);

	elem = calloc (1, sizeof (LibHalChangeSetElement));
	if (elem == NULL)
		goto out;
	elem->key = strdup (key);
	if (elem->key == NULL) {
		free (elem);
		elem = NULL;
		goto out;
	}

	for (i = 0; value[i] != NULL; i++)
		;
	len = i;

	value_copy = calloc (len + 1, sizeof (char *));
	if (value_copy == NULL) {
		free (elem->key);
		free (elem);
		elem = NULL;
		goto out;
	}

	for (i = 0; i < len; i++) {
		value_copy[i] = strdup (value[i]);
		if (value_copy[i] == NULL) {
			for (j = 0; j < i; j++) {
				free (value_copy[j]);
			}
			free (value_copy);
			free (elem->key);
			free (elem);
			elem = NULL;
			goto out;
		}
	}
	value_copy[i] = NULL;

	elem->change_type = LIBHAL_PROPERTY_TYPE_STRLIST;
	elem->value.val_strlist = value_copy;

	libhal_changeset_append (changeset, elem);
out:
	return elem != NULL;
}

/**
 * libhal_device_commit_changeset:
 * @ctx: the context for the connection to hald
 * @changeset: the changeset to commit
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 * 
 * Commit a changeset to the daemon.
 * 
 * Returns: True if the changeset was committed on the daemon side
 */
dbus_bool_t
libhal_device_commit_changeset (LibHalContext *ctx, LibHalChangeSet *changeset, DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(changeset->udi, FALSE);

	return FALSE;
}

/**
 * libhal_device_free_changeset:
 * @changeset: the changeset to free
 * 
 * Free a changeset.
 */
void
libhal_device_free_changeset (LibHalChangeSet *changeset)
{
	LibHalChangeSetElement *elem;
	LibHalChangeSetElement *elem2;

hal_logger("%s", __func__);

	for (elem = changeset->head; elem != NULL; elem = elem2) {
		elem2 = elem->next;

		switch (elem->change_type) {
		case LIBHAL_PROPERTY_TYPE_STRING:
			free (elem->value.val_str);
			break;
		case LIBHAL_PROPERTY_TYPE_STRLIST:
			libhal_free_string_array (elem->value.val_strlist);
			break;
                /* explicit fallthrough */
		case LIBHAL_PROPERTY_TYPE_INT32:
		case LIBHAL_PROPERTY_TYPE_UINT64:
		case LIBHAL_PROPERTY_TYPE_DOUBLE:
		case LIBHAL_PROPERTY_TYPE_BOOLEAN:
			break;
		default:
			fprintf (stderr, "%s %d : unknown change_type %d\n", __FILE__, __LINE__, elem->change_type);
			break;
		}
		free (elem->key);
		free (elem);
	}

	free (changeset->udi);
	free (changeset);
}


/**
 * libhal_device_acquire_interface_lock:
 * @ctx: the context for the connection to hald
 * @udi: the Unique id of device
 * @interface: the intername name to lock
 * @exclusive: whether the lock should be exclusive
 * @error: pointer to an initialized dbus error object for returning errors
 * 
 * Releases a lock on an interface for a specific device. 
 * 
 * Returns: TRUE iff the lock was acquired
 **/
dbus_bool_t 
libhal_device_acquire_interface_lock (LibHalContext *ctx,
                                      const char *udi,
                                      const char *interface,
                                      dbus_bool_t exclusive,
                                      DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(interface, "*interface", FALSE);

	return FALSE;
}

/**
 * libhal_device_release_interface_lock:
 * @ctx: the context for the connection to hald
 * @udi: the Unique id of device
 * @interface: the intername name to unlock
 * @error: pointer to an initialized dbus error object for returning errors
 * 
 * Acquires a lock on an interface for a specific device.
 * 
 * Returns: TRUE iff the lock was released.
 **/
dbus_bool_t libhal_device_release_interface_lock (LibHalContext *ctx,
                                                  const char *udi,
                                                  const char *interface,
                                                  DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_UDI_VALID(udi, FALSE);
	LIBHAL_CHECK_PARAM_VALID(interface, "*interface", FALSE);

	return FALSE;
}

/**
 * libhal_acquire_global_interface_lock:
 * @ctx: the context for the connection to hald
 * @interface: the intername name to lock
 * @exclusive: whether the lock should be exclusive
 * @error: pointer to an initialized dbus error object for returning errors
 * 
 * Acquires a global lock on an interface.
 * 
 * Returns: TRUE iff the lock was acquired
 **/
dbus_bool_t libhal_acquire_global_interface_lock (LibHalContext *ctx,
                                                  const char *interface,
                                                  dbus_bool_t exclusive,
                                                  DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_PARAM_VALID(interface, "*interface", FALSE);

	return FALSE;
}

/**
 * libhal_release_global_interface_lock:
 * @ctx: the context for the connection to hald
 * @interface: the intername name to unlock
 * @error: pointer to an initialized dbus error object for returning errors
 * 
 * Releases a global lock on an interface.
 * 
 * Returns: TRUE iff the lock was released
 **/
dbus_bool_t libhal_release_global_interface_lock (LibHalContext *ctx,
                                                  const char *interface,
                                                  DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, FALSE);
	LIBHAL_CHECK_PARAM_VALID(interface, "*interface", FALSE);

	return FALSE;
}

/**
 * libhal_device_is_caller_locked_out:
 * @ctx: the context for the connection to hald
 * @udi: the Unique id of device
 * @interface: the intername name to check
 * @caller: the caller to check for
 * @error: pointer to an initialized dbus error object for returning errors
 * 
 * Determines whether a given process on the system message bus is
 * locked out from an interface on a specific device. Only HAL helpers
 * are privileged to use this method.
 * 
 * Returns: Whether the given caller is locked out
 **/
dbus_bool_t
libhal_device_is_caller_locked_out (LibHalContext *ctx,
                                    const char *udi,
                                    const char *interface,
                                    const char *caller,
                                    DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, TRUE);
	LIBHAL_CHECK_UDI_VALID(udi, TRUE);
	LIBHAL_CHECK_PARAM_VALID(interface, "*interface", TRUE);
	LIBHAL_CHECK_PARAM_VALID(caller, "*caller", TRUE);

	return TRUE;
}


/**
 * libhal_ctx_set_global_interface_lock_acquired:
 * @ctx: the context for the connection to hald
 * @callback: the callback
 *
 * Set the callback for when a global interface lock is acquired.
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_global_interface_lock_acquired (LibHalContext *ctx, LibHalGlobalInterfaceLockAcquired callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT (ctx, FALSE);

	ctx->global_interface_lock_acquired = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_global_interface_lock_released:
 * @ctx: the context for the connection to hald
 * @callback: the callback
 *
 * Set the callback for when a global interface lock is released.
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_global_interface_lock_released (LibHalContext *ctx, LibHalGlobalInterfaceLockReleased callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT (ctx, FALSE);

	ctx->global_interface_lock_released = callback;
	return TRUE;
}


/**
 * libhal_ctx_set_interface_lock_acquired:
 * @ctx: the context for the connection to hald
 * @callback: the callback
 *
 * Set the callback for when an interface lock is acquired.
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_interface_lock_acquired (LibHalContext *ctx, LibHalInterfaceLockAcquired callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT (ctx, FALSE);

	ctx->interface_lock_acquired = callback;
	return TRUE;
}

/**
 * libhal_ctx_set_interface_lock_released:
 * @ctx: the context for the connection to hald
 * @callback: the callback
 *
 * Set the callback for when an interface lock is released.
 *
 * Returns: TRUE if callback was successfully set, FALSE otherwise
 */
dbus_bool_t
libhal_ctx_set_interface_lock_released (LibHalContext *ctx, LibHalInterfaceLockReleased callback)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT (ctx, FALSE);

	ctx->interface_lock_released = callback;
	return TRUE;
}



/**
 * libhal_device_is_locked_by_others:
 * @ctx: the context for the connection to hald
 * @udi: the Unique id of device
 * @interface: the intername name to check
 * @error: pointer to an initialized dbus error object for returning errors or NULL
 * 
 * Determines whether a determines other processes than the caller holds a lock on the given device.
 * 
 * Returns: If another process is holding a lock on the device
 **/
dbus_bool_t 
libhal_device_is_locked_by_others (LibHalContext *ctx,
                                   const char *udi,
                                   const char *interface,
                                   DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, TRUE);
	LIBHAL_CHECK_UDI_VALID(udi, TRUE);
	LIBHAL_CHECK_PARAM_VALID(interface, "*interface", TRUE);

	return TRUE;
}

/**
 * libhal_device_is_caller_privileged:
 * @ctx: the context for the connection to hald
 * @udi: the Unique id of device
 * @action: the action to check for
 * @caller: the caller to check for
 * @error: pointer to an initialized dbus error object for returning errors
 * 
 * Determines if a given caller have a given privilege on a given
 * device. Will always error out if HAL is not built with PolicyKit
 * support.
 * 
 * Returns: The textual reply from PolicyKit. See the #PolicyKitResult
 * enumeration in the PolicyKit documentation for details. The caller
 * is responsible for freeing this string with the function
 * libhal_free_string().
 **/
char*
libhal_device_is_caller_privileged (LibHalContext *ctx,
                                    const char *udi,
                                    const char *action,
                                    const char *caller,
                                    DBusError *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT(ctx, NULL);
	LIBHAL_CHECK_UDI_VALID(udi, NULL);
	LIBHAL_CHECK_PARAM_VALID(action, "*action", NULL);
	LIBHAL_CHECK_PARAM_VALID(caller, "*caller", NULL);

	return NULL;
}

/**
 * libhal_get_all_devices_with_properties:
 * @out_num_devices: Return location for number of devices
 * @out_udi: Return location for array of of udi's. Caller should free this with libhal_free_string_array() when done with it.
 * @out_properties: Return location for array of #LibHalPropertySet objects. Caller should free each one of them with libhal_free_property_set() when done with it
 * @error: Return location for error
 *
 * Get all devices in the hal database as well as all properties for each device.
 *
 * Return: %TRUE if success; %FALSE and @error will be set.
 **/
dbus_bool_t libhal_get_all_devices_with_properties (LibHalContext       *ctx, 
                                                    int                 *out_num_devices, 
                                                    char              ***out_udi,
                                                    LibHalPropertySet ***out_properties, 
                                                    DBusError           *error)
{
hal_logger("%s", __func__);
	LIBHAL_CHECK_LIBHALCONTEXT (ctx, FALSE);
	LIBHAL_CHECK_PARAM_VALID (out_num_devices, "*out_num_devices",FALSE);
	LIBHAL_CHECK_PARAM_VALID (out_udi, "***out_udi", FALSE);
	LIBHAL_CHECK_PARAM_VALID (out_properties, "***out_properties", FALSE);

	*out_num_devices = 0;
        *out_udi = NULL;
        *out_properties = NULL;

	return TRUE;
}
