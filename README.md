# srpo
Sysrepo plugin Openwrt library for wrapping UCI and UBUS data operations.

## srpo_ubus - Sysrepo plugin Openwrt UBUS API
---

This section describes the Sysrepo plugin Openwrt UBUS API. Functions and public data types are listed and described.

The API consists of the following elements:
* enumerations
	* `srpo_ubus_error_e`
* custom types
	* `srpo_ubus_result_value_t`
	* `srpo_ubus_result_values_t`
	* `srpo_ubus_transform_path_cb`
	* `srpo_ubus_transform_data_cb`
	* `srpo_ubus_call_data_t`
* functions
	* `srpo_ubus_call`
	* `srpo_ubus_init_result_values`
	* `srpo_ubus_result_values_add`
	* `srpo_ubus_free_result_values`
	* `srpo_ubus_error_description_get`

## srpo_ubus_error_e
Represents errors that can occure inside the library, with the appropriate error codes and descriptions. The enum is returned by most SRPO ubus functions.

## srpo_ubus_result_value_t
Tracks the value and xpath that will be stored in sysrepo as a libyang data node. As the xpath specifies where the data will be inserted they are kept together in this structure. The sysrepo plugin should set both the value and xpath.

## srpo_ubus_result_values
A simple type that contains an array of result_vaule_t values, and its current size

## void (*srpo_ubus_transform_data_cb)(const char *ubus_json, srpo_ubus_result_values_t *values)
Function pointer type that defines the callback which is registered with srpo_ubus_call, and is then called internally by ubus, when ubus has the call data ready. The type receives the ubus JSON result in a string, and is passed the values array which it should fill in with individual srpo_ubus_result_value_t values.

Parameters:
* [in] ubus_json - string that contains the JSON received from the ubus call, called by srpo_ubus_call
* [in] values - array of srpo_ubus_result_values_t values, that have to be filled by the callback by parsing ubos_json

## srpo_ubus_call_data_t
Contains the abovementioned transform callback, the ubus method and lookup_path, timeout and a json string containing additional data for the ubus invoke call. All of the data fields are used during the ubus call. It is used to wrap the data passed to srpo_ubus_call

## srpo_ubus_error_e srpo_ubus_call(srpo_ubus_result_values_t *values, srpo_ubus_call_data_t *transform)
Set up and initiate an ubus call with the lookup_path, method, timeout and json string arguments specified in the transform template. Passes the srpo_ubus_result_values_t array to the transform callback passed in the transform template. The json_call_arguments string can be NULL. A timeout of 0 means no waiting for the ubus call. If the srpo_ubus_transform_data_cb element is NULL then no callback is registered to process the ubus response data.

Parameters:
* [in] values - srpo_ubus_result_value_t array that will be passed to the transform callback
* [in] call_args - a data structure that contains the callback that will be called by ubus, the ubus lookup_path and ubus method, the timeout and any additional json arguments for the ubus call

Return:
* error code (SRPO_UBUS_ERR_OK on success)

## void srpo_ubus_init_result_values(srpo_ubus_result_values_t **values)
Initialize the srpo_ubus_result_values_t array type.

Parameters:
* [in] values, srpo_ubus_result_values_t array to be initialized

## srpo_ubus_result_values_add
Add a srpo_ubus_result_value_t value to the values array. The value is passed as a string. Additionally an xpath template is passed, which then completes the value xpath together with the xpath_value. If the xpath_template is NULL, the xpath_value is used as the srpo_ubus_result_value_t xpath. value and xpath_value must not be NULL.

Parameters:
* [in] values - values array to add new value to
* [in] value - string value to add to the array
* [in] values_size - length of the value string
* [in] xpath_template - xpath template, the static part of the xpath corresponding to the value
* [in] xpath_template_size - length of the xpath_template string
* [in] xpath_value - xpath value, dynamic part of the xpath
* [in] xpath_value_size - xpath value string length

Return:
* error code (SRPO_UBUS_ERR_OK on success)

## srpo_ubus_free_result_values
Free the ubus result values array.

Parameters:
* [in] values - array to free

## srpo_ubus_error_description_get
Get a string description of the SRPO ubus error enum.

Parameters:
[in] error - srpo_ubus_error_e enum

Return
* string description of the error

## srpo_uci - Sysrepo plugin Openwrt UCI API
---

In this section the Sysrepo plugin Openwrt UCI API calls will be explained in detail. For every API function a decription will be given which includes the behaviour of the function, definitions of function parameters and the returning value of a function, if the function has a return value.

Aditionally if the API defines a structure or any other data their meaning will also be described.

