/*
 * Copyright (c) 2011, Matthias Kovatsch and other contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Example for the CoAP REST Engine
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"


/* Define which resources to include to meet memory constraints. */
#define REST_RES_HELLO 1
#define REST_RES_MIRROR 0 /* causes largest code size */
#define REST_RES_CHUNKS 1
#define REST_RES_SEPARATE 1
#define REST_RES_PUSHING 1
#define REST_RES_EVENT 1
#define REST_RES_LEDS 1
#define REST_RES_TOGGLE 1
#define REST_RES_LIGHT 0
#define REST_RES_BATTERY 1



#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
#warning "Compiling with static routing!"
#include "static-routing.h"
#endif

#include "erbium.h"


#if defined (PLATFORM_HAS_BUTTON)
#include "dev/button-sensor.h"
#endif
#if defined (PLATFORM_HAS_LEDS)
#include "dev/leds.h"
#endif
#if defined (PLATFORM_HAS_LIGHT)
#include "dev/light-sensor.h"
#endif
#if defined (PLATFORM_HAS_BATTERY)
#include "dev/battery-sensor.h"
#endif
#if defined (PLATFORM_HAS_SHT11)
#include "dev/sht11-sensor.h"
#endif


/* For CoAP-specific example: not required for normal RESTful Web service. */
#if WITH_COAP == 3
#include "er-coap-03.h"
#elif WITH_COAP == 7
#include "er-coap-07.h"
#else
#warning "REST example without CoAP"
#endif /* CoAP-specific example */

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif


#if REST_RES_HELLO
/*
 * Resources are defined by the RESOURCE macro.
 * Signature: resource name, the RESTful methods it handles, and its URI path (omitting the leading slash).
 */
RESOURCE(helloworld, METHOD_GET, "hello", "title=\"Hello world: ?len=0..\";rt=\"Text\"");

/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
void
helloworld_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const char *len = NULL;
  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. For more, see the chunk resource. */
  char const * const message = "Hello World! ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy";
  int length = 12; /*           |<-------->| */

  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if (REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if (length<0) length = 0;
    if (length>REST_MAX_CHUNK_SIZE) length = REST_MAX_CHUNK_SIZE;
    memcpy(buffer, message, length);
  } else {
    memcpy(buffer, message, length);
  }

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
  REST.set_header_etag(response, (uint8_t *) &length, 1);
  REST.set_response_payload(response, buffer, length);
}
#endif

#if REST_RES_MIRROR
/* This resource mirrors the incoming request. It shows how to access the options and how to set them for the response. */
RESOURCE(mirror, METHOD_GET | METHOD_POST | METHOD_PUT | METHOD_DELETE, "debug/mirror", "title=\"Returns your decoded message\";rt=\"Debug\"");

void
mirror_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /* The ETag and Token is copied to the header. */
  uint8_t opaque[] = {0x0A, 0xBC, 0xDE};

  /* Strings are not copied, so use static string buffers or strings in .text memory (char *str = "string in .text";). */
  static char location[] = {'/','f','/','a','?','k','&','e', 0};

  /* Getter for the header option Content-Type. If the option is not set, text/plain is returned by default. */
  unsigned int content_type = REST.get_header_content_type(request);

  /* The other getters copy the value (or string/array pointer) to the given pointers and return 1 for success or the length of strings/arrays. */
  uint32_t max_age = 0;
  const char *str = NULL;
  uint32_t observe = 0;
  const uint8_t *bytes = NULL;
  uint32_t block_num = 0;
  uint8_t block_more = 0;
  uint16_t block_size = 0;
  const char *query = "";
  int len = 0;

  /* Mirror the received header options in the response payload. Unsupported getters (e.g., rest_get_header_observe() with HTTP) will return 0. */

  int strpos = 0;
  /* snprintf() counts the terminating '\0' to the size parameter.
   * The additional byte is taken care of by allocating REST_MAX_CHUNK_SIZE+1 bytes in the REST framework.
   * Add +1 to fill the complete buffer. */
  strpos += snprintf((char *)buffer, REST_MAX_CHUNK_SIZE+1, "CT %u\n", content_type);

  /* Some getters such as for ETag or Location are omitted, as these options should not appear in a request.
   * Max-Age might appear in HTTP requests or used for special purposes in CoAP. */
  if (strpos<=REST_MAX_CHUNK_SIZE && REST.get_header_max_age(request, &max_age))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "MA %lu\n", max_age);
  }

  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_header_host(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "UH %.*s\n", len, str);
  }

