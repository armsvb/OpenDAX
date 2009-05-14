/*  OpenDAX - An open source data acquisition and control system
 *  Copyright (c) 2007 Phil Birkelbach
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 
 * This file contains libdax configuration functions
 */

#include <libdax.h>
#include <dax/libcommon.h>
#include <getopt.h>
#include <ctype.h>

typedef struct OptAttr {
    char *name;
    char *longopt;
    char shortopt;
    char *defvalue;
    char *value;
    int flags;
    int (*callback)(char *name, char *value);
    struct OptAttr *next;
} optattr;
    
static optattr *_attr_head = NULL;
static lua_State *_L;
static char *_modulename;
static int _msgtimeout;

/* Initialize the module configuration */
int
dax_init_config(char *name)
{
	int result = 0;
	int flags;
	
	_L = luaL_newstate();
	if(_L == NULL) {
		return ERR_ALLOC;
	} else {
        _L = lua_open();
	}
	/* This sets up the configuration that is common to all modules */
	flags = CFG_CMDLINE | CFG_DAXCONF | CFG_MODCONF | CFG_ARG_REQUIRED;
	result += dax_add_attribute("socketname","socketname", 'S', flags, "/tmp/opendax");
	result += dax_add_attribute("serverip", "serverip", 'I', flags, "127.0.0.1");
	result += dax_add_attribute("serverport", "serverport", 'P', flags, "7777");
	result += dax_add_attribute("server", "server", 's', flags, "LOCAL");
	result += dax_add_attribute("debugtopic", "topic", 'T', flags, "MAJOR");
	result += dax_add_attribute("name", "name", 'N', flags, name);
	result += dax_add_attribute("cachesize", "cachesize", 'z', flags, "8");
	result += dax_add_attribute("msgtimeout", "msgtimeout", 'o', flags, DEFAULT_TIMEOUT);

	flags = CFG_CMDLINE | CFG_ARG_REQUIRED;
	result += dax_add_attribute("config", "config", 'C', flags, NULL);
	result += dax_add_attribute("confdir", "confdir", 'c', flags, ETC_DIR);
	//result += dax_add_attribute("", "", '', flags, "");

	_modulename = strdup(name);

	if(result) {
		return ERR_GENERIC;
	} else {
		return 0;
	}
}

/* Allows the module developer to add their own function for
 * the Lua configuration file */
int
dax_set_luafunction(int (*f)(void *L), char *name)
{
	lua_pushcfunction(_L, (int (*)(lua_State *))f);
    lua_setglobal(_L, name);
    return 0;
}

/* Determine whether or not the attribute name is okay */
static int
_validate_name(char *name)
{
    int n;
    
    if(name != NULL) {
        /* First character has to be a letter or '_' */
        if( !isalpha(name[0]) && name[0] != '_' ) {
            return ERR_ARG;
        }
        /* The rest of the name can be letters, numbers or '_' */
        for(n = 1; n < strlen(name) ;n++) {
            if( !isalpha(name[n]) && (name[n] != '_') &&  !isdigit(name[n]) ) {
                return ERR_ARG;
            }
        }
    }
    return 0;
}

/* Check the attributes symbols, returns 0 on success or an error code */
static int
_check_attr(char *name, char *longopt, char shortopt)
{
	optattr *this;
	
	this = _attr_head;
    
	if(_validate_name(name) || _validate_name(longopt)) {
	    return ERR_ARG;
	}
	
	/* This loop checks that none of the symbols have already been used. */
    while(this != NULL) {
        if(!strcmp(this->name, name)) {
            return ERR_DUPL;
        }
        if( (longopt != NULL) && (this->longopt != NULL) && (!strcmp(this->longopt, longopt))) {
            return ERR_DUPL;
        }
        if(this->shortopt == shortopt) {
            return ERR_DUPL;
        }
    	this = this->next;
    }
    return 0;
}