The API consinst of the following elements:
* enumerations:
  * `srpo_uci_error_e`
  * `srpo_uci_path_direction_t`
* function pointers:
  * `char *(*srpo_uci_transform_data_cb)(const char *uci_value, void *private_data)`
* structures:
  * `srpo_uci_xpath_uci_template_map_t`
* functions:
  * `int srpo_uci_init(void)`
  * `void srpo_uci_cleanup(void)`
  * `const char *srpo_uci_error_description_get(srpo_uci_error_e error)`
  * `int srpo_uci_ucipath_list_get(const char *uci_config, const char **uci_section_list, size_t uci_section_list_size, char ***ucipath_list, size_t *ucipath_list_size, bool convert_to_extended)`
  * `int srpo_uci_xpath_to_ucipath_convert(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, char **ucipath)`
  * `int srpo_uci_ucipath_to_xpath_convert(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, char **xpath)`
  * `char *srpo_uci_section_name_get(const char *ucipath)`
  * `int srpo_uci_path_get(const char *target, const char *from_template, const char *to_template, srpo_uci_transform_path_cb transform_path_cb, srpo_uci_path_direction_t direction, char **path)`
  * `int srpo_uci_transform_sysrepo_data_cb_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, srpo_uci_transform_data_cb *transform_sysrepo_data_cb)`
  * `int srpo_uci_transform_uci_data_cb_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, srpo_uci_transform_data_cb *transform_uci_data_cb)`
  * `int srpo_uci_section_type_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, const char **uci_section_type)`
  * `int srpo_uci_has_transform_sysrepo_data_private_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, bool *has_transform_sysrepo_data_private)`
  * `int srpo_uci_has_transform_uci_data_private_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, bool *has_transform_uci_data_private)`
  * `int srpo_uci_section_create(const char *ucipath, const char *uci_section_type)`
  * `int srpo_uci_section_delete(const char *ucipath)`
  * `int srpo_uci_option_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data)`
  * `int srpo_uci_option_remove(const char *ucipath)`
  * `int srpo_uci_list_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data)`
  * `int srpo_uci_list_remove(const char *ucipath, const char *value)`
  * `int srpo_uci_element_value_get(const char *ucipath, srpo_uci_transform_data_cb transform_uci_data_cb, void *private_data, char ***value_list, size_t *value_list_size)`
  * `int srpo_uci_revert(const char *uci_config)`
  * `int srpo_uci_commit(const char *uci_config)`

## srpo_uci_error_e

The enumeration represents error codes that can occure inside the library. Each error has its description assign to it.

## srpo_uci_path_direction_t
Represents whether a path is converted from UCI to XPath (with `SRPO_UCI_PATH_DIRECTION_XPATH`), or whether the path is converted from XPath into UCI path (with `SRPO_UCI_PATH_DIRECTION_UCI`).

## char *(*srpo_uci_transform_data_cb)(const char *value, void *private_data)

Function pointer used for functions that are used for transforming data read, either from UCI or Sysrepo datastores, and before setting them to Sysrepo datastores or UCI.

The function takes:
* value:
  * value, either Sysrepo or UCI, represented as an string, can not be NULL
* private_data:
  * data passed to the callback that needs to be used for transforming the data
  * user is resonsible for managing the private_data
  * can be NULL

The function returns:
  * the transformed value as a string
  * the return value can be NULL

## int (*srpo_uci_transform_path_cb)(const char *target, const char *from, const char *to, srpo_uci_path_direction_t direction, char **path)

Function pointer that defines a callback which is called when specified as part of the `srpo_uci_xpath_uci_template_map_t` map for a specific transformation entry.

Function arguments:
* target:
  * constant string containing either the UCI or XPath
* from_template:
  * constant string containing the template from which the conversion is made
  * can not be NULL
* to_template:
  * constant string containing the template to which the conversion is made
  * can not be NULL
* transform_path_cb:
  * specifies a custom `srpo_uci_transform_path_cb` function which will be responsible for generating the path
* direction:
  * a boolean variable which specifies whether the conversion is from UCI to XPath or vice-versa
* path
  * string containing the resulting UCI or XPath
  * allocated dynamically user needs to call free

Function return:
* `SRPO_UCI_ERR_OK` on success
* `SRPO_UCI_ERR_NOT_FOUND` if either the UCI or the XPath can't be found in the `uci_xpath_template_map`
* `SRPO_UCI_ERR_ARGUMENT` if the `ucipath` can't be found in the `uci_xpath_template_map`
* `srpo_uci_error_e` error code on failure

## srpo_uci_xpath_uci_template_map_t