/* CoAP-specific example: actions not required for normal RESTful Web service. */
#if WITH_COAP > 1
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_observe(request, &observe))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Ob %lu\n", observe);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_token(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "To 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_etag(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "ET 0x");
    int index = 0;
    for (index = 0; index<len; ++index) {
        strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%02X", bytes[index]);
    }
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "\n");
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_uri_path(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "UP ");
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%.*s\n", len, str);
  }
#if WITH_COAP == 3
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Lo %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Bl %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
#else
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location_path(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "LP %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = coap_get_header_location_query(request, &str)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "LQ %.*s\n", len, str);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block2(request, &block_num, &block_more, &block_size, NULL)) /* This getter allows NULL pointers to get only a subset of the block parameters. */
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "B2 %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
  /*
   * Critical Block1 option is currently rejected by engine.
   *
  if (strpos<=REST_MAX_CHUNK_SIZE && coap_get_header_block1(request, &block_num, &block_more, &block_size, NULL))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "B1 %lu%s (%u)\n", block_num, block_more ? "+" : "", block_size);
  }
  */
#endif /* CoAP > 03 */
#endif /* CoAP-specific example */

  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_query(request, &query)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "Qu %.*s\n", len, query);
  }
  if (strpos<=REST_MAX_CHUNK_SIZE && (len = REST.get_request_payload(request, &bytes)))
  {
    strpos += snprintf((char *)buffer+strpos, REST_MAX_CHUNK_SIZE-strpos+1, "%.*s", len, bytes);
  }

  if (strpos >= REST_MAX_CHUNK_SIZE)
  {
      buffer[REST_MAX_CHUNK_SIZE-1] = 0xBB; /* '»' to indicate truncation */
  }

  REST.set_response_payload(response, buffer, strpos);

  PRINTF("/mirror options received: %s\n", buffer);

  /* Set dummy header options for response. Like getters, some setters are not implemented for HTTP and have no effect. */
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_max_age(response, 10); /* For HTTP, browsers will not re-request the page for 10 seconds. CoAP action depends on the client. */
  REST.set_header_etag(response, opaque, 2);
  REST.set_header_location(response, location); /* Initial slash is omitted by framework */

/* CoAP-specific example: actions not required for normal RESTful Web service. */
#if WITH_COAP > 1
  coap_set_header_uri_host(response, "tiki");
  coap_set_header_observe(response, 10);
#if WITH_COAP == 3
  coap_set_header_block(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */
#else
  coap_set_header_proxy_uri(response, "ftp://x");
  coap_set_header_block2(response, 42, 0, 64); /* The block option might be overwritten by the framework when blockwise transfer is requested. */
  coap_set_header_block1(response, 23, 0, 16);
  coap_set_header_accept(response, TEXT_PLAIN);
  coap_set_header_if_none_match(response);
#endif /* CoAP > 03 */
#endif /* CoAP-specific example */
}
#endif /* REST_RES_MIRROR */

#if REST_RES_CHUNKS
/*
 * For data larger than REST_MAX_CHUNK_SIZE (e.g., stored in flash) resources must be aware of the buffer limitation
 * and split their responses by themselves. To transfer the complete resource through a TCP stream or CoAP's blockwise transfer,
 * the byte offset where to continue is provided to the handler as int32_t pointer.
 * These chunk-wise resources must set the offset value to its new position or -1 of the end is reached.
 * (The offset for CoAP's blockwise transfer can go up to 2'147'481'600 = ~2047 M for block size 2048 (reduced to 1024 in observe-03.)
 */