int
dax_add_attribute(char *name, char *longopt, char shortopt, int flags, char *defvalue)
{
    optattr *newattr;
    int result;
    
    /* Check for bad symbols */
    if( (result = _check_attr(name, longopt, shortopt)) ) {
    	return ERR_DUPL;
    }
    if(name == NULL) return ERR_ARG;
        
    newattr = malloc(sizeof(optattr));
    if(newattr == NULL) {
    	return ERR_ALLOC;
    } else {
    	newattr->name = strdup(name);
    	if(longopt) {
    		newattr->longopt = strdup(longopt);
    	} else {
    		newattr->longopt = NULL;
    	}
    	newattr->shortopt = shortopt;
    	if(defvalue) {
    		newattr->defvalue = strdup(defvalue);
    	} else {
    		newattr->defvalue = NULL;
    	}
    	newattr->flags = flags;
    	newattr->callback = NULL;
    	newattr->next = NULL;
    	newattr->value = NULL;
    }
    
    /* We'll just stick it in the top */
    newattr->next = _attr_head; 
    _attr_head = newattr;
    return 0;
}

/* This assigns the callback function that will be called by the 
 * configuration when the attribute given by 'name' is found
 * in the command line argument or in the configuration file. */
int
dax_attr_callback(char *name, int (*attr_callback)(char *name, char *value)) {
	optattr *this;
	
	this = _attr_head;
	
	while(this != NULL) {
		if( ! strcmp(this->name, name) ){
			this->callback = attr_callback;
			return 0;
		}
	}
	return ERR_NOTFOUND;
}

/* TODO: Add callback for before and after the module conf script is called */
/* TODO: Add a callback for each entry and exit point in the configuration.  I
 * should probably do this with a single callback and some flags?  */

/* This function sets the value and calls the callback function. */
static int
_set_attr(optattr *attr, char *value) {
    printf("_set_attr() called to set %s to %s\n", attr->name, value);
    /* Set the value and call the callback function if there is one */
    if( !(attr->flags & CFG_NO_VALUE) ) {
        attr->value = strdup(value);
    }
    printf("%s was set to %s", attr->name, attr->value);
    if(attr->callback) {
        attr->callback(attr->name, value);
    }
    return 0;
}

/* Compare the attribute list to the command line arguments
 * and sets the values appropriately */
static inline int
_parse_commandline(int argc, char **argv) {
	int attr_count = 0, o_count = 0;
	int attr_index = 0, o_index = 0, ch, index;
	char *shortopts;
	struct option *options;
	optattr *this;
	
	/* This loop counts the attribute list and the size of the 
	 * string that we'll need or the short opts so that we know
	 * how much memory to allocate. */
	this = _attr_head;
	while(this != NULL) {
		attr_count++;
		if(this->shortopt != '\0') {
			o_count++; /* we have one short opt character here */
			if(this->flags & CFG_ARG_OPTIONAL) {
				o_count += 2; /* for the two ':'s */
			} else if (this->flags & CFG_ARG_REQUIRED) {
				o_count ++; /* for the single ':' */
			}
		}
		this = this->next;
	}
	/* Allocate the memory on the stack */
	shortopts = malloc(sizeof(char) * (o_count + 1));
	if(shortopts == NULL) return ERR_ALLOC;
	options = malloc(sizeof(struct option) * (attr_count + 1));
	if(options == NULL) {
	    free(shortopts);
	    return ERR_ALLOC;
	}
	
	/* This loop assigns the option structure and strings
	 * from the attribute list */
	this = _attr_head;
	while(this != NULL) {
		/* If it's not supposed to be on the command line then
		 * we have no use for it here */
		if(this->flags & CFG_CMDLINE) {
			if(this->shortopt != '\0') {
				shortopts[o_index++] = this->shortopt;
				if(this->flags & CFG_ARG_OPTIONAL) {
					shortopts[o_index++] = ':';
					shortopts[o_index++] = ':';
				} else if(this->flags & CFG_ARG_REQUIRED) {
					shortopts[o_index++] = ':';
				}
			}
			if(this->longopt != NULL) {
				options[attr_index].name = this->longopt;
				options[attr_index].val = this->shortopt;
				options[attr_index].flag = 0;
				if(this->flags & CFG_ARG_OPTIONAL) {
					options[attr_index].has_arg = optional_argument;
				} else if(this->flags & CFG_ARG_REQUIRED) {
					options[attr_index].has_arg = required_argument;
				} else {
					options[attr_index].has_arg = no_argument;
				}
				attr_index++;
			}
		}
		this = this->next;
	}
	ch = 0;
	while(ch != -1) {
		ch = getopt_long(argc, (char * const *)argv, shortopts, options, &index);
		if(ch > 0) {
		    this = _attr_head;
			while(this != NULL) {
				if(this->shortopt == ch) {
				    _set_attr(this, optarg);
					break;
				}
				this = this->next;
			}
		} else if(ch == 0) {
		    this = _attr_head;
            while(this != NULL) {
                if(!strcasecmp(this->longopt, options[index].name)) {
                    _set_attr(this, optarg);
                    break;
                }
                this = this->next;
            }
		}
	}
	free(shortopts);
	free(options);
	return 0;
}