Structure for holding Sysrepo to UCI mapping. The mappings are organized in the following order
* libyang list mapts to UCI section
* libyang leaflist maps to UCI list
* libyang leaf maps to UCI option

To generalize the transformations the following attributes are placed in the structure:
* xpath:
  * constant string that represents a XPath to a libyang list, leaflist or leaf
  * the string can contain a formater (i.e. '%s', '%d', etc.) so a specific libyang list XPath can be referenced using a specific XPath list key
  * can not be NULL
* ucipath:
  * constant string that represents a UCI path to a section, list or option
  * the string can contain a formater (i.e. '%s', '%d', etc.) so a specific UCI section can be referenced using a specific section name
  * can not be NULL
* transform_sysrepo_data_cb:
  * function pointer of type `char *(*srpo_uci_transform_data_cb)(const char *value, void *private_data)`
  * used for additional transformations for a value that is from Sysrepo datastores
  * can be NULL
* transform_uci_data_cb:
  * function pointer of type `char *(*srpo_uci_transform_data_cb)(const char *value, void *private_data)`
  * used for additional transformations for a value that is read from UCI configuration file
  * can be NULL
* has_transform_sysrepo_data_private:
  * boolean value
  * gives information if the transform_sysrepo_data_cb callback function taks an additional argument as private data or not
  * is ignored if the transform_sysrepo_data_cb is NULL
* has_transform_uci_data_private:
  * boolean value
  * gives information if the transform_uci_data_cb callback function taks an additional argument as private data or not
  * is ignored if the transform_uci_data_cb is NULL

## int srpo_uci_init(void)

Function for initializing the `srpo_uci` module. Needs to be called before any other `srpo_uci` module function.

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure


## void srpo_uci_cleanup(void)

Function for cleaning up all the module data needed in runtime. Needs to be called on application exit. After calling this function
all other `srop_uci` API function calls should not be made because the result is undefined.

## const char *srpo_uci_error_description_get(srpo_uci_error_e error)

Function for retrieving the error descrioption string.

Function argumetns:
* error:
  * an enumeration of type `srpo_uci_error_e`

Function return:
* constant string representing the error code description
* the return string is a constant and should not be changed or freed
* returns a default string if error is not part of `srpo_uci_error_e`

## int srpo_uci_ucipath_list_get(const char *uci_config, const char **uci_section_list, size_t uci_section_list_size, char ***ucipath_list, size_t *ucipath_list_size, bool convert_to_extended)

Function for retrieving UCI path consisting of package.section.option. The package and secions that are going to be retrieved are specifed by the argumets.

Function argumets:
* uci_config:
  * constant string specifying the UCI configuration file
  * only the name of the UCI file not the apsolute path
  * can not be NULL
* uci_seciton_list:
  * list of constant string containing the name of the sections that are of interest in the specified UCI configuration file
  * can not contain NULL elements in the array
* uci_seciton_list_size:
  * `size_t` number that specifies how many elements are in the `uci_seciton_list` list
* ucipath_list:
  * list to hold the resulting UCI paths of sections, lists and options
  * allocated dinamically user needs to call free
* ucipath_list_size:
  * `size_t` number specifying how many elements are in the `ucipath_list` list
* convert_to_extended:
  * `bool` whether to convert unnamed UCI sections to extended UCI syntax

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_xpath_to_ucipath_convert(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, char **ucipath)

Function for converting the XPath to UCI path.

Function arguments:
* xpath:
  * constant string containing the XPath to the desired libyang list, leaflist or leaf
  * can not be NULL
* xpath_uci_template_map:
  * map of type `srpo_uci_xpath_uci_template_map_t` used for finding the mapped UCI path for the given XPath
  * can not be NULL
* xpath_uci_template_map_size:
  * `size_t` number specifying the number of entries in the `xpath_uci_template_map` map
* ucipath:
  * string containing the resulting UCI path that is mapped with the provided `xpath`
  * allocated dynamically user needs to call free

Function return:
* `SRPO_UCI_ERR_OK` on success
* `SRPO_UCI_ERR_NOT_FOUND` if the `xpath` can't be found in the `xpath_uci_template_map`
* `srpo_uci_error_e` error code on failure

## int srpo_uci_ucipath_to_xpath_convert(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, char **xpath)

Function for converting the UCI path to XPath.

Function arguments:
* ucipath:
  * constant string containing the UCI path to the desired UCI section, list or option
  * can not be NULL
* uci_xpath_template_map:
  * map of type `srpo_uci_xpath_uci_template_map_t` used for finding the mapped XPath for the given UCI path
  * can not be NULL