RESOURCE(chunks, METHOD_GET, "debug/chunks", "title=\"Blockwise demo\";rt=\"Data\"");

#define CHUNKS_TOTAL    2050

void
chunks_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int32_t strpos = 0;

  /* Check the offset for boundaries of the resource data. */
  if (*offset>=CHUNKS_TOTAL)
  {
    REST.set_response_status(response, REST.status.BAD_OPTION);
    /* A block error message should not exceed the minimum block size (16). */

    const char *error_msg = "BlockOutOfScope";
    REST.set_response_payload(response, error_msg, strlen(error_msg));
    return;
  }

  /* Generate data until reaching CHUNKS_TOTAL. */
  while (strpos<preferred_size)
  {
    strpos += snprintf((char *)buffer+strpos, preferred_size-strpos+1, "|%ld|", *offset);
  }

  /* snprintf() does not adjust return value if truncated by size. */
  if (strpos > preferred_size)
  {
    strpos = preferred_size;
  }

  /* Truncate if above CHUNKS_TOTAL bytes. */
  if (*offset+(int32_t)strpos > CHUNKS_TOTAL)
  {
    strpos = CHUNKS_TOTAL - *offset;
  }

  REST.set_response_payload(response, buffer, strpos);

  /* IMPORTANT for chunk-wise resources: Signal chunk awareness to REST engine. */
  *offset += strpos;

  /* Signal end of resource representation. */
  if (*offset>=CHUNKS_TOTAL)
  {
    *offset = -1;
  }
}
#endif

#if defined (PLATFORM_HAS_BUTTON) && REST_RES_SEPARATE && WITH_COAP > 3
/* Required to manually (=not by the engine) handle the response transaction. */
#include "er-coap-07-separate.h"
#include "er-coap-07-transactions.h"
/*
 * CoAP-specific example for separate responses.
 * Note the call "rest_set_pre_handler(&resource_separate, coap_separate_handler);" in the main process.
 * The pre-handler takes care of the empty ACK and updates the MID and message type for CON requests.
 * The resource handler must store all information that required to finalize the response later.
 */
RESOURCE(separate, METHOD_GET, "debug/separate", "title=\"Separate demo\"");

/* A structure to store the required information */
typedef struct application_separate_store {
  /* Provided by Erbium to store generic request information such as remote address and token. */
  coap_separate_t request_metadata;
  /* Add fields for addition information to be stored for finalizing, e.g.: */
  char buffer[16];
} application_separate_store_t;

static uint8_t separate_active = 0;
static application_separate_store_t separate_store[1];

void
separate_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  /*
   * Example allows only one open separate response.
   * For multiple, the application must manage the list of stores.
   */
  if (separate_active)
  {
    REST.set_response_status(response, REST.status.SERVICE_UNAVAILABLE);
    const char *msg = "AlreadyInUse";
    REST.set_response_payload(response, msg, strlen(msg));
  }
  else
  {
    separate_active = 1;

    /* Take over and skip response by engine. */
    coap_separate_yield(request, &separate_store->request_metadata);
    /* Be aware to respect the Block2 option, which is also stored in the coap_separate_t. */

    /*
     * At the moment, only the minimal information is stored in the store (client address, port, token, MID, type, and Block2).
     * Extend the store, if the application requires additional information from this handler.
     * buffer is an example field for custom information.
     */
    snprintf(separate_store->buffer, sizeof(separate_store->buffer), "StoredInfo");
  }
}

