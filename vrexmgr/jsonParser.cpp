/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/


/**
* @defgroup iarmmgrs
* @{
* @defgroup vrexmgr
* @{
**/


#include "jsonParser.h"

#include "iarmUtil.h"

#include <stdio.h>
#include <string.h>

static int parse_null(void * ctx)
{
    JSONParser *parser = (JSONParser *)ctx;
    parser->newNull();
    //__TIMESTAMP(); printf ("Parse NULL\n");
    //yajl_gen g = (yajl_gen) ctx;
    //return yajl_gen_status_ok == yajl_gen_null(g);
    return 1;
}

static int parse_boolean(void * ctx, int boolean)
{
    JSONParser *parser = (JSONParser *)ctx;
    parser->newBool(boolean);
    //__TIMESTAMP(); printf ("Parse Bool\n");
    //yajl_gen g = (yajl_gen) ctx;
    //return yajl_gen_status_ok == yajl_gen_bool(g, boolean);
    return 1;
}

static int parse_number(void * ctx, const char * s, size_t l)
{
    std::string str;
    str.append(s, l);
    JSONParser *parser = (JSONParser *)ctx;
    parser->newString(str);
    //currentMap[currentMapKey] = str;
    //__TIMESTAMP(); printf ("NUMBER: <%s>\n", str.c_str());
    //yajl_gen g = (yajl_gen) ctx;
    //return yajl_gen_status_ok == yajl_gen_number(g, s, l);
    return 1;
}

static int parse_string(void * ctx, const unsigned char * stringVal,
                           size_t stringLen)
{
    std::string str;
    str.append((const char *)stringVal, stringLen);
    JSONParser *parser = (JSONParser *)ctx;
    if(str=="true" || str=="false")
    {
    	parser->newBool((str=="true")?true:false);
    }else
    {
    	parser->newString(str);
    }
    //currentMap[currentMapKey] = str;
    //__TIMESTAMP(); printf ("STRING: <%s>\n", str.c_str());
    //yajl_gen g = (yajl_gen) ctx;
    //return yajl_gen_status_ok == yajl_gen_string(g, stringVal, stringLen);
    return 1;
}

static int parse_map_key(void * ctx, const unsigned char * stringVal,
                            size_t stringLen)
{
    std::string str;
    str.append((const char *)stringVal, stringLen);
    JSONParser *parser = (JSONParser *)ctx;
    parser->newKey(str);
    //__TIMESTAMP(); printf ("MAPKEY: <%s>\n", str.c_str());
    //currentMapKey = str;
    //yajl_gen g = (yajl_gen) ctx;
    //return yajl_gen_status_ok == yajl_gen_string(g, stringVal, stringLen);
    return 1;
}

static int parse_start_map(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
    //return yajl_gen_status_ok == yajl_gen_map_open(g);
    return 1;
}


static int parse_end_map(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
    //return yajl_gen_status_ok == yajl_gen_map_close(g);
    return 1;
}

static int parse_start_array(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
//    printf("got array start\n\r");
    JSONParser *parser = (JSONParser *)ctx;
    parser->newArray();
    //return yajl_gen_status_ok == yajl_gen_array_open(g);
    return 1;
}

static int parse_end_array(void * ctx)
{
    yajl_gen g = (yajl_gen) ctx;
//    printf("got array end\n\r");
    JSONParser *parser = (JSONParser *)ctx;
    parser->endArray();
    //return yajl_gen_status_ok == yajl_gen_array_close(g);
    return 1;
}

static yajl_callbacks callbacks = {
    parse_null,
    parse_boolean,
    NULL,
    NULL,
    parse_number,
    parse_string,
    parse_start_map,
    parse_map_key,
    parse_end_map,
    parse_start_array,
    parse_end_array
};


JSONParser::JSONParser()
{
//	m_arrays=new stack<queue<JSONParser::varVal *> *>();
	m_array=NULL;
}

JSONParser::~JSONParser()
{
}

std::map<string, JSONParser::varVal *> JSONParser::parse(const unsigned char *json)
{
    yajl_handle hand;
    yajl_gen g;
    yajl_status status;
    yajl_gen_config config;
    size_t jsonLen = strlen((const char *)json);

    yajl_parser_config cfg;

    config.beautify = 1;

    g = yajl_gen_alloc(&config, NULL);

    cfg.checkUTF8 = 0;
    cfg.allowComments = 1;
    //hand = yajl_alloc(&callbacks, &cfg, NULL, (void *) g);
    hand = yajl_alloc(&callbacks, &cfg, NULL, (void *) this);
    status = yajl_parse(hand, json, jsonLen);

    if (status != yajl_status_ok) {
        __TIMESTAMP();printf("JSONParser: Parse failed\n");
        goto done;
    }

    status = yajl_parse_complete(hand);
    if (status != yajl_status_ok) {
        unsigned char *errorString = yajl_get_error(hand, 1, json, jsonLen);
        printf("%s", (const char *) errorString);
        yajl_free_error(hand, errorString);
    }

done:
    yajl_gen_free(g);
    yajl_free(hand);

    return m_dict;
}


/** @} */
/** @} */