* uci_xpath_template_map_size:
  * `size_t` number specifying the number of entries in the `uci_xpath_template_map` map
* xpath:
  * string containing the resulting Xpath that is mapped with the provided `ucipath`
  * allocated dynamically user needs to call free

Function return:
* `SRPO_UCI_ERR_OK` on success
* `SRPO_UCI_ERR_NOT_FOUND` if the `ucipath` can't be found in the `uci_xpath_template_map`
* `srpo_uci_error_e` error code on failure

## char *srpo_uci_section_name_get(const char *ucipath)

Function for returning the section name from a UCI path.

Function arguments:
* ucipath:
  * constant string containing the UCI path to the desired UCI section, list or option
  * can not be NULL

Function return:
* function returns a string with the UCI section name exstracted from the UCI path
* if the UCI path doesnt contain a section name NULL is returned
* allocated dynamically user needs to call free

## int srpo_uci_path_get(const char *target, const char *from_template, const char *to_template, srpo_uci_transform_path_cb transform_path_cb, srpo_uci_path_direction_t direction, char **path)

Function for constructing XPath from UCI path, or an UCI path from an XPath.

Function arguments:
* target:
  * constant string containing either the UCI or XPath
* from_template:
  * constant string containing the template from which the conversion is made
  * can not be NULL
* to_template:
  * constant string containing the template to which the conversion is made
  * can not be NULL
* transform_path_cb:
  * specifies a custom `srpo_uci_transform_path_cb` function which will be responsible for generating the path
* direction:
  * a boolean variable which specifies whether the conversion is from UCI to XPath or vice-versa
* path
  * string containing the resulting UCI or XPath
  * allocated dynamically user needs to call free

Function return:
* `SRPO_UCI_ERR_OK` on success
* `SRPO_UCI_ERR_NOT_FOUND` if either the UCI or the XPath can't be found in the `uci_xpath_template_map`
* `SRPO_UCI_ERR_ARGUMENT` if the `ucipath` can't be found in the `uci_xpath_template_map`
* `srpo_uci_error_e` error code on failure

## int srpo_uci_transform_sysrepo_data_cb_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, srpo_uci_transform_data_cb *transform_sysrepo_data_cb)

Function for returning the `srpo_uci_transform_data_cb` data transformation callback for Sysrepo data for a given `xpath`.

Function arguments:
* xpath:
  * constant string containing the XPath to the desired libyang list, leaflist or leaf
  * can not be NULL
* xpath_uci_template_map:
  * map of type `srpo_uci_xpath_uci_template_map_t` used for finding the mapped UCI path for the given XPath
  * can not be NULL
* xpath_uci_template_map_size:
  * `size_t` number specifying the number of entries in the `xpath_uci_template_map` map
* transform_sysrepo_data_cb:
  * function pointer to the data transform callback for Sysrepo data
  * can be NULL if no callback is registered for the provided `xpath`

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_transform_uci_data_cb_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, srpo_uci_transform_data_cb *transform_uci_data_cb)

Function for returning the `srpo_uci_transform_data_cb` data transformation callback for UCI data for a given `ucipath`.

Function arguments:
* ucipath:
  * constant string containing the UCI path to the desired UCI section, list or option
  * can not be NULL
* uci_xpath_template_map:
  * map of type `srpo_uci_xpath_uci_template_map_t` used for finding the mapped XPath for the given UCI path
  * can not be NULL
* uci_xpath_template_map_size:
  * `size_t` number specifying the number of entries in the `uci_xpath_template_map` map
* transform_uci_data_cb:
  * function pointer to the data transform callback for UCI data
  * can be NULL if no callback is registered for the provided `ucipath`

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_has_transform_sysrepo_data_private_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, bool *has_transform_sysrepo_data_private)

Function for returnig the flag describing if the `srpo_uci_transform_data_cb` registered for the given `xpath` takes any additional arguments as private data.

Function arguments:
* xpath:
  * constant string containing the XPath to the desired libyang list, leaflist or leaf
  * can not be NULL
* xpath_uci_template_map:
  * map of type `srpo_uci_xpath_uci_template_map_t` used for finding the mapped UCI path for the given XPath
  * can not be NULL
* xpath_uci_template_map_size:
  * `size_t` number specifying the number of entries in the `xpath_uci_template_map` map
* has_transform_sysrepo_data_private
  * returning boolean flag describining if the registered `srpo_uci_transform_data_cb` for transforming Sysrepo data takes any additional arguments as private data
  * if no `srpo_uci_transform_data_cb` function is registered `false` is returned

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_has_transform_uci_data_private_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, bool *has_transform_uci_data_private)