void
separate_finalize_handler()
{
  if (separate_active)
  {
    coap_transaction_t *transaction = NULL;
    if ( (transaction = coap_new_transaction(separate_store->request_metadata.mid, &separate_store->request_metadata.addr, separate_store->request_metadata.port)) )
    {
      coap_packet_t response[1]; /* This way the packet can be treated as pointer as usual. */

      /* Restore the request information for the response. */
      coap_separate_resume(response, &separate_store->request_metadata, CONTENT_2_05);

      coap_set_payload(response, separate_store->buffer, strlen(separate_store->buffer));

      /*
       * Be aware to respect the Block2 option, which is also stored in the coap_separate_t.
       * As it is a critical option, this example resource pretends to handle it for compliance.
       */
      coap_set_header_block2(response, separate_store->request_metadata.block2_num, 0, separate_store->request_metadata.block2_size);

      /* Warning: No check for serialization error. */
      transaction->packet_len = coap_serialize_message(response, transaction->packet);
      coap_send_transaction(transaction);
      /* The engine will clear the transaction (right after send for NON, after acked for CON). */

      separate_active = 0;
    }
    else
    {
      /*
       * Set timer for retry, send error message, ...
       * The example simply waits for another button press.
       */
    }
  } /* if (separate_active) */
}
#endif

#if REST_RES_PUSHING
/*
 * Example for a periodic resource.
 * It takes an additional period parameter, which defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions by managing a list of subscribers to notify.
 */
PERIODIC_RESOURCE(pushing, METHOD_GET, "debug/push", "title=\"Periodic demo\";rt=\"Observable\"", 5*CLOCK_SECOND);

void
pushing_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);

  /* Usually, a CoAP server would response with the resource representation matching the periodic_handler. */
  const char *msg = "It's periodic!";
  REST.set_response_payload(response, msg, strlen(msg));

  /* A post_handler that handles subscriptions will be called for periodic resources by the REST framework. */
}

/*
 * Additionally, a handler function named [resource name]_handler must be implemented for each PERIODIC_RESOURCE.
 * It will be called by the REST manager process with the defined period.
 */
int
pushing_periodic_handler(resource_t *r)
{
  static uint32_t periodic_i = 0;
  static char content[16];

  PRINTF("TICK /%s\n", r->url);
  periodic_i = periodic_i + 1;

  /* Notify the registered observers with the given message type, observe option, and payload. */
  REST.notify_subscribers(r->url, 1, periodic_i, (uint8_t *)content, snprintf(content, sizeof(content), "TICK %lu", periodic_i));
  /*                              |-> implementation-specific, e.g. CoAP: 1=CON and 0=NON notification */

  return 1;
}
#endif

#if defined (PLATFORM_HAS_BUTTON) && REST_RES_EVENT
/*
 * Example for an event resource.
 * Additionally takes a period parameter that defines the interval to call [name]_periodic_handler().
 * A default post_handler takes care of subscriptions and manages a list of subscribers to notify.
 */
EVENT_RESOURCE(event, METHOD_GET, "sensors/button", "title=\"Event demo\";rt=\"Observable\"");

void
event_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  /* Usually, a CoAP server would response with the current resource representation. */
  const char *msg = "It's eventful!";
  REST.set_response_payload(response, (uint8_t *)msg, strlen(msg));

  /* A post_handler that handles subscriptions/observing will be called for periodic resources by the framework. */
}

/* Additionally, a handler function named [resource name]_event_handler must be implemented for each PERIODIC_RESOURCE defined.
 * It will be called by the REST manager process with the defined period. */
int
event_event_handler(resource_t *r)
{
  static uint32_t event_i = 0;
  static char content[10];

  PRINTF("EVENT /%s\n", r->url);
  ++event_i;

  /* Notify registered observers with the given message type, observe option, and payload.
   * The token will be set automatically. */

  // FIXME provide a rest_notify_subscribers call; how to manage specific options such as COAP_TYPE?
  REST.notify_subscribers(r->url, 0, event_i, content, snprintf(content, sizeof(content), "EVENT %lu", event_i));
  return 1;
}
#endif /* PLATFORM_HAS_BUTTON */