/* This reads the global configuration options from the Lua
 * script that is passed by *L.  type indicates whether it's
 * the module configuration or the main opendax.conf */
static int
_get_lua_globals(lua_State *L, int type) {
    optattr *this;
    const char *s;
    
    this = _attr_head;
    
    while(this != NULL) {
        /* If the value has not already been set and we
         * are actually looking for the attribute in this
         * configuration file */
        if((this->flags & type)) {
            lua_getglobal(L, this->name);
            if(lua_isboolean(L, -1)) {
                if(lua_toboolean(L, -1)) {
                    s = "true";
                } else {
                    s = "false";    
                }
            } else {
                s = lua_tostring(L, -1);
            }
            if(s) {
                /* We only set the value if it has not already been set */
                if(this->value == NULL) {
                    _set_attr(this, (char *)s);
                }
            }
            lua_pop(L, 1);
        }
        this = this->next;
    }
    return 0;
}

/* This function tries to open the main configuration file,
 * typically opendax.h and run it. */
static inline int
_main_config_file(void) {
	int length, result = 0;
	char *cfile, *cdir;
	lua_State *L;
	
	cdir = dax_get_attr("confdir");
	length = strlen(cdir) + strlen("/opendax.conf") + 1;
	cfile = malloc(sizeof(char) * length);
	if(cfile) {
	    sprintf(cfile, "%s/opendax.conf", cdir);
	} else {
	    return ERR_ALLOC;
	}

    L = lua_open();
    /* We don't open any librarires because we don't really want any
     * function calls in the configuration file.  It's just for
     * setting a few globals. */
    
    /* load and run the configuration file */
    if(luaL_loadfile(L, cfile)  || lua_pcall(L, 0, 0, 0)) {
        dax_error("Problem executing configuration file - %s", lua_tostring(L, -1));
        result = ERR_GENERIC;
    } else {
        /* tell lua to push these variables onto the stack */
        _get_lua_globals(L, CFG_DAXCONF);
    }
    
    free(cfile);
    return 0;
}

/* This function tries to open the module specific configuration
 * file and run it. */
static inline int
_mod_config_file(void) {
	int length;
	char *cfile, *cdir;
	
	/* This gets the default configuration file name
	 * We add 2 for the NULL character and the '/' */
	cfile = dax_get_attr("config");
	printf("dax_get_attr(\"config\" returned %s\n", cfile);
	if(cfile == NULL) { 
	    /* No configuration file in the attribute list then we'll
	     * build the path from the default config directory and 
	     * the module name that was passed to dax_init_config() */
        cdir = dax_get_attr("confdir");
	    length = strlen(cdir) + strlen(_modulename) + strlen("/.conf") + 1;
        cfile = malloc(sizeof(char) * length);
        if(cfile) {
            sprintf(cfile, "%s/%s.conf", cdir, _modulename);
        } else {
            return ERR_ALLOC;
        }
	}
	    
    /* load and run the configuration file */
    if(luaL_loadfile(_L, cfile)  || lua_pcall(_L, 0, 0, 0)) {
        dax_error("Problem executing configuration file - %s", lua_tostring(_L, -1));
        return ERR_GENERIC;
    } else {
        _get_lua_globals(_L, CFG_MODCONF);
    }
    
    free(cfile);
    return 0;
}