Function for returnig the flag describing if the `srpo_uci_transform_data_cb` registered for the given `ucipath` takes any additional arguments as private data.

Function arguments:
* ucipath:
  * constant string containing the UCI path to the desired UCI section, list or option
  * can not be NULL
* uci_xpath_template_map:
  * map of type `srpo_uci_xpath_uci_template_map_t` used for finding the mapped XPath for the given UCI path
  * can not be NULL
* uci_xpath_template_map_size:
  * `size_t` number specifying the number of entries in the `uci_xpath_template_map` map
* has_transform_uci_data_private
  * returning boolean flag describining if the registered `srpo_uci_transform_data_cb` for transforming UCI data takes any additional arguments as private data
  * if no `srpo_uci_transform_data_cb` function is registered `false` is returned

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_section_create(const char *ucipath, const char *uci_section_type)

Function for creating a new UCI section.

Function arguments:
* ucipath:
  * constant string containing the UCI path to the desired UCI section
  * can not be NULL
* uci_section_type:
  * constant string specifyin the UCI section type
  * section type depends on the UCI configuration in which the section is being created
  * can not be NULL

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_section_delete(const char *ucipath)

Function for deleting the UCI section.

Function arguments:
* ucipath:
  * constant string containing the UCI path to the desired UCI section
  * can not be NULL

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_option_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data)

Function for seting the value of an UCI option.

Function arguments:
* ucipath:
  * constant string containing the UCI path to the desired UCI option
  * can not be NULL
* value:
  * string value of an UCI option
  * can not be NULL
* transform_sysrepo_data_cb
  * function callback for transforimng the `value` before setting it to the UCI option specified by `ucipath`
  * if NULL is returned the value will not be set and it will be silently ignored
  * can be NULL
* private_data
  * data to be passed to the `transform_sysrepo_data_cb` function as the second argument
  * can be NULL

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_option_remove(const char *ucipath)

Function for removing an UCI option specified by the `ucipath`

Function arguments:
  * constant string containing the UCI path to the desired UCI option
  * if the UCI option specified by the `ucipath` doesn't exist nothing will happen
  * can not be NULL

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_list_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data)

Function for adding a value for an UCI list.

* ucipath:
  * constant string containing the UCI path to the desired UCI option
  * can not be NULL
* value:
  * string value of an UCI list
  * can not be NULL
* transform_sysrepo_data_cb
  * function callback for transforimng the `value` before setting it to the UCI list specified by `ucipath`
  * if NULL is returned the value will not be set and it will be silently ignored
  * can be NULL
* private_data
  * data to be passed to the `transform_sysrepo_data_cb` function as the second argument
  * can be NULL

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_list_remove(const char *ucipath, const char *value)

Functio for removing a value from an UCI.

* ucipath:
  * constant string containing the UCI path to the desired UCI list
  * can not be NULL
* value:
  * string value of an entry in the UCI list specifyed by `ucipath`
  * can not be NULL

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_element_value_get(const char *ucipath, srpo_uci_transform_data_cb transform_uci_data_cb, void *private_data, char ***value_list, size_t *value_list_size)

Function for getting the value of an UCI option or an UCI list.

Function arguments:
* ucipath:
  * constant string containing the UCI path to the desired UCI option or UCI list
  * can not be NULL
* transform_uci_data_cb:
  * function for transforming data read from UCI configuration
  * if NULL is returned the value will not be added to the value_list, it will be silently ignored
  * can be NULL
* private_data
  * data to be passed to the `transform_sysrepo_data_cb` function as the second argument
  * can be NULL
* value_list:
  * list of strings representing values of UCI option value or UCI list values
  * if the `ucipath` referenced an UCI option `value_list` will contain only one element
  * if the `ucipath` referenced an UCI list `value_list` will contain as much elements as there are values in an UCI list
* value_list_size:
  * `size_t` number specifying the number of entries in the `value_list` map

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_revert(const char *uci_config)

Function for reverting changes made to UCI.

Function arguments:
* uci_config:
  * constant string specifying the UCI configuration file
  * only the name of the UCI file not the apsolute path
  * can not be NULL

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure

## int srpo_uci_commit(const char *uci_config)

Function for commiting the changes to a UCI configuration file.

Function arguments:
* uci_config:
  * constant string specifying the UCI configuration file
  * only the name of the UCI file not the apsolute path
  * can not be NULL

Function return:
* `SRPO_UCI_ERR_OK` on success, a `srpo_uci_error_e` error code on failure