#if defined (PLATFORM_HAS_LEDS)
#if REST_RES_LEDS
/*A simple actuator example, depending on the color query parameter and post variable mode, corresponding led is activated or deactivated*/
RESOURCE(leds, METHOD_POST | METHOD_PUT , "actuators/leds", "title=\"LEDs: ?color=r|g|b, POST/PUT mode=on|off\";rt=\"Control\"");

void
leds_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *color = NULL;
  const char *mode = NULL;
  uint8_t led = 0;
  int success = 1;

  if ((len=REST.get_query_variable(request, "color", &color))) {
    PRINTF("color %.*s\n", len, color);

    if (strncmp(color, "r", len)==0) {
      led = LEDS_RED;
    } else if(strncmp(color,"g", len)==0) {
      led = LEDS_GREEN;
    } else if (strncmp(color,"b", len)==0) {
      led = LEDS_BLUE;
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (success && (len=REST.get_post_variable(request, "mode", &mode))) {
    PRINTF("mode %s\n", mode);

    if (strncmp(mode, "on", len)==0) {
      leds_on(led);
    } else if (strncmp(mode, "off", len)==0) {
      leds_off(led);
    } else {
      success = 0;
    }
  } else {
    success = 0;
  }

  if (!success) {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  }
}
#endif

#if REST_RES_TOGGLE
/* A simple actuator example. Toggles the red led */
RESOURCE(toggle, METHOD_GET | METHOD_PUT | METHOD_POST, "actuators/toggle", "title=\"Red LED\";rt=\"Control\"");
void
toggle_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  leds_toggle(LEDS_RED);
}
#endif
#endif /* PLATFORM_HAS_LEDS */

#if defined (PLATFORM_HAS_LIGHT) && REST_RES_LIGHT
/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(light, METHOD_GET, "sensors/light", "title=\"Photosynthetic and solar light (supports JSON)\";rt=\"LightSensor\"");
void
light_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  uint16_t light_photosynthetic = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
  uint16_t light_solar = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);

  const uint16_t *accept = NULL;
  int num = REST.get_header_accept(request, &accept);

  if ((num==0) || (num && accept[0]==REST.type.TEXT_PLAIN))
  {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    snprintf(buffer, REST_MAX_CHUNK_SIZE, "%u;%u", light_photosynthetic, light_solar);

    REST.set_response_payload(response, (uint8_t *)buffer, strlen(buffer));
  }
  else if (num && (accept[0]==REST.type.APPLICATION_XML))
  {
    REST.set_header_content_type(response, REST.type.APPLICATION_XML);
    snprintf(buffer, REST_MAX_CHUNK_SIZE, "<light photosynthetic=\"%u\" solar=\"%u\"/>", light_photosynthetic, light_solar);

    REST.set_response_payload(response, buffer, strlen(buffer));
  }
  else if (num && (accept[0]==REST.type.APPLICATION_JSON))
  {
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    snprintf(buffer, REST_MAX_CHUNK_SIZE, "{'light':{'photosynthetic':%u,'solar':%u}}", light_photosynthetic, light_solar);

    REST.set_response_payload(response, buffer, strlen(buffer));
  }
  else
  {
    REST.set_response_status(response, REST.status.UNSUPPORTED_MADIA_TYPE);
    const char *msg = "Supporting content-types text/plain, application/xml, and application/json";
    REST.set_response_payload(response, msg, strlen(msg));
  }
}
#endif /* PLATFORM_HAS_LIGHT */