/* This loops through all the configuration attributes and sets
 * them to the defaults if the value is still NULL */
static inline int
_set_defaults(void)
{
    optattr *this;
		
    this = _attr_head;

    while(this != NULL) {
        if(this->value == NULL) {
            if(this->defvalue != NULL) {
                this->value = strdup(this->defvalue);
            } else {
                this->value = NULL;
            }
        }
        this = this->next;
    }
    return 0;
}

/* Just for testing */
static inline void
_print_config(void)
{
    optattr *this;
        
    this = _attr_head;

    /* This loop checks that none of the symbols have already been used. */
    while(this != NULL) {
        if(this->value != NULL) {
            printf("%s = %s\n", this->name, this->value);
        } else {
            printf("%s = NULL\n", this->name);
        }
        this = this->next;
    }
}

/* This verifies that the configuratio that we have is valid and won't
 * get us in too much trouble.  It should also store any information that
 * the libary would need so that the user can free the configuration at
 * any time. */
static int
_verify_config(void)
{
    _msgtimeout = strtol(dax_get_attr("msgtimeout"), NULL, 0);
    if(_msgtimeout < MIN_TIMEOUT || _msgtimeout > MAX_TIMEOUT) {
        _msgtimeout = strtol(DEFAULT_TIMEOUT, NULL, 0);
    }
//    if(inet_pton(AF_INET, dax_get_attr("serverip"), NULL)) != 1) {
//        dax_error("serverip not set properly.  Going with default.");
//
    return 0;
}

/* Executes the configuration routines */
int
dax_configure(int argc, char **argv, int flags)
{
	if(_modulename == NULL) {
		return ERR_NO_INIT;
	}
	if(flags & CFG_CMDLINE)
		_parse_commandline(argc, argv);
	/* This sets the confdir parameter to ETC_DIR if it
	 * has not already been set on the command line */
	if(dax_get_attr("confdir") == NULL) {
	    dax_set_attr("confdir", ETC_DIR);
	}
	
	if(flags & CFG_MODCONF)
        _mod_config_file();
    if(flags & CFG_DAXCONF)
        _main_config_file();
        
    _set_defaults();
    _print_config();
    return _verify_config();
}

/* Returns the pointer to the requested attribute */
char *
dax_get_attr(char *name) {
	optattr *this;
	
	this = _attr_head;
	while(this != NULL) {
		if(!strcmp(this->name, name)) {
			return this->value;
		}
		this = this->next;
	}
	return NULL;
}

/* Sets the given attribute to *value.  Frees the existing
 * attribute value if it has been set.  Not very efficient
 * but this is easier and it's only configuration. If value
 * is NULL the attribute will be set to its default. */
int
dax_set_attr(char *name, char *value)
{
    optattr *this;
    
    this = _attr_head;
    while(this != NULL) {
        if(!strcmp(this->name, name)) {
            if(this->value != NULL) free(this->value);
            if(value == NULL) {
                this->value = strdup(this->defvalue);
            } else {
                this->value = strdup(value);
            }
            return 0;
        }
        this = this->next;
    }
    return ERR_NOTFOUND;
}

/* Frees the data in the configuration linked list */
int
dax_free_config(void)
{
    /* TODO: Write dax_free_config */
    return 0;
}

int
opt_get_msgtimeout(void)
{
    return _msgtimeout;
}