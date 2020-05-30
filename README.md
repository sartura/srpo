# srpo
Sysrepo plugin Openwrt library for wrapping UCI and UBUS data operations.

## srpo_ubus - Sysrepo plugin Openwrt UBUS API
---

## srpo_uci - Sysrepo plugin Openwrt UCI API
---

In this section the Sysrepo plugin Openwrt UCI API calls will be explained in detail. For every API function a decription will be given which includes the behaviour of the function, definitions of function parameters and the returning value of a function, if the function has a return value.

Aditionally if the API defines a structure or any other data their meaning will also be described.

The API consinst of the following elements:
* enumerations:
  * srpo_uci_error_e
* function pointers:
  * char *(*srpo_uci_transform_data_cb)(const char *uci_value, void *private_data)
* structures:
  * srpo_uci_xpath_uci_template_map_t
* functions:
  * int srpo_uci_init(void);
  * void srpo_uci_cleanup(void);
  * const char *srpo_uci_error_description_get(srpo_uci_error_e error);
  * int srpo_uci_ucipath_list_get(const char *uci_config, const char **uci_section_list, size_t uci_section_list_size, char ***ucipath_list, size_t *ucipath_list_size);
  * int srpo_uci_xpath_to_ucipath_convert(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, char **ucipath);
  * int srpo_uci_ucipath_to_xpath_convert(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, char **xpath);
  * char *srpo_uci_section_name_get(const char *ucipath);
  * int srpo_uci_transform_sysrepo_data_cb_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, srpo_uci_transform_data_cb   *transform_sysrepo_data_cb);
  * int srpo_uci_transform_uci_data_cb_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, srpo_uci_transform_data_cb *transform_uci_data_cb);
  * int srpo_uci_section_type_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, const char **uci_section_type);
  * int srpo_uci_has_transform_sysrepo_data_private_get(const char *xpath, srpo_uci_xpath_uci_template_map_t *xpath_uci_template_map, size_t xpath_uci_template_map_size, bool *has_transform_sysrepo_data_private);
  * int srpo_uci_has_transform_uci_data_private_get(const char *ucipath, srpo_uci_xpath_uci_template_map_t *uci_xpath_template_map, size_t uci_xpath_template_map_size, bool *has_transform_uci_data_private);
  * int srpo_uci_section_create(const char *ucipath, const char *uci_section_type);
  * int srpo_uci_section_delete(const char *ucipath);
  * int srpo_uci_option_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data);
  * int srpo_uci_option_remove(const char *ucipath);
  * int srpo_uci_list_set(const char *ucipath, const char *value, srpo_uci_transform_data_cb transform_sysrepo_data_cb, void *private_data);
  * int srpo_uci_list_remove(const char *ucipath, const char *value);
  * int srpo_uci_element_value_get(const char *ucipath, srpo_uci_transform_data_cb transform_uci_data_cb, void *private_data, char ***value_list, size_t *value_list_size);
  * int srpo_uci_revert(const char *uci_config);
  * int srpo_uci_commit(const char *uci_config);

## srpo_uci_error_e

The enumeration represents error codes that can occure inside the library. Each error has its description assign to it.

## char *(*srpo_uci_transform_data_cb)(const char *value, void *private_data)

Function pointer used for functions that are used for transforimg data read, either from UCI or Sysrepo datastores, and before setting them to Sysrepo datastores or UCI.

The function takes:
* value:
  * value, either Sysrepo or UCI, represented as an string, can not be NULL
* private_data:
  * data passed to the callback that needs to be used for transforming the data
  * user is resonsible for managing the private_data
  * can be NULL
* return:
  * funciton returns the transformed value as a string
  * the return value can be NULL