#if defined (PLATFORM_HAS_BATTERY) && REST_RES_BATTERY
/* A simple getter example. Returns the reading from light sensor with a simple etag */
RESOURCE(battery, METHOD_GET, "sensors/battery", "title=\"Battery status\";rt=\"Battery\"");
void
battery_handler(void* request, void* response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int battery = battery_sensor.value(0);

  const uint16_t *accept = NULL;
  int num = REST.get_header_accept(request, &accept);

  if ((num==0) || (num && accept[0]==REST.type.TEXT_PLAIN))
  {
    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    snprintf(buffer, REST_MAX_CHUNK_SIZE, "%d", battery);

    REST.set_response_payload(response, (uint8_t *)buffer, strlen(buffer));
  }
  else if (num && (accept[0]==REST.type.APPLICATION_JSON))
  {
    REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
    snprintf(buffer, REST_MAX_CHUNK_SIZE, "{'battery':%d}", battery);

    REST.set_response_payload(response, buffer, strlen(buffer));
  }
  else
  {
    REST.set_response_status(response, REST.status.UNSUPPORTED_MADIA_TYPE);
    const char *msg = "Supporting content-types text/plain and application/json";
    REST.set_response_payload(response, msg, strlen(msg));
  }
}
#endif /* PLATFORM_HAS_BATTERY */

PROCESS(rest_server_example, "Rest Server Example");
AUTOSTART_PROCESSES(&rest_server_example);

PROCESS_THREAD(rest_server_example, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("Rest Example\n");

#ifdef RF_CHANNEL
  PRINTF("RF channel: %u\n", RF_CHANNEL);
#endif
#ifdef IEEE802154_PANID
  PRINTF("PAN ID: 0x%04X\n", IEEE802154_PANID);
#endif

  PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
  PRINTF("LL header: %u\n", UIP_LLH_LEN);
  PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
  PRINTF("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);

/* if static routes are used rather than RPL */
#if !UIP_CONF_IPV6_RPL && !defined (CONTIKI_TARGET_MINIMAL_NET)
  set_global_address();
  configure_routing();
#endif

  /* Initialize the REST engine. */
  rest_init_engine();

  /* Activate the application-specific resources. */
#if REST_RES_HELLO
  rest_activate_resource(&resource_helloworld);
#endif
#if REST_RES_MIRROR
  rest_activate_resource(&resource_mirror);
#endif
#if REST_RES_CHUNKS
  rest_activate_resource(&resource_chunks);
#endif
#if REST_RES_PUSHING
  rest_activate_periodic_resource(&periodic_resource_pushing);
#endif
#if defined (PLATFORM_HAS_BUTTON) && REST_RES_EVENT
  rest_activate_event_resource(&resource_event);
#endif
#if defined (PLATFORM_HAS_BUTTON) && REST_RES_SEPARATE && WITH_COAP > 3
  /* Use this pre-handler for separate response resources. */
  rest_set_pre_handler(&resource_separate, coap_separate_handler);
  rest_activate_resource(&resource_separate);
#endif
#if defined (PLATFORM_HAS_BUTTON) && (REST_RES_EVENT || (REST_RES_SEPARATE && WITH_COAP > 3))
  SENSORS_ACTIVATE(button_sensor);
#endif
#if defined (PLATFORM_HAS_LEDS)
#if REST_RES_LEDS
  rest_activate_resource(&resource_leds);
#endif
#if REST_RES_TOGGLE
  rest_activate_resource(&resource_toggle);
#endif
#endif /* PLATFORM_HAS_LEDS */
#if defined (PLATFORM_HAS_LIGHT) && REST_RES_LIGHT
  SENSORS_ACTIVATE(light_sensor);
  rest_activate_resource(&resource_light);
#endif
#if defined (PLATFORM_HAS_BATTERY) && REST_RES_BATTERY
  SENSORS_ACTIVATE(battery_sensor);
  rest_activate_resource(&resource_battery);
#endif

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
#if defined (PLATFORM_HAS_BUTTON)
    if (ev == sensors_event && data == &button_sensor) {
      PRINTF("BUTTON\n");
#if REST_RES_EVENT
      /* Call the event_handler for this application-specific event. */
      event_event_handler(&resource_event);
#endif
#if REST_RES_SEPARATE && WITH_COAP>3
      /* Also call the separate response example handler. */
      separate_finalize_handler();
#endif
    }
#endif /* PLATFORM_HAS_BUTTON */
  } /* while (1) */

  PROCESS_END();
}