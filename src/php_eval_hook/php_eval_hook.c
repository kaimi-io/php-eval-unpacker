#define PHP_WIN32
#define ZEND_WIN32
#define ZTS 1
#define ZEND_DEBUG 0

#pragma comment(lib, "php5ts.lib")

#include "zend_config.w32.h"
#include "php.h"


PHP_MINIT_FUNCTION(evalhook);
PHP_MSHUTDOWN_FUNCTION(evalhook);
PHP_MINFO_FUNCTION(evalhook);


zend_module_entry evalhook_ext_module_entry = {
    STANDARD_MODULE_HEADER,
    "Eval Hook",
    NULL,
    PHP_MINIT(evalhook),
	PHP_MSHUTDOWN(evalhook),
	NULL, NULL, NULL,
    "1.0",
    STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(evalhook_ext);

static zend_op_array *(*orig_compile_string)(zval *source_string, char *filename TSRMLS_DC);
static zend_bool evalhook_hooked = 0;

static zend_op_array *evalhook_compile_string(zval * input, char *filename TSRMLS_DC)
{
	const unsigned char delim[] = {0xDE, 0xAD, 0xBE, 0xEF};

    if (Z_TYPE_P(input) != IS_STRING)
	{
        return orig_compile_string(input, filename TSRMLS_CC);
    }

	
	fwrite(input->value.str.val, 1, input->value.str.len, stdout);
	fwrite(delim, 1, sizeof(delim), stdout);

	
	return orig_compile_string(input, filename TSRMLS_CC);
}


PHP_MINIT_FUNCTION(evalhook)
{
	setvbuf(stdout, NULL, _IONBF, 0);

    if (evalhook_hooked == 0)
	{
        evalhook_hooked = 1;
        orig_compile_string = zend_compile_string;
        zend_compile_string = evalhook_compile_string;
    }
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(evalhook)
{
    if (evalhook_hooked == 1)
	{
        evalhook_hooked = 0;
        zend_compile_string = orig_compile_string;
    }

    return SUCCESS;
}
